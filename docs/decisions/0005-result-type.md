# Result<T, E>

[0001](0001-error-strategy.md) named `Result<T, E>` as the decoder's return type without specifying its shape. This records that shape, built and tested standalone with `Result<int, Error>` before `decode()` exists ([0006](0006-decode.md)).

## Decision

- Wraps a private `std::variant<T, E>`; nothing outside the class reads it directly.
- `static_assert(!std::is_same_v<T, E>)`, so the two discriminants can never collide.
- `[[nodiscard]]` on the class, not on every function that returns one.
- No `value()`. `assert` is gone under `-DNDEBUG`, so an accessor that misuses it must either not exist or force a check first.
- Production access is `match(on_ok, on_err)`, a `std::visit` over the variant: a missing or mistyped handler fails to compile. Tests use `ok()`/`err()`, null-returning like the framer's `std::get_if`, with `ASSERT_NE`.
- `constexpr` and `noexcept` throughout, per [0001](0001-error-strategy.md).

The missing-handler compile failure was verified once by hand, not kept as a test: `tm_tests` cannot assert a build breaks, the same reason the framer's `>`/`>=` boundary was confirmed by hand before being pinned as a `static_assert`.

## Rejected alternatives

- A public `std::variant<T, E>`: leaves `std::get_if` available everywhere, defeating `match`.
- `value()` guarded by `assert`: gone in Release, the same objection [0001](0001-error-strategy.md) raises for exceptions.
- A `bool ok` flag plus both a `T` and an `E` member: the `{Kind, value}` bug from [0001](0001-error-strategy.md#rejected-alternatives) again.
