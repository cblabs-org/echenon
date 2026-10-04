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

## Mandate

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

## Work Order Ledger

| | WO | Branch | Complexity | Deps | Register rows | Status |
|---|---|---|---|---|---|---|
| **[x] [wo0-adc-spike](wo0-adc-spike/WORKORDER.md):** | ADC-continuous spike: throughput with WiFi, ADC+ISR pad sharing, encoder Ch B analog capture | `signal-logger/wo0-adc-spike` | `standard` | — | #8 (partial) | ✅ Complete |
| **[x] [wo1-scaffold](wo1-scaffold/WORKORDER.md):** | Project scaffold + framework bump to `v0.0.12-dev.82` (BLE off) | `signal-logger/wo1-scaffold` | `standard` | WO0 (base) | — | ✅ Complete |
| **[x] [wo2-capture-engine](wo2-capture-engine/WORKORDER.md):** | SignalLogger: ride log (decimated + events) and burst capture (triggered kHz) | `signal-logger/wo2-capture-engine` | `architect` | WO0 report, WO1 | — | ✅ Complete |
| **[x] [wo3-dashboard](wo3-dashboard/WORKORDER.md):** | Live dashboard, CSV export, mDNS / `echenon.local` | `signal-logger/wo3-dashboard` | `standard` | WO2 | — | ✅ Complete |
| **[ ] [wo4-scope-gate](wo4-scope-gate/WORKORDER.md):** | Bench setup + scope characterisation gate | `signal-logger/wo4-scope-gate` | `interactive` | WO3, bench-ready | #1, #2, #11 | ⬜ Not started |
| **[ ] [wo5-register-campaign](wo5-register-campaign/WORKORDER.md):** | Register campaign: ride, burst, remaining rows #3–#10, #12–#13 | `signal-logger/wo5-register-campaign` | `interactive` | WO4 | #3–#10, #12–#13 | ⬜ Not started |
| **[ ] [wo6-native-tests](wo6-native-tests/WORKORDER.md):** | Native unit tests (decimation, CSV writer) | `signal-logger/wo6-native-tests` | `standard` | WO2 (code), WO5 (base) | — | ⬜ Not started |

**File overlaps:** All WOs are linearly stacked; no parallel conflicts. `EchenonModule.*`
touched by WO1→WO2→WO3 (additive). `SignalLogger.*` by WO2→WO3 (additive).
Evidence files under `docs/measurements/phase0/` by WO0→WO4→WO5 (new files only).

## Session Log

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

## Status
**Plan Status:** WO4a spawning — encoder integration (fix/adc-gpio-conflict)
