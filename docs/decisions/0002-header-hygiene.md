# Header hygiene

Headers use include guards, not `#pragma once`. `tm_header_tus` compiles every header as its own TU to put it on the `misc-include-cleaner` path, and GCC's `#pragma once in main file` warning is fatal under `-Werror` with no way to suppress it.

The guard macro is the header's path below `include/`, uppercased, separators replaced by `_`, so `include/tlm/crc.hpp` becomes `TLM_CRC_HPP`. The whole path, never a shortened or hand-picked name: `#pragma once` made uniqueness the compiler's problem, and dropping it moved that problem here. Two headers that land on the same macro do not collide loudly. Whichever is included second expands to nothing, and the errors surface at every use of its declarations instead of at the guard. `crc.hpp` was briefly guarded on `TLM_HPP`, which a later `include/tlm/tlm.hpp` would have taken as its own. The naming scheme is unenforced; `llvm-header-guard` is the candidate once its derived name is confirmed to match.

The namespace name must not collide with a C global identifier. `tm` was tried and rejected because `struct tm` from `<ctime>` occupies that name. The failure is platform-dependent, because it depends on which headers pull in `<ctime>`, so a build that is clean on one toolchain breaks on another. `tlm` is the replacement, and it is the only namespace in the library.

Everything that appears in source follows the namespace. The include directory is `include/tlm/`, so the path rule above derives `TLM_` guards without a second rule to remember, and an include path reads the same as the namespace it provides. The earlier decision kept `include/tm/` on the grounds that a directory cannot collide with a C identifier, which is true and was the wrong thing to optimise: the cost of the mismatch was not a collision but a guard prefix nobody could derive from the path, which is how `TLM_HPP` got written.

CMake target names stay `tm_*`. They are not identifiers in any translation unit, they appear in three other decision records, and renaming them would churn that history for no gain.

Clang gets `-Wno-unused-const-variable`, scoped to those header TUs only. A constant in a header compiled alone is unused by construction, so every instance there is a false positive. Nothing is lost: Clang only fires this for the main file, so dead constants in included headers never warned, and main-file constants stay covered. Declaring header constants `inline constexpr` silences it without the suppression and is the C++17 idiom; `kMaxFrameSize` is the only one affected, and once it moves the flag can be deleted.
