# telemetry

A decoder for a binary telemetry protocol, built to not misread or crash on corrupted input.

![CI](https://github.com/alexjoham/telemetry/actions/workflows/ci.yml/badge.svg)

## What it does

A vehicle sends a stream of bytes over a radio link. Bytes get flipped, dropped or cut off mid-frame, and a ground station still has to make sense of what arrives. The core of this C++20 library is header-only and finds frames in the stream, checks the CRC, decodes the payload into typed structs and, after damage, resynchronizes on the next sync word. Every outcome is a value the caller handles, and the core neither throws nor allocates. `tm_io` is a separate, compiled target that reads files.

## Usage

```cpp
#include "tlm/stream.hpp"
#include <cstddef>
#include <cstdio>
#include <variant>
#include <vector>

// Called whenever the radio hands over more bytes; `buffer` keeps what is still unparsed.
void on_bytes(std::vector<std::byte> &buffer) {
    tlm::Stream stream{buffer};
    while (const auto event = stream.next()) {
        if (const auto *frame = std::get_if<tlm::DecodedFrame>(&*event)) {
            std::printf("seq %u steering %u\n", frame->frame_header.sequence,
                        frame->vehicle_state.steering_count);
        } else if (std::holds_alternative<tlm::Resync>(*event)) {
            std::puts("corrupted frame, resynchronizing");
        }
    }
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(stream.consumed()));
}
```

`next()` returns one of four events: a `DecodedFrame`, an `Error` for a well-formed frame that is not usable (unknown id, wrong version, wrong payload length), a `Discard` for bytes skipped while searching for a sync word, or a `Resync` after a bad CRC or malformed frame. It returns `std::nullopt` when it needs more bytes.

## Wire format

Little-endian, a 12-byte header, the payload, then a CRC-16.

| Offset | Width | Field           |
| ------ | ----- | --------------- |
| 0      | 2     | Sync word `A5 C3` |
| 2      | 1     | Version         |
| 3      | 1     | Message id      |
| 4      | 2     | Sequence number |
| 6      | 4     | Timestamp (µs)  |
| 10     | 1     | Payload length  |
| 11     | 1     | Reserved        |
| 12     | n     | Payload         |
| 12+n   | 2     | CRC-16          |

The full specification, including CRC coverage and a worked example, is in [docs/format.md](docs/format.md).

## How it is verified

- CI builds with GCC and Clang, Debug and Release, with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror`.
- The test suite (60 tests) also runs under ASan and UBSan on both compilers.
- clang-tidy and clang-format run in CI and fail on any finding.
- The CRC check value and the worked example are `static_assert`s, so they are checked at compile time.
- Mutation testing of `include/tlm/stream.hpp` with `tools/mutate_stream.py`: 69 mutants, 52 killed, 0 survived, 17 equivalent (each with a stated reason), recorded in [d85b3df](https://github.com/alexjoham/telemetry/commit/d85b3df).

## Design decisions

Each is written up in [docs/decisions](docs/decisions).

- [Outcomes are defined by what the caller does next, not by what went wrong](docs/decisions/0001-error-strategy.md)
- [Headers use include guards, not `#pragma once`](docs/decisions/0002-header-hygiene.md)
- [Decoding returns raw counts, calibration lives outside the core](docs/decisions/0003-decoded-units.md)
- [The core is header-only, so `constexpr` checks stay compile-time](docs/decisions/0004-header-only.md)
- [Errors are values: `Result<T, E>` has no `value()` to misuse](docs/decisions/0005-result-type.md)
- [Every precondition in `decode()` is a checked gate, not an `assert`](docs/decisions/0006-decode.md)
- [The stream is pull-based, so work per call is bounded](docs/decisions/0007-stream.md)

## Status

Complete as of v0.1.0 and not under active development. It is scoped to one message type (`VehicleState`, id `0x01`) by design. Any other id is reported as `UnknownMessageId` and skipped.

### Known limitations

`Stream::next()` returns `std::nullopt` both when it needs more bytes and when the input has ended. A caller reading a finished file must compare `consumed()` against the buffer size, or a damaged tail is dropped silently.

## Building and testing

Presets are defined in `CMakePresets.json`: `debug`, `release`, and `asan` (ASan + UBSan). Each configures into its own `build/<preset>` directory.

Configure, build, and run the tests:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Swap `debug` for `release` or `asan` to use the other presets. `asan` builds with `-fsanitize=address,undefined` and halts on the first UBSan error.

## Linting and formatting

Run both before pushing — CI (`.github/workflows/ci.yml`) runs clang-format-18 and clang-tidy-18 and fails on any diff or warning.

`tools/check.sh` runs both plus the build, with Homebrew's `llvm@18` (set `LLVM_BIN` elsewhere). To run it on every push, put `exec tools/check.sh` in `.git/hooks/pre-push` and `chmod +x` it.

### clang-format

Check only, no files changed (matches the CI job):

```sh
git ls-files -- '*.cpp' '*.hpp' '*.h' '*.cc' '*.hh' '*.cxx' '*.hxx' | xargs clang-format --dry-run --Werror
```

Apply fixes in place:

```sh
git ls-files -- '*.cpp' '*.hpp' '*.h' '*.cc' '*.hh' '*.cxx' '*.hxx' | xargs clang-format -i
```

### clang-tidy

Configure once with clang as the compiler, to get a `compile_commands.json`:

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
```

Run:

```sh
run-clang-tidy -p build
```
