# PR-02: Calibration + Power Engine (Phase 1 — Calibrate)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).
> **PR target base:** `main` after PR-01 merged **and DR-1 approved** (both met: DR-1 approved with conditions 2026-10-12, **unblocked**)
> **Dev Phase:** Phase 1 — Calibrate
> **Depends on:** PR-01, **DR-1** ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §5.1–§5.4 (cadence, position sensor and runaway detection, level mapping and scales, encoder — frozen at DR-1), §6.1–§6.4 (power table, calibration, power-data sources, NVS — frozen at DR-1), §13.4 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 1
> **Parallel with:** PR-03. **Shared files:** `EchenonModule.*`, `platformio.ini`, `main.cpp`. Whichever of PR-02 / PR-03 merges second rebases onto `main` and resolves the conflict.

## Mandate

Turn the raw Phase 0 signals into calibrated metrics, implementing the level
mapping and power model **as frozen at DR-1** (SPEC §5–§6; DR-1 §2.1, §2.6,
§2.11 and the §4 conditions; audit S-01, S-02).

1. **`CadenceSensor`** — Production RPM from cadence edges (ISR or the analog
   event path chosen in PR-01 WO0), with timeout and filtering.
2. **`ResistanceSensor`** — Display-path wiper → continuous level → canonical
   internal scale `i ∈ 0…255` (SPEC §5.2–§5.3). Default: the linear B03 fit
   `wiper = 39.1 × level + 97` over 165–1327 counts; replaced by the unit's
   passive-calibration wiper table once recorded. **No cam-angle tracking in
   v1** (that is PR-07 / DR-2). Motor-sense taps (GPIO 6/7) used for
   **monitoring only**: motor-direction and seek events.
3. **Scale mappings** (DR-1 §4, condition 2.11) — Echelon
   `L = round(i × 31 / 255) + 1` and `i = round((L − 1) × 255 / 31)`; the FTMS
   resistance-level field carries `L` (PR-05 encodes it); power, cadence and HR
   are what Garmin, Zwift and Strava consume. **Peloton 0–100 by power
   equivalence**: our per-level table against the published Peloton power curve
   at the same cadence. Implement in this PR, or defer it with the owner's
   agreement if the Peloton curve source needs review (provenance, licence,
   accuracy); record which in the Session Log.
4. **`PowerEngine`** — An **empirical per-level × cadence table** (32 levels ×
   5 RPM bins, bilinear interpolation on the continuous level). **No
   polynomial.** Tiers: default table, selected/custom matrix, recalibrated
   table (SPEC §6.3).
5. **`CalibrationWizard`** — Portal page implementing **passive calibration**: the
   rider steps the console through its 32 levels while the firmware records the
   wiper counts at each level. The console's level is the resistance proxy. The
   ESP32 does not drive the motor in this PR; motor-driven (full-revolution)
   calibration belongs to PR-07. Owned by this PR, not PR-09.
6. **Recalibration UI** (DR-1 §4, condition 2.6) — The user enters **level and
   power read from the console or the Echelon app screen**; the firmware
   correlates each entry with its live readings (wiper → `i`, cadence) over the
   same window and updates the per-level table. Grid, averaging window, update
   rule and point limit are this PR's to set (SPEC §6.3 Tier 3). This is the
   **primary power-data acquisition path** (below), not an optional extra.
7. **Live WS feed** — A live WebSocket feed on the dashboard for testing.
8. **Power data acquisition** — A `complexity: interactive` WO in which the
   owner reads the Echelon app's displayed power over a level × cadence grid
   and enters it through the recalibration UI, with evidence committed.

**Hardware:** The PR-01 fly-wire / Y-harness rig with series-resistor taps,
USB-C powered. No changes.

**Validation:**
- Cadence within ±1 RPM of the Echelon app.
- The displayed level agrees with the console at every level of the **calibrated
  passive table** (the table recorded by the wizard), in both stepping directions.
- Power within ±15% of the **Echelon app's displayed power**. Echenon's power
  matches the Echelon app's model; it is not independently accurate.
- Runaway detection fires on a bench simulation (SPEC §13.4 test 1.8).
- Native tests (`env:native`) for the level mapping, scale mappings (Echelon
  round trip, Peloton if implemented), runaway timer, power table and
  recalibration update rule.

After this PR merges, the firmware shows calibrated cadence, resistance and
estimated power via the live WS feed, agreeing with the Echelon app.

## Owner decision 2026-10-12: runaway detection

Add stock-runaway **detection and alert**: the wiper stays outside the working range (165–1327 counts) for more than `[PR-02: N]` s (this PR sets N; SPEC §5.2, NVS `runaway_timeout_s`). Publish an EventBus event and alert in the portal (PR-14 persists and surfaces it); the motor-sense taps showing continuous drive corroborate it. Test on the bench by simulation, never by provoking a real runaway. A passive board cannot prevent a runaway (known v1 limitation; prevention is v2/DR-2).

## DR-1 outcome (2026-10-12): unblocked

DR-1 was approved with conditions on 2026-10-12
([record §4](../reviews/DR-1-design-freeze.md)). This PR is **unblocked**. The
items above reflect the frozen decisions: linear B03 level model, canonical
0–255 scale with Echelon / Peloton / FTMS mappings, empirical per-level power
table with no polynomial, recalibration UI, motor sense for monitoring only,
runaway detection.

## Owner decision 2026-10-12: power-data source (resolved)

The power-data source, carried open from DR-1 §4, was decided by the owner on
2026-10-12 (*"Approach for power curve is probably (2)"*, with community
tables *"also an option"*):

- **Primary:** the Echelon app's displayed power, read by the user over a grid
  of level × cadence points and entered through the recalibration UI.
- **Alternative / seed:** community Echelon power tables, which may seed the
  default table before user calibration. UK/EU database-right caveat: not
  bundled in the firmware unless their licence permits; the rider may load
  them as a custom table.
- **Rejected:** a reference power meter (removing the cranks is impractical;
  pedals cost about £700).
- **Rejected:** sniffing the console's BLE broadcast (power may not be
  transmitted at all, and Echelon actively obfuscates its protocol).
- **Accuracy:** Echenon's power matches the Echelon app's model; it is not
  independently accurate.

**PA entry item:** confirm this choice with the owner at planning (it was
previously listed as an open item to raise) and record the confirmation in
the Session Log before decomposing the power-data WO.

## Prior Art

- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md) — linear level model; DR-1 re-adopts a linear wiper fit (B03) for v1, mapped onto the 0–255 internal scale
- [wo3-power/WORKORDER.md](../archive-pa-decomposition/wo3-power/WORKORDER.md) — indexed by level; DR-1 keeps level indexing for v1 but as an empirical table, not a polynomial
- [Old PR-02 scope](../archive-pr-v1/pr-02-power-engine/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
