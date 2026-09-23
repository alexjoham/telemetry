# Decode

`decode()` turns a byte span already identified as one candidate frame into either a typed frame or one of the outcomes [0001](0001-error-strategy.md#decoder) names. This records `decode()`'s own contract.

## Signature and ownership

`decode(std::span<const std::byte> frame) noexcept -> Result<DecodedFrame, Error>`, `[[nodiscard]]` and `constexpr` where the body allows. It takes the whole frame as a span, owns nothing, allocates nothing.

Every precondition is a checked gate, not an `assert`: `assert` is gone under `-DNDEBUG`, and the exit criterion is no out-of-bounds read on any input, not only ones `frame()` produced, `decode()` is called directly in tests too.

## CRC coverage

Covers `span.first(span.size() - kCrcSize)`, from `span.size()`, never the length field. The span's size is a property of the caller's object; the length field is untrusted content until the CRC that covers it has passed, the same rule that keeps the framer off the version byte (see [Resynchronisation](../format.md#resynchronisation)). The subtraction only runs once the size gate below has confirmed the span is at least `kFixedHeaderSize + kCrcSize` long.

## A fifth outcome: malformed frame

Shorter than `kFixedHeaderSize + kCrcSize`, or a length field that disagrees with the span size once the CRC has passed. Distinct from bad checksum: it means the caller or the framer built the span wrong, not that the link is noisy, so the action is to count it and fix the code, not resynchronise.

Rejected: a `Frame` type only the framer can construct, making this unrepresentable rather than checked. Stronger in principle, but `frame()` returns `Found{length}`, not a span or an object wrapping one ([0001](0001-error-strategy.md#rejected-alternatives)) — the caller does the slicing this check exists to catch, so the branded type would be checking itself. The guarantee here is one comparison away.

## Gate order

| Order | Gate                      | Needs                                                     | Fails as              |
| ----- | ------------------------- | ---------------------------------------------------------- | ---------------------- |
| 1     | Size                      | nothing but the span                                        | Malformed frame        |
| 2     | CRC                       | a span at least `kFixedHeaderSize + kCrcSize` long           | Bad checksum           |
| 3     | Length-field consistency  | a verified span, so the length field is trustworthy          | Malformed frame        |
| 4     | Version                   | a consistent length field                                    | Unsupported version    |
| 5     | Message id                | a supported version, since the id table is version-dependent | Unknown message id     |
| 6     | Payload length            | a known id, since only a known id has an expected length     | Wrong payload length   |
| 7     | Extraction                | all of the above                                              | Decoded                |

Order by what each gate needs to be meaningful, not by severity: an unknown id has no expected length, so length cannot be asked first; the id table is version-dependent, so version cannot be asked second.

Pinned by two frames, each with a valid CRC and each failing several later gates at once, asserting which error comes back, one frame with an unsupported version that would also be an unknown id, one with a known version and unknown id whose payload length would also be wrong.

## Result<T, E>

Returns `Result<DecodedFrame, Error>` ([0005](0005-result-type.md)). `Error` is [0001](0001-error-strategy.md)'s existing `{code, detail}` struct, gaining a fifth code; `detail` stays unused for it, as for bad checksum.

## Decoded types

`FrameHeader` (version, message id, sequence, timestamp) and `VehicleState` (actuator flags, drive mode, steering count, brake count) are returned together as `DecodedFrame`. When a second message id exists, the payload becomes a variant; the header does not change.

`sizeof`/`alignof` for both were derived by hand, then pinned as `static_assert`s: `FrameHeader` packs to 8 bytes at 4-byte alignment, `VehicleState` to 6 bytes at 2-byte alignment, neither with padding at these field widths and orderings. Raw counts only, per [0003](0003-decoded-units.md).

`DriveMode` is `enum class : std::uint8_t` with all four values from [VehicleState](../format.md#vehiclestate)'s drive-mode table defined, converted by a total `static_cast` from the 2-bit mask at bits 4-5, no `switch`, no `default:` — the mask has exactly four possible values and the table defines all four, so the cast cannot produce an enumerator the table doesn't cover. The comment at the cast site says so, since the totality is a fact about the mask's width, not something the line itself shows.
