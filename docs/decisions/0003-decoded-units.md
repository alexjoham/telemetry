# Decoded units

`decode` returns raw counts. A 12-bit sensor field arrives as a `uint16_t` holding 0..4095, and nothing in `tm_core` turns it into degrees or bar. The 0..4095 range holds by construction: extraction masks twelve bits, so the bound is a property of the arithmetic and not something the decoder validates.

## Decision

Raw decoding is lossless: a recorder stores exactly what arrived, so a replay can apply a calibration corrected afterwards. And a calibration belongs to a particular sensor on a particular day, not to the wire format, so putting it in the decoder makes a recalibration a change to the parsing code. It also keeps `tm_core` free of floating point, which we prefer but which is not the deciding reason.

Conversion must not live in any target `tm_core` depends on; `tm_units` or the consumer both qualify, and the name is incidental. That constraint is what makes the separate function a real answer rather than a rename. It does not remove the objection, since every consumer must still know to call it, but the failure mode shrinks from several implementations that disagree to one that someone forgot to call, and the second shows up in a plot as counts while the first is invisible and wrong by a few percent.

## Rejected alternatives

- Engineering units from `decode`: lossy, so a recording can never be re-derived under a corrected calibration.
- Conversion as a member function on the decoded struct: it is `tm_core` code, so the struct's header drags floating point into every translation unit that includes it.

The concrete struct layout that follows this rule is in [0006](0006-decode.md#decoded-types).
