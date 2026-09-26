# Echenon — Iterative Development Roadmap

## Philosophy

Each phase is **useful on its own** and retires specific unknowns before the
next phase adds complexity. Hardware and firmware develop together — we don't
build the full intercept board and then write firmware; we build what we need
for the current phase and extend it.

The product is **passive-first**. The default permanent install is the
passive tap board (UC1–UC3). The MITM intercept board is optional and adds
Override and ERG (UC4–UC6). Nothing is soldered until the Design Freeze
Review (DR-1) has turned Phase 0 measurements into a frozen design.

The phase numbering and PR mapping follow the programme plan
([`PROGRAM-PLAN.md`](../.agents/skills/principal-architect-workspace/PROGRAM-PLAN.md))
and SPEC §1.5:

| Phase | Work | PR | Hardware configuration ([hardware spec §1](hardware_specification.md)) |
|---|---|---|---|
| 0 | Observe | PR-01 | Breadboard observer (USB-powered) |
| — | **Design Freeze** | **DR-1** | — (docs-only milestone) |
| 1 | Calibrate | PR-02 (gated on DR-1) | Breadboard observer |
| 2 | HR relay | PR-03 | HR-only (USB) |
| 3 | Passive tap board | PR-04a | Passive tap board |
| 3 | FTMS telemetry | PR-05 | Breadboard observer is enough; passive tap board once built |
| 4 | Session + TCX + Strava | PR-06 | No new hardware (any of the above) |
| 5 | MITM board + motor control + safety | PR-07 | Intercept board |
| 6 | ERG + Control Point | PR-08 | Intercept board |
| 7 | Integration / release candidate | PR-09 | Passive tap board and intercept board |

```
Phase 0    DR-1      Phase 1    Phase 2    Phase 3        Phase 4     Phase 5     Phase 6    Phase 7
Observe  → Freeze → Calibrate → HR Relay → Passive tap  → Session   → MITM      → ERG      → Integration
  │          │         │          │         + FTMS         + Strava     │           │           │
  ▼          ▼         ▼          ▼          ▼              ▼           ▼           ▼           ▼
  Read      Design    Passive    BLE HR    Permanent      Rides       Control     Close the   UC1–UC6
  signals   from data level map  proxy     install, apps  upload      the motor   loop        end-to-end
```

---

## Phase 0 — Passive Observation (PR-01)

> **Goal:** Read every signal on the bike without changing anything.
> Learn the actual electrical behaviour before we try to control it.

### Bench-Ready Entry Condition

PR-01's first hardware Work Order does not start until the bench has:

- an oscilloscope with **≥ 2 channels and single-shot triggering**;
- a way to measure **motor current** (shunt plus scope, or a DC clamp);
- the **JST-SM 9-pin and JST 3-pin Y / back-probe harness** — no alligator
  clips on JST contacts;
- the **22 kΩ series resistors** for the direct taps.

Details: [`hardware_specification.md`](hardware_specification.md) §2.1 and
§5.1.

### Motor-Drive Characterisation Gate

A DMM measured **3.3V** on the motor wires, but a DMM shows the *average*
voltage — it cannot distinguish 3.3V DC from 9V PWM at ~37% duty cycle.
The gate **characterises** the drive rather than confirming an expected
answer: scope both motor wires, in both directions and at rest, on a slow
timebase (10 ms/div) and a fast one (µs/div) so kHz PWM is visible.

**Either result lets logging proceed** — the ÷4 divider keeps the ESP32 pin
safe up to about 13V. The captures feed Phase 0 register row 1 and DR-1
(sense network, relay logic, motor rail).

> **Step-by-step procedure:** [`hardware_specification.md`](hardware_specification.md)
> **§5.2 — Motor-Drive Characterisation Gate.**

### Hardware — Breadboard Observer

No soldered board. The Y / back-probe harness plugs in between the looms and
the console — **no wires cut, no connectors split.** There are **10 tap
signals** (7 on the 9-pin loom, 3 on the 3-pin loom); White (+9V) and Brown
(GND) are power, not taps. They map one-to-one onto GPIO 1–10, the ten ADC1
channels; the authoritative pin map is SPEC §4.4.1.

- Motor sense (GPIO 6/7) uses 30kΩ/10kΩ ÷4 dividers for bidirectional
  protection.
- Every other tap has a **22 kΩ series resistor**, and every tap pin is
  configured with **no internal pulls**, so the logger cannot back-power or
  bias the console's lines.
- The logger is **powered from USB-C** (power bank or laptop on battery),
  with ESP32 GND tied to Brown. The buck converter is not used before the
  passive tap board. The logger can be flashed at any time without rewiring,
  and it is already logging when the bike is switched on (register row 13).

The console stays fully connected and controls everything as stock.

> **Full schematic, parts list, build checklist, and divider analysis:**
> See [`hardware_specification.md`](hardware_specification.md) **§5 — Phase 0/1
> Breadboard Observer.**

### Firmware

`SignalLogger` uses the ESP-IDF **ADC continuous (DMA)** driver across all
ten ADC1 channels — not `analogRead()` polling from `tick()` — and has two
capture modes:

| Mode | What it records | Sizing |
|---|---|---|
| **(a) Ride log** | Decimated per-channel min / mean / max at 10–20 Hz, plus an event log with µs timestamps: cadence edges, encoder edges, and motor-drive onset / offset from analog threshold crossings | A full 60-minute ride fits in PSRAM and exports as CSV |
| **(b) Burst capture** | Triggered 5–30 s windows at 1–5 kHz on all ten ADC1 channels into PSRAM, downloaded as CSV | Triggers: manual (portal button), motor-drive onset, power-on |

| Module | Purpose |
|---|---|
| `SignalLogger` | ADC-continuous acquisition, ride log, burst capture, event log |
| `WebServerManager` (framework) | Serves the live dashboard (an `EchenonModule` portal tab) showing raw signal values in real time |
| `WifiManager` (framework) | Connect to home WiFi for portal access (`echenon.local`, with IP fallback) |
| `SignalLogger` CSV export | Download ride logs and burst captures as CSV via the portal |

**No BLE, no FTMS, no session manager, no power calculation yet.** Pure
observation.

**WO0 spike (first Work Order).** Before the logger is built, a spike
answers three questions:

1. ADC-continuous throughput with WiFi on.
2. Whether one pad can be sampled by ADC-continuous and serve a GPIO
   interrupt at the same time (GPIO 5, 8, 9).
3. What encoder Ch B actually looks like, captured as analog. The teardown
   reports a ~1.5–2V pulse, below V_IH, so a digital ISR or PCNT may not see
   it without conditioning.

**Framework bump.** PR-01 WO1 bumps the `framework/` submodule to
`v0.0.12-dev.82` with BLE **off**, to pick up the WifiManager fix (#236).
PR-03 turns BLE on.

**Hardware configuration (established in Phase 0):** one per-target
directory (see SPEC §4.3 and §12):

- `hardware/ex3-waveshare-nano/board.h` — pin map, bike characteristics and
  `HAS_*` signal flags. Built from the framework's
  `hardware/_common/board_contract.h` and `hardware/_TEMPLATE/`, not cloned
  from the reference `esp32s3nano` board.
- `hardware/ex3-waveshare-nano/partitions.csv` — flash partition table
- `hardware/ex3-waveshare-nano/sdkconfig.defaults` — SDK overrides

`HAS_*` means "this bike signal exists"; `WITH_*` means "compile this
framework or app module". The `[env:ex3-waveshare-nano]` environment follows
`framework/template-app/platformio.ini` and **must not** define
`WITH_DISPLAY_U8G2` or `WITH_BUTTONS` — the reference board maps display
DC/CS and a pulled-up button onto GPIO 8, 9 and 10, which are tap pins.
Supporting a different board or bike is a new target directory +
environment, not a code change.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| **Motor drive (CRITICAL)** | Scope captures: DC or PWM, levels, polarity, behaviour at rest. Feeds the DR-1 sense and driver design. |
| Motor current | Shunt or clamp: start inrush and running current under brake load. Sizes the Phase 5 driver and rail. |
| Motor timing | Duration of one full cam revolution; how quickly the console responds to an encoder click; coast after drive stops. |
| Cadence signal shape | Pulse width, rise/fall times, bounce characteristics. Tune debounce parameters. |
| Pot wiper vs cam angle | Wiper, Yellow and Green at every console level, both directions. The cam makes the relationship non-linear. |
| Pot reference voltages | What voltage does the console apply to Yellow and Green, and how stiff is it? Determines the standalone reference design (R4, Green–GND link). |
| Encoder pulses per detent | Count exact pulses per click. Verify quadrature direction logic and compatibility with the framework's `EncoderModule` (PCNT). |
| Encoder VCC and Ch B | What voltage does the console supply on Enc VCC (Red)? Does Ch B need conditioning? |
| Cadence return line | What does Grey do? Is it GND? VCC? Floating? Resolves the colour-mapping contradiction. |
| Console power-on behaviour | Does the console run a calibration sweep at power-on, and how does it find its reference? |
| Signal correlations | Encoder fires → motor drives → pot changes. Log all three with timestamps to see the cascade and measure end-to-end latency. |

### Risks Retired

- ✅ Motor drive characterised — the MITM design is based on measurement
- ✅ All signal characteristics measured, not assumed from teardown
- ✅ Pot reference circuit understood (Yellow/Green voltages)
- ✅ Debounce parameters tuned from real data
- ✅ ADC-continuous capture proven with WiFi on
- ✅ Web portal and WiFi stack working
- ✅ Basic firmware architecture proven

### Go/No-Go Criteria (PR-01 exit)

The exit checklist is the **Phase 0 register**,
[`measurements/phase0/README.md`](measurements/phase0/README.md).

- [ ] **Every register row (#1–#13)** has an Evidence file and a Result
- [ ] All **10 tap signals** read cleanly on the dashboard
- [ ] Ride log: at least one full ride captured and exported as CSV
- [ ] Burst capture: manual, motor-drive-onset and power-on triggers each
      produce a downloadable CSV
- [ ] WO0 spike questions 1–3 answered and recorded
- [ ] Cadence matches manual pedal count (±1 RPM)

### Deliverable

**A raw data dashboard and a completed Phase 0 register** — ride the bike
normally, see all signals live in the browser, download the ride log and
burst captures. The register, with evidence, is the input to DR-1.

---

## DR-1 — Design Freeze Review

> **Goal:** Turn Phase 0 evidence into a frozen design before anything is
> soldered or calibrated against. Docs-only; owned by the Chief Architect.

- **Inputs:** the Phase 0 register with every row's evidence (scope
  captures, CSVs, photos) under `docs/measurements/phase0/`, and the WO0
  spike results.
- **Outputs:**
  - revised SPEC §4–§6 (working range, loop rates, dead zones);
  - the passive tap board design
    ([hardware spec §6.2](hardware_specification.md)) and a revised
    intercept board ([hardware spec §7–§8](hardware_specification.md));
  - decision D3 (motor driver and rail) and H-05 (fail-safe);
  - the standalone-supply design (R3, R4, R5, Green–GND link; audit H-02);
  - a cam-aware control-loop architecture (full-revolution angle tracking,
    working half-revolution, angle-based level table, one-revolution
    run-time bound, reset-safe driver inputs);
  - the power model re-indexed on cam angle.
- **Record:** `.agents/skills/principal-architect-workspace/reviews/DR-1-design-freeze.md`.
- **Rule:** no hardware is soldered or permanently installed, and PR-02 does
  not start, until DR-1 is approved.

---

## Phase 1 — Calibration & Power Validation (PR-02)

> **Goal:** Convert raw signals to meaningful metrics and validate against
> the Echelon app's own readings. Answer: "How accurate is our data?"
> Depends on PR-01 **and DR-1**.

### Hardware

Same breadboard observer as Phase 0. Still passive taps, still USB-powered.

### Calibration Model (passive)

The ESP32 cannot drive the motor until Phase 5, so Phase 1 uses **passive
calibration**: the rider steps the console through its 32 levels while the
logger records wiper voltage and cam angle at each level. The console's
level is the resistance proxy. Power is indexed by cam angle within the
working half-revolution; "level" is only a display scale.

Motor-driven calibration — a full-revolution sweep that finds the cam
extremes — belongs to Phase 5 (PR-07).

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `CadenceSensor` | Production cadence calculation (RPM from ISR intervals, timeout, filtering) |
| `ResistanceSensor` | Wiper → cam angle → level, using the angle-based level table from DR-1 |
| `PowerEngine` | First power model, indexed by cam angle and cadence. Start with physics model or community table. |
| `CalibrationWizard` | Portal page: guides the rider through the 32 console levels and stores the table in NVS |

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Our cadence vs Echelon's cadence | Side-by-side comparison. Ride a class, note Echelon app's cadence display, compare to our portal. They should match exactly (same reed switch). |
| Our level vs Echelon's | After passive calibration, when the Echelon app says "19", do we say "19"? If not, the table or the angle model is off. |
| Power curve accuracy | Calculate watts from cadence × cam angle using our model. Compare to what Echelon displays for power. How far off are we? |

### Independent Power Verification

The Echelon app shows power, but how accurate is *their* number? Options for
independent assessment:

| Method | Pros | Cons | Cost |
|---|---|---|---|
| **A. Reference power meter pedals** (e.g. Favero Assioma Duo) | Gold standard. Direct watts measurement at the crank. | Expensive. Must be compatible with the EX-3 pedal threads. | ~£400-600 |
| **B. Borrow a power meter** | Free if you know someone with one. | Short-term, limited data. | £0 |
| **C. Compare to Echelon's displayed power** | Zero cost. Already available. | Echelon's power may itself be inaccurate — we're comparing to an unknown reference. | £0 |
| **D. Roller/dyno test** | Physics-based. Known inertia, known torque. | Complex setup. The magnetic brake makes this harder than a friction brake. | £50-100 |
| **E. Cross-reference Xert/Strava relative effort** | Compare our power to perceived effort (HR, RPE). If 200W feels like a walk in the park, our numbers are wrong. | Subjective. Not a calibration tool. | £0 |

**Recommendation:** Start with **C** (compare to Echelon app) — it's free
and immediately available. If the numbers diverge by >10%, investigate
further. If they're close, we're probably in the right ballpark because
Echelon almost certainly calibrated against *something* during development.

If you want ground truth later, **A** is the real answer. A set of Assioma
Duo-Shi pedals (Shimano SPD-SL compatible) would give you a reference you can
use forever — not just for this project but for all future training. They also
broadcast power over BLE/ANT+, so Xert/Strava would get reference power while
Echenon provides its estimate.

### Risks Retired

- ✅ Cadence reading validated against Echelon
- ✅ Passive calibration captured and persisted
- ✅ Power estimation baseline established
- ✅ Calibration persistence (NVS) working
- ✅ Understanding of how Echelon's power compares to ours

### Go/No-Go Criteria

- [ ] Cadence matches Echelon app within ±1 RPM
- [ ] Derived level matches the Echelon app's display (1–32) after passive calibration
- [ ] Power estimate within ±15% of Echelon's displayed power
- [ ] Calibration survives power cycle (NVS persistence)

### Deliverable

**A calibrated dashboard** — ride the bike, see accurate cadence, level,
and estimated power in the browser. Side-by-side with the Echelon app, the
numbers should match.

---

## Phase 2 — HR Armband Relay (PR-03)

> **Goal:** Connect to the user's HR armband over BLE, re-broadcast the
> heart rate as a standard BLE HR service. The Echelon app pairs with the
> ESP32 for HR instead of the armband directly. **Immediate user value with
> zero bike risk.**

### Entry Gates

- The Echelon-app HR spike passes (the app pairs a third-party `0x180D`
  peripheral; Phase 0 register row 12).
- Framework central-role acceptance evidence exists: RFC §17.2 items 4–9 run
  against a real `0x180D` peripheral on a framework tag that includes the
  WO-8 use-after-free fix. **This gate is OPEN** (at `dev.82` the framework
  marks central acceptance "N/A").

### Hardware

HR-only (USB): just the ESP32 on USB-C. The HR relay is entirely BLE — it
doesn't interact with any bike signals. Can be developed in parallel with
Phase 1.

### Firmware (New Modules)

PR-03 turns on the framework's `BLEManager` (`WITH_BLE`).

| Module | Purpose |
|---|---|
| `BLEManager` (framework) | NimBLE stack initialisation, advertising, connection management. Foundation for all future BLE work. |
| `HRMClient` | BLE Central role: scan for and connect to the configured HR armband (by name or MAC address). Subscribe to HR Measurement notifications (`0x2A37`) and publish HR on the EventBus. |
| `HRMRepeater` | BLE Peripheral role: subscribes to HR events and re-broadcasts them as a standard Heart Rate Service (`0x180D`). The Echelon app sees `Echenon-XXXX` as an HR device and pairs with it. |
| Portal: BLE page | Scan for nearby HR devices, select and pair, store MAC in NVS. |

### Why This Phase Matters

The HR relay is the **simplest useful BLE feature** and proves several things
we'll need for every subsequent phase:

1. **NimBLE dual-role** — Central (connecting to armband) and Peripheral
   (advertising to Echelon app) running simultaneously on the same radio.
2. **BLE coexistence** — Does the ESP32's advertising interfere with the
   Echelon console's own BLE? (The console has its own BLE for the Echelon
   app's bike metrics pairing.)
3. **Connection management** — Auto-reconnect when the armband drops
   (fast backoff, then an indefinite slow retry), multiple simultaneous
   connections, advertising.
4. **Portal integration** — The BLE scan/pair page is the foundation for
   FTMS device discovery later.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| NimBLE dual-role on ESP32-S3 | Does Central + Peripheral work simultaneously? Any resource conflicts? |
| BLE coexistence with Echelon PCB | Does the ESP32's advertising interfere with the factory Echelon BLE? Can the Echelon app still pair with the bike normally? |
| HR armband pairing reliability | Does the armband pair on first scan? Auto-reconnect after power cycle? |
| Re-broadcast latency | Is the HR relay latency perceptible? (Expected: <100 ms — imperceptible.) |
| Connection limits | How many simultaneous Peripheral connections can we support? (Target: 3.) |

### Risks Retired

- ✅ BLE stack initialisation and dual-role proven
- ✅ NimBLE works alongside WiFi (portal + BLE simultaneously)
- ✅ HR armband pairing and re-broadcast working
- ✅ BLE coexistence with factory Echelon PCB confirmed (progressive check)
- ✅ Portal BLE page functional

### Go/No-Go Criteria

- [ ] HR armband pairs via portal scan page
- [ ] Echelon app sees `Echenon-XXXX` as HR device and displays live heart rate
- [ ] HR relay latency < 500 ms (armband → ESP32 → Echelon app)
- [ ] Echelon app still pairs with the factory bike PCB for cadence/resistance
- [ ] Armband auto-reconnects after power cycle (≤ 20 s)
- [ ] BLE + WiFi coexist (portal accessible while BLE active)

### Deliverable

**A working HR relay** — put on the armband, the Echelon app shows your heart
rate via the ESP32 proxy. This works during any Echelon class with zero
changes to the bike. First tangible user-facing feature.

---

## Phase 3 — Passive Tap Board + FTMS Telemetry (PR-04a, PR-05)

> **Goal:** Make Echenon a permanent, daily-use install without touching
> the motor, and make the bike visible to fitness apps. Together with
> Phase 2, this delivers UC1 and UC3.

### Hardware — Passive Tap Board (PR-04a)

The first soldered board, built to the DR-1 design
([`hardware_specification.md`](hardware_specification.md) §6.2):

- J1–J4 pass-through, high-impedance taps, **no motor split** — the console
  drives the motor exactly as stock.
- Powered from the MP1584EN buck on the bike's 9V rail.
- **Manual OTA enabled and tested** before the board is sealed in the
  console housing. `autoCheck` is forced to `false` after config load and
  after factory reset, until framework #226 lands.

**Key test:** a stock ride with the board fitted is indistinguishable from
no board; all sensor taps read correctly; the board fits in the housing.

### Firmware — FTMS Telemetry (PR-05)

PR-05 needs no new hardware: the breadboard observer is enough to develop
it. It runs in parallel with PR-04a.

| Module | Purpose |
|---|---|
| `FTMSServer` | FTMS GATT service (`0x1826`), Indoor Bike Data notifications at 2 Hz, Feature flags, Fitness Machine Control Point (`0x2AD9`) |

`BLEManager`, `HRMClient`, and `HRMRepeater` are already built and proven
from Phase 2. Phase 3 adds the FTMS service to the existing BLE stack and
verifies that FTMS + HR coexist with multiple simultaneous client
connections.

**Control Point scope in PR-05:** expose `0x2AD9` and handle `0x00` Request
Control. Target opcodes are answered with `0x05` (Control Not Permitted) in
Intercept mode and `0x02` (Op Code Not Supported) otherwise. The target
opcodes and Fitness Machine Status are implemented in Phase 6 (PR-08).

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Passive board transparency | Does the console behave identically with the board fitted? |
| FTMS data format acceptance | Do Zwift, Xert, TrainerRoad all parse our Indoor Bike Data correctly? Any field misinterpretations? |
| Multi-service BLE | Does advertising FTMS (`0x1826`) + HR (`0x180D`) simultaneously work? Do apps discover both services? |
| Multi-client connections | Can the Echelon tablet connect for HR while a fitness app connects for FTMS? (3 simultaneous Peripheral connections) |

### Risks Retired

- ✅ Permanent install proven without modifying the motor path
- ✅ OTA works before the board is sealed
- ✅ FTMS data accepted by major fitness apps
- ✅ Multi-service + multi-client BLE validated (progressive coexistence check)
- ✅ Indoor Bike Data packet format correct (flags, field order, resolution)

### Go/No-Go Criteria

- [ ] PR-04a: stock ride indistinguishable from no board; taps read correctly; board fits
- [ ] PR-04a: manual OTA succeeds with the board installed
- [ ] Zwift sees `Echenon-XXXX` and displays live power/cadence
- [ ] Xert EBC connects and receives data
- [ ] All three BLE connections coexist: Echelon tablet (HR) + fitness app (FTMS) + ESP32 Central (armband)
- [ ] Control Point present: Request Control handled; target opcodes answered `0x05` (Intercept) or `0x02`

### Deliverable

**A permanent passive install and a visible smart trainer** — the bike
appears in any FTMS-compatible app with live power, cadence, resistance, and
heart rate data, with the console still in full control.

---

## Phase 4 — Session Recording + Strava Upload (PR-06)

> **Goal:** Record every ride and upload it to Strava automatically.
> Delivers UC2.

### Hardware

No hardware changes. Passive tap board.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `SessionManager` | Auto-start/stop recording based on cadence |
| `RideStore` | PSRAM ride buffer with LittleFS checkpoints |
| `TCXGenerator` | Streams TCX trackpoints and produces `tcx.gz` |
| `StravaUploader` | Implements `IUploader` (capabilities only): BYOA OAuth, token refresh, upload request |
| `SessionOverlay` (part) | Session REST API, authenticated TCX download, Session History tab (the `/overlay` page follows in PR-09) |
| `UploadQueue` | The only upload execution path (an `IDataSource`). Runs the split pipeline as separate `fetchUpdate()` calls, each within the 15 s deadline: generate `tcx.gz` → refresh token → POST /uploads → poll GET /uploads/{id}. A duplicate is treated as success. |

`ScanInhibit` (suppress BLE scans during upload) is compiled only under
`#ifdef WITH_BLE`, so PR-06 has no dependency on PR-03. No Echenon class
reuses a framework name (`ConfigManager`, `DataManager`, `Config.h`).

BYOA OAuth covers a validated `state` parameter, a documented Authorization
Callback Domain, persistence of rotated refresh tokens, and Redactor
re-registration on every refresh.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Strava upload reliability | Does the OAuth flow work? Does the TCX parse correctly? Do rides appear with all data fields? |
| Session integrity | Do sessions survive WiFi drops and resets? Are ghost rides avoided? |

### Risks Retired

- ✅ Strava integration end-to-end
- ✅ Session auto-management (start/stop/upload) reliable

### Go/No-Go Criteria

- [ ] Auto-session: pedal → "REC" → stop 3 min → upload → appears in Strava
- [ ] Re-uploading the same ride is treated as success (duplicate)
- [ ] Token refresh persists the rotated refresh token

### Deliverable

**Automatic ride history** — every ride appears in Strava without user
action.

---

## Phase 5 — Motor Interception (PR-07)

> **Goal:** Build the intercept board to the DR-1 design. Prove we can
> transparently relay the motor, then prove we can override it. This is the
> highest-risk phase and is optional for the passive product.

### Hardware (First Motor-Path Build)

Build the intercept board from the DR-1 revision of
[`hardware_specification.md`](hardware_specification.md) §7–§8. Until DR-1,
those sections are provisional and not to be built.

- Perfboard with ESP32 carrier, motor driver and rail (decision D3), buck
- JST connectors (J1–J4)
- **Split the motor wires** (the MITM)
- 30kΩ/10kΩ ÷4 motor-sense dividers (R1a/R1b, R2a/R2b)
- Switchable standalone supplies (R3, R4, R5, Green–GND link), open by default
- C1/C2 filtering
- Fail-safe per decision H-05

**Build in two sub-phases:**

#### Phase 5a — Intercept (Transparent Relay)

Wire the intercept board with the motor split. Firmware reads console motor
sense on **GPIO 6/7** (ADC1, sense only) and relays to the motor driver on
**GPIO 10/17**. The user should not notice any difference from stock.

**Key test:** Ride a full class with the intercept board. Does the motor
respond identically? Any lag? Any missed commands?

#### Phase 5b — Override (Direct Control)

Switch to Override Mode. The ESP32 reads the encoder and drives the motor
directly using the cam-aware position controller with pot feedback.

**Key test:** Turn the knob → motor moves → pot settles at target → repeat
across the full working range. Whether 100 levels are achievable is a goal
to validate, not an assumption.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `InterceptController` | Mode switching, motor relay (Intercept) and direct control (Override) |
| `ResistanceMotor` | Motor driver control with the cam-aware position controller |
| `EncoderModule` (framework) | PCNT quadrature decoding of the red knob |
| `CalibrationWizard` (extended) | Motor-driven calibration: full-revolution sweep that finds the cam extremes |

Operating mode, calibration data and controller parameters are persisted
through the framework's `ConfigManager` (`IConfigurable`).

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Motor relay fidelity | Does the relay feel identical to stock? |
| Motor control precision | Can we hit specific pot targets consistently? What's the dead zone? |
| Level resolution | Is the pot ADC resolution sufficient? Is mechanical play a factor? |
| Cam behaviour | Does angle tracking stay correct over many revolutions? Does homing work after a cold start? |
| Heat / power | Does the driver run cool under sustained motor drive? Any brown-outs? |
| Fail-safe | Does the chosen fail-safe keep the bike usable if the ESP32 resets? |

### Risks Retired

- ✅ Motor interception proven (the MITM concept works)
- ✅ Position controller tuned with real motor and pot
- ✅ Intercept board physically fits in the console housing
- ✅ No damage to bike or console during installation

### Go/No-Go Criteria

- [ ] Phase 5a: Full Echelon class with intercept board — no user-perceptible difference
- [ ] Phase 5b: Encoder-to-motor closed loop works across the working range
- [ ] Achievable level count measured (100 is the goal)
- [ ] Motor dead zone tuned — no oscillation, reliable settling
- [ ] Fail-safe verified
- [ ] Board fits in console housing

### Deliverable

**A working intercept board** — the bike works identically in Intercept
Mode, and the user can switch to Override Mode for direct control.

---

## Phase 6 — ERG Mode + Control Point (PR-08)

> **Goal:** Close the loop. External apps control resistance. Delivers
> UC4 and UC5.

### Hardware

No hardware changes. Intercept board.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `ERGController` | Target watts → power model → target cam angle. Smooths cadence; applies a setpoint deadband and rate limit. |
| `FTMSServer` (extended) | Control Point target opcodes (Set Target Power / Resistance) and Fitness Machine Status (`0x2ADA`) |

**ERG state machine:** ACTIVE, SUSPENDED_BY_RIDER, RELEASED. A knob turn
moves ERG to SUSPENDED_BY_RIDER; the state latches until the client sends a
new Request Control or Reset, or until `[DR-1/PR-08: hold time]` expires.
Target writes that arrive while suspended are stored, not applied.
Fitness Machine Status sends `0x08` (Target Power Changed) and `0x07`
(Target Resistance Level Changed) on target changes, and `0xFF` (Control
Permission Lost) on release (SPEC §5.6; opcodes confirmed against FTMS v1.0
in PR-08).

**Web command:** `POST /api/resistance`, authenticated, Override mode only.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| ERG control loop stability | Does the motor oscillate when chasing a power target? How quickly does it converge? |
| Power model accuracy | At 200W target + 80 RPM, does the computed cam angle actually produce ~200W? |
| Cadence-resistance interaction | When cadence changes mid-ERG, does the resistance adjustment keep up? Any overshoot? |
| Manual override during ERG | Does a knob turn cleanly suspend ERG? Does a new Request Control resume it? |

### Risks Retired

- ✅ ERG closed-loop control works on real hardware
- ✅ Power model validated against actual riding feel

### Go/No-Go Criteria

- [ ] Set 200W target → power stabilises within ±10W across cadence range
- [ ] Knob turn suspends ERG immediately; status notifications sent
- [ ] Complete a structured workout in Xert EBC with automatic resistance changes

### Deliverable

**A full smart trainer** — ERG mode works and the bike responds to FTMS
commands from any app.

---

## Phase 7 — Integration & Use Case Validation (PR-09)

> **Goal:** Walk through every use case from
> [`use_case_analysis.md`](use_case_analysis.md) and verify it works
> end-to-end. Fix the gaps.

### Acceptance Matrix

| UC | Description | Earliest Phase | Validated? |
|---|---|---|---|
| **UC1** | Pure pass-through + HR relay | Phase 2 + 3 (passive tap board) | [ ] |
| **UC2** | As UC1 + Strava recording | Phase 2 + 3 + 4 | [ ] |
| **UC3** | As UC1 + FTMS fitness app recording | Phase 2 + 3 | [ ] |
| **UC4** | Override Mode + ERG, console idle | Phase 5b + 6 | [ ] |
| **UC5** | Echelon class + external HR-zone app | Phase 5b + 6 | [ ] |
| **UC6** | Console disconnected, pure FTMS | Phase 5b + 6 | [ ] |

### Additional Work

- **Final BLE / WiFi coexistence test** (progressive checks already done in
  Phases 2 and 3)
- **Portal polish** — calibration wizard, settings, session history
- **`SessionOverlay`** — standalone `/overlay` floating window with live data for the tablet (WebSocket)
- **Edge cases** — power cycling, WiFi reconnection, BLE reconnection,
  multiple riders, factory reset
- **Standalone sensor power verification** — disconnect console, close the
  standalone-supply switches, verify all sensors stay alive (UC6)
- **Pot recalibration** — verify that switching between console-connected
  and standalone Override prompts recalibration

OTA is already in place from Phase 3 (PR-04a).

### Deliverable

**A production-quality system** — all 6 use cases validated, edge cases
handled, portal polished.

---

## Phase Summary

| Phase | PR | Hardware | Key Question Answered | New Firmware Modules |
|---|---|---|---|---|
| **0** | PR-01 | Breadboard observer (USB) | "What do the signals actually look like?" | SignalLogger (ride log, burst capture, CSV export) |
| **DR-1** | — | — | "What exactly do we build?" | — |
| **1** | PR-02 | Same | "Do our readings match Echelon's?" | CadenceSensor, ResistanceSensor, PowerEngine, CalibrationWizard |
| **2** | PR-03 | HR-only (USB) | "Can we relay HR to the Echelon app?" | HRMClient, HRMRepeater (+ framework BLEManager) |
| **3** | PR-04a, PR-05 | Passive tap board | "Can we install permanently and can fitness apps see our data?" | FTMSServer, OTA enablement |
| **4** | PR-06 | Same | "Do rides upload?" | SessionManager, RideStore, TCXGenerator, IUploader / StravaUploader, UploadQueue |
| **5a** | PR-07 | Intercept board (build) | "Can we relay the motor transparently?" | InterceptController, ResistanceMotor |
| **5b** | PR-07 | Same | "Can we control resistance ourselves?" | EncoderModule integration, CalibrationWizard motor-driven sweep |
| **6** | PR-08 | Same | "Can apps control us?" | ERGController, FTMSServer Control Point targets |
| **7** | PR-09 | Same | "Do all 6 use cases work end-to-end?" | SessionOverlay (`/overlay`), portal polish, edge cases |

### Risk Profile

Phase 5 is the riskiest — it's where we physically modify the motor path and
drive a motor for the first time. Everything before it is **read-only,
pass-through or BLE-only** (no change to how the console controls the bike),
and DR-1 ensures the Phase 5 design rests on measurement. Everything after
Phase 5 is **software-only** (no new hardware risk).

```
Risk ▲
     │                    ██
     │                    ████
     │                    ██████
     │                    ████████
     │           ██       ██████████
     │ ██████████████████████████████
     └──────────────────────────────────►
       P0  P1  P2  P3  P4  P5  P6  P7
                   │       │
       read-only / │   hardware risk
       BLE-only    │   peaks here
             passive board
             (pass-through)
```
