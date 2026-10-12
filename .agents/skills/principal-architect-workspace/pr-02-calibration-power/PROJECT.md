# PR-02: Calibration + Power Engine (Phase 1 — Calibrate)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-01 merged **and DR-1 approved**
> **Dev Phase:** Phase 1 — Calibrate
> **Depends on:** PR-01, **DR-1** ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §5.1–§5.3 (cadence, position sensor, level mapping — as revised at DR-1), §6.1–§6.4 (power engine, calibration, NVS — as revised at DR-1), §13.4 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 1
> **Parallel with:** PR-03. **Shared files:** `EchenonModule.*`, `platformio.ini`, `main.cpp`. Whichever of PR-02 / PR-03 merges second rebases onto `main` and resolves the conflict.

## Mandate

Turn the raw Phase 0 signals into calibrated metrics, implementing the level
table and power model **as frozen at DR-1** (SPEC §5–§6 revised from Phase 0
evidence; audit S-01, S-02).

1. **`CadenceSensor`** — Production RPM from cadence edges (ISR or the analog
   event path chosen in PR-01 WO0), with timeout and filtering.
2. **`ResistanceSensor`** — Wiper ADC → cam angle → level, using the angle-based
   level table defined at DR-1, persisted in NVS. No linear mV→level mapping.
3. **`PowerEngine`** — Three-tier (physics defaults, community matrix, reference
   power meter coefficients), **indexed by cam angle within the working
   half-revolution**. "Level" is only a display scale.
4. **`CalibrationWizard`** — Portal page implementing **passive calibration**: the
   rider steps the console through its 32 levels while the logger records wiper
   voltage and cam angle at each level. The console's level is the resistance
   proxy. The ESP32 does not drive the motor in this PR; motor-driven
   (full-revolution) calibration belongs to PR-07. Owned by this PR, not PR-09.
5. **Live WS feed** — A live WebSocket feed on the dashboard for testing.
6. **Power data acquisition** — A `complexity: interactive` WO capturing known
   power data across cadence and resistance, with evidence committed.

**Hardware:** The PR-01 fly-wire / Y-harness rig with series-resistor taps,
USB-C powered. No changes.

**Validation:**
- Cadence within ±1 RPM of the Echelon app.
- The displayed level agrees with the console at every level of the **calibrated
  passive table** (the table recorded by the wizard), in both stepping directions.
- Power within ±15% of the reference, with the tolerance confirmed at DR-1.
- Native tests (`env:native`) for the level table, angle mapping and power model.

After this PR merges, the firmware shows calibrated cadence, resistance and
estimated power via the live WS feed, agreeing with the Echelon app.

## Owner decision 2026-10-12: runaway detection

Add stock-runaway **detection and alert**: the wiper stays outside the working range (165–1327 counts) for more than `[N]` s. Publish an EventBus event and alert in the portal (PR-14 persists and surfaces it). A passive board cannot prevent a runaway (known v1 limitation; prevention is v2/DR-2).

## Prior Art

- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md) — uses the superseded linear level model
- [wo3-power/WORKORDER.md](../archive-pa-decomposition/wo3-power/WORKORDER.md) — indexed by level, superseded by angle indexing
- [Old PR-02 scope](../archive-pr-v1/pr-02-power-engine/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
