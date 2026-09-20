# PR-02: Power Calculation Engine

> **Programme:** Echenon Firmware
> **PR target base:** PR-01 (`scaffold-sensors` branch tip)
> **Spec sections:** §6.1–6.4

## Mandate

Implement `PowerEngine` with three-tier calibration:
1. Physics polynomial defaults (bivariate: resistance × cadence)
2. Community power matrix tables with bilinear interpolation (32 × 31 lookup)
3. Reference power meter least-squares coefficients

Create `PowerMatrix.h` with a placeholder EX-3 community table. Wire into
`EchenonModule::tick()` so watts are calculated and logged alongside RPM and
resistance every tick.

After this PR merges, the firmware calculates instantaneous power from sensor
inputs using configurable calibration.

## Prior Art

- [wo3-power/WORKORDER.md](../archive-pa-decomposition/wo3-power/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
