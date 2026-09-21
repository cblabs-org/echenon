# Echenon — Hardware Build Guide

**Phase-by-phase circuit designs for the Echenon intercept system.**

*This document is organized by development phase. Each phase has a complete
schematic, parts list, and build checklist so you can build incrementally
on a breadboard and eventually move to perfboard.*

*For stock EX-3 hardware details (teardown), see
[`ex3_specification.md`](ex3_specification.md).
For development strategy and firmware, see
[`implementation-strategy.md`](implementation-strategy.md).*

---

## 1. Design Overview

The Echenon system evolves across three hardware configurations:

| Phase | Config | What You Build |
|---|---|---|
| **Phase 0–1** | Breadboard observer | ESP32 + buck converter + fly wires to bike looms. Read-only — the bike is unmodified. |
| **Phase 2** | ESP32 standalone | Just the ESP32 with USB power. BLE only, no bike wiring. |
| **Phase 3+** | Intercept board | Perfboard with ESP32, L9110S motor driver, buck converter, JST connectors. Sits between bike and console. |

Each phase adds to the previous — no rework required.

```
Phase 0/1              Phase 2              Phase 3+
Breadboard             ESP32 only           Intercept Board
┌──────────────┐       ┌──────────┐         ┌──────────────────────┐
│ ESP32        │       │ ESP32    │         │ ESP32  L9110S        │
│ MP1584EN     │       │ (USB)    │         │ MP1584EN             │
│ 4× divider R │       │          │         │ JST connectors       │
│ fly wires ×12│       │          │         │ R1-R5, C1-C2         │
└──────────────┘       └──────────┘         │ Motor split (MITM)   │
                                            └──────────────────────┘
```

---

## 2. Master Bill of Materials

Every component across all phases. The "Phase" column shows when each part
is first needed.

| Ref | Item | Qty | Phase | Status |
|---|---|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 | 0 | Owned |
| U2 | L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | 3 | Owned (5-pack) |
| U3 | GTIWUNG MP1584EN DC-DC Adjustable Buck Converter Module | 1 | 0 | Owned (10-pack) |
| J1 | JST-SM 9-pin socket (female, bike side) | 1 | 3 | To purchase |
| J2 | JST-SM 9-pin plug (male, console side) | 1 | 3 | To purchase |
| J3 | JST 3-pin socket (female, encoder side) | 1 | 3 | To purchase |
| J4 | JST 3-pin plug (male, console side) | 1 | 3 | To purchase |
| J5 | 30-pin female header strips ×2 (ESP32 carrier) | 1 set | 3 | To purchase |
| J6 | 2-pin screw terminal block (motor output) | 1 | 3 | To purchase |
| R1a | 30kΩ resistor (motor+ sense divider, series) | 1 | 0 | To purchase |
| R1b | 10kΩ resistor (motor+ sense divider, to GND) | 1 | 0 | To purchase |
| R2a | 30kΩ resistor (motor− sense divider, series) | 1 | 0 | To purchase |
| R2b | 10kΩ resistor (motor− sense divider, to GND) | 1 | 0 | To purchase |
| R3 | 10kΩ resistor (cadence pull-up — standalone power) | 1 | 3 | To purchase |
| R4 | 100Ω resistor (pot reference — contention protection) | 1 | 3 | To purchase |
| R5 | 10Ω resistor (encoder power — contention protection) | 1 | 3 | To purchase |
| C1 | 100 nF ceramic capacitor (cadence debounce) | 1 | 3 | To purchase |
| C2 | 100 nF ceramic capacitor (pot ADC filter) | 1 | 0* | To purchase |
| — | Breadboard (standard half-size) | 1 | 0 | To purchase |
| — | Alligator clip fly wires (assorted) | 12+ | 0 | To purchase |
| — | Perfboard / protoboard (70×50 mm) | 1 | 3 | To purchase |
| — | Hookup wire (22 AWG, assorted colours) | — | 3 | To purchase |

*C2 is optional in Phase 0 (helps with ADC noise) but required in Phase 3.

---

## 3. Power Supply

### 3.1 Source

The bike's 9V DC power supply (YINGJIAO YS25-0903000, 9V/3A) enters the
frame and travels up to the console via the 9-pin loom (White = +9V,
Brown = GND). We tap this 9V rail.

### 3.2 Regulation

A single GTIWUNG MP1584EN adjustable buck converter steps 9V down to 3.3V.

```
J1 Pin 8 (White, +9V) ───────────────────────────┐
                                                   │
J1 Pin 9 (Brown, GND) ────────────────────────┐   │
                                               │   │
                                    ┌──────────┤───┤──────────────┐
                                    │  U3      │   │              │
                                    │  MP1584EN GND  VIN          │
                                    │          │                  │
                                    │          │   VOUT ──────────┼──► 3.3V RAIL
                                    │          │   (set via       │
                                    │          │    trimpot)      │
                                    └──────────┤──────────────────┘
                                               │
                                               ▼
                                           COMMON GND
```

> [!IMPORTANT]
> **Before connecting any load**, power the MP1584EN from 9V with the output
> disconnected. Measure the output with a multimeter and adjust the blue
> trimpot until it reads **3.3V ± 0.05V**. Then connect loads.

### 3.3 Rail Distribution

The 3.3V rail powers everything on the intercept board:

```
3.3V RAIL ──┬──► U1 ESP32-S3-Nano pin 2 (3V3) — direct to SoC
            │
            ├──► U2 L9110S VCC — motor driver supply (Phase 3+)
            │
            ├──► R3 (10kΩ) ──► Cadence pull-up (Phase 3+)
            │
            ├──► R4 (100Ω) ──► Pot reference (Phase 3+)
            │
            └──► R5 (10Ω) ──► Encoder power (Phase 3+)
```

In Phase 0/1, only U1 is connected to the 3.3V rail.

### 3.4 Current Budget

| Consumer | Typical | Peak | Duty | Phase |
|---|---|---|---|---|
| ESP32-S3 (WiFi + BLE active) | 200 mA | 500 mA | Continuous | 0+ |
| L9110S + motor | 0 mA | 800 mA | Intermittent (~2 sec) | 3+ |
| Sensors + pull-ups | 5 mA | 5 mA | Continuous | 3+ |
| **Total** | **205 mA** | **1,305 mA** | — | — |

The MP1584EN is rated for 3A continuous — ample headroom.

### 3.5 USB-C Programming Power

When programming/debugging via USB-C, the USB port provides its own 5V to
the ESP32's VBUS pin, which the onboard LDO drops to 3.3V. This creates two
3.3V sources driving the same rail.

> [!WARNING]
> **Disconnect the buck converter's output** (lift the wire from pin 2, or
> disconnect the 9V input) before plugging in USB-C. Two regulators fighting
> over the 3.3V rail can cause voltage droop or overshoot.

In production use, the USB-C port is not connected — power comes solely from
the buck converter via the bike's 9V supply.

---

## 4. Connectors

### 4.1 Connector Table

| Ref | Type | Side | Mates With | Phase | Purpose |
|---|---|---|---|---|---|
| **J1** | JST-SM 9-pin socket | Bike frame | 9-pin plug from lower wiring hub | 3 | All bike-side signals |
| **J2** | JST-SM 9-pin plug | Console | 9-pin socket on console PCB | 3 | Pass-through to console |
| **J3** | JST 3-pin socket | Bike frame | 3-pin plug from resistance knob | 3 | Encoder signals |
| **J4** | JST 3-pin plug | Console | 3-pin socket on console PCB | 3 | Encoder pass-through |
| **J5** | 30-pin female headers ×2 | Board | ESP32-S3-Nano pin headers | 3 | ESP32 carrier socket |
| **J6** | 2-pin screw terminal | Motor | Hookup wire to L9110S output | 3 | Motor connection |

> [!NOTE]
> In Phase 0/1, there are no JST connectors. All connections are via
> alligator clip fly wires directly to the existing looms.

### 4.2 J1 — Main Wiring Loom (Bike Side, 9-Pin JST-SM)

| Pin | Wire Colour | Signal Name | Stock Function |
|---|---|---|---|
| 1 | Red | MOTOR_POS | Resistance motor + |
| 2 | Black | MOTOR_NEG | Resistance motor − |
| 3 | Yellow | POT_REF_HI | Potentiometer outer terminal |
| 4 | Blue | POT_WIPER | Potentiometer wiper (position signal) |
| 5 | Green | POT_REF_LO | Potentiometer outer terminal |
| 6 | Orange | CADENCE | Cadence reed switch signal |
| 7 | Grey | CADENCE_RTN | Cadence reed switch return |
| 8 | White | V9_SUPPLY | +9V DC from power supply |
| 9 | Brown | GND | System ground |

### 4.3 J3 — Encoder Loom (Knob Side, 3-Pin JST)

| Pin | Wire Colour | Signal Name | Function |
|---|---|---|---|
| 1 | Red | ENC_VCC | Encoder common (3.3V reference) |
| 2 | Brown | ENC_CHA | Encoder Channel A (quadrature) |
| 3 | Orange | ENC_CHB | Encoder Channel B (quadrature) |

---

## 5. Phase 0/1 — Passive Observation Breadboard

> **Goal:** Read every signal on the bike without changing anything.
> The bike operates completely normally — we're invisible high-impedance taps.

### 5.1 What You Need

| Ref | Item | Qty |
|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 |
| U3 | MP1584EN buck converter | 1 |
| R1a | 30kΩ resistor | 1 |
| R1b | 10kΩ resistor | 1 |
| R2a | 30kΩ resistor | 1 |
| R2b | 10kΩ resistor | 1 |
| C2 | 100 nF capacitor (optional, reduces ADC noise) | 1 |
| — | Breadboard | 1 |
| — | Alligator clip fly wires | 12+ |

No JST connectors, no L9110S, no R3/R4/R5. The console provides all sensor
power — we just observe.

### 5.2 Pre-Gate — Oscilloscope Motor Voltage Verification

> [!CAUTION]
> **Complete this measurement BEFORE connecting motor fly wires to GPIO 10/11.**
> All other signals (cadence, pot, encoder) are safe to connect immediately.

A digital multimeter measured **3.3V** on the motor wires, but a DMM shows
the *average* voltage — it cannot distinguish 3.3V DC from 9V PWM at ~37%
duty cycle (both average ~3.3V). The voltage dividers protect either way,
but we need to confirm for the firmware design.

**Oscilloscope measurement (30 seconds, one-time):**

1. Power the bike normally.
2. Set scope: **DC coupling**, **5V/div** vertical, **10 ms/div** horizontal.
3. Connect scope **ground clip** to any GND wire (Brown on 9-pin loom).
4. Touch probe to **Red wire** (motor+, pin 1).
5. Turn the resistance knob one click up. Observe:

```
If 3.3V bang-bang (expected):       If 9V PWM (must investigate):
    ┌────┐                              ┌┐ ┌┐ ┌┐ ┌┐
3.3V│    │                          9V  ││ ││ ││ ││
    │    │                              ││ ││ ││ ││
────┘    └──────────── 0V          ─────┘└─┘└─┘└─┘└──── 0V
```

**If 3.3V DC:** Proceed as planned. Voltage dividers will read 0.825V (clear digital HIGH).

**If 9V PWM:** Voltage dividers still protect the ESP32 (9V ÷ 4 = 2.25V, safe).
But the firmware may need PWM decoding instead of simple digital reads.
Document the duty cycle and frequency before proceeding.

### 5.3 Schematic

```
═══════════════════════════════════════════════════════════════════════════════
           PHASE 0 — PASSIVE OBSERVATION BREADBOARD SCHEMATIC
═══════════════════════════════════════════════════════════════════════════════

POWER (tap from 9-pin loom)
──────────────────────────

White wire (pin 8, +9V) ─── fly wire ──► U3 MP1584EN VIN
Brown wire (pin 9, GND) ─── fly wire ──► U3 MP1584EN GND ──┐
                                                             │
U3 MP1584EN VOUT ──────────────────────► U1 ESP32 3V3 pin   │
U3 MP1584EN GND ───────────────────────► U1 ESP32 GND ◄────┘
                                                        COMMON GND


MOTOR SENSE (tap from 9-pin loom — AFTER oscilloscope gate passes)
──────────────────────────────────────────────────────────────────

Red wire (pin 1, motor+) ── fly wire ──► R1a (30kΩ) ──┬──► U1 GPIO 10 (D7)
                                                       │
                                                   R1b (10kΩ)
                                                       │
                                                      GND

Black wire (pin 2, motor−) ── fly wire ──► R2a (30kΩ) ──┬──► U1 GPIO 11 (D8)
                                                         │
                                                     R2b (10kΩ)
                                                         │
                                                        GND


SENSOR TAPS (direct fly wires — all 0–3.3V, safe)
──────────────────────────────────────────────────

Yellow wire (pin 3, pot outer HI)   ── fly wire ──► U1 GPIO 2  (A1, ADC)
Blue wire   (pin 4, pot wiper)      ── fly wire ──► U1 GPIO 1  (A0, ADC)
                                                    ├── C2 (100nF) ── GND
                                                         (optional)
Green wire  (pin 5, pot outer LO)   ── fly wire ──► U1 GPIO 3  (A2, ADC)
Orange wire (pin 6, cadence)        ── fly wire ──► U1 GPIO 5  (D2, interrupt)
Grey wire   (pin 7, cadence return) ── fly wire ──► U1 GPIO 4  (A3, ADC)


ENCODER TAPS (direct fly wires from 3-pin loom — all 0–3.3V, safe)
──────────────────────────────────────────────────────────────────

Enc Red wire    (pin 1, VCC)  ── fly wire ──► U1 GPIO 12 (D9, ADC)
Enc Brown wire  (pin 2, Ch A) ── fly wire ──► U1 GPIO 8  (D5, interrupt)
Enc Orange wire (pin 3, Ch B) ── fly wire ──► U1 GPIO 9  (D6, interrupt)

═══════════════════════════════════════════════════════════════════════════════
```

### 5.4 Motor Sense Voltage Dividers (R1a/R1b, R2a/R2b)

The motor is driven bidirectionally by the console's H-bridge — Red goes HIGH
while Black goes LOW (forward), then the reverse for backward. Back-EMF when
the motor stops can also push wires below ground.

The 30kΩ + 10kΩ voltage divider handles all scenarios:

```
Motor wire ──► R1a/R2a (30kΩ) ──┬──► GPIO (reads divided voltage)
                                │
                            R1b/R2b (10kΩ)
                                │
                               GND

Division ratio: 10k / (30k + 10k) = 1/4
```

| Motor wire voltage | GPIO sees | Current through divider | Safe? |
|---|---|---|---|
| 3.3V (expected drive) | 0.825V | 0.08 mA | ✅ Above ~0.4V logic threshold |
| 0V (idle) | 0V | 0 mA | ✅ Clean LOW |
| 9V (if PWM — unlikely) | 2.25V | 0.23 mA | ✅ Below 3.3V max |
| −5V (back-EMF spike) | −1.25V | 0.17 mA | ✅ Safe for ESP32 internal ESD diodes |

The 30kΩ series resistance limits current to <0.3 mA under any plausible
condition. Cost: 4 resistors, ~£0.10.

> [!NOTE]
> **Why not a simple pull-down?** A 10kΩ pull-down would work for unidirectional
> 3.3V sensing, but it wouldn't protect against 9V (if the motor turns out to
> be PWM) or negative back-EMF (from bidirectional drive). The voltage divider
> handles all cases.

### 5.5 Phase 0 Observation-Only Pins

These GPIO taps are **temporary** — used only in Phase 0/1 to characterise
the bike's signals. They don't appear on the final intercept board.

| GPIO | Signal | What We're Measuring | Expected |
|---|---|---|---|
| GPIO 2 (A1) | Yellow — pot outer HI | Console's pot reference voltage | ~3.3V |
| GPIO 3 (A2) | Green — pot outer LO | Console's pot ground reference | ~0V |
| GPIO 4 (A3) | Grey — cadence return | Reed switch circuit return path | GND or floating |
| GPIO 12 (D9) | Encoder VCC (Red, 3-pin) | Console's encoder supply voltage | ~3.3V |

Once we understand these signals (Phase 0 deliverable), we can design the
standalone power circuits (R3, R4, R5) for Phase 3 with confidence.

### 5.6 Build Checklist

- [ ] Set up breadboard with ESP32-S3-Nano
- [ ] Prepare MP1584EN: connect to 9V, adjust trimpot to 3.3V output
- [ ] Connect MP1584EN VOUT to ESP32 3V3, GND to ESP32 GND
- [ ] Build voltage dividers for motor sense on breadboard (R1a/R1b, R2a/R2b)
- [ ] **GATE: Oscilloscope motor voltage verification** (§5.2)
- [ ] Connect motor sense fly wires (Red → R1a, Black → R2a) — ONLY after gate passes
- [ ] Connect all sensor fly wires (pot, cadence, encoder)
- [ ] Flash Phase 0 firmware via USB-C (**disconnect buck converter first**)
- [ ] Disconnect USB, reconnect buck converter, verify boot via WiFi
- [ ] Verify all 12 signals reading on the web dashboard

### 5.7 Test Points

| Signal | GPIO | Expected Reading |
|---|---|---|
| Motor+ sense | GPIO 10 | 0V idle, ~0.825V when console drives motor forward |
| Motor− sense | GPIO 11 | 0V idle, ~0.825V when console drives motor reverse |
| Pot wiper | GPIO 1 | ~1160–1700 mV (varies with resistance position) |
| Pot outer HI | GPIO 2 | ~3300 mV (console reference) |
| Pot outer LO | GPIO 3 | ~0 mV (console ground) |
| Cadence | GPIO 5 | 3.3V idle, 0V on magnet pulse |
| Cadence return | GPIO 4 | TBD (Phase 0 will determine) |
| Encoder VCC | GPIO 12 | ~3300 mV (console supply) |
| Encoder Ch A | GPIO 8 | Quadrature pulses on knob turn |
| Encoder Ch B | GPIO 9 | Quadrature pulses on knob turn (90° offset) |

---

## 6. Phase 2 — HR Relay (BLE Only)

> **Goal:** Pair an HR armband and re-broadcast to the Echelon app.
> No bike wiring needed — this is purely BLE.

### 6.1 What You Need

| Ref | Item | Qty |
|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 |
| — | USB-C cable | 1 |

That's it. The ESP32 is powered via USB-C. No buck converter, no fly wires,
no breadboard circuits. Flash the Phase 2 firmware and pair your HR armband.

> [!TIP]
> You can develop and test Phase 2 in parallel with Phase 1 since it needs
> no bike hardware at all. Just use USB power.

---

## 7. Phase 3+ — Intercept Board (The Full Build)

> **Goal:** Build the permanent intercept board that sits between the bike
> frame and the Echelon console. This is where the MITM happens.

### 7.1 What You Need (Additions to Phase 0)

Everything from Phase 0 plus:

| Ref | Item | Qty | Purpose |
|---|---|---|---|
| U2 | L9110S motor driver module | 1 | Drives the resistance motor |
| J1 | JST-SM 9-pin socket | 1 | Connects to bike frame loom |
| J2 | JST-SM 9-pin plug | 1 | Connects to console |
| J3 | JST 3-pin socket | 1 | Connects to encoder loom |
| J4 | JST 3-pin plug | 1 | Connects to console encoder |
| J5 | Female header strips ×2 | 1 set | ESP32 carrier socket |
| J6 | 2-pin screw terminal | 1 | Motor output connection |
| R3 | 10kΩ resistor | 1 | Cadence pull-up (standalone power) |
| R4 | 100Ω resistor | 1 | Pot reference (standalone power) |
| R5 | 10Ω resistor | 1 | Encoder power (standalone power) |
| C1 | 100 nF capacitor | 1 | Cadence debounce |
| C2 | 100 nF capacitor | 1 | Pot ADC filter |
| — | Perfboard 70×50 mm | 1 | Main carrier board |
| — | Hookup wire 22 AWG | — | Internal wiring |

**What changes from Phase 0:**
- Move from breadboard → perfboard
- Replace fly wires → JST connectors
- Add L9110S motor driver
- Add standalone power components (R3, R4, R5)
- Add debounce/filter capacitors (C1, C2)
- Add motor drive outputs (GPIO 6, GPIO 7)
- **Remove** Phase 0 observation pins (GPIO 2, 3, 4, 12) — replaced by
  standalone power circuits

### 7.2 Board Layout

```
┌──────────────────────────────────────────────────────────────┐
│                   INTERCEPT BOARD                             │
│                                                               │
│  J1 (9-pin, bike) ──┬──────────────────────── J2 (9-pin, console)
│                      │  7 wires pass through                  │
│                      │  2 motor wires split                   │
│                      │                                        │
│  J3 (3-pin, knob) ──┼──────────────────────── J4 (3-pin, console)
│                      │  3 wires pass through                  │
│                      │                                        │
│                      ▼                                        │
│              ┌───────────────┐     ┌───────────┐              │
│              │  ESP32-S3-Nano│────►│  L9110S   │──► Motor     │
│              └───────────────┘     └───────────┘              │
│                      │                                        │
│              ┌───────────────┐                                 │
│              │  MP1584EN     │ 9V → 3.3V                      │
│              │  Buck Converter│                                │
│              └───────────────┘                                 │
└──────────────────────────────────────────────────────────────┘
```

**No factory wires are cut.** The board plugs into existing JST connectors.
Removing the board restores the bike to stock.

### 7.3 Motor Circuit — The MITM

This is the core innovation — the motor is **always** driven by the L9110S,
never directly by the Echelon console.

#### 7.3.1 Signal Routing

```
STOCK BIKE (no intercept board):
    Console motor output ──────────────────────────── Motor
    (Red/Black on 9-pin)                              (in actuator hub)


WITH INTERCEPT BOARD:
    Console motor output ──► R1a/R2a (30kΩ) ──┬──► ESP32 GPIO 10/11 (sense)
                                              │
                                          R1b/R2b (10kΩ)
                                              │
                             (voltage         GND
                              divider)
                              │
                              │  (read but NOT connected to motor)
                              │
    ESP32 GPIO 6/7 ──────────► L9110S A-IA/A-IB (control inputs)
                                        │
                                        ▼
                                  L9110S A-OA/A-OB (power outputs)
                                        │
                                        ▼
                                      Motor
                                  (in actuator hub)
```

#### 7.3.2 Why the Motor Wires Are Split

In the stock bike, the console sends 3.3V directly to the motor via pins 1–2
of the 9-pin loom. The intercept board breaks this connection:

- **Bike side** (J1 pins 1–2): Motor wires connect to **L9110S outputs**
  (A-OA, A-OB), not to J2.
- **Console side** (J2 pins 1–2): Console's motor drive signals connect to
  **ESP32 GPIO inputs** (GPIO 10/11) via **voltage dividers** (R1a/R1b,
  R2a/R2b).

The console *thinks* it's driving the motor. Actually, its motor signals are
read by the ESP32, which decides whether to relay them (Intercept Mode) or
ignore them (Override Mode).

#### 7.3.3 Console Motor Sense — Voltage Dividers (R1a/R1b, R2a/R2b)

The motor is driven bidirectionally by the console's H-bridge. R1a/R1b and
R2a/R2b form voltage dividers that safely scale the console's motor drive
signals for the ESP32's GPIO inputs.

See §5.4 for the full voltage divider analysis — the same circuit is used
in Phase 0 (fly wires) and Phase 3 (intercept board).

Two additional benefits on the intercept board:

1. **Defined idle state**: Even without pull-downs, the divider's R1b/R2b
   (10kΩ to GND) ensures GPIO 10/11 read LOW when the console is not driving.

2. **Clean disconnect**: In Override Mode or standalone (UC6), the console
   JST is unplugged. R1b/R2b pull GPIO 10/11 to GND — no floating inputs.

#### 7.3.4 L9110S Truth Table

| GPIO 6 (A-IA) | GPIO 7 (A-IB) | L9110S A-OA | L9110S A-OB | Motor |
|---|---|---|---|---|
| LOW | LOW | LOW | LOW | **Stopped** (brake) |
| HIGH | LOW | HIGH | LOW | **Forward** (increase resistance) |
| LOW | HIGH | LOW | HIGH | **Reverse** (decrease resistance) |
| HIGH | HIGH | HIGH | HIGH | **Stopped** (not used) |

### 7.4 Sensor Circuits

#### 7.4.1 Cadence Sensor

```
                    ┌──── R3 (10kΩ) ──── 3V3 RAIL
                    │
J1 Pin 6 (Orange) ──┼──── C1 (100nF) ──── GND
  CADENCE            │
                    └──── U1 GPIO 5 (INPUT, interrupt FALLING)
                    │
                    └──── J2 Pin 6 (pass-through to console)
```

The reed switch is a normally-open contact. When the pedal magnet passes:
- Switch closes → CADENCE pulled to GND → GPIO 5 reads LOW (FALLING edge)
- Switch opens → R3 pulls CADENCE back to 3.3V → GPIO 5 reads HIGH

**R3 (10kΩ pull-up)** provides the pull-up voltage for the reed switch circuit.
In the stock bike, the Echelon console provides this pull-up. The intercept
board provides its own via R3, ensuring the cadence sensor works even when
the console is disconnected (UC6 — standalone operation).

When the console IS connected, both the console's pull-up and R3 are in
parallel. Two pull-ups to the same 3.3V rail is safe — the combined resistance
is lower (~5kΩ), producing a slightly stronger pull-up. No contention.

**C1 (100 nF)** provides hardware debounce, filtering contact bounce from the
reed switch. Combined with the 25 ms software lockout, this eliminates all
spurious pulses.

#### 7.4.2 Resistance Potentiometer

```
3V3 RAIL ── R4 (100Ω) ──┐
                         │
J1 Pin 3 (Yellow) ───────┼──── J2 Pin 3 (pass-through to console)
  POT_REF_HI             │
                         │
              ┌──────── POT ────────┐
              │     (inside hub)    │
              │                     │
J1 Pin 4 (Blue) ──┼──── C2 (100nF) ──── GND
  POT_WIPER        │
                   └──── U1 GPIO 1 (ADC1_CH0, 12-bit, 11dB atten)
                   │
                   └──── J2 Pin 4 (pass-through to console)
                         │
J1 Pin 5 (Green) ────────┼──── J2 Pin 5 (pass-through to console)
  POT_REF_LO             │
                         └──── GND (direct connection)
```

**Dual-source pot reference power:**

| Scenario | POT_REF_HI Source | POT_REF_LO | Wiper Range |
|---|---|---|---|
| **Console connected** | Console's 3.3V regulator (via J2) | Console's GND | ~1160–1700 mV |
| **Console disconnected** | 3V3 rail via R4 (100Ω) | GND | Range may differ — **recalibrate** |

**R4 (100Ω series resistor)** protects against contention when both the
console and the intercept board drive POT_REF_HI simultaneously:

- Both drive 3.3V: ~0 mA through R4. Safe.
- Slight voltage difference (e.g., 3.28V vs 3.32V): R4 limits current to
  ~0.4 mA. No damage, minimal voltage error.
- Console disconnected: R4 drops ~0.5 mV at 5 mA. Negligible.

**C2 (100 nF)** filters ADC noise on the wiper signal.

> [!IMPORTANT]
> When switching between console connected and disconnected, the pot
> reference voltage changes. **Run the calibration wizard** via the portal
> after changing the console connection state.

#### 7.4.3 Rotary Encoder (Resistance Knob)

```
3V3 RAIL ── R5 (10Ω) ──┐
                         │
J3 Pin 1 (Red) ──────────┼──── J4 Pin 1 (pass-through to console)
  ENC_VCC                │
                         │
J3 Pin 2 (Brown) ────────┼──── J4 Pin 2 (pass-through to console)
  ENC_CHA                │
                         └──── U1 GPIO 8 (INPUT, interrupt CHANGE)

J3 Pin 3 (Orange) ───────┼──── J4 Pin 3 (pass-through to console)
  ENC_CHB                │
                         └──── U1 GPIO 9 (INPUT, interrupt CHANGE)
```

**R5 (10Ω series resistor)** on the encoder's 3.3V supply prevents bus-fight
between the intercept board's 3.3V and the console's 3.3V when both are
connected. At 10Ω with <10 mA, the voltage drop is <100 mV — negligible.

### 7.5 Standalone Operation (Console Disconnected)

When the Echelon console is disconnected (UC6), all sensors must continue
to function. The intercept board provides replacement power:

| Sensor | Stock Power Source | Intercept Board Replacement | Component |
|---|---|---|---|
| Cadence reed switch | Console pull-up | R3: 10kΩ pull-up from 3V3 | Always connected |
| Pot reference | Console 3.3V | R4: 100Ω from 3V3 to POT_REF_HI; GND to POT_REF_LO | Always connected |
| Encoder | Console 3.3V | R5: 10Ω from 3V3 to ENC_VCC | Always connected |

All three standalone power paths are **always connected** — they coexist
safely with the console's own power when it is present (see §7.4).

### 7.6 Complete Schematic

```
═══════════════════════════════════════════════════════════════════════════════
              PHASE 3+ — INTERCEPT BOARD COMPLETE SCHEMATIC
═══════════════════════════════════════════════════════════════════════════════

J1 (9-PIN JST-SM, BIKE SIDE)                 J2 (9-PIN JST-SM, CONSOLE SIDE)
┌──────────────────────────┐                 ┌──────────────────────────┐
│ 1  Red    MOTOR_POS ─────┼──► U2 A-OA     │ 1  Red  ──► R1a (30k) ──┤
│   (to L9110S output)     │                 │           ──┬── GPIO 10 │
│                          │                 │           R1b (10k)     │
│                          │                 │             └── GND     │
│ 2  Black  MOTOR_NEG ─────┼──► U2 A-OB     │ 2  Black ──► R2a (30k) ──┤
│   (to L9110S output)     │                 │           ──┬── GPIO 11 │
│                          │                 │           R2b (10k)     │
│                          │                 │             └── GND     │
│ 3  Yellow POT_REF_HI ────┼── pass-thru ───┼── 3                     │
│     └── R4 (100Ω) ◄─ 3V3│                 │                         │
│ 4  Blue   POT_WIPER ─────┼── pass-thru ───┼── 4                     │
│     ├── C2 (100nF) ── GND│                 │                         │
│     └── GPIO 1 (ADC)     │                 │                         │
│ 5  Green  POT_REF_LO ────┼── pass-thru ───┼── 5                     │
│     └── GND              │                 │                         │
│ 6  Orange CADENCE ────────┼── pass-thru ───┼── 6                     │
│     ├── R3 (10k) ◄── 3V3 │                 │                         │
│     ├── C1 (100nF) ── GND│                 │                         │
│     └── GPIO 5 (INT)     │                 │                         │
│ 7  Grey   CADENCE_RTN ───┼── pass-thru ───┼── 7                     │
│ 8  White  V9_SUPPLY ─────┼── pass-thru ───┼── 8                     │
│     └── U3 VIN           │                 │                         │
│ 9  Brown  GND ────────────┼── pass-thru ───┼── 9                     │
│     └── COMMON GND       │                 │                         │
└──────────────────────────┘                 └──────────────────────────┘


J3 (3-PIN JST, ENCODER)                     J4 (3-PIN JST, CONSOLE)
┌──────────────────────────┐                 ┌──────────────────────────┐
│ 1  Red    ENC_VCC ────────┼── pass-thru ───┼── 1                     │
│     └── R5 (10Ω) ◄── 3V3 │                 │                         │
│ 2  Brown  ENC_CHA ────────┼── pass-thru ───┼── 2                     │
│     └── GPIO 8 (INT)     │                 │                         │
│ 3  Orange ENC_CHB ────────┼── pass-thru ───┼── 3                     │
│     └── GPIO 9 (INT)     │                 │                         │
└──────────────────────────┘                 └──────────────────────────┘


U1 — WAVESHARE ESP32-S3-NANO                U2 — L9110S H-BRIDGE MODULE
┌──────────────────────────────┐            ┌───────────────────────┐
│                              │            │                       │
│  Pin 2 (3V3) ◄── 3V3 RAIL   │            │  VCC ◄── 3V3 RAIL    │
│  GND ◄── COMMON GND         │            │  GND ◄── COMMON GND  │
│                              │            │                       │
│  GPIO 1  (A0) ◄── POT_WIPER │            │  A-IA ◄── GPIO 6     │
│  GPIO 5  (D2) ◄── CADENCE   │            │  A-IB ◄── GPIO 7     │
│  GPIO 6  (D3) ──► L9110S_IA │            │                       │
│  GPIO 7  (D4) ──► L9110S_IB │            │  A-OA ──► MOTOR_POS  │
│  GPIO 8  (D5) ◄── ENC_CHA   │            │  A-OB ──► MOTOR_NEG  │
│  GPIO 9  (D6) ◄── ENC_CHB   │            │                       │
│  GPIO 10 (D7) ◄── CONS_M+   │            └───────────────────────┘
│  GPIO 11 (D8) ◄── CONS_M−   │
│                              │            U3 — MP1584EN BUCK CONVERTER
│  USB-C (programming only)    │            ┌───────────────────────┐
│                              │            │  VIN ◄── V9_SUPPLY    │
└──────────────────────────────┘            │  GND ◄── COMMON GND  │
                                            │  VOUT ──► 3V3 RAIL   │
                                            │  (trimpot set 3.3V)  │
                                            └───────────────────────┘
═══════════════════════════════════════════════════════════════════════════════
```

### 7.7 Net List

Every electrical connection on the intercept board, grouped by function.

#### Power Nets

| Net Name | Source | Destinations |
|---|---|---|
| **V9_SUPPLY** | J1 pin 8 (White) | J2 pin 8 (pass-through), U3 VIN |
| **GND** | J1 pin 9 (Brown) | J2 pin 9, U1 GND, U2 GND, U3 GND, R1b, R2b, C1, C2 |
| **3V3** | U3 VOUT | U1 pin 2 (3V3), U2 VCC, R3, R4, R5 |

#### Motor Nets (Split Path — the MITM)

| Net Name | Source | Destination | Notes |
|---|---|---|---|
| **MOTOR_POS** | J1 pin 1 (Red, bike) | U2 A-OA (output) | Bike's motor + wire → L9110S output, NOT to console |
| **MOTOR_NEG** | J1 pin 2 (Black, bike) | U2 A-OB (output) | Bike's motor − wire → L9110S output, NOT to console |
| **CONSOLE_MTR_POS** | J2 pin 1 (Red, console) | R1a (30kΩ) → R1b (10kΩ) to GND, tap to U1 GPIO 10 | Console motor + → ESP32 via voltage divider |
| **CONSOLE_MTR_NEG** | J2 pin 2 (Black, console) | R2a (30kΩ) → R2b (10kΩ) to GND, tap to U1 GPIO 11 | Console motor − → ESP32 via voltage divider |
| **L9110S_IA** | U1 GPIO 6 | U2 A-IA (input) | ESP32 controls motor forward |
| **L9110S_IB** | U1 GPIO 7 | U2 A-IB (input) | ESP32 controls motor reverse |

#### Sensor Nets (Tap + Pass-Through)

| Net Name | J1 Pin | → J2 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **POT_REF_HI** | 3 (Yellow) | Pass-through | — | R4 (100Ω) from 3V3 rail |
| **POT_WIPER** | 4 (Blue) | Pass-through | U1 GPIO 1 (ADC) | C2 (100 nF to GND) |
| **POT_REF_LO** | 5 (Green) | Pass-through | — | Connected to GND |
| **CADENCE** | 6 (Orange) | Pass-through | U1 GPIO 5 (interrupt) | R3 (10kΩ to 3V3), C1 (100 nF to GND) |
| **CADENCE_RTN** | 7 (Grey) | Pass-through | — | — |

#### Encoder Nets (Tap + Pass-Through)

| Net Name | J3 Pin | → J4 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **ENC_VCC** | 1 (Red) | Pass-through | — | R5 (10Ω) from 3V3 rail |
| **ENC_CHA** | 2 (Brown) | Pass-through | U1 GPIO 8 (interrupt) | — |
| **ENC_CHB** | 3 (Orange) | Pass-through | U1 GPIO 9 (interrupt) | — |

### 7.8 ESP32-S3-Nano Pin Map (Phase 3)

| Header | Label | GPIO | Direction | Net Name | Function |
|---|---|---|---|---|---|
| Left | 3V3 | — | Power In | 3V3 | 3.3V supply from buck converter |
| Left | GND | — | Power | GND | Common ground |
| Left | A0 | GPIO 1 | Input (ADC) | POT_WIPER | Resistance position (12-bit ADC) |
| Left | D2 | GPIO 5 | Input (INT) | CADENCE | Reed switch pulse (FALLING edge) |
| Left | D3 | GPIO 6 | Output | L9110S_IA | Motor forward drive |
| Left | D4 | GPIO 7 | Output | L9110S_IB | Motor reverse drive |
| Left | D5 | GPIO 8 | Input (INT) | ENC_CHA | Encoder channel A (CHANGE) |
| Left | D6 | GPIO 9 | Input (INT) | ENC_CHB | Encoder channel B (CHANGE) |
| Right | D7 | GPIO 10 | Input | CONSOLE_MTR_POS | Console motor sense + (via divider) |
| Right | D8 | GPIO 11 | Input | CONSOLE_MTR_NEG | Console motor sense − (via divider) |

### 7.9 Wire Routing Summary

#### 7.9.1 Wires That Pass Straight Through (7 of 9 on main loom)

| J1 Pin | Colour | Signal | ESP32 Tap |
|---|---|---|---|
| 3 | Yellow | Pot outer (REF_HI) | — (powered via R4) |
| 4 | Blue | Pot wiper | GPIO 1 ADC (high-Z) |
| 5 | Green | Pot outer (REF_LO) | — (tied to GND) |
| 6 | Orange | Cadence | GPIO 5 interrupt (high-Z) |
| 7 | Grey | Cadence return | — |
| 8 | White | +9V supply | Buck converter VIN |
| 9 | Brown | GND | Common ground |

#### 7.9.2 Wires That Are Split (2 of 9 — the MITM)

| J1 Pin | Colour | Bike Side (J1) Goes To | Console Side (J2) Goes To |
|---|---|---|---|
| 1 | Red | L9110S A-OA (motor output) | ESP32 GPIO 10 via R1a (30kΩ) / R1b (10kΩ) divider |
| 2 | Black | L9110S A-OB (motor output) | ESP32 GPIO 11 via R2a (30kΩ) / R2b (10kΩ) divider |

#### 7.9.3 Encoder Loom (All 3 wires pass through)

| J3 Pin | Colour | Signal | ESP32 Tap |
|---|---|---|---|
| 1 | Red | Encoder 3.3V | — (powered via R5) |
| 2 | Brown | Channel A | GPIO 8 interrupt |
| 3 | Orange | Channel B | GPIO 9 interrupt |

---

## 8. Assembly Notes (Phase 3)

### 8.1 Build Sequence

1. **Prepare the perfboard.** Mark out component positions. The board should
   fit inside the console housing alongside the Echelon motherboard.

2. **Solder female header strips (J5)** for the ESP32 carrier socket. Verify
   the ESP32 seats and can be removed.

3. **Mount the L9110S module (U2)** on the perfboard. Solder VCC, GND, and
   fly-wire the I/O pins.

4. **Mount the MP1584EN module (U3).** Solder VIN and GND. **Do not connect
   VOUT yet.** Power from 9V and adjust the trimpot to 3.3V output first.

5. **Wire the 3.3V rail** from U3 VOUT to: U1 3V3 pad, U2 VCC pad, R3, R4,
   R5 landing pads.

6. **Solder passive components:** R1a/R1b, R2a/R2b, R3, R4, R5, C1, C2.

7. **Build the voltage dividers** for motor sense:
   - J2 pin 1 → R1a (30kΩ) → junction → GPIO 10
   - Junction → R1b (10kΩ) → GND
   - J2 pin 2 → R2a (30kΩ) → junction → GPIO 11
   - Junction → R2b (10kΩ) → GND

8. **Wire the JST connectors.** Use colour-matched hookup wire. The 7
   pass-through wires run J1→J2 directly. The 2 motor wires split as
   described in §7.9.2.

9. **Test with a multimeter** before powering:
   - Continuity: each pass-through wire J1 pin N → J2 pin N (pins 3–9)
   - No continuity: J1 pin 1 → J2 pin 1 (motor split)
   - No continuity: J1 pin 2 → J2 pin 2 (motor split)
   - Divider ratio: R1a+R1b from J2 pin 1 to GND (~40kΩ total)
   - Divider ratio: R2a+R2b from J2 pin 2 to GND (~40kΩ total)
   - 3.3V rail: no shorts to GND

10. **Power-on test** (ESP32 not installed):
    - Connect 9V to J1 pins 8/9
    - Verify 3.3V on the rail
    - Verify 3.3V at U1 3V3 pad, U2 VCC, R3/R4/R5 destinations

11. **Install ESP32.** Flash firmware via USB-C (with buck converter
    disconnected). Then disconnect USB, reconnect buck converter, and verify
    boot via serial or WiFi AP.

### 8.2 Test Points

Solder short pin headers at these locations for easy multimeter access:

| Test Point | Net | Expected Voltage |
|---|---|---|
| TP1 | 3V3 RAIL | 3.30V ± 0.05V |
| TP2 | V9_SUPPLY | 8.5–9.5V (from PSU) |
| TP3 | POT_WIPER | ~1160–1700 mV (varies with position) |
| TP4 | CADENCE | 3.3V idle, 0V on pulse |
| TP5 | CONSOLE_MTR_POS (at GPIO 10) | 0V idle, ~0.825V when console drives forward |
| TP6 | CONSOLE_MTR_NEG (at GPIO 11) | 0V idle, ~0.825V when console drives reverse |

### 8.3 Physical Dimensions

| Component | Approx Size | Notes |
|---|---|---|
| Perfboard | 70 × 50 mm | Main carrier board |
| ESP32-S3-Nano | 49.8 × 18.2 mm | Mounted via female headers (removable) |
| L9110S module | 28 × 21 mm | Soldered or socketed on perfboard |
| MP1584EN module | 22 × 17 mm | Soldered on perfboard (trimpot accessible) |

The console housing has space alongside the Echelon motherboard for a board
of this size. Route JST cables through the existing cable entry points.

---

## 9. Safety Notes

> [!CAUTION]
> - **Never exceed 3.3V on any ESP32 GPIO pin.** The 9V rail must only
>   connect to the buck converter VIN. Verify wiring before power-on.
> - **Complete the oscilloscope motor voltage gate (§5.2)** before connecting
>   motor sense wires in Phase 0. The voltage dividers protect the ESP32
>   either way, but the firmware design depends on the result.
> - **Disconnect buck converter output before plugging in USB-C** to avoid
>   contention between the buck converter and USB power.
> - **The motor can draw up to 800 mA.** Ensure hookup wire between J6 and
>   the L9110S output is rated for at least 1A (22 AWG is sufficient).
> - **The L9110S has internal flyback clamp diodes**, so no external diodes
>   are needed for the motor's inductive kickback.
> - **Pot wiper is a high-impedance signal.** Keep the wire run from J1 pin 4
>   to GPIO 1 as short as possible to minimise noise pickup. C2 (100 nF)
>   must be placed close to the ESP32 ADC pin.
