---
schema_version: 1
charter:
  pr_id: PR-01
  slug: pr-01-signal-logger
  title: Signal Logger (Phase 0 — Observe)
  branch: feat/pr-01-signal-logger
  base_branch: main
  allowed_modules:
  - src/echenon/
  - docs/
  - hardware/
  - framework/
  - platformio.ini
  - sdkconfig.defaults
  - main.cpp
  charter_locked: false
status: GRADUATED
work_orders:
- id: WO-01.0
  slug: wo0-adc-spike
  title: ADC-Continuous Spike
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.1
  slug: wo1-scaffold
  title: Project Scaffold + Framework Bump
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.2
  slug: wo2-capture-engine
  title: SignalLogger Capture Engine
  complexity: architect
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.3
  slug: wo3-dashboard
  title: Live Dashboard, CSV Export, mDNS
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.40
  slug: wo4-scope-gate
  title: Bench Setup + Scope Characterisation Gate
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.41
  slug: wo4a-encoder-integration
  title: EncoderModule Code Integration
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.42
  slug: wo4b-bench-gate
  title: 'Bench Gate: Pin Idle, Motor Scope, Current, VIN Tolerance'
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.43
  slug: wo4d-logger-fixes
  title: Logger Fixes (Chief Architect Review Items)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.5
  slug: wo5-register-campaign
  title: 'Register Campaign: Ride + Burst + Remaining Rows'
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.6
  slug: wo6-native-tests
  title: Native Unit Tests
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-01.7
  slug: wo7-doc-sync
  title: Doc-Sync
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
---
## 2. Scope

_See legacy content below._


# PR-01: Signal Logger (Phase 0 — Observe)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main`
> **Dev Phase:** Phase 0 — Observe
> **Next gate:** DR-1 Design Freeze ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §3.5 (Foundation integration), §4.1–§4.4 (target board, hardware configuration, **§4.4.1 Phase 0/1 pin map**), §4.7 (voltage safety), §4.8 (parts and bench equipment), §11.1 (`echenon.local`), §12.1–§12.3 (build system), §13.1–§13.2 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §2.1 (bench equipment), §5 (Phase 0/1 breadboard observer: §5.2 characterisation gate, §5.3 schematic, §5.4 dividers, §5.5 series resistors, §5.6 observation taps, §5.7 build checklist, §5.8 test points)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 0
> **Exit register:** [`docs/measurements/phase0/README.md`](../../../../docs/measurements/phase0/README.md) (the Phase 0 register)
> **Readiness review:** [2026-09-25](../reviews/2026-09-25-readiness-review.md) (RR-04 to RR-08)

## 1. Mandate

Set up the Echenon PlatformIO project on the ESP32 App Foundation, then build a
**passive signal logger** that observes every bike signal without influencing
the console, and produce the evidence that closes every row of the Phase 0
register. Nothing is cut, driven or permanently installed in this PR.

After this PR merges, the firmware boots on the Waveshare ESP32-S3-Nano, logs all
ten tap signals in two capture modes, shows them live in a browser dashboard,
exports them as CSV, and the Phase 0 register is complete with evidence, ready
for DR-1.

### 1. Pin map and taps

- **The pin map is SPEC §4.4.1 (Phase 0/1 map).** It is the single source of truth;
  `board.h`, the dashboard and every WO reference it rather than restating it.
  In summary: ten tap signals on GPIO 1–10, which are exactly the ten ADC1
  channels (pot wiper, pot outer HI, pot outer LO, cadence return, cadence,
  motor + and motor − via the ÷4 divider, encoder Ch A, encoder Ch B, encoder VCC).
- **There are 10 tap signals.** White (+9 V) and Brown (GND) are power, not taps.
- **Tap safety (audit H-02, RR-05):**
  - Every *direct* tap (every tap pin except GPIO 6 and 7) has a **22 kΩ series
    resistor** (acceptable range 10–47 kΩ). It limits back-powering of the ESP32
    through its ESD diodes when the bike is on and the logger is not, and limits
    loading of the console's pot reference and encoder supply.
  - GPIO 6 and 7 are protected by the 30 kΩ / 10 kΩ ÷4 divider, which is
    protection only, not a logic-level interface (audit H-01). It is safe to about 13 V.
  - **Every tap pin is configured with no internal pull-up or pull-down**,
    including pins that also serve a GPIO interrupt.
- **Power:** the logger is powered from **USB-C** (laptop or power bank) for all of
  Phase 0 and Phase 1, with ESP32 GND tied to the loom's Brown wire. The MP1584EN
  buck converter is not used before the PR-04a board.

### 2. Capture design (SignalLogger)

The logger uses the ESP-IDF **ADC continuous (DMA) driver**, not `analogRead()`
polling from `tick()`. It has two capture modes:

- **(a) Ride log.** A decimated per-channel min / mean / max at 10–20 Hz, plus an
  event log with µs timestamps. Events: cadence edges, encoder edges, and
  motor-drive onset / offset detected from analog threshold crossings on GPIO 6/7.
  Sized so a full 60-minute ride fits in PSRAM and exports as CSV.
- **(b) Burst capture.** Triggered 5–30 s windows at 1–5 kHz on all ten ADC1
  channels into PSRAM, downloaded as CSV. Triggers: manual (portal button),
  motor-drive onset, and power-on.

Storage budgets are stated against the real limits: 8 MB PSRAM and a 2.75 MiB
LittleFS partition. Raw kHz logging of a full ride does not fit; that is why the
two modes exist.

**WO0 (first WO) is a spike** that answers three questions before the logger
design is committed:

1. ADC-continuous throughput across all ten channels **with WiFi on**.
2. Whether one pad can be sampled by ADC-continuous and serve a GPIO interrupt at
   the same time (GPIO 5, 8, 9). If not, edge events are derived from the analog
   stream and the WO records that decision.
3. What encoder Ch B actually looks like, captured as analog. The teardown reports
   a ~1.5–2 V momentary pulse, below V<sub>IH</sub> = 2.475 V, so a digital ISR or
   PCNT may not see it (audit H-06). The capture is register evidence.

The spike's findings are written to the WO Completion Report and summarised in this
PROJECT.md Session Log before the logger WOs are finalised.

### 3. Build configuration

- **One target directory:** `hardware/ex3-waveshare-nano/` containing `board.h`,
  `partitions.csv` and `sdkconfig.defaults`. `board.h` is built from the
  framework's `hardware/_common/board_contract.h` and `hardware/_TEMPLATE/`, **not**
  cloned from the reference `hardware/esp32s3nano/board.h`. There is no `config.h`.
- **Flags:** `HAS_*` means "this bike signal exists"; `WITH_*` means "compile this
  framework or app module".
- **`platformio.ini`** follows `framework/template-app/platformio.ini`: Echenon
  declares its own base env with `../` rewritten to `framework/`, including the
  include paths, `lib_extra_dirs`, `extra_scripts` and the sdkconfig chain. The env
  name is `ex3-waveshare-nano`. The framework submodule is `framework/`.
- **The env must NOT define `WITH_DISPLAY_U8G2` or `WITH_BUTTONS`.** The
  framework's reference board maps display DC / CS to GPIO 9 / 10 and a pulled-up
  mode button to GPIO 8 (verified at `v0.0.12-dev.82`). All three are tap pins;
  enabling either module would drive or bias the console's encoder loom and break
  the passive premise.
- **WO1 bumps the framework submodule to `v0.0.12-dev.82` with BLE off.** This picks
  up the #236 WifiManager fix (fallback to an open, unauthenticated AP), which the
  PR-01 portal would otherwise ship with, and avoids a cross-cutting bump in the
  middle of the programme. No `[ble]` mix-in, no `WITH_BLE`, no NimBLE dependency;
  PR-03 turns BLE on.

### 4. Gates

- **Bench-ready entry condition** (before the first WO that connects anything to
  the bike, including WO0's encoder Ch B capture):
  - an oscilloscope with at least 2 channels and single-shot triggering;
  - a way to measure motor current (shunt plus scope, or a DC clamp);
  - JST-SM 9-pin and JST 3-pin mating pairs made into a Y / back-probe harness,
    instead of alligator clips on JST contacts;
  - the series resistors listed in §1 (hardware spec §2.1).
- **Scope characterisation gate** (before the motor-sense taps on GPIO 6/7 are
  connected): *characterise*, not confirm. Scope both motor wires in both drive
  directions and at rest, on a slow timebase (10 ms/div) and a fast timebase
  (µs/div) so kHz PWM is visible. Either result — DC bang-bang or PWM, 3.3 V or
  higher — lets logging proceed, because the ÷4 divider is safe to about 13 V.
  The captures are register evidence (row #1) and feed DR-1.
- **Exit gate:** [`docs/measurements/phase0/README.md`](../../../../docs/measurements/phase0/README.md)
  — every row of the Phase 0 register has an **Evidence** file committed under
  `docs/measurements/phase0/`, named `A<row>-<yyyymmdd>-<short-description>[-<n>].<ext>`
  as the register specifies, and a **Result**. A row with no evidence keeps the
  PR open or is explicitly escalated to the Chief Architect as a DR-1 input gap.

### 5. Interactive hardware and bench work

- Every WO that touches the bike, the scope, the harness or the P12 tablet is a
  **`complexity: interactive`** WO, run with the human at the bench under the
  debug-handoff protocol. It is never spawned as an autonomous worker.
- Each interactive WO commits its evidence (scope captures, CSV exports, photos,
  DMM readings) under `docs/measurements/phase0/` and updates the matching
  register row's Evidence and Result columns.
- Software-only WOs (scaffold, logger, dashboard, export) may be autonomous.

### 6. Portal, dashboard and mDNS

- Live dashboard (an `EchenonModule` portal tab served by the framework's
  `WebServerManager`) showing all ten channels, and CSV download for both
  capture modes; the burst-capture manual trigger is a portal button.
- **Verify `echenon.local` in Chrome on the Echelon P12 tablet** (audit W-02). The
  device's IP address is always available as a fallback (shown in the portal and
  on serial) and the result is recorded as evidence.

### 7. Prior-art warning

The archived WOs below are reference only. **The archived WO1 prescribes the
superseded `[env:echenon]` environment, `hardware/echenon/board.h` and
`src/echenon/Config.h` layout, a two-pin `board.h` with linear ADC limits, and
`extends = env:esp32s3nano` inheritance.** None of that applies: use §3 above and SPEC §4.3, §4.4.1 and §12.

## Prior Art

- [wo1-scaffold/WORKORDER.md](../archive-pa-decomposition/wo1-scaffold/WORKORDER.md) — superseded layout, see §7
- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md)
- [Old PR-01 scope](../archive-pr-v1/pr-01-scaffold-sensors/PROJECT.md)

## 3. Work Order Ledger

| | WO | Branch | Complexity | Deps | Register rows | Status |
|---|---|---|---|---|---|---|
| **[x] [wo0-adc-spike](wo0-adc-spike/WORKORDER.md):** | ADC-continuous spike: throughput with WiFi, ADC+ISR pad sharing, encoder Ch B analog capture | `signal-logger/wo0-adc-spike` | `standard` | — | #8 (partial) | ✅ Complete |
| **[x] [wo1-scaffold](wo1-scaffold/WORKORDER.md):** | Project scaffold + framework bump to `v0.0.12-dev.82` (BLE off) | `signal-logger/wo1-scaffold` | `standard` | WO0 (base) | — | ✅ Complete |
| **[x] [wo2-capture-engine](wo2-capture-engine/WORKORDER.md):** | SignalLogger: ride log (decimated + events) and burst capture (triggered kHz) | `signal-logger/wo2-capture-engine` | `architect` | WO0 report, WO1 | — | ✅ Complete |
| **[x] [wo3-dashboard](wo3-dashboard/WORKORDER.md):** | Live dashboard, CSV export, mDNS / `echenon.local` | `signal-logger/wo3-dashboard` | `standard` | WO2 | — | ✅ Complete |
| **[x] [wo4-scope-gate](wo4-scope-gate/WORKORDER.md):** | Bench setup + scope characterisation gate | `signal-logger/wo4-scope-gate` | `interactive` | WO3, bench-ready | #1, #2, #11 | ⏳ Partial (A,F,G done; B,C,D,E → WO4b) |
| **[x] [wo4a-encoder-integration](wo4a-encoder-integration/WORKORDER.md):** | Wire up EncoderModule on GPIO 8/9 via PCNT | `signal-logger/wo4a-encoder-integration` | `standard` | WO4 | — | ✅ Complete |
| **[x] wo4c:** | Branch housekeeping: rename `fix/adc-gpio-conflict` → `signal-logger/wo4a-encoder-integration` | _(in-place rename)_ | `mechanical` | WO4a | — | ✅ Complete |
| **[x] [wo4b-bench-gate](wo4b-bench-gate/WORKORDER.md):** | Scope gate outstanding items: pin idle, motor scope captures, motor current, VIN tolerance | `signal-logger/wo4b-bench-gate` | `interactive` | WO4c, bench-ready | #1, #2, #11 | ✅ Complete |
| **[x] [wo4d-logger-fixes](wo4d-logger-fixes/WORKORDER.md):** | Logger fixes from CA review: L-01–L-05, L-07, L-08 + native tests for de-interleave, motor FSM, timestamps | `signal-logger/wo4d-logger-fixes` | `standard` | WO4b | — | ✅ Complete |
| **[x] [wo5-register-campaign](wo5-register-campaign/WORKORDER.md):** | Register campaign: rows #3–#10, #12–#13 + E-02 tap cross-check, row #6 load test, console fault-tolerance tests (#1,#2,#3,#7,#9), min pulse width, mid-seek behaviour | `signal-logger/wo5-register-campaign` | `interactive` | WO4d | #3–#10, #12–#13 | ⚠️ Complete with gaps (see 2026-10-11 log): CA-addendum tests carried to DR-1 prep |
| **[x] [wo6-native-tests](wo6-native-tests/WORKORDER.md):** | Native unit tests (decimation, event log, CSV writer, calibration) | `signal-logger/wo6-native-tests` | `standard` | WO5 (base), WO2 (code) | — | ✅ Complete (`3888a83`) |
| **[x] [wo7-doc-sync](wo7-doc-sync/WORKORDER.md):** | Doc-sync: SPEC, hardware spec, ex3 spec + E-01 pot polarity, E-03 A08 superseded, E-04 single formula, E-05 B-report→register mapping, bench wrap-up commit, absolute path cleanup | `signal-logger/wo7-doc-sync` | `standard` | WO6 | — | ✅ Complete (`149b5d1`) |

**File overlaps:** All WOs are linearly stacked; no parallel conflicts. `EchenonModule.*`
touched by WO1→WO2→WO3 (additive). `SignalLogger.*` by WO2→WO3→WO4d (additive).
Evidence files under `docs/measurements/phase0/` by WO0→WO4→WO4b→WO5 (new files only).
`docs/*.md` and `SPEC.md` by WO7 only. `test/native/` by WO4d (new) and WO6 (new).
`src/echenon/SignalLogger.*` and `board.h` by WO4d (fixes).

## 4. Session Log

### 2026-09-26 — Decomposition approved

- **PA assigned:** Tier 2, PR-01 Signal Logger (Phase 0: Observe).
- **Pre-flight:** `main` at `5c04505`, clean working tree, no stash.
  Operating model rule identical to skill asset.
- **Decomposition:** 7 WOs (WO0–WO6). 4 autonomous (`standard`/`architect`),
  2 interactive (bench + bike), 1 autonomous (native tests). Linear stack,
  no parallel conflicts.
- **Chief Architect approval:** Approved the decomposition on 2026-09-26
  via artifact feedback (proceed). Recorded here as the provenance anchor
  per `.agents/rules/00-operating-model.md`.
- **Next:** Create `signal-logger/wo1-scaffold` branch, write WO1
  WORKORDER, spawn worker.

### 2026-09-26 — WO0 complete (ADC-continuous spike)

- **Branch:** `signal-logger/wo0-adc-spike` at `a492755`.
- **Experiment 1 — ADC-continuous throughput with WiFi on:**
  8,333 Sa/s per channel (83.3 kSa/s total) sustained for 60+ seconds.
  Burst capture at 1–5 kHz on all 10 channels is feasible (exceeds 5 kHz
  target by 1.66×).
- **Experiment 2 — ADC + GPIO ISR coexistence:**
  **YES.** `adc_continuous` disables the digital input buffer via
  `gpio_config_as_analog()`. Calling `gpio_input_enable()` re-enables it,
  allowing both edge ISR and continuous ADC conversions on the exact same
  pad. 100/100 edges detected on GPIO 5, 8 and 9. **WO2 design decision:
  cadence and encoder Ch A use GPIO ISR for edge events alongside ADC
  sampling.**
- **Experiment 3 — Encoder Ch B analog capture:**
  Peak 1,449–1,452 mV, typical ~1,444 mV. Well below V_IH (2,475 mV).
  Digital ISR triggered 0/100 edges at sub-threshold. **WO2 design decision:
  Ch B edges must be derived from the analog stream via software threshold,
  not digital ISR or PCNT.** Evidence: `A08-20260926-chb-adc-spike.csv`.
- **GPIO 3 strapping check:** PASS — clean boot with Green tap connected.
- **Next:** WO1 scaffold + framework bump.

### 2026-09-26 — WO1 complete (project scaffold + framework bump)

- **Branch:** `signal-logger/wo1-scaffold` at `beb9321`.
- Framework bumped from `v0.0.12-dev.57-19` to `v0.0.12-dev.82`. BLE off.
- `platformio.ini` follows SPEC §12.2 exactly; BLE mix-in declared but not
  referenced; all app `WITH_*` flags commented out with their PR annotations.
- `board.h` matches SPEC §12.3 / §4.4.1: all 10 pins, `HAS_*` flags,
  no display/button blocks. Board contract passes.
- `EchenonModule` skeleton: ITickable, IConfigurable (`"echenon"`, schema v1),
  IWebContributor (`/api/echenon`). TODO comments for WO2/WO3.
- Partitions: 2 × 6 MB OTA, 1 MB config, 2.75 MB LittleFS, 64 KB coredump.
- Build: 0 errors; RAM 47 KB (14.4%), Flash 1.16 MB (18.5%).
- Boot: clean to setup AP on `esp32-device`, HTTP server on port 80.
- **Next:** WO2 capture engine.

### 2026-09-26 — WO2 complete (SignalLogger capture engine)

- **Branch:** `signal-logger/wo2-capture-engine` at `188694b`.
- `SignalLogger` implements two-mode capture: decimated ride log (20 Hz,
  min/mean/max) and triggered burst capture (kHz, 10 channels, PSRAM).
- ADC-continuous driver: 83.3 kSa/s across 10 ADC1 channels.
- GPIO ISR with `gpio_input_enable()` for cadence (GPIO 5) and encoder
  Ch A (GPIO 8). Spinlock-protected event ring buffer.
- Software threshold edge detection for encoder Ch B (800 mV) and motor
  onset/offset (500 mV) — correctly derives events from analog stream.
- CSV export endpoints: `/api/echenon/ride-log.csv`, `/events.csv`,
  `/burst.csv`, `/burst/start` (POST), `/status` (JSON).
- Power-on burst capture auto-triggers on `begin()`.
- **PSRAM decision:** `kRideLogMaxRows` reduced from 72,000 to 36,000
  (30 min instead of 60 min) to fit ride log + burst buffer within 8 MB.
  The 60-min ride test (SPEC 0.9) may require clearing the burst buffer
  first — acceptable for Phase 0.
- **Noted:** GPIO ISR pins have `PULLUP_ENABLE` — should be `PULLUP_DISABLE`
  per mandate (no internal pulls on tap pins). Non-functional in Phase 0
  observation mode. Tracked for cleanup.
- **Next:** WO3 dashboard.

### 2026-09-26 — WO3 complete (dashboard + mDNS)

- **Branch:** `signal-logger/wo3-dashboard` at `406c5eb`.
- Dashboard JS (14 KB): live channel display, burst trigger, CSV downloads,
  status bar. Registered as portal tab at order 50.
- Live telemetry endpoint `/api/echenon/live` for per-channel stats.
- mDNS responder: `echenon.local` via `MDNS.begin("echenon")`. Framework
  doesn't provide mDNS natively (tracked upstream as issue #232).
- Build: 0 errors. Flash 19.5%, RAM 15.3%.
- **All autonomous firmware WOs complete (WO0–WO3).** Firmware is
  feature-complete for Phase 0: scaffold, ADC capture engine, dashboard,
  CSV export, mDNS.
- **Next:** WO4 + WO5 are interactive (bench + bike). WO6 is autonomous.

### 2026-10-04 — Encoder integration decomposition approved

- **Context:** ADC-based encoder analysis complete (B02, B03 reports committed).
  39% bounce, 10.7% skip, 28% console miss rate. Resistance calibration and
  eccentric cam model documented. MITM design notes captured.
- **Branch state:** `fix/adc-gpio-conflict` at `0f31c04`, 13 commits ahead of
  `signal-logger/wo4-scope-gate`. Clean working tree. Tag `v0.1.0-dev.4`.
- **Decomposition:** 3 sub-WOs for encoder integration:
  - **WO4a** (`standard`, autonomous): Wire up EncoderModule — release GPIO 8/9
    from ADC, add `BOARD_ENCODERS[]`, enable `WITH_ENCODER`, register module,
    strip diagnostic instrumentation. Build-verify.
  - **WO4b** (`interactive`, bench): Flash, compare PCNT vs ADC baseline,
    determine `stepsPerDetent`, commit B04 comparison report.
  - **WO4c** (`mechanical`, autonomous): Merge `fix/adc-gpio-conflict` into
    `signal-logger/wo4-scope-gate`, delete fix branch.
- **User approval:** Approved the decomposition on 2026-10-04 via artifact
  feedback (proceed). Recorded here as the provenance anchor per
  `.agents/rules/00-operating-model.md`.
- **Next:** Spawn WO4a worker.
- **WO4a complete:** `305e20c` on `fix/adc-gpio-conflict`. EncoderModule wired,
  ADC reduced to 8 channels, diagnostics stripped, build clean (0 errors,
  RAM 15.3%, Flash 19.6%). Worker completion report at
  `wo4a-encoder-integration/WORKORDER.md`.
- **WO4b complete:** B04 PCNT vs ADC comparison committed. PCNT eliminates
  all encoder quality issues (0% bounce, 0% skip, 100% click detection).
  `stepsPerDetent=4` confirmed. 5-minute soak test passed (stable heap, no crashes).

### 2026-10-04 — WO4 session wrap-up

- **WO4 status:** Partial. Checklist items A, F, G complete. Items B, C, D, E
  (pin idle, scope captures, motor current, VIN) deferred — require dedicated
  scope time. Recommend separate interactive WO.
- **WO4a status:** Complete. EncoderModule on GPIO 8/9 via PCNT, validated.
- **Branch:** `fix/adc-gpio-conflict` at `0297d50`, 18 commits ahead of
  `signal-logger/wo4-scope-gate`. Tag `v0.1.0-dev.5`. Merge pending (WO4c).
- **Evidence committed:** B01 (sensor bringup), B02 (encoder ADC baseline),
  B03 (resistance calibration + cam model), B04 (PCNT comparison).
  9 files total under `docs/measurements/phase0/`.
- **Design documents committed:** `docs/encoder-resistance-analysis.md`
  (consolidated analysis), `docs/mitm-encoder-design-notes.md` (architecture
  options, 256-level model, console fault tolerance tests).
- **Key findings for DR-1:**
  - Encoder signal has 39% bounce / 28% console miss rate → PCNT solves completely
  - Resistance mechanism is eccentric cam (8mm, 0–151°) → non-linear feel
  - Console uses bang-bang motor control with 98% overshoot at mid/high levels
  - 256 internal levels, Echelon app must stay functional, multiple MITM architectures viable
- **Decisions:** Cadence resistors 4.7kΩ+100nF, encoder wiring corrected,
  GPIO 8/9 released from ADC, stepsPerDetent=4 confirmed.
- **Handoff:** Session wrap-up report passed to Chief Architect.

### 2026-10-04 — Resumption: ledger approved, WO4c complete

- **PA resumed:** Tier 2, session context restored from MEMORY.md + PROJECT.md +
  bench wrap-up report.
- **Pre-flight:** `signal-logger/wo4a-encoder-integration` (renamed from
  `fix/adc-gpio-conflict`) at `da5ffee`, clean working tree, no stash, 19 commits
  ahead of `signal-logger/wo4-scope-gate`. Tags `v0.1.0-dev.1` through `v0.1.0-dev.5`.
- **Owner decisions recorded (approved during bench sessions):**
  1. Use framework EncoderModule (PCNT) for the knob. GPIO 8/9 off ADC, 8 ADC
     channels remain. Encoder VCC stays on GPIO 10 as ADC monitor.
  2. Cadence tap: 4.7 kΩ + 100 nF (not 22 kΩ). Bench-verified in B01.
  3. Encoder wire colours: Black = Ch A, Red = Ch B, Orange = VCC (measured in B01).
  4. 256 internal resistance levels and "console-as-signal-source" MITM direction
     are design inputs for DR-1. Chief Architect folds them in. NOT implemented in PR-01.
- **Decomposition:** 5 new WOs (WO4c, WO4b, WO5, WO6, WO7). 2 interactive
  (bench + bike), 2 autonomous (tests + doc-sync), 1 mechanical (branch rename).
  Linear stack, no parallel conflicts.
- **User approval:** Approved the updated ledger on 2026-10-04 ("implement these
  work orders autonomously … stage the PR for review"). Recorded here as the
  provenance anchor per `.agents/rules/00-operating-model.md`.
- **`diag:` commits** (`2a12820`, `fe60d76`): keeping in history. Code was stripped
  in WO4a (`a8c7dd4`); net contribution is zero. Will note in PR description at
  submission. No history rewrite.
- **WO4c complete:** Branch renamed `fix/adc-gpio-conflict` →
  `signal-logger/wo4a-encoder-integration`. Ledger updated.
- **Next:** Write WO4b WORKORDER + bridge prompt (interactive).

### 2026-10-04 — CA progress review addendum integrated

- **CA review:** L-01 through L-08 (logger defects), E-01 through E-06 (evidence gaps),
  P-A through P-E (process items).
- **WO4d inserted:** Logger fixes (L-01–L-08) between WO4b and WO5. Standard complexity,
  autonomous worker.
- **WO5 expanded:** E-02 tap cross-check, row #6 load test, console fault-tolerance
  tests (#1/#2/#3/#7/#9 safe subset; #4/#6 blocked without owner go-ahead), minimum
  quadrature pulse width, mid-seek behaviour.
- **WO7 expanded:** E-01 pot polarity, E-03 A08 superseded, E-04 single formula,
  E-05 B-report mapping, bench wrap-up commit.
- **All WORKORDERs updated and committed** at `b1d0e07`.

### 2026-10-11 — WO4b bench gate complete, WO4d logger fixes complete

- **WO4b (interactive bench session):**
  - Owner at bench with Rigol DHO804 + DMM, PA interpreting remotely.
  - Item B (pin idle): already done in B01 sensor bringup.
  - Item C (motor scope captures): 3 captures — reverse drive end (10ms/div),
    forward onset (20ms/div), fast timebase (50µs/div). ±3.25V flat DC, no PWM.
    Probes directly on motor wires (not divider output).
  - Item D (motor current): DMM in series. Reverse: 175mA inrush → 150mA steady.
    Forward: 95mA inrush → 50mA steady. Motor drives eccentric cam; load independent
    of flywheel speed (magnetic braking, not friction).
  - Item E (VIN tolerance): Datasheet — Waveshare VIN 6–21V (MP2322GQH), 9V safe.
  - Register rows #1, #2, #11 closed with evidence.
  - **Owner observation (parked for DR-1):** Motor drive lines may be used as signal
    inputs only, with actual motor drive via separate H-bridge. Power architecture
    deferred to DR-1.
  - Committed at `5b4afbf` on `signal-logger/wo4b-bench-gate`.

- **WO4d (autonomous worker, flash model):**
  - Worker spawned and completed in ~17 minutes.
  - All 8 review items fixed: burst de-interleave (L-01), per-channel motor FSM
    with debounce (L-02), per-sample timestamps + overflow counter (L-03), encoder
    EventBus subscription (L-04), 8-channel rows + 10Hz + wrap flag (L-05), debris
    cleanup (L-07), WITH_SIGNAL_LOGGER guard (L-08).
  - New file: `src/echenon/SignalLogic.h` — testable pure logic extracted.
  - New file: `test/test_native/test_signal_logic.cpp` — 14 GoogleTest cases, all pass.
  - Build verified: with and without WITH_SIGNAL_LOGGER. No printf remaining.
  - L-06 (export race condition) noted as out of scope — needs design decision.
  - Committed at `60409b9` on `signal-logger/wo4d-logger-fixes`.

- **Next:** Tag, build, flash corrected firmware. Then WO5 (interactive bench session
  with corrected firmware).

### 2026-10-11 — WO5, WO6, WO7 complete; PR-01 merged (recorded by Chief Architect)

No completion reports were written for WO5 or WO7. This entry is reconstructed
from the commits, the Phase 0 register and the Chief Architect's review.

- **WO5 register campaign** (interactive, `3c5b83c`, `36f8a14`):
  - Rows #3, #5: full 360° pot profile (2 runs). Two symmetrical working ramps
    (0–2120 counts), dead zones at 0° and 180°. Motor run about 16 s per half
    revolution; coast < 15 counts.
  - Row #4: 32 levels mapped; `wiper = 39.1 × level + 97`; 4 detents (16 encoder
    steps) per level, 512 steps full range.
  - Row #6: Green is a 1.66 V source with 2.4 kΩ impedance; Yellow is 0 V.
  - Row #9: the console tolerates a disconnected motor (it hunts, does not fault,
    and recovers when reconnected). Its bang-bang servo has no motor timeout.
  - Row #13: the console powers on at level 1 with no calibration sweep; seeking
    starts only when a class starts.
  - Row #12: **INCONCLUSIVE.** Android could not see the nRF Connect mock. The
    definitive test moves to **PR-03 WO0** (ESP32 HR peripheral). This corrects
    the commit's "handoff to PR-02"; PR-02 has BLE off.
  - Row #10: **Open.** Expected to be waived at DR-1, because E-01 removes
    parallel supplies onto console rails.
  - **Not done** (CA addendum, 2026-10-04): E-02 tap cross-check; console
    fault-tolerance tests #1, #2, #3, #7, #9; minimum quadrature pulse width;
    mid-seek click behaviour. These go to the **DR-1 prep bench session**
    ([`pr-dr1-prep`](../pr-dr1-prep/PROJECT.md)).
  - SPEC test 0.9 (60-minute ride log) has no committed evidence.
- **WO6 native tests** (`3888a83`): pure logic extracted into `SignalLogic.h`;
  googletest suites cover decimation, event ordering and CSV output.
- **WO7 doc-sync** (`149b5d1`): SPEC, the hardware spec and the ex3 spec now
  match the measurements (pot polarity, wire colours, cadence tap, ADC/PCNT split,
  A08 superseded, B03 formula). Absolute paths removed from `docs/`.
- **PR #1 merged** to `main` at `5897f6f` (merge commit, 46 commits) on
  2026-10-11. **No review happened before the merge**, so the "review before
  merge" checkpoint was skipped. The Chief Architect's evidence-level review found
  nothing that should have blocked it. The WO4d and WO6 code has not been
  code-reviewed.

### 2026-10-11 — Project Wrap-Up

- **Architectural outcome:**
  - Phase 0 observation firmware: ADC-continuous capture on 8 ADC1 channels
    (83.3 kS/s), a decimated ride log plus event log, triggered bursts, and CSV
    export, all behind `WITH_SIGNAL_LOGGER`.
  - Knob decoded by framework `EncoderModule` (PCNT).
  - Target `hardware/ex3-waveshare-nano/` on framework `v0.0.12-dev.82`.
- **Design-changing evidence for DR-1:**
  - flat ±3.25 V DC motor drive with no PWM;
  - motor current ≤ 175 mA;
  - VIN tolerates 9 V;
  - no power-on calibration sweep;
  - the console tolerates a disconnected motor;
  - pot linear in angle (one of two ramps per revolution used);
  - reversed pot reference (Green is a 1.66 V / 2.4 kΩ source).
- **Lessons:**
  - Interactive bench sessions drift from their WO boundary. Declare
    exploration up front, and require a completion report even for
    interactive WOs.
  - Re-check derived metrics when a new measurement changes the model. The B02
    "28% missed clicks" figure is unreconciled with 4 detents per level.
  - Keep WOs on stack branches; `fix/*` branches broke provenance until
    WO4c fixed it.
  - Hold the pre-merge review checkpoint.
- **Unresolved, carried forward:**
  - Row #10, to be waived at DR-1.
  - Row #12, to PR-03 WO0.
  - CA-addendum tests, to DR-1 prep.
  - Row #1 sign convention: is any wire negative relative to GND? To DR-1 prep.
  - L-06 export race, a design decision deferred.
  - 21 MB of CSV evidence in git; gzip it or use LFS in future phases.

## Status
**Plan Status:** ✅ COMPLETE — merged to `main` as PR #1 (`5897f6f`) on 2026-10-11
