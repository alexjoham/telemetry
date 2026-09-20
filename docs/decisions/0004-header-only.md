# Header-only library

`tm_core` is header-only. Every symbol it exports is `constexpr` and defined inline under `include/tlm/`, so the target has no sources and nothing to link.

[0001](0001-error-strategy.md) left this open. It expected a compiled library back at the first symbol that could not be `constexpr`, and guessed the framer would be it. The framer is written now, and it is `constexpr`. It reads the front of a span and returns a variant of three small structs. No state, no allocation, no I/O. So nothing forced the change, and the policy is settling in by accumulation instead of being picked.

## Decision

`tm_core` stays header-only. Adding a source file to it is a decision on its own, not a side effect of building something else.

The reason is `constexpr`. `crc_test.cpp` pins the catalogue check value and the worked example in `static_assert`s, so both are checked when the test compiles. A compiled `crc16` cannot run in a constant expression. Moving it to a source file would quietly turn both into runtime checks.

This follows from the purity rule in 0001. A `tm_core` with no files, sockets or OS headers is the same shape that stays `constexpr`. Anything that forces a source file breaks that rule first.

The scope is `tm_core` alone. `tm_io` and the `tm_units` of [0003](0003-decoded-units.md) are not bound by it. Conversion needs floating point, and transport needs the OS.

## Costs

There is no compile firewall and no ABI boundary. Every TU that includes `crc.hpp` rebuilds when the CRC loop changes. At three headers that is cheap.

It also makes [0002](0002-header-hygiene.md)'s `tm_header_tus` permanent. Nothing else ever compiles a header, so it is the only path by which the warning set and `misc-include-cleaner` reach the library. 0001 says `tm_core` links `tm_warnings` again once it has sources. It never will, so `tm_header_tus` is the whole of that coverage.

## Revisiting

- A symbol that cannot be `constexpr` and belongs in `tm_core`. Mutable static state is the likely one.
- Compile time becoming noticeable, which means the headers grew.
- A consumer that needs a stable ABI.

## Rejected alternatives

- A compiled `tm_core`: buys a compile firewall the project is too small to need, and costs the `static_assert`s.
- A split, `constexpr` inline and the rest compiled: there is no rest. The boundary would follow the rule, not the code.
- Header-only across the whole project: too broad. `tm_io` and `tm_units` need sources.
