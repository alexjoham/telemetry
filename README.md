# telemetry

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
