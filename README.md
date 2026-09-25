# telemetry

## Linting and formatting

Run both before pushing — CI (`.github/workflows/ci.yml`) runs clang-format-18 and clang-tidy-18 and fails on any diff or warning.

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
