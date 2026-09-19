# Error strategy
This document is to decide first on how we handle possible errors in the pipeline. The number of outcomes is set by the number of distinct actions the caller takes, not by the number of things that can go wrong. In the following I am rolling out on what the possible outcomes can be and what the caller does.

## Library boundaries

`tm_core` is the library target in the CMake layout. It contains the framer, decoder, CRC, and typed message structs. This is pure computation: no files, sockets, `<iostream>`, or OS headers. It must be unit-testable with hand-built byte arrays.

Reading from a file or socket belongs in a later `tm_io` target or similar. `tm_tests` links `tm_core`.

## Decisions

- `tm_core` does not throw exceptions. A corrupt frame is normal on a radio link, not exceptional, and control flow expected thousands of times must not go through exception machinery.
- Decoding does not allocate. It writes into a fixed-size struct owned by the caller, and every failure payload is a few bytes.
- The framer returns a small custom result type with the three outcomes below and outcome-specific payloads.
- `Error` is a struct containing an error code and a `uint8_t detail` field. The detail is unused for a bad checksum, but the simple fixed-size representation keeps decoder control flow and call sites straightforward.
- The framer and decoder share one locally implemented `Result<T, E>` template with distinct payload and error types.
- Unknown message IDs do not carry the raw payload, which keeps the decoder allocation-free.
- A known message id with the wrong payload length is its own outcome, not folded into unknown id. The caller's action differs: an unknown id is skipped in confidence, a length mismatch is counted as a defect.

## Framer
| Outcome               | Carries                   | What the caller does                                 |
| ----------------------|---------------------------|------------------------------------------------------|
| Found a frame         | offset and length         | hand that span to the decoder, then drop those bytes |
| Incomplete            | nothing                   | wait for more data, call again                       |
| No frame at the front | how many bytes to discard | drop those bytes, call again immediately             |

When no frame is at the front, the framer scans to the next sync word and reports the whole garbage prefix in one result. The sync word can occur by chance inside a payload, causing a false start; the CRC catches that, and the cost of resynchronising again is one wasted frame.

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
