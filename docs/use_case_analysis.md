# Use Case Analysis — MITM Intercept Board Design Validation

## Use Case Summary

| UC | Mode | Console | Echelon App | FTMS Client | Resistance Control | Key Test |
|---|---|---|---|---|---|---|
| **1** | Intercept | Connected | Active (class) | — | Manual knob | Pure pass-through + HR relay |
| **2** | Intercept | Connected | Active (class) | — | Manual knob | As UC1 + Strava recording |
| **3** | Intercept | Connected | Active (class) | Fitness app | Manual knob | As UC1 + FTMS telemetry to external app |
| **4** | Override | Connected (idle) | Not used | Fitness app | FTMS Control Point (ERG) | External app drives resistance, console ignored |
| **5** | Override | Connected (idle) | Active (class) | External app | FTMS Control Point (HR-zone) | Echelon class running, resistance via external app |
| **6** | Override | **Disconnected** | N/A | Fitness app | FTMS / Encoder | Console broken or removed, pure FTMS trainer |

---

## UC1 — Pure Pass-Through (Echelon Class + HR Relay)

> *Intercept Mode. Echelon app running a class. HR armband paired with ESP32. Resistance 19/32, cadence 80 RPM.*

### Signal Trace

```
Rider pedals at 80 RPM
    → Reed switch fires 80×/min → GPIO 5 (ISR, cadence tap) → pass-through to console
    → Console counts cadence independently (for Echelon app UI)

Rider sets resistance to 19/32 via red knob
    → Encoder sends quadrature clicks → pass-through to console
    → Console decodes clicks, sends motor HIGH on Red/Black wires (console side)
    → ESP32 reads GPIO 10/11 (console motor sense) → mirrors to GPIO 6/7 (L9110S)
    → L9110S drives motor → motor moves brake carriage
    → Pot wiper voltage changes → GPIO 1 ADC reads new position
    → Console also reads pot (pass-through) → displays "19" in Echelon app

HR armband → ESP32 BLE Central (HRMClient)
    → ESP32 re-broadcasts HR via HRMRepeater (0x180D)
    → Echelon tablet connects to ESP32 for HR (§8.3)
    → Echelon app shows HR during class
```

### Verdict: ✅ WORKS

All signals pass through transparently. The ESP32 reads them for telemetry but doesn't interfere. The motor relay (console → GPIO 10/11 → L9110S) introduces <1ms latency — imperceptible. The Echelon app works exactly as stock.

### Test Case

| Step | Action | Expected Result |
|---|---|---|
| 1 | Set Intercept Mode. Connect console. Pair HR armband. | Portal shows HR, mode = Intercept |
| 2 | Open Echelon app. Start a class. | Class loads normally on tablet |
| 3 | Set resistance to 19 via red knob. | Echelon app shows 19/32. Motor moves. Pot ADC reads corresponding mV. |
| 4 | Pedal at ~80 RPM. | Echelon app shows cadence. Overlay shows cadence ~80. |
| 5 | Verify HR on Echelon app. | Echelon app shows HR from ESP32 re-broadcast (not direct armband). |
| 6 | Change resistance during class. | Motor responds normally. Overlay tracks resistance changes. |

---

## UC2 — Pass-Through + Strava Recording

> *Same as UC1, but the ride is recorded and uploaded to Strava after the class ends.*

### Signal Trace

Same as UC1, plus:

```
Cadence first detected → SessionManager auto-starts recording
    → RideDataPoint appended at 2 Hz to PSRAM ring buffer
    → Each point: {timestamp, cadence, resistance, power, hr}

Rider stops pedalling → 3 min timeout → SessionManager ends session
    → TCX generated (1 Hz trackpoints streamed to LittleFS)
    → StravaUploader picks up pending session via DataManager
    → ScanInhibit token acquired (suppresses BLE scans during upload)
    → HTTPS multipart POST → Strava API
    → TCX file cleanup
```

### Verdict: ✅ WORKS

Session recording is fully independent of operating mode and FTMS. The power engine calculates watts from cadence + resistance (read via the ADC tap). Upload runs post-ride via the DataManager pipeline.

### Test Case

| Step | Action | Expected Result |
|---|---|---|
| 1–6 | Same as UC1. | Same as UC1. |
| 7 | Verify overlay shows "● REC" during ride. | Auto-start triggered by first cadence pulse. |
| 8 | Stop pedalling. Wait 3 minutes. | Session ends. Overlay shows post-ride summary. |
| 9 | Check Strava. | Activity appears with power, cadence, HR, duration. |
| 10 | Check Xert (if Strava-synced). | Activity synced, MPA model updated. |

---

## UC3 — Pass-Through + FTMS Fitness App Recording

> *Same as UC1, but an external fitness app (Xert EBC, Zwift, TrainerRoad) connects via FTMS to record and chart live data.*

### Signal Trace

Same as UC1, plus:

```
Fitness app connects to ESP32 as BLE Central (FTMS 0x1826)
    → BLEManager accepts connection (up to 3 Peripheral connections)
    → FTMSServer notifies at 2 Hz on Indoor Bike Data (0x2AD2)
    → Packet contains: speed, cadence, resistance, power, HR

Tablet BLE connections (§8.3):
    Connection 1: Echelon tablet → Factory Echelon PCB (bike metrics for Echelon app)
    Connection 2: Echelon tablet → ESP32 HRMRepeater (HR re-broadcast)
    Connection 3: Fitness app → ESP32 FTMSServer (FTMS data)
```

### Verdict: ✅ WORKS

Multi-client BLE (§7.8) supports up to 3 simultaneous Peripheral connections. The fitness app receives FTMS data independently of the Echelon app. All three BLE connections coexist.

> [!NOTE]
> If the fitness app sends FTMS Control Point writes (Set Target Power / Set Target Resistance), they are **accepted but ignored** in Intercept Mode — the console controls resistance, not the ESP32. The spec handles this with result code `0x05` (Control Not Permitted). The fitness app should treat Echenon as a power source only in this mode.

### Test Case

| Step | Action | Expected Result |
|---|---|---|
| 1–6 | Same as UC1. | Same as UC1. |
| 7 | Open fitness app (e.g. Xert EBC). Pair "Echenon" as FTMS device. | App connects. Shows live power, cadence, HR. |
| 8 | Ride at 80 RPM, resistance 19. | App charts real-time data matching overlay values. |
| 9 | App sends FTMS Control Point command (if ERG-capable). | Response: `0x05` (Control Not Permitted). Resistance unchanged. |
| 10 | Stop ride. | App records ride. Strava also records (if configured). |

---

## UC4 — External App with Dynamic Resistance (Console Idle)

> *Override Mode. FTMS fitness app drives resistance via Control Point. Console board is powered and mounted but not being used.*

### Signal Trace

```
Console board is connected and powered (9V → buck → 3.3V)
    → Console's BLE is advertising, but nobody connects to it
    → Console provides reference voltages to pot and encoder (via pass-through)
    → Console may attempt to drive motor when user turns knob
       → BUT: Console's motor output goes to GPIO 10/11 (sense inputs) only
       → In Override Mode: GPIO 10/11 are READ but IGNORED
       → Motor is driven ONLY by L9110S under ESP32 control

Fitness app connects via FTMS
    → Sends Set Target Power (0x05): 200W
    → ERGController: 200W at current 80 RPM → power table lookup → level 47
    → Level 47 → target_pot_mV = 1365 mV (calibration table)
    → ResistanceMotor::update(current_mV, 1365)
    → L9110S drives motor → pot feedback → loop until within dead zone

Cadence changes: 80 → 90 RPM
    → ERGController recalculates: 200W at 90 RPM → level 38
    → Motor adjusts to new position → power stays ~200W
```

### Verdict: ✅ WORKS

The key insight: the motor is **always** driven by the L9110S, never directly by the console. The console's motor output wires go to ESP32 GPIO inputs, not to the motor. So even though the console is powered and potentially trying to send motor commands, those commands reach only the ESP32's input pins — and in Override Mode, they're ignored.

> [!IMPORTANT]
> **User experience note:** If someone turns the red knob while the Echelon console is connected but idle, the console will decode the encoder clicks and try to drive the motor — but nothing happens (its motor signals are ignored in Override Mode). The console's display/LED may show a resistance change that hasn't actually occurred. This is expected but should be documented.

### Test Case

| Step | Action | Expected Result |
|---|---|---|
| 1 | Set Override Mode via portal. Console connected but app not in use. | Portal shows mode = Override, 100 levels. |
| 2 | Open FTMS fitness app. Pair "Echenon". | App connects, shows FTMS features including Control Point. |
| 3 | Start workout. App sends Set Target Power: 200W. | Motor moves. Pot ADC settles at target. Overlay shows ~200W. |
| 4 | Vary cadence 60–100 RPM. | Resistance adjusts automatically to maintain 200W. |
| 5 | Turn red knob mid-ERG. | ERG cancels. Motor moves to encoder-commanded position. |
| 6 | App sends new target. | ERG re-engages. Motor moves to new computed level. |
| 7 | Verify console motor sense pins (GPIO 10/11). | May show activity if console is trying to drive motor — but these are ignored. |

---

## UC5 — Echelon Class + External Auto-Resistance (HR Zones)

> *Echelon app running a class. An external app or the portal takes over resistance control based on HR zones.*

### Signal Trace — What the User Wants

```
Echelon app → running a class (video, instructor, UI)
    → Echelon app shows class content on tablet
    → Rider sees instructor, hears music, follows along

Simultaneously:
    HR armband → ESP32 → current HR = 155 BPM
    HR zone engine → Zone 3 (Tempo) → target resistance = level 55
    → ResistanceMotor drives to level 55
    → As HR drifts, resistance adjusts to keep rider in target zone
```

### Verdict: ✅ WORKS (via external app — Approach A)

**Console-connected Override is fully handled by the MITM design:**

The encoder and pot signals pass through to the console, but the console's
motor output signals reach only GPIO 10/11 (ignored in Override Mode). The
motor is driven exclusively by the L9110S under ESP32 control. The console
wastes effort trying to drive the motor, but nothing happens.

**The Echelon app never commands resistance** — it only reads the pot
(pass-through) for its resistance display. The instructor calls out levels
verbally; the user physically turns the knob. There is no software-level
resistance control in the Echelon app to conflict with Override Mode. The
Echelon app's resistance bar will show the *actual* pot position (set by the
ESP32/motor), which may not match instructor callouts. This is expected.

**HR-zone auto-resistance — Approach A (external app) for v1:**

| Approach | Where Logic Lives | FTMS? | New Firmware? | Status |
|---|---|---|---|---|
| **A. External app** | Third-party app reads HR, calculates target, sends FTMS Set Target Power/Resistance | Uses existing Control Point | No — already supported | **v1 ✅** |
| **B. Portal-native** | ESP32 maps HR → zone → resistance level, drives motor directly | No FTMS needed | Yes — new `HRZoneController` module | **Deferred to v2** |

### Test Case (Approach A — external app, v1)

| Step | Action | Expected Result |
|---|---|---|
| 1 | Set Override Mode. Console connected. Echelon class loaded. | Portal shows mode = Override. |
| 2 | Open HR-zone training app (e.g. Wahoo SYSTM). Pair "Echenon" as FTMS. | App connects, receives HR from FTMS packet. |
| 3 | App determines HR-zone target and sends Set Target Resistance. | Motor moves. Resistance changes. |
| 4 | HR drifts up → app sends lower resistance to ease effort. | Motor adjusts. Power drops. |
| 5 | Echelon app on tablet still shows class video and instructor. | Class content unchanged. Resistance bar shows actual (motor-driven) position. |
| 6 | Rider turns knob. | ERG/zone control cancels. Manual mode. |

---

## UC6 — Console Disconnected / Broken (Pure FTMS Trainer)

> *Override Mode. Console board completely disconnected or broken. Bike as a pure FTMS device.*

### Signal Trace

```
Console disconnected → J2 (9-pin) and J4 (3-pin) unplugged
    → GPIO 10/11 read LOW (10kΩ pull-downs) — correct, ignored in Override Mode

BUT: Where do the sensors get their reference voltages?
```

### Verdict: 🔴 EXPOSES CRITICAL SPEC GAP

**Gap 3: Standalone Sensor Power**

When the console is disconnected, the sensors lose their power source:

| Sensor | Power Source (Stock) | With Console Disconnected |
|---|---|---|
| **Cadence reed switch** | Console provides 3.3V pull-up via Orange/Grey wires | ❌ **No pull-up → floating input, no signal** |
| **Potentiometer** | Console provides reference voltage across Yellow/Green outer terminals | ❌ **No reference → wiper reads indeterminate** |
| **Rotary encoder** | Console provides 3.3V on Red wire (3-pin JST) | ❌ **No power → encoder dead** |

**All three sensor circuits depend on the console's 3.3V regulator for excitation.** With the console disconnected, nothing works.

### Required Fix: Intercept Board Standalone Power Supply

The intercept board already has 3.3V from the AMS1117-3.3 LDO. It needs to supply this to the sensors when the console is absent:

```
AMS1117-3.3 output (3.3V)
    │
    ├──► L9110S VCC (existing)
    │
    ├──► Cadence pull-up: 10kΩ resistor from 3.3V → J1 pin 6 (Orange)
    │    (Always connected — console's pull-up and our pull-up in parallel
    │     is safe; both are 3.3V. Combined pull-up is ~5kΩ, still fine.)
    │
    ├──► Pot reference: 3.3V → J1 pin 3 (Yellow outer terminal)
    │    GND → J1 pin 5 (Green outer terminal)
    │    ⚠️ When console IS connected, console also drives these.
    │    Need: Schottky diode OR series resistor to avoid contention.
    │
    └──► Encoder power: 3.3V → J3 pin 1 (Red)
         ⚠️ Same contention issue — console's 3.3V and our 3.3V
         both driving the same wire.
```

**Contention analysis:**

| Signal | Dual-drive risk | Mitigation |
|---|---|---|
| **Cadence pull-up** | LOW — both are 3.3V to same rail via resistors. Parallel pull-ups reduce impedance. Safe. | None needed — always connect via 10kΩ. |
| **Pot outer terminals** | MEDIUM — if console drives Yellow/Green to specific voltages (not just 3.3V rail), our supply could conflict. But the measured range (1.16–1.7V across Yellow↔Green) suggests the console applies a voltage divider or current-limited reference. | **Schottky diode** (e.g. BAT54) from our 3.3V to Yellow, with reverse-blocking. OR: use a **100Ω series resistor** so our supply yields gracefully when the console is driving. |
| **Encoder power** | LOW — both are 3.3V supplies. A 10Ω series resistor prevents bus-fight if voltages differ slightly. | **10Ω series resistor** from our 3.3V to J3 pin 1. |

**Gap 4: Cadence Pull-Up Source Not Specified**

The current spec (§5.1) says the cadence pin is `INPUT` mode with interrupt on `FALLING`. But it doesn't specify the pull-up source. The reed switch is a simple contact closure — it needs a pull-up resistor to 3.3V to produce a clean HIGH→LOW→HIGH signal.

In the stock bike, the console provides this. With the console disconnected, we need it from the intercept board. The ESP32's internal pull-up (~45kΩ) may be too weak for a long wire run. A discrete 10kΩ external pull-up from the LDO's 3.3V rail is safer.

### Test Case (After Spec Fix)

| Step | Action | Expected Result |
|---|---|---|
| 1 | Disconnect console (J2 and J4 unplugged). Set Override Mode. | GPIO 10/11 read LOW. ESP32 boots normally. |
| 2 | Verify cadence: pedal slowly. | Reed switch pulses on GPIO 5 (pull-up from intercept board LDO). |
| 3 | Verify pot: read ADC on GPIO 1. | Pot wiper reads within calibrated range (reference from intercept board 3.3V). |
| 4 | Verify encoder: turn red knob. | Quadrature clicks register on GPIO 8/9 (encoder powered from intercept board 3.3V). |
| 5 | Connect FTMS app. Start ERG workout. | Motor responds. Resistance tracks target. |
| 6 | Reconnect console (J2 and J4). Switch to Intercept Mode. | All pass-through functions restored. No contention issues. |

---

## Summary of Spec Gaps — Resolution Status

### Gap 1: Standalone Sensor Power ✅ RESOLVED

Added to [`hardware_specification.md`](file:///Users/mcneillm/Documents/Projects/Echenon/docs/hardware_specification.md) §7–8 and SPEC.md §4.7, §5.1:
- R3 (10kΩ pull-up) for cadence reed switch
- R4 (100Ω series) for pot reference contention protection
- R5 (10Ω series) for encoder power contention protection
- All always-connected — safe with console present or absent

### Gap 2: HR-Zone Auto-Resistance ✅ DECIDED

**Approach A (external app via FTMS Control Point) for v1.** Portal-native
HR-zone controller deferred to v2 and listed in SPEC.md §1.4 Out of Scope.

### Gap 3: Cadence Pull-Up Source ✅ RESOLVED

Documented in SPEC.md §5.1 and hardware_specification.md §7.1. R3 (10kΩ)
from 3.3V rail provides standalone pull-up.

### Gap 4: Console-Connected Override Mode UX ✅ RESOLVED

Documented in SPEC.md §5.6 "Console-connected Override" subsection:
- Console's motor commands reach GPIO 10/11 only (ignored in Override Mode)
- Echelon app **never commands resistance** — it only reads the pot
- Echelon app's resistance bar shows actual motor-driven position
- No software conflict between Echelon app and Override Mode
