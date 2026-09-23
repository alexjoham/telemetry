# Wire format

In the following the byte-level format that `tm_core`'s framer and decoder implement is defined.

**Endianness**: all multi-byte fields are little-endian, including the CRC field itself. The sync word is the only exception, because it is a byte pattern rather than a number; see [Sync word](#sync-word).

## Frame layout

Fixed header: 12 bytes. CRC: 2 bytes. Total frame length `L = 12 + n + 2`, where `n` is the payload length.

| Offset | Width | Field           | Meaning                                              |
| ------ | ----- | --------------- | ---------------------------------------------------- |
| 0      | 2     | Sync word       | `A5 C3`, see below                                   |
| 2      | 1     | Version         | currently `1`                                        |
| 3      | 1     | Message id      | Predefined Message id                                |
| 4      | 2     | Sequence number | increments per frame, wraps after 65535              |
| 6      | 4     | Timestamp       | microseconds since boot, wraps at about 71 minutes   |
| 10     | 1     | Payload length  | `n`                                                  |
| 11     | 1     | Reserved        | written as zero, ignored on read                     |
| 12     | n     | Payload         | message-specific, see VehicleState below             |
| 12+n   | 2     | CRC-16          | little-endian, coverage below                        |

The length field is one byte, so `n` reaches 255 and the largest frame is `12 + 255 + 2 = 269` bytes. `kMaxFrameSize` in `include/tlm/constants.hpp` is the value to be used anywhere in the code.

### Message ids

| Message id | Name         | Payload length |
| ---------- | ------------ | -------------- |
| `0x01`     | VehicleState | 4 bytes        |

Any id not in this table is unknown. The list grows with the format version, so a receiver must treat an unknown id as a frame to skip rather than an error in the stream; see [0001](decisions/0001-error-strategy.md).

The payload length is fixed per message id, so a listed id arriving with any other length is a defect rather than a variation. That is the wrong payload length outcome in 0001, and it is distinct from an unknown id.

### Sync word

Byte 0 is `0xA5` and byte 1 is `0xC3`. The sync word is a byte sequence rather than an integer, and it is the one field exempt from the little-endian rule above.

The exemption is operational. The framer searches for it as two consecutive bytes and never decodes it as an integer, and every hex dump shows `A5 C3` at the start of a frame.

### Reserved fields

A receiver ignores the reserved byte at offset 11 whatever its value, and ignores reserved payload bits as well. These fields exist so that a version 2 can use them without breaking version 1 framers, and rejecting a frame because a reserved field is nonzero would defeat exactly that purpose. A validator must not reject on them.

### Wrapping fields

Both counters restart at zero when they overflow: the sequence number after 65535 frames, the timestamp after 2^32 microseconds, which is about 71.6 minutes.

Subtracting two readings still gives the right answer across a restart, because unsigned subtraction overflows the same way the counter does. That holds as long as less than one full period passed between the two readings. Subtract at the counter's own width; widening a reading first gives a large wrong number instead.

## Framing

The framer reads exactly two fields: the sync word and the payload length. Frame length is computed rather than read, as `L = fixed_header_size + n + crc_size`, where the length field supplies only `n`. Those two constants live in one place shared by framer and decoder, because the day the header grows a field both must change together or the framer hands the decoder a span off by the difference.

The framer only looks at the front of the buffer, so a frame it finds always starts at offset zero and the result carries a length alone. When the front is not a sync word it scans for the next one and reports that prefix as bytes to discard, in its own result; the frame behind it is reported by the following call. See [0001](decisions/0001-error-strategy.md) for why skipping and finding are never the same call.

### Resynchronisation

When the front is not a sync word, the framer scans for the smallest offset at which a sync word *could* begin and discards everything before it. An offset `i` is such a candidate when either both bytes are present and read `A5 C3`, or `i` is the last byte of the buffer and holds `A5`, because its second byte may still be in flight. With a two-byte sync word only the final byte can be a partial one.

The trailing `A5` is not a detail. On a buffer of `00 00 A5` the framer discards 2, not 3. Discarding all three eats the first byte of a frame that was about to arrive, and that frame can then never be recognised: the `C3` shows up at the front of the next call with nothing in front of it to match.

**The discard count is never zero.** If the candidate offset is zero, the buffer starts with either a complete sync word or a lone `A5`, and neither is a discard: the first is handled by the length logic, the second is `Incomplete`. This is what stops the caller spinning. Every discard is at least one byte, so the buffer strictly shrinks on each drop-and-call-again, and the one case with nothing to drop returns the outcome that means wait for more data rather than call again immediately.

`Incomplete` on a buffer that is entirely a partial sync word is a correct answer even if no further byte ever arrives. The caller waits on its input; it does not loop.

The framing contract is three items, and none may change between versions without breaking every deployed receiver:

1. the sync word,
2. the offset and width of the length field,
3. the total size of the fixed header plus CRC.

Everything else in the header is free to change.

The third item is the easiest to miss, because it is never read from the wire. Suppose version 2 inserts a 4-byte source id into the header. A version-1 framer still assumes a 12-byte header, so every length it computes is four bytes short. It hands the decoder a truncated span and leaves four stray bytes at the front of the buffer.

**The framer does not read the version.** The version gate belongs to the decoder. The framer runs before the CRC is verified, so the version byte is unverified content, and [0001](decisions/0001-error-strategy.md) puts the checksum ahead of any use of content. Acting on it would be impossible anyway: skipping a frame needs a length, and a version that moved the length field leaves the framer nothing to read.

Rejected, and why:

- **A length field near the front of the header.** It would let the framer report how many bytes are still missing, but 0001 gives the incomplete outcome no payload, so the return value is identical either way. Preallocation is the one case where it would matter, and a growing receive buffer does not preallocate.
- **Grouping header fields for locality.** Offsets are compile-time constants and the framer jumps straight to byte 10.

## Checksum

The frame checksum is CRC-16/CCITT-FALSE, catalogued by RevEng under its primary name `CRC-16/IBM-3740`.

| Parameter        | Value  |
| ---------------- | ------ |
| Polynomial       | 0x1021 |
| Initial value    | 0xFFFF |
| Input reflected  | no     |
| Output reflected | no     |
| Final XOR        | 0x0000 |

**Check value: the CRC of the nine ASCII bytes `123456789` under these parameters is `0x29B1`.**

Source: the [RevEng CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/16.htm), entry `width=16 poly=0x1021 init=0xffff refin=false refout=false xorout=0x0000 check=0x29b1 residue=0x0000 name="CRC-16/IBM-3740"`, aliases CRC-16/CCITT-FALSE and CRC-16/AUTOSAR.

The CRC field is stored little-endian on the wire, like every other multi-byte field: low byte first.

### Coverage

The CRC covers bytes 0 through 12+n-1: the sync word and header through the end of the payload, but not the CRC field itself. A checksum cannot cover its own value, which does not exist yet when the encoder computes it.

The sync word is inside the CRC on purpose. [0001](decisions/0001-error-strategy.md) has the framer resynchronise by scanning for the next sync word, and accepts that `A5 C3` inside a payload will sometimes cause a false start. That is only safe if a false start fails the CRC. Covering the sync word is what makes it fail: a frame read from the wrong offset has different bytes under the checksum, and passes about 1 time in 65536.

`decode()` computes this span from the span's own size, never from the length field; see [0006](decisions/0006-decode.md#crc-coverage).

Do not use the `residue=0x0000` from the catalogue entry. It only holds when the CRC is appended high byte first, and this format stores it low byte first. On the example frame below, appending big-endian gives `0x0000` but little-endian gives `0x2EC9`.

## Bit numbering

Bit 0 is the least significant bit of byte 0, and fields fill toward more significant bits, continuing into byte 1.

Bit-packed formats are ambiguous without this sentence, and the 12-bit fields below straddle byte boundaries, so the question cannot be dodged.

## VehicleState

Message id `0x01`. The payload is 4 bytes.

| Bits  | Width | Field                  | Meaning                                    |
| ----- | ----- | ---------------------- | ------------------------------------------ |
| 0–3   | 4     | Actuator engaged flags | one bit per actuator, bit 0 is actuator 0  |
| 4–5   | 2     | Drive mode             | see below                                  |
| 6–17  | 12    | Steering angle         | raw ADC count, 0..4095                     |
| 18–29 | 12    | Brake pressure         | raw ADC count, 0..4095                     |
| 30–31 | 2     | Reserved               | written as zero, ignored on read           |

| Drive mode | Name       |
| ---------- | ---------- |
| 0          | Manual     |
| 1          | Assisted   |
| 2          | Autonomous |
| 3          | Fault      |

See [0006](decisions/0006-decode.md#decoded-types) for the `DriveMode` enum this maps to.

## Worked example

A complete VehicleState frame, 18 bytes (`L = 12 + 4 + 2`):

```
A5 C3 01 01 41 9C 4D 3C 2B 1A 04 00 EB 68 7D 32 85 4A
```

| Field            | Bytes         | Decoded value                          |
| ---------------- | ------------- | -------------------------------------- |
| Sync word        | `A5 C3`       | —                                      |
| Version          | `01`          | 1                                      |
| Message id       | `01`          | VehicleState                           |
| Sequence number  | `41 9C`       | 40001                                  |
| Timestamp        | `4D 3C 2B 1A` | 439041101 µs, about 7 min 19 s         |
| Payload length   | `04`          | 4                                      |
| Reserved         | `00`          | 0                                      |
| Payload          | `EB 68 7D 32` | fields below                           |
| CRC-16           | `85 4A`       | `0x4A85`, over bytes 0–15              |

| Payload field          | Decoded value                              |
| ---------------------- | ------------------------------------------ |
| Actuator engaged flags | `0b1011`, actuators 0, 1 and 3 engaged     |
| Drive mode             | 2, Autonomous                              |
| Steering angle         | `0x5A3` = 1443 counts                      |
| Brake pressure         | `0xC9F` = 3231 counts                      |
| Reserved               | 0                                          |

The four payload bytes follow from the bit numbering above. Packed MSB first, the 32-bit word is `00 | 110010011111 | 010110100011 | 10 | 1011` = `0x327D68EB`, which little-endian on the wire is `EB 68 7D 32`. The steering angle field starts at bit 6, so it occupies the top two bits of byte 0, all of byte 1, and the low two bits of byte 2.

The CRC `0x4A85` was computed from this document's parameters over bytes 0–15, by two independent implementations that both reproduce the `0x29B1` check value. It does not come from a decoder, so a decoder test written against it is not circular.

The example is normative: a conforming decoder maps that byte sequence to exactly those field values. Like the check value above, it originates outside the implementation.

## Decoded values

`decode` returns raw counts, not engineering units. A 12-bit sensor field arrives as a `uint16_t` holding 0..4095. See [0003-decoded-units.md](decisions/0003-decoded-units.md) for the rule and [0006-decode.md](decisions/0006-decode.md#decoded-types) for the struct layout.

The mappings below are **informative**. `tm_core` does not implement them, and a conversion function must live outside it. They are recorded here because a count is meaningless without the range it maps to.

| Field          | Mapping                       | Full scale             |
| -------------- | ----------------------------- | ---------------------- |
| Steering angle | `deg = (count - 2048) * 0.25` | -512.00 .. +511.75 deg |
| Brake pressure | `bar = count * 0.0625`        | 0 .. 255.94 bar        |

For the example frame above: 1443 counts is -151.25 deg, and 3231 counts is 201.94 bar.
