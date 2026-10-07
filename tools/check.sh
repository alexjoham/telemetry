#!/bin/sh
# tools/check.sh: everything CI checks except tests. Run from repo root.
# LLVM 18 to match CI; override LLVM_BIN off macOS.
set -e
LLVM_BIN=${LLVM_BIN:-/opt/homebrew/opt/llvm@18/bin}
git ls-files '*.hpp' '*.cpp' | xargs "$LLVM_BIN/clang-format" --dry-run --Werror
cmake --build build
"$LLVM_BIN/run-clang-tidy" -p build -quiet -clang-tidy-binary "$LLVM_BIN/clang-tidy"
