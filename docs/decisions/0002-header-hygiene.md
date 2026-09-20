# Header hygiene

Headers use include guards, not `#pragma once`. The guard macro is the header's path below `include/`, uppercased, separators replaced by `_`, so `include/tm/crc.hpp` becomes `TM_CRC_HPP`. `tm_header_tus` compiles every header as its own TU to put it on the `misc-include-cleaner` path, and GCC's `#pragma once in main file` warning is fatal under `-Werror` with no way to suppress it. The naming scheme is unenforced; `llvm-header-guard` is the candidate once its derived name is confirmed to match.

The namespace name must not collide with a C global identifier. `tm` was tried and rejected because `struct tm` from `<ctime>` occupies that name. The failure is platform-dependent, because it depends on which headers pull in `<ctime>`, so a build that is clean on one toolchain breaks on another. `tlm` is the replacement, and it is the only namespace in the library.

The rule binds the namespace only. The include directory stays `include/tm/`, the guard prefix stays `TM_`, and the CMake target stays `tm_core`: none of them is an identifier the C global namespace can collide with, and renaming them would churn every include path for no gain.

Clang gets `-Wno-unused-const-variable`, scoped to those header TUs only. A constant in a header compiled alone is unused by construction, so every instance there is a false positive. Nothing is lost: Clang only fires this for the main file, so dead constants in included headers never warned, and main-file constants stay covered. Declaring header constants `inline constexpr` silences it without the suppression and is the C++17 idiom; `kMaxFrameSize` is the only one affected, and once it moves the flag can be deleted.
