# Stream

`frame()` and `decode()` each handle one step on one span. The loop that connects them, and that [0001](0001-error-strategy.md#framer) leaves to "the caller", moves into `tm_core` as a stream.

## Output shape

The stream is a pull-style object over a borrowed span: `Stream{std::span<const std::byte>}`, `next() noexcept -> std::optional<Event>` and `consumed()`, all `constexpr`. `Event` is `std::variant<DecodedFrame, Error, Discard, Resync>`. `Resync{Error error; std::size_t count;}` is the only new type. `next()` returns `std::nullopt` only when the stream needs more bytes. The caller pulls events, then drops `consumed()` bytes from its buffer.

Pull bounds the work per call, which a Teensy control loop needs. Each `next()` makes at most one `frame()` call and at most one `decode()` call. That is at most one CRC over at most `kMaxFrameSize` bytes, plus the framer's scan for the next sync word, which is linear in the remaining buffer. The control loop decides how many events fit in a tick, and can stop after any one of them. A callback handed a buffer that filled during a burst runs a CRC for every frame in it before returning.

Discards are their own event, not an internal counter, because [0001](0001-error-strategy.md#rejected-alternatives) treats the discard count as a link-quality metric.

## Resumption invariant

**After any event, dropping `consumed()` and building a new `Stream` loses nothing and repeats nothing.** The caller rebuilds the `Stream` for each read and may stop early, so the stream may hold no state that `consumed()` cannot express. Every call therefore drops its bytes before it returns, and `Stream` holds only a span and an offset.

## Policy per error

| Code               | Event                                     | Why                                                          |
| ------------------ | ----------------------------------------- | ------------------------------------------------------------ |
| BadChecksum        | `Resync{error, kResyncShift}`             | [0001, recovery](0001-error-strategy.md#recovery-after-a-rejected-frame) |
| UnsupportedVersion | `Error`, frame skipped: drops `length`    | [0001, decoder](0001-error-strategy.md#decoder)              |
| UnknownMessageId   | `Error`, frame skipped                    | same                                                         |
| WrongPayloadLength | `Error`, frame skipped                    | same                                                         |
| MalformedFrame     | `Resync{error, kResyncShift}`             | below                                                        |

The caller can count and log every rejection per 0001's table, from `Error` or from `Resync::error`. `Event` can also represent states the stream never produces, so the rule is written here: a plain `Error` event carries only the three skip codes, and the tests pin it.

The stream cannot reach MalformedFrame. `frame()` sizes `Found{length}` from byte 10, the stream slices exactly `length` bytes, and `decode()`'s size and length-consistency gates read that same byte. If MalformedFrame comes back anyway, the frame boundary is wrong, which is exactly what resynchronisation recovers from. Sharing the BadChecksum path means that path's tests run every line this branch would.

## Byte accounting

Each event reports what its call dropped:

- `DecodedFrame` and a skip `Error`: the frame's `length`.
- `Discard{n}`: `n` bytes.
- `Resync{e, n}`: `n` bytes.

So `consumed()` equals the lengths of decoded and skipped frames, plus all `Discard` and `Resync` counts. The link-quality metric is the sum of the `Discard` and `Resync` counts, false sync words included, and it can be read from the events alone.

`Resync` carries its count, so the caller never multiplies `kResyncShift` by a number of bad checksums. That would be a derived count, which 0001 rejected for found-at-offset. The count cannot ride in `Error::detail`, because `detail` feeds log lines, never buffer arithmetic. The caller's buffer drop is always `consumed()`, never an event field.

Events do not carry frame lengths, so the full invariant cannot be checked from the event stream alone. Tests take the expected total from the fixtures they built.

## Ownership

The caller owns the bytes. `Stream` must not outlive the buffer, and compacting or appending invalidates it.

Nothing returned points into the input: every event is returned by value. That relies on [0001](0001-error-strategy.md#decisions) not carrying the raw payload for unknown ids. An event that carries payload bytes would reopen this section.

A false start whose length byte points past the end of the buffer comes back as `Incomplete`. The stream then waits for up to `kMaxFrameSize` bytes, and those bytes will fail their CRC. So the caller's buffer must hold at least `kMaxFrameSize` bytes, or the stream stalls for good.

## What the tests will pin

- **Decoded:** the worked example on its own yields one `DecodedFrame`, then `std::nullopt`, with `consumed() == 18`.
- **Discard:** garbage before the worked example yields `Discard{n}`, then the frame.
- **BadChecksum:** a false `A5 C3` in garbage, with a length byte that fits inside the buffer, yields `Resync{BadChecksum, 2}`, the framer's discard, then the real frame.
- **UnsupportedVersion:** a valid-CRC frame with version `0x02`, followed by the worked example, yields the error, then the frame, with nothing between them. A `Resync` or a `Discard` there would mean a resync instead of a skip.
- **UnknownMessageId:** the same, with id `0x02`.
- **WrongPayloadLength:** the same, with a 5-byte VehicleState payload.
- **MalformedFrame:** unreachable from any input, and its path is pinned by the BadChecksum row. The mapping onto that path is an equivalent mutant, recorded as one.
- **Incomplete:** the worked example minus its last byte yields `std::nullopt`, with `consumed() == 0`.

Every stream test also asserts the byte-accounting total, because a wrong drop anywhere breaks it, and asserts that no plain `Error` event carries BadChecksum or MalformedFrame.

The expected shift is the literal `2`, derived from the spec's sync word, not `kResyncShift`. The constant is the value on test, so an oracle built from it would agree with a wrong shift. Offsets and sizes stay named constants, because they only build the fixtures. This is the rule `recovery_test.cpp` already follows.

One mixed stream: garbage, a good frame, a frame with one payload byte flipped, garbage holding a false sync word, an unknown-id frame, a good frame, and a partial frame at the tail. The flip is in the payload because a flipped length byte could end the test in `Incomplete` before the resync runs. The false sync word has the same constraint: its claimed frame must fit inside the buffer, or the framer returns `Incomplete`, and the unknown-id and final good frames never run. Its byte 10 may fall inside the following real frame, so the fixture computes the claimed length from its own bytes and `static_asserts` that it fits, as `recovery_test.cpp` does for its adjacent false start. The test asserts the exact event sequence, and that `consumed()` stops at the partial frame's first byte.

The same bytes are then fed in three more ways, each with the buffer compacted by `consumed()` between `Stream`s:

- split at each single offset, with each `Stream` drained;
- byte-at-a-time, with each `Stream` drained; this is the hardest split because of the trailing-`A5` rule;
- an event budget: all bytes at once, but at most one event per `Stream` before rebuilding. This mode pins the resumption invariant. The two modes above drain every `Stream`, so they cannot see state lost on a rebuild.

The framer reports garbage per call, so events cannot match one for one. Unsplit, `11 22 33 A5 C3 …` is `Discard{3}`, but split after `22` it is `Discard{2}`, `Discard{1}`. The invariant is the same sequence after merging adjacent `Discard`s, plus the same total consumed, summed over all `Stream`s.

## Rejected alternatives

- **`std::vector<Event>`:** unbounded allocation, against [0001](0001-error-strategy.md#decisions).
- **A callback per frame:** unbounded work per call, see above.
- **An `Incomplete` alternative in `Event`:** it carries nothing, and every visitor would have to handle it just to end the loop.
- **A stream that owns a ring buffer:** it moves compaction and a capacity into `tm_core`, and it loses the property that one hand-built array is a complete test case.
- **An `Error` event followed by `Discard{kResyncShift}` on the next call:** a pending flag in the `Stream` breaks the resumption invariant. A caller that stops after the error drops nothing, its next `Stream` meets the same false start, and the error repeats. A budget of one event per tick then livelocks on it.
- **Dropping the shift on the `Error` event without saying so:** resumable, but the metric would leave out false sync words unless every caller added back `kResyncShift` per bad checksum.
- **Stopping on MalformedFrame:** a new `Stream` stops again at the same offset, so every caller would have to invent its own resync. On a ground station, that is a telemetry blackout until restart, in code no test can execute.
- **`std::terminate` on MalformedFrame:** it turns a library defect into a crash, which is a worse blackout.
