# PR-07: MITM Board + Motor Control + Safety (Phase 5 — MITM board + motor control + safety)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 merged and DR-1 approved
> **Dev Phase:** Phase 5 — MITM board + motor control + safety
> **Depends on:** PR-02, DR-1 ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §4.4.2 (Phase 3+ pin map), §4.6–§4.8 (intercept board, voltage safety, parts), §5.3–§5.6 (level mapping, encoder, motor control, operating modes), §6.3 (motor-driven calibration) — all as revised at DR-1; §13.8 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 as revised at DR-1 (§7.5 standalone supplies SW1–SW4, §7.8 pin map)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 5

## Mandate

Build the optional **MITM intercept board** (adds Override and, with PR-08, ERG)
and implement cam-aware motor control, to the design frozen at DR-1.

1. **Hardware scope** — Intercept board with motor split, the fail-safe chosen at
   DR-1 (normally-closed bypass or equivalent; audit H-05), the motor driver and rail
   chosen at DR-1 (decision D3), and switchable R3/R4/R5 (audit H-02). GPIO 6/7 are
   never both driver outputs and sense inputs.
2. **Motor control** — Cam-aware position controller (audit S-01, M-02): full-
   revolution angle tracking, working half-revolution and approach direction,
   angle-based level table, run-time bound of one revolution, reset-safe driver
   inputs, IRAM-safe ISR paths (audit M-01).
3. **Motor-driven calibration** — A full-revolution sweep, driven by the ESP32, that
   finds the cam extremes and click-off points and refines the passive calibration
   table from PR-02. This PR owns motor-driven calibration.
4. **`EncoderModule`** — Reuse the framework's PCNT-based decoder via a
   `BoardEncoder` descriptor (audit H-06), with the signal conditioning PR-01 WO0
   showed to be necessary for encoder Ch B.
5. **OTA regression** — Manual OTA (enabled in PR-04a) still works with the
   intercept board fitted and `autoCheck` stays forced to `false`.

Board build, motor tests and fail-safe tests are `complexity: interactive` WOs with
evidence committed.

**Verification:**
- Intercept mode is indistinguishable from stock, including the console's power-on
  calibration.
- **Override level resolution is measured and recorded; 100 levels is a goal to
  validate, not an acceptance criterion.**
- Fail-safe activates on reset, crash, brownout and an unseated ESP32.
- The motor stops within one revolution in every fault case.
- Motor-driven calibration reproduces the cam extremes across repeated sweeps.
- OTA works with the intercept board fitted.

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md) — pre-audit intercept design
- [`hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 are PROVISIONAL until DR-1

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
