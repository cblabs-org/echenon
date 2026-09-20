# Echenon — Iterative Development Roadmap

## Philosophy

Each phase is **useful on its own** and retires specific unknowns before the
next phase adds complexity. Hardware and firmware develop together — we don't
build the full intercept board and then write firmware; we build what we need
for the current phase and extend it.

```
Phase 0    Phase 1    Phase 2    Phase 3      Phase 4     Phase 5      Phase 6
Observe  → Calibrate → HR Relay → Intercept  → FTMS      → ERG+Strava → Polish
  │           │           │          │            │           │            │
  ▼           ▼           ▼          ▼            ▼           ▼            ▼
  Read      Compare     BLE HR   Control the   Broadcast   Close the    UC1-UC6
  signals   to Echelon  proxy    motor (MITM)  to apps     loop         end-to-end
```

---

## Phase 0 — Passive Observation

> **Goal:** Read every signal on the bike without changing anything.
> Learn the actual electrical behaviour before we try to control it.

### Phase 0 Pre-Gate — Motor Voltage Verification

> ⚠️ **This measurement MUST be completed before connecting any motor
> wires to the ESP32.** Everything else (cadence, pot, encoder) is safe
> to connect immediately.

A digital multimeter measured **3.3V** on the motor wires, which rules out
9V bang-bang. However, a multimeter shows the *average* voltage — it cannot
distinguish 3.3V DC from 9V PWM at ~37% duty cycle (both average ~3.3V).
The capacitor across the motor terminals is consistent with either scenario
(brush noise suppression or PWM smoothing).

**Oscilloscope measurement** (30 seconds, one-time):

1. Power the bike normally.
2. Set scope: **DC coupling**, **5V/div** vertical, **10 ms/div** horizontal.
3. Connect scope **ground clip** to any GND wire (Brown on 9-pin loom).
4. Touch probe to **Red wire** (motor+, pin 1).
5. Turn the resistance knob one click up. Observe:

```
If 3.3V bang-bang (expected):   If 9V PWM (must redesign):
    ┌────┐                          ┌┐ ┌┐ ┌┐ ┌┐
3.3V│    │                      9V  ││ ││ ││ ││
    │    │                          ││ ││ ││ ││
  0V┘    └───                   0V  ┘└─┘└─┘└─┘└──
    DMM reads 3.3V ✓                DMM reads ~3.3V ✓
    Scope confirms flat              Scope reveals pulses
```

6. If 3.3V flat DC → **proceed to full Phase 0 wiring**.
7. If 9V PWM → **stop**. Redesign motor sense circuit with voltage
   divider (30kΩ + 10kΩ: 9V → 2.25V) and update L9110S to run from 9V
   rail. Update SPEC.md §4.5 and hardware_specification.md before
   proceeding.

### Hardware — Full Passive Tap

No intercept board yet. Fly-wire the ESP32 to the *existing* loom using
alligator clips or solder taps — **no wires cut, no connectors split.**
Tap **all** wires for maximum observability.

```
Existing 9-pin loom (undisturbed) — tap every wire
    │
    ├── Pin 1  Red    (motor+)     ── 30kΩ ──┬── GPIO 10     ← motor sense
    │                                        10kΩ             (voltage divider:
    │                                         │                ÷4 for both
    │                                        GND               polarities)
    │
    ├── Pin 2  Black  (motor−)     ── 30kΩ ──┬── GPIO 11     ← motor sense
    │                                        10kΩ             (same divider)
    │                                         │
    │                                        GND
    │
    ├── Pin 3  Yellow (pot outer)  ──────────── GPIO 2 (ADC)   ← reference V
    ├── Pin 4  Blue   (pot wiper)  ──────────── GPIO 1 (ADC)   ← position
    ├── Pin 5  Green  (pot outer)  ──────────── GPIO 3 (ADC)   ← reference V
    ├── Pin 6  Orange (cadence)    ──────────── GPIO 5          ← reed switch
    ├── Pin 7  Grey   (cadence)    ──────────── GPIO 4          ← return line
    ├── Pin 8  White  (9V)         ──► MP1584EN IN+ ──► ESP32 3V3 pin
    └── Pin 9  Brown  (GND)        ──► MP1584EN IN− ──► ESP32 GND
                                        (common ground)

Existing 3-pin loom (undisturbed) — tap all 3 wires
    ├── Pin 1  Red    (Enc VCC)    ──────────── GPIO 12 (ADC)   ← supply voltage
    ├── Pin 2  Brown  (Enc Ch A)   ──────────── GPIO 8          ← quadrature
    └── Pin 3  Orange (Enc Ch B)   ──────────── GPIO 9          ← quadrature
```

**Protection on motor sense pins — voltage divider (not zener):**

The motor is driven bidirectionally by the console's H-bridge — Red goes HIGH
while Black goes LOW (forward), then the reverse for backward. Back-EMF when
the motor stops can also push wires below ground. A simple zener-to-GND would
only clamp positive overvoltage and let negative transients through.

The 30kΩ + 10kΩ voltage divider handles all scenarios:

| Motor wire voltage | GPIO sees | Safe? |
|---|---|---|
| 3.3V (expected HIGH) | 0.825V | ✅ Above logic threshold (~0.4V) |
| 0V (expected LOW) | 0V | ✅ Clean LOW |
| 9V (if PWM — unlikely) | 2.25V | ✅ Below 3.3V max |
| −5V (back-EMF spike) | −1.25V | ✅ Current via 30kΩ = 0.17 mA — safe for ESP32 internal ESD diodes |

The 30kΩ series resistance limits current to <0.3 mA under any plausible
condition. Cost: 4 resistors, ~£0.10.

**All other signals** are safe to connect directly — cadence, pot, and
encoder are all 0–3.3V logic or analog signals from passive components.

The console stays fully connected and controls everything as stock. We are
invisible — high-impedance taps only.

### Firmware

| Module | Purpose |
|---|---|
| `SignalLogger` | Sample all inputs at high frequency: cadence ISR, ADC at 100 Hz (pot wiper + both pot outers), motor sense polling at 1 kHz, encoder ISR. Log to PSRAM ring buffer with microsecond timestamps. |
| `WebServer` | Live dashboard showing raw signal values in real time |
| `WiFi` | Connect to home WiFi for portal access |
| `DataExport` | Download captured sessions as CSV via the portal |

**No BLE, no FTMS, no session manager, no power calculation yet.** Pure
observability.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| **Motor voltage (CRITICAL)** | Oscilloscope confirms 3.3V DC or reveals 9V PWM. This gates the entire MITM design. |
| Motor drive pattern | When does the console drive the motor? For how long? Is it truly bang-bang or PWM? What's the duty cycle? |
| Motor timing | How long does a full min→max sweep take? How quickly does the console respond to an encoder click? |
| Cadence signal shape | Pulse width, rise/fall times, bounce characteristics. Tune debounce parameters. |
| Pot wiper voltage range | Actual min/max mV across the full resistance range. Compare to teardown measurement (1160–1700 mV). |
| Pot reference voltages | What voltage does the console apply to Yellow and Green? Is Green at GND? Is Yellow at 3.3V? This determines our standalone reference design (R4). |
| Pot linearity | Is the voltage-to-position relationship linear? Or does it need a polynomial fit? |
| Encoder pulses per detent | Count exact pulses per click. Verify quadrature direction logic. |
| Encoder VCC and protocol | What voltage does the console supply on Enc VCC (Red)? Confirm the output is standard quadrature (A leads B = CW, B leads A = CCW). Verify compatibility with the framework's `RotaryEncoder` module. |
| Cadence return line | What does Grey do? Is it GND? VCC? Floating? This clarifies the reed switch circuit. |
| Signal correlations | Encoder fires → motor drives → pot changes. Log all three with timestamps to see the cascade and measure end-to-end latency. |

### Risks Retired

- ✅ **Motor voltage confirmed** — MITM design validated or redesigned
- ✅ All signal characteristics measured, not assumed from teardown
- ✅ Pot reference circuit understood (Yellow/Green voltages)
- ✅ Debounce parameters tuned from real data
- ✅ ADC noise floor and multisampling strategy validated
- ✅ Web portal and WiFi stack working
- ✅ Basic firmware architecture proven

### Go/No-Go Criteria

- [ ] **GATE: Motor voltage verified by oscilloscope** — confirm 3.3V DC (or redesign if 9V PWM)
- [ ] All 9+2 signals read cleanly and match expected behaviour
- [ ] CSV export works — at least one full ride captured
- [ ] Cadence matches manual pedal count (±1 RPM)
- [ ] Pot voltage range measured and documented (wiper + both outers)
- [ ] Motor drive behaviour documented (voltage, timing, polarity, duration)
- [ ] Encoder pulses per detent counted and direction verified
- [ ] Signal cascade timing documented (encoder → motor → pot)

### Deliverable

**A raw data dashboard** — ride the bike normally, see all signals live in the
browser, download a CSV of the entire ride. Plus a confirmed motor voltage
that validates (or redirects) the MITM design.

---

## Phase 1 — Calibration & Power Validation

> **Goal:** Convert raw signals to meaningful metrics and validate against
> the Echelon app's own readings. Answer: "How accurate is our data?"

### Hardware

Same as Phase 0. No changes. Still passive taps.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `CadenceSensor` | Production cadence calculation (RPM from ISR intervals, timeout, filtering) |
| `ResistanceSensor` | ADC → resistance level mapping. Run calibration wizard: user turns knob to min/max, firmware records mV range, divides into 32 levels (matching Echelon), stores in NVS |
| `PowerEngine` | First power curve implementation. Start with physics model or community table. |
| `CalibrationWizard` | Portal page: guided min/max pot calibration |

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Our cadence vs Echelon's cadence | Side-by-side comparison. Ride a class, note Echelon app's cadence display, compare to our portal. They should match exactly (same reed switch). |
| Our resistance level vs Echelon's | Map our ADC readings to levels 1–32. When Echelon app says "19", do we say "19"? If not, our calibration curve is off. |
| Power curve accuracy | Calculate watts from cadence × resistance using our model. Compare to what Echelon displays for power. How far off are we? |

### Independent Power Verification

This is worth discussing. The Echelon app shows power, but how accurate is
*their* number? Options for independent assessment:

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
- ✅ Resistance mapping calibrated and validated
- ✅ Power estimation baseline established
- ✅ Calibration persistence (NVS) working
- ✅ Understanding of how Echelon's power compares to ours

### Go/No-Go Criteria

- [ ] Cadence matches Echelon app within ±1 RPM
- [ ] Resistance levels match Echelon app's display (1–32 scale)
- [ ] Power estimate within ±15% of Echelon's displayed power
- [ ] Calibration survives power cycle (NVS persistence)

### Deliverable

**A calibrated dashboard** — ride the bike, see accurate cadence, resistance,
and estimated power in the browser. Side-by-side with Echelon app, the
numbers should match.

---

## Phase 2 — HR Armband Relay

> **Goal:** Connect to the user's HR armband over BLE, re-broadcast the
> heart rate as a standard BLE HR service. The Echelon app pairs with the
> ESP32 for HR instead of the armband directly. **Immediate user value with
> zero bike risk.**

### Hardware

Same as Phase 0/1. No changes. The HR relay is entirely BLE — it doesn't
interact with any bike signals. Can be developed in parallel with Phase 0/1
if desired.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `BLEManager` | NimBLE stack initialisation, advertising, connection management. Foundation for all future BLE work. |
| `HRMClient` | BLE Central role: scan for and connect to configured HR armband (by name or MAC address). Subscribe to HR Measurement notifications (`0x2A37`). |
| `HRMRepeater` | BLE Peripheral role: re-broadcast HR as a standard Heart Rate Service (`0x180D`). The Echelon app sees `"Echenon"` as an HR device and pairs with it. |
| Portal: BLE page | Scan for nearby HR devices, select and pair, store MAC in NVS. |

### Why This Phase Matters

The HR relay is the **simplest useful BLE feature** and proves several things
we'll need for every subsequent phase:

1. **NimBLE dual-role** — Central (connecting to armband) and Peripheral
   (advertising to Echelon app) running simultaneously on the same radio.
2. **BLE coexistence** — Does the ESP32's advertising interfere with the
   Echelon console's own BLE? (The console has its own BLE for the Echelon
   app's bike metrics pairing.)
3. **Connection management** — Auto-reconnect when the armband drops, handle
   multiple simultaneous connections, manage advertising.
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
- ✅ BLE coexistence with factory Echelon PCB confirmed
- ✅ Portal BLE page functional

### Go/No-Go Criteria

- [ ] HR armband pairs via portal scan page
- [ ] Echelon app sees `"Echenon"` as HR device and displays live heart rate
- [ ] HR relay latency < 500 ms (armband → ESP32 → Echelon app)
- [ ] Echelon app still pairs with the factory bike PCB for cadence/resistance
- [ ] Armband auto-reconnects after power cycle
- [ ] BLE + WiFi coexist (portal accessible while BLE active)

### Deliverable

**A working HR relay** — put on the armband, the Echelon app shows your heart
rate via the ESP32 proxy. This works during any Echelon class with zero
changes to the bike. First tangible user-facing feature.

---

## Phase 3 — Motor Interception

> **Goal:** Build the intercept board. Prove we can transparently relay the
> motor, then prove we can override it. This is the highest-risk phase.

### Hardware (First Real Build)

This is where we build the intercept board from
[`docs/hardware_specification.md`](docs/hardware_specification.md):

- Solder the perfboard with ESP32 carrier, L9110S, MP1584EN
- Wire the JST connectors (J1–J4)
- **Split the motor wires** (the MITM)
- Add R1/R2 pull-downs, R3/R4/R5 standalone power, C1/C2 filtering

**Build in two sub-phases:**

#### Phase 2a — Intercept (Transparent Relay)

Wire the intercept board with the motor split. Firmware reads console
motor sense (GPIO 10/11) and relays to L9110S (GPIO 6/7). The user
should not notice any difference from stock.

**Key test:** Ride a full class with the intercept board. Does the motor
respond identically? Any lag? Any missed commands?

#### Phase 2b — Override (Direct Control)

Switch to Override Mode. ESP32 reads the encoder and drives the motor
directly using the bang-bang position controller with pot feedback.

**Key test:** Turn the knob → motor moves → pot settles at target → repeat
across the full range. Are 100 levels achievable? Is the control faster
than the stock console?

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `InterceptController` | Mode switching, motor relay (Intercept) and direct control (Override) |
| `ResistanceMotor` | L9110S driver with bang-bang position controller |
| `ResistanceEncoder` | Quadrature decoder for the red knob |
| `ConfigManager` | NVS: operating mode, calibration data, motor dead zone |

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| Motor relay fidelity | Does the 1 ms relay loop feel identical to stock? |
| Motor control precision | Can we hit specific pot mV targets consistently? What's the dead zone? |
| 100-level resolution | Is the pot ADC resolution sufficient? Is mechanical play a factor? |
| Mechanical limits | What happens when the motor hits the end stops? Does it stall? How do we detect limits? |
| Heat / power | Does the L9110S run cool under sustained motor drive? |

### Risks Retired

- ✅ Motor interception proven (the core MITM concept works)
- ✅ Bang-bang controller tuned with real motor and pot
- ✅ Intercept board physically fits in the console housing
- ✅ JST connectors mate correctly with factory loom
- ✅ No damage to bike or console during installation

### Go/No-Go Criteria

- [ ] Phase 2a: Full Echelon class with intercept board — no user-perceptible difference
- [ ] Phase 2b: Encoder-to-motor closed loop works across full range
- [ ] 100 discrete resistance levels achievable (or determine actual max)
- [ ] Motor dead zone tuned — no oscillation, reliable settling
- [ ] Board fits in console housing

### Deliverable

**A working intercept board** — the bike works identically in Intercept Mode,
and the user can switch to Override Mode for direct 100-level control.

---

## Phase 4 — FTMS Broadcast

> **Goal:** Add FTMS service to the existing BLE stack. External fitness
> apps can see the bike and receive live telemetry. Combined with the HR
> relay from Phase 2, the bike is now a full BLE data source.

### Hardware

No hardware changes. Same intercept board from Phase 3.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `FTMSServer` | FTMS GATT service (`0x1826`), Indoor Bike Data notifications at 2 Hz, Feature flags, Fitness Machine Status |

`BLEManager`, `HRMClient`, and `HRMRepeater` are already built and proven
from Phase 2. Phase 4 adds the FTMS service to the existing BLE stack and
verifies that FTMS + HR coexist with multiple simultaneous client connections.

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| FTMS data format acceptance | Do Zwift, Xert, TrainerRoad all parse our Indoor Bike Data correctly? Any field misinterpretations? |
| Multi-service BLE | Does advertising FTMS (`0x1826`) + HR (`0x180D`) simultaneously work? Do apps discover both services? |
| Multi-client connections | Can the Echelon tablet connect for HR while a fitness app connects for FTMS? (3 simultaneous Peripheral connections) |

### Risks Retired

- ✅ FTMS data accepted by major fitness apps
- ✅ Multi-service + multi-client BLE validated
- ✅ Indoor Bike Data packet format correct (flags, field order, resolution)

### Go/No-Go Criteria

- [ ] Zwift sees `"Echenon"` and displays live power/cadence
- [ ] Xert EBC connects and receives data
- [ ] All three BLE connections coexist: Echelon tablet (HR) + fitness app (FTMS) + ESP32 Central (armband)
- [ ] Control Point characteristic present (writes rejected with `0x05` in Intercept Mode — no motor control yet)

### Deliverable

**A visible smart trainer** — the bike appears in any FTMS-compatible app with
live power, cadence, resistance, and heart rate data.

---

## Phase 5 — ERG Mode + Cloud Upload

> **Goal:** Close the loop. External apps can control resistance. Rides
> upload to Strava automatically.

### Hardware

No hardware changes. Same intercept board.

### Firmware (New Modules)

| Module | Purpose |
|---|---|
| `ERGController` | FTMS Control Point handler: target watts → power table lookup → target resistance level |
| `FTMSControlPoint` | GATT Control Point characteristic with mode-aware response codes |
| `SessionManager` | Auto-start/stop recording based on cadence |
| `StravaUploader` | TCX generation, OAuth flow, HTTPS upload |
| `DataManager` | Upload queue, retry logic, ScanInhibit during upload |

### What We Learn

| Unknown | How We Resolve It |
|---|---|
| ERG control loop stability | Does the motor oscillate when chasing a power target? How quickly does it converge? |
| Power table accuracy | At 200W target + 80 RPM, does the computed resistance actually produce ~200W? Or do we need a feedback correction? |
| Cadence-resistance interaction | When cadence changes mid-ERG, does the resistance adjustment keep up? Any overshoot? |
| Strava upload reliability | Does the OAuth flow work? Does the TCX parse correctly? Do rides appear with all data fields? |
| Manual override during ERG | Does the encoder click cleanly cancel ERG? Can the user resume ERG? |

### Risks Retired

- ✅ ERG closed-loop control works on real hardware
- ✅ Power table validated against actual riding feel
- ✅ Strava integration end-to-end
- ✅ Session auto-management (start/stop/upload) reliable

### Go/No-Go Criteria

- [ ] Set 200W target → power stabilises within ±10W across cadence range
- [ ] Encoder click cancels ERG immediately
- [ ] Auto-session: pedal → "REC" → stop 3 min → upload → appears in Strava
- [ ] Complete a structured workout in Xert EBC with automatic resistance changes

### Deliverable

**A full smart trainer** — ERG mode works, rides upload to Strava, the bike
responds to FTMS commands from any app.

---

## Phase 6 — Use Case Validation & Polish

> **Goal:** Walk through every use case from
> [`docs/use_case_analysis.md`](docs/use_case_analysis.md) and verify it
> works end-to-end. Fix the gaps.

### Acceptance Matrix

| UC | Description | Earliest Phase | Validated? |
|---|---|---|---|
| **UC1** | Pure pass-through + HR relay | Phase 3a + 2 + 4 | [ ] |
| **UC2** | As UC1 + Strava recording | Phase 3a + 2 + 4 + 5 | [ ] |
| **UC3** | As UC1 + FTMS fitness app recording | Phase 3a + 2 + 4 | [ ] |
| **UC4** | Override Mode + ERG, console idle | Phase 3b + 4 + 5 | [ ] |
| **UC5** | Echelon class + external HR-zone app | Phase 3b + 4 + 5 | [ ] |
| **UC6** | Console disconnected, pure FTMS | Phase 3b + 4 + 5 | [ ] |

### Additional Work

- **OTA firmware updates** — so we can iterate without opening the console
- **Portal polish** — calibration wizard, settings, session history
- **Overlay** — floating window with live data for the tablet
- **Edge cases** — power cycling, WiFi reconnection, BLE reconnection,
  multiple riders, factory reset
- **Standalone sensor power verification** — disconnect console, verify R3/R4/R5
  keep all sensors alive (UC6)
- **Pot recalibration** — verify that switching between Intercept and standalone
  Override modes prompts recalibration

### Deliverable

**A production-quality system** — all 6 use cases validated, edge cases
handled, OTA working, portal polished.

---

## Phase Summary

| Phase | Hardware | Key Question Answered | New Firmware Modules |
|---|---|---|---|
| **0** | Fly wires + ESP32 | "What do the signals actually look like?" | SignalLogger, WebServer, DataExport |
| **1** | Same | "Do our readings match Echelon's?" | CadenceSensor, ResistanceSensor, PowerEngine |
| **2** | Same (BLE only) | "Can we relay HR to the Echelon app?" | BLEManager, HRMClient, HRMRepeater |
| **3a** | Intercept board (build) | "Can we relay the motor transparently?" | InterceptController, ResistanceMotor |
| **3b** | Same | "Can we control resistance ourselves?" | ResistanceEncoder, ConfigManager |
| **4** | Same | "Can fitness apps see our data?" | FTMSServer |
| **5** | Same | "Can apps control us? Do rides upload?" | ERGController, SessionManager, StravaUploader |
| **6** | Same | "Do all 6 use cases work end-to-end?" | OTA, Portal polish, edge cases |

### Risk Profile

Phase 3 is the riskiest — it's where we physically modify signal paths and
drive a motor for the first time. Everything before Phase 3 is **read-only
or BLE-only** (zero risk to the bike). Everything after Phase 3 is
**software-only** (zero new hardware risk).

```
Risk ▲
     │    ██
     │    ████
     │    ██████
     │    ████████
     │    ██████████
     │ ████████████████
     │ ██████████████████
     └───────────────────────────►
       P0  P1  P2  P3  P4  P5  P6
               │   │
           BLE only │
             (safe) hardware risk
                    peaks here
```

