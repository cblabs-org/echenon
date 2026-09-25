# PR-02: Calibration + Power Engine (Phase 1 — Calibrate)

> **Programme:** Echenon Firmware
> **PR target base:** PR-01 (`signal-logger` branch tip)
> **Dev Phase:** Phase 1 — Calibrate
> **Spec sections:** §5.1–5.2, §6.1–6.4
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 1
> **Parallel with:** PR-03 (no file overlap)

## Mandate

Convert raw signals from Phase 0 into meaningful metrics:

1. **`CadenceSensor`** — Production RPM from ISR intervals with timeout and filtering.
2. **`ResistanceSensor`** — ADC → resistance level mapping with NVS calibration.
3. **`PowerEngine`** — Three-tier: physics polynomial, community matrix with
   bilinear interpolation, reference power meter coefficients.
4. **`CalibrationWizard`** — Portal page: guided min/max pot calibration (owned by this PR, not PR-09).
5. **Live WS feed** — Add a live WebSocket feed to the dashboard for testing.
6. **Power data acquisition** — Include a WO to capture known power data across cadence/resistance.

**Hardware:** Same fly wires as PR-01. No changes.

**Validation:** Side-by-side comparison with Echelon app. Cadence ±1 RPM.
Resistance levels match 1–32 display. Power within ±15%.

After this PR merges, the firmware shows calibrated cadence, resistance, and
estimated power via the live WS feed. The numbers should match the Echelon app.

## Prior Art

- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md)
- [wo3-power/WORKORDER.md](../archive-pa-decomposition/wo3-power/WORKORDER.md)
- [Old PR-02 scope](../archive-pr-v1/pr-02-power-engine/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
