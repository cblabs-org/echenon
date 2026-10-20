---
schema_version: 1
charter:
  pr_id: "PR-02"
  slug: "pr-02-calibration-power"
  title: "Calibration + Power Engine"
  branch: "feat/pr-02-calibration-power"
  base_branch: "main"
  allowed_modules:
    - "src/echenon/"
    - "docs/"
    - "hardware/"
    - "framework/"
    - "platformio.ini"
    - "tools/calibration-rig/"
    - "test/"
  charter_locked: true
status: APPROVED
work_orders:
- id: WO-02.1
  slug: wo1-sensors
  title: CadenceSensor + ResistanceSensor + Level Mapping
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 6542790ea33a5008fae56e74fc8fa8fc088b0141
- id: WO-02.2
  slug: wo2-power-engine
  title: PowerEngine + Default Table Structure
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 64c8b30150ac863cd137bd76602b0c7d6aa56a98
- id: WO-02.3
  slug: wo3-calibration-ui
  title: CalibrationWizard + RecalibrationUI
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.4
  slug: wo4-ws-feed
  title: Live WebSocket Feed
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.5
  slug: wo5-rig-firmware
  title: CalibrationRig Firmware Module
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.6
  slug: wo6-rig-host
  title: Host Script + Preflight
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.7
  slug: wo7-rig-sweep
  title: Rig Sweep (90-min)
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.8
  slug: wo8-runaway
  title: Runaway Detection
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-02.9
  slug: wo9-doc-sync
  title: Doc Sync + Integration
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
---

## 2. Scope

_See mandate below._


# PR-02: Calibration + Power Engine (Phase 1 — Calibrate)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).
> **PR target base:** `main` after PR-01 merged **and DR-1 approved** (both met: DR-1 approved with conditions 2026-10-12, **unblocked**)
> **Dev Phase:** Phase 1 — Calibrate
> **Depends on:** PR-01, **DR-1** ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §5.1–§5.4 (cadence, position sensor and runaway detection, level mapping and scales, encoder — frozen at DR-1), §6.1–§6.4 (power table, calibration, power-data sources, NVS — frozen at DR-1), §13.4 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 1
> **Parallel with:** PR-03. **Shared files:** `EchenonModule.*`, `platformio.ini`, `main.cpp`. Whichever of PR-02 / PR-03 merges second rebases onto `main` and resolves the conflict.

## 1. Mandate

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
   N RPM bins, bilinear interpolation on the continuous level). **No
   polynomial.** Tiers: default table (populated by the rig sweep), selected /
   custom matrix, recalibrated table (SPEC §6.3).
5. **`CalibrationWizard`** — Portal page implementing **passive calibration**: the
   rider steps the console through its 32 levels while the firmware records the
   wiper counts at each level. The console's level is the resistance proxy. The
   ESP32 does not drive the motor in this PR; motor-driven (full-revolution)
   calibration belongs to PR-07. Useful for end-user recalibration on their own
   bike without the automated rig.
6. **Recalibration UI** (DR-1 §4, condition 2.6) — The user enters **level and
   power read from the console or the Echelon app screen**; the firmware
   correlates each entry with its live readings (wiper → `i`, cadence) over the
   same window and updates the per-level table. Grid, averaging window, update
   rule and point limit are this PR's to set (SPEC §6.3 Tier 3). This is a
   **secondary data-entry path** for individual point corrections after the rig
   sweep.
7. **Live WS feed** — A live WebSocket feed on the dashboard for testing.
8. **Automated power calibration rig** (owner decision 2026-10-18, replaces the
   former manual interactive WO) — A fully automated sweep that extracts the
   power table directly from the Echelon Fit app:
   - **`CalibrationRig` firmware module** (behind `WITH_CALIBRATION_RIG` build
     flag): encoder quadrature synthesis (2 spare GPIOs → console's encoder
     input, physical encoder disconnected during sweep), cadence reed-switch
     simulation (1 spare GPIO via MOSFET/NPN → cadence line), pot ADC
     monitoring for motor-settle confirmation, serial command protocol
     (`SET level=N cadence=M`, `HOME`, `STATUS`, `STOP` → `SETTLED`, `SEEKING`,
     `ERROR`).
   - **Host script** (`tools/calibration-rig/calibration_sweep.py`): serial
     communication (pyserial), ADB screen capture (`uiautomator dump` preferred,
     screencap + OCR fallback), CSV output
     `(level, cadence_rpm, power_watts, pot_adc, timestamp)`, resume capability.
   - **Sweep matrix:** 32 levels × 10 cadence steps (40–130 RPM in 10 RPM
     increments) = 320 data points; outer loop cadence, inner loop resistance.
     Estimated ~90 minutes unattended.
   - **Preflight:** confirm ADB `uiautomator dump` captures the watts field
     from the Echelon Fit app; confirm "Just Ride" mode shows watts; select and
     document the spare GPIOs; verify bench wiring.
   - **Evidence:** the 320-point CSV is committed as the default power table
     evidence and as early MITM encoder path validation (DR-2 prep input).
   - **Hardware:** temporary bench setup with jumper wires. Spare GPIOs with
     direct connections to the console's encoder loom (physical encoder
     unplugged). Small N-FET or NPN for cadence. Existing pot tap (A0)
     unchanged.
9. **Runaway detection** (owner decision 2026-10-12) — the wiper stays outside
   the working range (165–1327 counts) for more than `[PR-02: N]` s (this PR
   sets N; SPEC §5.2, NVS `runaway_timeout_s`). Publish an EventBus event and
   alert in the portal (PR-14 persists and surfaces it); the motor-sense taps
   showing continuous drive corroborate it. Test on the bench by simulation,
   never by provoking a real runaway. A passive board cannot prevent a runaway
   (known v1 limitation; prevention is v2/DR-2).

**Hardware:** The PR-01 fly-wire / Y-harness rig with series-resistor taps,
USB-C powered, **plus the temporary calibration-rig wiring** (spare GPIOs →
console encoder inputs + cadence MOSFET). No permanent changes.

**Validation:**
- Cadence within ±1 RPM of the Echelon app.
- The displayed level agrees with the console at every level of the **calibrated
  passive table** (the table recorded by the wizard), in both stepping directions.
- Power within ±15% of the **Echelon app's displayed power** across the 320-point
  rig sweep. Echenon's power matches the Echelon app's model; it is not
  independently accurate.
- Runaway detection fires on a bench simulation (SPEC §13.4 test 1.8).
- Native tests (`env:native`) for the level mapping, scale mappings (Echelon
  round trip, Peloton if implemented), runaway timer, power table and
  recalibration update rule.

After this PR merges, the firmware shows calibrated cadence, resistance and
estimated power via the live WS feed, agreeing with the Echelon app, backed by
a 320-point empirical table extracted from the Echelon Fit app.

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

## Owner decision 2026-10-18: automated calibration rig (supersedes manual data acquisition)

The owner proposed and approved replacing the manual power-data interactive WO
with an **automated calibration rig** (in chat, 2026-10-18):

- ESP32 synthesises encoder quadrature and drives the cadence reed switch;
  host-side Python + ADB captures power from the Echelon Fit app.
- Produces a 320-point Power(level, cadence) CSV in ~90 minutes unattended.
- **Confirmed:** the Echelon Fit app displays instantaneous watts (screenshot
  evidence — bottom-right "Output (watts)" field).
- **GPIOs:** spare GPIOs with direct jumper wires (existing tap pins have
  4.7 kΩ + 100 nF filters unsuitable for output driving).
- **ADB check:** preflight item in the rig WO.
- The `CalibrationWizard` and `Recalibration UI` are retained for end-user
  recalibration.

## Prior Art

- [power-calibration-rig.md](../../../../.gemini/antigravity/brain/b7c788c5-d340-418c-a2b5-0879f4b3df3f/power-calibration-rig.md) — design note (2026-10-18)
- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md) — linear level model; DR-1 re-adopts a linear wiper fit (B03) for v1, mapped onto the 0–255 internal scale
- [wo3-power/WORKORDER.md](../archive-pa-decomposition/wo3-power/WORKORDER.md) — indexed by level; DR-1 keeps level indexing for v1 but as an empirical table, not a polynomial
- [Old PR-02 scope](../archive-pr-v1/pr-02-power-engine/PROJECT.md)

## 3. Work Order Ledger

Project name for branches: `calibration-power`. Linear stack from `main`.
Proposed 2026-10-18; **approved by owner 2026-10-20.**

**Owner decisions at approval:**
- **Peloton 0–100 mapping:** deferred to PR-05 (where FTMS telemetry lands and
  apps actually consume it). WO1 does not stub it; WO9 does not resolve it.
  SPEC §5.3 Peloton mapping is a PR-05 deliverable.
- **`runaway_timeout_s` default:** 10 seconds, adjustable via NVS (confirmed).
- **Plan:** 9-WO decomposition approved as proposed.

### Summary Table

| WO | Title | Branch (base) | Complexity | Charter §§ | Tests (§13.4) | Status |
|---|---|---|---|---|---|---|
| **WO-02.1** | CadenceSensor + ResistanceSensor + Level Mapping | `calibration-power/wo1-sensors` (`main`) | `standard` | §1, §2, §3 | 1.1, 1.3, 1.4, (native) | ⬜ |
| **WO-02.2** | PowerEngine + Default Table Structure | `calibration-power/wo2-power-engine` (wo1) | `standard` | §4 | 1.6 logic (native) | ⬜ |
| **WO-02.3** | CalibrationWizard + RecalibrationUI | `calibration-power/wo3-calibration-ui` (wo2) | `standard` | §5, §6 | 1.2, 1.5 (portal) | ⬜ |
| **WO-02.4** | Live WebSocket Feed | `calibration-power/wo4-ws-feed` (wo3) | `standard` | §7 | — (visual) | ⬜ |
| **WO-02.5** | CalibrationRig Firmware Module | `calibration-power/wo5-rig-firmware` (wo4) | `standard` | §8 (firmware) | (native: quadrature gen) | ⬜ |
| **WO-02.6** | Host Script + Preflight | `calibration-power/wo6-rig-host` (wo5) | `interactive` | §8 (host + preflight) | — (bench) | ⬜ |
| **WO-02.7** | Rig Sweep (90-min) | `calibration-power/wo7-rig-sweep` (wo6) | `interactive` | §8 (sweep) | 1.1, 1.3, 1.4, 1.6 | ⬜ |
| **WO-02.8** | Runaway Detection | `calibration-power/wo8-runaway` (wo7) | `standard` | §9 | 1.8 (native + bench sim) | ⬜ |
| **WO-02.9** | Doc Sync + Integration | `calibration-power/wo9-doc-sync` (wo7 or wo8) | `standard` | — | 1.7, 1.9 | ⬜ |

### WO-02.1 — CadenceSensor + ResistanceSensor + Level Mapping

**Objective:** Build the two production sensor classes and the canonical internal
scale with all external mappings (Echelon, Peloton if source available, FTMS,
internal). Wire them into `EchenonModule::tick()`.

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/CadenceSensor.h`
- `src/echenon/CadenceSensor.cpp`
- `src/echenon/ResistanceSensor.h`
- `src/echenon/ResistanceSensor.cpp`
- `src/echenon/LevelMapping.h`
- `src/echenon/LevelMapping.cpp`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `src/main.cpp`
- `hardware/ex3-waveshare-nano/board.h` (read only; add `PIN_RIG_*` in WO5)
- `test/test_native/test_level_mapping.cpp`
- `test/test_native/test_cadence_sensor.cpp`
- `platformio.ini` (native test deps only)

**Scope:**
- **CadenceSensor** (SPEC §5.1): ISR or ADC-event on GPIO 5, 25 ms debounce,
  RPM = 60,000,000 / Δt_µs, 3 s timeout → 0, clamp 0–200 RPM.
- **ResistanceSensor** (SPEC §5.2): display-path EMA at 20 Hz, α = 0.15 on the
  ADC-continuous wiper (GPIO 1, A0). Derives the continuous level ℓ from the
  B03 fit `wiper = 39.1 × level + 97`, clamped 1–32.
- **LevelMapping** (SPEC §5.3): canonical `i ∈ 0…255` from ℓ; Echelon
  `L = round(i × 31/255) + 1` and inverse. **Peloton 0–100 mapping deferred to
  PR-05** (owner decision 2026-10-20).
- **Native tests:** level mapping round-trip (exact endpoints 0 ↔ 1, 255 ↔ 32),
  cadence timeout logic, EMA convergence.
- **PCNT verification** (SPEC §5.4, last paragraph): confirm the existing
  `EncoderModule` PCNT config enables no internal pulls on GPIO 8/9. Record
  finding in the Completion Report.
- **Verification:** `pio test -e native` passes. `pio run -e ex3-waveshare-nano`
  compiles with zero errors.

**Tests covered:** §13.4 tests 1.1 (cadence accuracy), 1.3 (level display),
1.4 (sweep tracking — logic only; full validation in WO7).

---

### WO-02.2 — PowerEngine + Default Table Structure

**Objective:** Build the per-level × cadence power table engine with bilinear
interpolation, an API to load/save tables, and a placeholder default table.

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/PowerEngine.h`
- `src/echenon/PowerEngine.cpp`
- `src/echenon/PowerTable.h`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `test/test_native/test_power_engine.cpp`
- `platformio.ini` (native test deps only)

**Scope:**
- **PowerEngine** (SPEC §6.2): 32 levels × N cadence bins (5 RPM steps,
  0–150 RPM = 31 bins), bilinear interpolation on the continuous level ℓ and
  live cadence. Power = max(0, interpolated). Zero cadence → 0 W.
- **PowerTable** (SPEC §6.3, §6.4): a struct for the table (32 × 31 int16),
  plus a table ID and tier (1 = default, 2 = selected/custom, 3 = recalibrated).
  API to load from a CSV blob, serialise to NVS, and reset to default.
- **Default table:** a placeholder generated from the physics heuristic
  `P ≈ k × level × RPM²` with reasonable coefficients. This is explicitly a
  placeholder — the real table lands from the rig sweep in WO7.
- **NVS keys:** `cal_power_tier`, `cal_matrix_id`, `cal_matrix_custom`,
  `cal_power_table` (SPEC §6.4).
- **Native tests:** bilinear interpolation (known grid, query between points),
  boundary cases (level 0, level 32, cadence 0, cadence 150), CSV parse,
  round-trip NVS serialise.
- **Verification:** `pio test -e native` passes. Target compiles.

**Tests covered:** §13.4 test 1.6 (power accuracy — logic, against placeholder).

---

### WO-02.3 — CalibrationWizard + RecalibrationUI

**Objective:** Build the portal pages for passive position calibration and
recalibration (individual power point corrections).

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/CalibrationWizard.h`
- `src/echenon/CalibrationWizard.cpp`
- `src/echenon/RecalibrationUI.h`
- `src/echenon/RecalibrationUI.cpp`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `web/echenon/calibration-wizard.js` (or similar portal asset)
- `web/echenon/recalibration.js`
- `test/test_native/test_recalibration.cpp`
- `platformio.ini` (native deps, portal builder config)

**Scope:**
- **CalibrationWizard** (SPEC §6.3 Passive Calibration): portal page that
  guides the rider through levels 1–32, recording wiper counts at each level.
  Stores the 32-entry `cal_level_table` (SPEC §6.4). Sets `cal_pos_done`. The
  wizard replaces the B03 default fit in `ResistanceSensor`.
- **RecalibrationUI** (SPEC §6.3 Tier 3): the rider enters level and power
  from the console/app; the firmware correlates with its live wiper → `i` and
  cadence over a window and stores the point. Grid, averaging window, update
  rule and maximum points defined in this WO (values recorded in SPEC §6.3
  `[PR-02]` placeholders).
- **API endpoints:**
  - `POST /api/echenon/calibration/start` / `stop` / `record-level`
  - `GET /api/echenon/calibration/status`
  - `POST /api/echenon/recalibration/point` — `{level, power}`
  - `GET /api/echenon/recalibration/points`
  - `POST /api/echenon/recalibration/reset`
  - `POST /api/echenon/power-table/upload` — custom CSV (Tier 2)
  - `GET /api/echenon/power-table/active` — current table preview
- **NVS keys:** `cal_level_table`, `cal_pos_done`, `cal_recal_points` (§6.4).
- **Native tests:** recalibration update rule (point averaging, table update
  region), wiper table interpolation.
- **Verification:** Target compiles. Native tests pass.

**Tests covered:** §13.4 tests 1.2 (passive calibration), 1.5 (recalibration
grid entry).

---

### WO-02.4 — Live WebSocket Feed

**Objective:** Add a real-time WebSocket dashboard showing cadence, resistance
level, power and encoder events for testing.

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/LiveFeed.h`
- `src/echenon/LiveFeed.cpp`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `web/echenon/echenon-dashboard.js` (extend existing)
- `platformio.ini` (if WS lib needed)

**Scope:**
- **WebSocket** on `/ws/echenon/live` (or via the existing PsychicHttp WS API):
  pushes JSON at 2–4 Hz with `{ cadence_rpm, level_echelon, level_i,
  power_watts, pot_adc, motor_dir, encoder_delta, timestamp_ms }`.
- **Dashboard:** extend the existing Echenon tab to show a live display of
  cadence, level, power and a simple time-series strip chart. Replace or extend
  the current `GET /api/echenon/live` poll with the WS feed.
- **Verification:** Target compiles. Dashboard accessible at
  `http://echenon.local/`, shows live data.

**Tests covered:** §13.4 test 1.3 (visual: live feed shows level 19 matching
the app — validated in WO7).

---

### WO-02.5 — CalibrationRig Firmware Module

**Objective:** Build the `CalibrationRig` module behind `WITH_CALIBRATION_RIG`,
implementing encoder quadrature synthesis, cadence reed-switch simulation, pot
monitoring for motor settle, and the serial command protocol.

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/CalibrationRig.h`
- `src/echenon/CalibrationRig.cpp`
- `src/echenon/QuadratureGenerator.h`
- `src/echenon/QuadratureGenerator.cpp`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `src/main.cpp`
- `hardware/ex3-waveshare-nano/board.h` (add `PIN_RIG_ENC_A`, `PIN_RIG_ENC_B`,
  `PIN_RIG_CADENCE` under `#ifdef WITH_CALIBRATION_RIG`)
- `platformio.ini` (add `WITH_CALIBRATION_RIG` commented out; add a
  `ex3-waveshare-nano-rig` env that enables it)
- `test/test_native/test_quadrature_generator.cpp`

**Scope:**
- **GPIO selection (rig outputs):** GPIO 17 (D8) for encoder Ch A, GPIO 18
  (D12 — *verify Waveshare Nano header availability*) for encoder Ch B, GPIO 11
  (D9) for cadence MOSFET gate. These are free in v1 and do not collide with any
  tap pin. Added to `board.h` under `#ifdef WITH_CALIBRATION_RIG`. If GPIO 18 is
  not broken out on the Waveshare Nano, escalate for pin substitution (GPIO 12,
  13, 14, 15, 16 are candidates).
- **QuadratureGenerator:** generates Gray-code quadrature on two GPIOs. 4
  detents × 4 steps = 16 edges per level. Configurable direction (UP/DOWN).
  ~50 ms between state transitions (~200 ms per detent, ~800 ms per level).
  Hardware timer or `millis()`-based state machine.
- **Cadence timer:** toggles the cadence GPIO LOW for 30 ms then HIGH for the
  remainder of the period. Period = 60000/RPM ms. Supports 0–200 RPM.
- **Pot settle monitor:** polls the existing ADC wiper (A0) and compares to the
  expected value from the B03 fit. Settled when |actual − expected| ≤ 15 counts
  for 500 ms.
- **Serial command protocol** (design note §6.1):
  - `SET level=<1-32> cadence=<0-200>` → seek to level, set cadence
  - `HOME` → return to level 1 (31 levels × 16 edges of reverse pulses)
  - `STATUS` → report pot ADC, derived level, cadence
  - `STOP` → stop cadence, hold encoder
  - Responses: `SETTLED level=N pot=ADC cadence=RPM`, `SEEKING level=N pot=ADC`,
    `ERROR <message>`
- **Build flag:** `WITH_CALIBRATION_RIG` in a separate env
  `ex3-waveshare-nano-rig`. Excluded from the default production build.
- **Native tests:** `QuadratureGenerator` state machine (step sequence, direction
  reversal, level count).
- **Verification:** `pio test -e native` passes. `pio run -e ex3-waveshare-nano`
  compiles (without `WITH_CALIBRATION_RIG`). `pio run -e ex3-waveshare-nano-rig`
  compiles.

---

### WO-02.6 — Host Script + Preflight (Interactive)

**Objective:** Build the host-side Python calibration sweep script and run the
preflight checks: ADB access, GPIO selection confirmation, bench wiring
verification.

**Complexity:** `interactive`

**Allowed files:**
- `tools/calibration-rig/calibration_sweep.py`
- `tools/calibration-rig/requirements.txt`
- `tools/calibration-rig/README.md`
- `docs/measurements/phase1/` (preflight evidence)

**Scope:**
- **calibration_sweep.py** (design note §5):
  - Serial communication with ESP32 (pyserial): `SET`, `HOME`, `STATUS`, `STOP`
  - ADB screen reading: `uiautomator dump` (preferred), screencap + OCR
    (pytesseract fallback)
  - Sweep logic: outer loop cadence (40–130 RPM, 10 steps), inner loop
    resistance (1–32). 320 data points.
  - CSV output: `level, cadence_rpm, power_watts, pot_adc, timestamp`
  - Resume capability: reads existing CSV, skips completed points.
  - Timeout and retry: per-point timeout, retry with HOME+re-seek.
  - `--dry-run` mode: serial commands only, no ADB, for wiring verification.
- **Preflight (interactive bench session):**
  1. Flash the `ex3-waveshare-nano-rig` firmware.
  2. Disconnect the physical encoder from the console (unplug the 3-pin loom).
  3. Wire GPIO 17 → console Ch A, GPIO 18 → console Ch B (or substituted pins).
  4. Wire GPIO 11 → MOSFET gate → cadence line Orange (with Grey as source/GND).
  5. Confirm `adb shell uiautomator dump` captures the watts field from the
     Echelon Fit app in "Just Ride" mode.
  6. Run `calibration_sweep.py --dry-run` to verify serial commands and wiring
     (motor seeks, pot ADC tracks, cadence simulation works).
  7. Record preflight evidence in `docs/measurements/phase1/`.
- **Evidence committed:** preflight ADB XML snippet (or screenshot showing
  watts field), wiring photo, dry-run serial log.
- **Verification:** preflight checks pass. The script runs in dry-run mode
  without errors.

---

### WO-02.7 — Rig Sweep (Interactive, ~90 min)

**Objective:** Run the full 320-point calibration sweep and commit the resulting
power table as the default.

**Complexity:** `interactive`

**Allowed files:**
- `tools/calibration-rig/` (CSV output)
- `src/echenon/PowerTable.h` (update default table `const` data)
- `src/echenon/PowerEngine.cpp` (if default table initialisation changes)
- `docs/measurements/phase1/` (sweep evidence)
- `docs/` (evidence index)

**Scope:**
- Start a "Just Ride" class on the Echelon Fit app.
- Run `calibration_sweep.py` end-to-end (~90 minutes unattended).
- **Validate the data:**
  - 320 points (32 × 10), no gaps.
  - Power monotonically increases with level at each cadence.
  - Power monotonically increases with cadence at each level (above ~40 RPM).
  - Spot-check: level 16 @ 80 RPM should give ~100–200 W (reasonable).
  - No `ERROR` or timeout entries.
- **Commit the CSV** as `docs/measurements/phase1/power-table-rig-sweep.csv`.
- **Update the default table** in `PowerTable.h` with the 320-point data
  (replacing the placeholder from WO2).
- **Evidence:** CSV, sweep log, timing summary, any anomalies noted.
- **DR-2 prep value:** this sweep is the first live test of encoder synthesis
  against the console — record findings about console acceptance, seek timing,
  and any encoder-rate limitations for the DR-2 prep dossier.
- **Verification:** The updated firmware compiles. Live feed at level 16 / 80
  RPM shows power within ±15% of the Echelon app.

**Tests covered:** §13.4 tests 1.1 (cadence accuracy across sweep), 1.3 (level
display tracks console), 1.4 (sweep tracking both directions), 1.6 (power
accuracy ±15% of app).

---

### WO-02.8 — Runaway Detection

**Objective:** Implement the stock-runaway detection timer, EventBus event, and
portal alert.

**Complexity:** `standard`

**Allowed files:**
- `src/echenon/RunawayDetector.h`
- `src/echenon/RunawayDetector.cpp`
- `src/echenon/EchenonModule.h`
- `src/echenon/EchenonModule.cpp`
- `test/test_native/test_runaway_detector.cpp`
- `web/echenon/echenon-dashboard.js` (alert banner)

**Scope:**
- **RunawayDetector** (SPEC §5.2, last paragraph): monitors the display-path
  wiper. When the filtered wiper stays outside 165–1327 counts for longer than
  `runaway_timeout_s` (NVS, default `[PR-02: N]` — set to 10 s as a starting
  value; adjust based on bench testing), publish a `runaway.detected` event on
  the EventBus and set an alert flag.
- **Motor-sense corroboration:** optionally check GPIO 6/7 for continuous motor
  drive during the runaway window (monitoring only).
- **Portal alert:** a banner or notification on the dashboard ("Stock runaway
  detected — turn the knob back to a valid level").
- **NVS key:** `runaway_timeout_s` (SPEC §6.4).
- **Native tests:** runaway timer logic — in-range resets timer, out-of-range
  for N s triggers, re-entry cancels, edge cases (exactly at boundary counts).
- **Bench simulation test** (§13.4 test 1.8): hold the wiper outside range
  using the calibration rig (SET level beyond 32 or HOME and hold), or by
  manually disconnecting the pot. Do NOT provoke a real runaway.
- **Verification:** `pio test -e native` passes. Target compiles.

**Tests covered:** §13.4 test 1.8 (runaway detection fires on bench simulation).

---

### WO-02.9 — Doc Sync + Integration

**Objective:** Cross-check SPEC, resolve all `[PR-02]` placeholders, fill
evidence index, rebase if PR-03 merged first, and run the persistence test.

**Complexity:** `standard`

**Allowed files:**
- `SPEC.md` (resolve `[PR-02]` placeholders only)
- `docs/` (evidence index, measurements)
- `.agents/plugins/principal-architect-workspace/pr-02-calibration-power/PROJECT.md`
  (Part III outcome, session log)
- `src/echenon/LevelMapping.h` (Peloton mapping if deferred from WO1)
- `src/echenon/LevelMapping.cpp`
- `platformio.ini` (only if rebase conflict resolution needed)
- `src/echenon/EchenonModule.h` (only if rebase conflict)
- `src/echenon/EchenonModule.cpp` (only if rebase conflict)
- `src/main.cpp` (only if rebase conflict)

**Scope:**
- **SPEC placeholders:** resolve `[PR-02: N]` for `runaway_timeout_s`, recal
  grid size, averaging window, point limit, reference cadence for the Peloton
  zero-cadence case.
- **Peloton mapping:** deferred to PR-05 (owner decision 2026-10-20). No action
  in this WO. Record the deferral in SPEC §5.3.
- **Evidence index:** ensure all Phase 1 evidence is indexed under
  `docs/measurements/phase1/README.md`.
- **Persistence test** (§13.4 test 1.7): reboot the device, confirm calibration
  data and the recalibrated table persist.
- **Rebase:** if PR-03 has merged to `main` by this point, rebase the
  `calibration-power` stack onto the new `main` and resolve conflicts in the
  shared files (`EchenonModule.*`, `platformio.ini`, `main.cpp`).
- **PR wrap-up:** update PROJECT.md Part III (outcome, session log).
- **Verification:** target compiles after rebase. Native tests pass.

**Tests covered:** §13.4 test 1.7 (persistence). Peloton test 1.9 deferred to PR-05.

---

### File Overlaps

- **Inside the stack:** `EchenonModule.*` and `main.cpp` change in WO1, WO2,
  WO3, WO4, WO5 and WO8 (additive, linear stack).
- **Against other projects:**
  - PR-03 is in parallel (PR #3). Both share `EchenonModule.*`,
    `platformio.ini`, `main.cpp`. Whichever merges second rebases. WO9 handles
    the rebase if needed.
  - `board.h` is modified only in WO5 (adding `PIN_RIG_*` under
    `#ifdef WITH_CALIBRATION_RIG`), which does not conflict with any PR-03
    change.

### Standing Instructions

- Rules 03 (Tag → Build → Flash) and 04 (PlatformIO MCP tools only).
- Surface findings in Completion Reports rather than filing issues directly.
- Every hardware / bench WO is `complexity: interactive` with evidence committed.
- The `WITH_CALIBRATION_RIG` build flag and `ex3-waveshare-nano-rig` env are
  excluded from the production build.
- Framework submodule: use `v0.0.12-beta.3` (`726dce2d`) or the same/later tag
  from `main` that PR-03 used.

## 4. Session Log

### 2026-10-18: PA decomposition proposed

- **Charter read:** PR-02 charter (updated 2026-10-18 with automated calibration
  rig), SPEC §5–§6, §13.4, design note
  `power-calibration-rig.md`, prior art WOs, existing code.
- **Decomposition:** 9 work orders proposed (WO1–WO9). The charter's 9 numbered
  items map roughly one-to-one, with two adjustments:
  1. The calibration rig is split into three WOs (WO5 firmware, WO6 host +
     preflight, WO7 sweep) because WO6 and WO7 are `interactive` (bench/hardware
     sessions) while WO5 is pure software.
  2. CalibrationWizard and RecalibrationUI are combined in WO3 because they
     share the same portal asset pipeline and the recalibration API depends on
     the calibration wizard's wiper table.
- **GPIO selection:** GPIO 17 (D8), GPIO 18, GPIO 11 proposed for the rig.
  GPIO 17 is confirmed free on the Waveshare Nano (unused in v1, v2 draft
  encoder out A). GPIO 18 and 11 need verification against the Waveshare Nano
  header pinout — escalated to the CA for confirmation.
- **Open decisions:**
  1. **GPIO 18 / GPIO 11 availability:** are these broken out on the Waveshare
     ESP32-S3-Nano? Alternatives: GPIO 12, 13, 14, 15, 16.
- **Closed decisions (owner, 2026-10-20):**
  1. **Peloton mapping:** deferred to PR-05. No stub or resolution in PR-02.
  2. **`runaway_timeout_s` default:** 10 s confirmed.
  3. **9-WO plan:** approved.
- **Risks:**
  1. **ADB accessibility:** if `uiautomator dump` can't read the Echelon app's
     power field (e.g. custom rendering), the OCR fallback adds complexity.
     Mitigated by the preflight check in WO6.
  2. **Console encoder acceptance:** the console might reject synthesised
     quadrature at non-human rates or have timing-sensitive input. The design
     note proposes ~50 ms between edges (~human speed), which should be safe.
     WO6's dry-run validates this.
  3. **PR-03 merge ordering:** if PR-03 merges before PR-02, the rebase in WO9
     could have non-trivial conflicts in `EchenonModule.*`. Mitigated by the
     additive nature of PR-02's changes (new members, new tick calls).
- **Awaiting:** PA to begin execution.

## Status
**Plan Status:** APPROVED (owner 2026-10-20). Execution may begin.
