# Error strategy
This document is to decide first on how we handle possible errors in the pipeline. The number of outcomes is set by the number of distinct actions the caller takes, not by the number of things that can go wrong. In the following I am rolling out on what the possible outcomes can be and what the caller does.

## Library boundaries

`tm_core` is the library target in the CMake layout. It contains the framer, decoder, CRC, and typed message structs. This is pure computation: no files, sockets, `<iostream>`, or OS headers. It must be unit-testable with hand-built byte arrays.

It is an `INTERFACE` target today, because `crc16` is `constexpr` and the only symbol so far, so its definition sits in the header and no TU is left to compile. It becomes a compiled library again at the first symbol that cannot be `constexpr`, which the framer's buffer handling probably is. The purity described above is what makes the header-only shape available at all, so the two are not in tension, but the target type is an artefact of how little exists yet and should not be read as a decision to stay header-only. Superseded by [0004](0004-header-only.md): the framer turned out to be `constexpr`, and header-only is now the decision.

While `tm_core` has no sources it does not link `tm_warnings`: there is nothing to warn about, and the headers are compiled under the full warning set by `tm_header_tus` (see [0002](0002-header-hygiene.md)). That link must come back with the sources.

Reading from a file or socket belongs in a later `tm_io` target or similar. `tm_tests` links `tm_core`.

## Decisions

- `tm_core` does not throw exceptions. A corrupt frame is normal on a radio link, not exceptional, and control flow expected thousands of times must not go through exception machinery. Where the claim is true of a given function it is written as `noexcept` rather than left to this document, so a later change that breaks it fails to compile instead of silently contradicting the decision. `crc16` is marked; the framer and decoder are expected to be.
- Decoding does not allocate. It writes into a fixed-size struct owned by the caller, and every failure payload is a few bytes.
- The framer returns `std::variant<Found, Incomplete, Discard>`, with `Found{length}`, `Incomplete{}` empty, and `Discard{count}`. Each alternative carries exactly the payload its outcome has.
- The framer only inspects the front of the buffer. It never reports a found frame at a nonzero offset.
- `Error` is a struct containing an error code and a `uint8_t detail` field. The detail is unused for a bad checksum, but the simple fixed-size representation keeps decoder control flow and call sites straightforward.
- The decoder uses a locally implemented `Result<T, E>` template with distinct payload and error types. The framer does not: it has three outcomes, not two.
- Unknown message IDs do not carry the raw payload, which keeps the decoder allocation-free.
- A known message id with the wrong payload length is its own outcome, not folded into unknown id. The caller's action differs: an unknown id is skipped in confidence, a length mismatch is counted as a defect.

## Framer
| Outcome               | Carries                   | What the caller does                            |
| ----------------------|---------------------------|--------------------------------------------------|
| Found a frame         | length                    | decode the first `length` bytes, then drop `length` |
| Incomplete            | nothing                   | wait for more data, call again                  |
| No frame at the front | how many bytes to discard | drop those bytes, call again immediately        |

The framer only inspects the front of the buffer, so a found frame always starts at offset zero and the outcome carries a length alone. Garbage in front of a frame comes back as its own discard outcome, and only the following call reports found. Every call therefore drops exactly the one number it was given.

When no frame is at the front, the framer scans to the next sync word and reports the whole garbage prefix in one result. The sync word can occur by chance inside a payload, causing a false start; the CRC catches that, and the cost of resynchronising again is one wasted frame.

A trailing `0xA5` is a sync word the framer cannot rule out yet, so it is never part of the prefix, and a discard count is therefore never zero. See [Resynchronisation](../format.md#resynchronisation) for the rule and for why a zero count would spin the caller's loop.

`Incomplete` stays empty: a byte count is unavailable when the header itself is truncated, and unactionable in any case.

## Decoder

First the checksum is checked to see if the frame is even valid before looking into the other fields.
The idea of the following table is to decide on what the possible outcomes by the decoders are.
The decoder returns a `Result<T, Error>` that returns either a decoded frame or an error describing why decoding failed.

| Outcome             | Carries          | What the caller does                            |
| --------------------|------------------|-------------------------------------------------|
| Decoded             | the typed frame  | use it, continue                                |
| Bad checksum        | nothing          | discard, and distrust the framing of this frame |
| Unsupported version | the version byte | discard this frame, log once, continue          |
| Unknown message id  | the id           | skip this frame, continue with full confidence  |
| Wrong payload length | the message id  | count it, skip this frame, continue             |

Note to the bad checksum and unknown message id cases: A bad checksum makes the length field untrustworthy, and the framer used that length to find the frame end, so the boundary itself is suspect. An unknown id sits inside a frame whose integrity is proven, so the boundary holds.

Wrong payload length means a known message id carrying a payload that is not the size that id defines. The boundary holds for the same reason as the unknown id case, and the length field was itself covered by the CRC, so the caller continues normally. It is a separate outcome from unknown id because the two mean opposite things: an unknown id is forward compatibility working as intended, while a length mismatch means sender and receiver disagree about a message both claim to understand. That is a defect somewhere, so a receiver counts it rather than skipping it silently. The error carries the message id, because the useful action is attributing the defect to a message type; the length itself is in the frame the caller already holds. The expected length per id is in [format.md](../format.md#message-ids).

## Rejected alternatives

- `std::optional<Frame>` was rejected because it cannot carry a reason, and the caller's action differs by reason.
- Exceptions were rejected because corrupt frames are expected radio-link input and should not use exception machinery.
- Discarding one byte at a time during resynchronisation was rejected because scanning to the next sync word discards a whole garbage run in one call; false sync inside a payload is caught by the CRC at the cost of one wasted frame.
- **A framer that skips garbage and reports found at an offset**, so that a run of noise followed by a frame yields one result of `found at offset 5`. Three reasons. It forces the caller to compute `offset + length` to know what to drop, so the drop count is derived rather than given. A single call can both skip and find, so a bug in the skip path is only observable through the find path, which makes the function harder to test. And discarded bytes become invisible: the caller learns of them only by noticing a nonzero offset, whereas a separate discard outcome can be counted, and on a radio link that count is a link quality metric.
- **A struct of `{Kind kind; std::size_t value;}`** for the framer result. It permits a caller to read a payload that does not exist. The concrete failure: a caller hoists `buffer.drop_front(r.value)` out of the branch because two of the three outcomes drop `value`, which compiles and is correct for found and discard. On incomplete it drops whatever the framer left in the field. The symptom is silent frame loss that appears only when frames span read boundaries, so it is invisible on file input and on a quiet link, and shows up as a few percent of frames lost under load with no error reported anywhere. The variant makes that line fail to compile.
- **A class with private fields and asserting accessors**, for the same result. It converts the bug above into a debug-build abort rather than a compile error, which requires the path to execute and assertions to be on.

The rule that reconciles the variant with the decoder's `Error{code, detail}`, where `detail` is meaningless for a bad checksum: a sometimes-meaningless field is acceptable when misreading it produces a wrong message, and unacceptable when misreading it produces wrong control flow. `detail` feeds a log line. `value` would feed arithmetic on the buffer.
