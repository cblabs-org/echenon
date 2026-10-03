# Echenon — Hardware Build Guide

**Phase-by-phase circuit designs for the Echenon hardware configurations.**

*This document is organized by development phase. Each phase has a
schematic, parts list, and build checklist so you can build incrementally,
from a bench logger on USB power to a permanent board in the console.*

*For stock EX-3 hardware details (teardown), see
[`ex3_specification.md`](ex3_specification.md).
For development strategy and firmware, see
[`implementation-strategy.md`](implementation-strategy.md).
The authoritative pin maps are SPEC §4.4.1 (Phase 0/1) and §4.4.2 (Phase 3+,
provisional) ([`../SPEC.md`](../SPEC.md)); the
Phase 0 exit checklist is the Phase 0 register,
[`measurements/phase0/README.md`](measurements/phase0/README.md).*

---

## 1. Design Overview

Echenon is passive-first. The system uses four hardware configurations, each
tied to the programme phases (numbering as in the programme plan and SPEC §1.5):

| # | Configuration | Phases (PRs) | What You Build | Bike wiring |
|---|---|---|---|---|
| 1 | **Breadboard observer** | 0 Observe (PR-01), 1 Calibrate (PR-02) | ESP32 on a breadboard, powered from USB-C. Y / back-probe harness on the looms, series-resistor taps, ÷4 motor-sense dividers. | Read-only. The bike is unmodified. |
| 2 | **HR-only (USB)** | 2 HR relay (PR-03) | Just the ESP32 on USB-C. BLE only. | None |
| 3 | **Passive tap board** | 3 Passive tap + FTMS telemetry (PR-04a, PR-05), 4 Session / upload (PR-06) | Permanent board in the console housing: J1–J4 pass-through, high-impedance taps, buck-powered, manual OTA. **Design finalised at DR-1** (§6.2). | Pass-through. No wires split. |
| 4 | **Intercept board** | 5 MITM (PR-07), 6 ERG (PR-08), 7 Integration (PR-09) | Perfboard with ESP32, motor driver, JST connectors, switchable standalone supplies. Motor wires split. **PROVISIONAL — superseded at DR-1** (§7–§8). | Motor split (MITM). |

PR-05 (FTMS telemetry) needs no new hardware: the breadboard observer is
enough to develop it; the passive tap board is the daily-use host once built.

DR-1 (Design Freeze Review) sits between Phase 0 and every soldered board.
No hardware is soldered or permanently installed until DR-1 is approved.

```
Phase 0/1            Phase 2          Phase 3–4              Phase 5–7
Breadboard observer  HR-only          Passive tap board      Intercept board
┌───────────────┐    ┌──────────┐     ┌──────────────────┐   ┌──────────────────────┐
│ ESP32 (USB-C) │    │ ESP32    │     │ ESP32  MP1584EN  │   │ ESP32  motor driver  │
│ ÷4 dividers   │    │ (USB-C)  │     │ J1–J4 pass-thru  │   │ buck / motor rail    │
│ 22k taps ×8   │    │          │     │ hi-Z taps        │   │ J1–J4, J6            │
│ Y harness     │    │          │     │ manual OTA       │   │ R3–R5 + SW1–SW4      │
└───────────────┘    └──────────┘     │ [DR-1]           │   │ Motor split (MITM)   │
                                      └──────────────────┘   │ PROVISIONAL [DR-1]   │
                                                             └──────────────────────┘
```

---

## 2. Master Bill of Materials

Every component across all configurations. ✓ = needed in that
configuration. `[DR-1]` = the part or its value is decided at DR-1.

| Ref | Item | Qty | 0/1 Observer | 2 HR | 3 Passive (PR-04a) | 5 Intercept (PR-07) | Status |
|---|---|---|---|---|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 | ✓ | ✓ | ✓ | ✓ | Owned |
| U2 | L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | — | — | — | ✓ `[DR-1: D3]` | Owned (5-pack) |
| U3 | GTIWUNG MP1584EN DC-DC Adjustable Buck Converter Module | 1 | — | — | ✓ `[DR-1]` | ✓ `[DR-1]` | Owned (10-pack) |
| H1 | JST-SM 9-pin mating pair (plug + socket), made into the Y / back-probe harness | 1 pair | ✓ | — | — | — | To purchase |
| H2 | JST 3-pin mating pair (plug + socket), made into the Y / back-probe harness | 1 pair | ✓ | — | — | — | To purchase |
| J1 | JST-SM 9-pin socket (female, bike side) | 1 | — | — | ✓ | ✓ | To purchase |
| J2 | JST-SM 9-pin plug (male, console side) | 1 | — | — | ✓ | ✓ | To purchase |
| J3 | JST 3-pin socket (female, encoder side) | 1 | — | — | ✓ | ✓ | To purchase |
| J4 | JST 3-pin plug (male, console side) | 1 | — | — | ✓ | ✓ | To purchase |
| J5 | 30-pin female header strips ×2 (ESP32 carrier) | 1 set | — | — | ✓ | ✓ | To purchase |
| J6 | 2-pin screw terminal block (motor output) | 1 | — | — | — | ✓ | To purchase |
| R1a | 30 kΩ resistor (motor+ sense divider, series) | 1 | ✓ | — | ✓ | ✓ | To purchase |
| R1b | 10 kΩ resistor (motor+ sense divider, to GND) | 1 | ✓ | — | ✓ | ✓ | To purchase |
| R2a | 30 kΩ resistor (motor− sense divider, series) | 1 | ✓ | — | ✓ | ✓ | To purchase |
| R2b | 10 kΩ resistor (motor− sense divider, to GND) | 1 | ✓ | — | ✓ | ✓ | To purchase |
| RS1–RS8 | 22 kΩ series resistor on each direct tap (range 10–47 kΩ) | 8 | ✓ | — | ✓ `[DR-1: count]` | ✓ `[DR-1: count]` | To purchase |
| R3 | 10 kΩ resistor (cadence pull-up, standalone only) | 1 | — | — | — | ✓ `[DR-1]` | To purchase |
| R4 | 100 Ω resistor (pot reference, standalone only) | 1 | — | — | — | ✓ `[DR-1]` | To purchase |
| R5 | 10 Ω resistor (encoder power, standalone only) | 1 | — | — | — | ✓ `[DR-1]` | To purchase |
| R6, R7 | 10 kΩ resistors (reset-safe pull-downs on driver inputs A-IA / A-IB, GPIO 10 / 17) | 2 | — | — | — | ✓ `[DR-1: D3]` | To purchase |
| SW1–SW4 | Jumpers (minimum) or high-side / analog switches for R3, R4, R5 and the Green–GND link | 4 | — | — | — | ✓ `[DR-1]` | To purchase |
| C1 | 100 nF ceramic capacitor (cadence debounce) | 1 | — | — | `[DR-1]` | ✓ `[DR-1]` | To purchase |
| C2 | 100 nF ceramic capacitor (pot wiper ADC filter) | 1 | optional | — | `[DR-1]` | ✓ | To purchase |
| — | USB-C cable, plus a power bank or battery-powered laptop | 1 | ✓ | ✓ | — | — | Owned |
| — | Breadboard (standard half-size) | 1 | ✓ | — | — | — | To purchase |
| — | Perfboard / protoboard (70×50 mm) | 1 | — | — | ✓ | ✓ | To purchase |
| — | Hookup wire (22 AWG, assorted colours) | — | ✓ | — | ✓ | ✓ | To purchase |

### 2.1 Bench Equipment (Phase 0 bench-ready entry condition)

PR-01's first hardware Work Order does not start until all of these are on
the bench:

| Item | Requirement | Used for (Phase 0 register row) |
|---|---|---|
| Oscilloscope | **≥ 2 channels, single-shot triggering.** A low-cost USB scope is enough. | Motor drive characterisation (#1), power-on behaviour (#13), encoder Ch B shape (#8) |
| Motor-current measurement | A low-value shunt in the motor lead read by the scope, **or** a DC current clamp | Motor inrush and running current (#2) |
| Y / back-probe harness | H1 and H2 (above) built into a pass-through harness with tap tails (§5.1.1) | Every tap, with no clips on JST contacts |
| Series resistors | RS1–RS8 (22 kΩ) and the ÷4 dividers | Safe, non-loading taps (§5.4, §5.5) |
| Multimeter | Any DMM | Continuity, static voltages (#6, #10) |

---

## 3. Power Supply

In Phase 0/1 the logger is powered **from USB-C only** (§5.3). The buck
converter described here is first used on the passive tap board (PR-04a) and
then on the intercept board.

### 3.1 Source

The bike's 9V DC power supply (YINGJIAO YS25-0903000, 9V/3A) enters the
frame and travels up to the console via the 9-pin loom (White = +9V,
Brown = GND). The passive tap and intercept boards tap this 9V rail.

### 3.2 Regulation

A GTIWUNG MP1584EN adjustable buck converter steps 9V down to 3.3V.
Whether the Waveshare board's VIN pin can take 9V directly (removing the
external buck for the SoC) is Phase 0 register row 11, decided at DR-1.

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
> trimpot until it reads **3.3V ± 0.05V**. Then connect loads. The ESP32-S3
> absolute maximum is 3.6V; on a vibrating bike, fix the trimpot (glue) or
> replace it with fixed resistors `[DR-1]` (audit H-03).

### 3.3 Rail Distribution

On the passive tap board, the 3.3V rail powers only U1. On the intercept
board it is distributed as follows (**PROVISIONAL `[DR-1]`**):

```
3.3V RAIL ──┬──► U1 ESP32-S3-Nano pin 2 (3V3) — direct to SoC
            │
            ├──► SW1 ─► R3 (10kΩ) ──► Cadence pull-up (standalone only)
            │
            ├──► SW2 ─► R4 (100Ω) ──► Pot reference (standalone only)
            │
            └──► SW4 ─► R5 (10Ω) ──► Encoder power (standalone only)

MOTOR RAIL [DR-1: D3] ──► U2 motor driver supply (separate from the SoC rail)
```

### 3.4 Current Budget `[DR-1]`

| Consumer | Typical | Peak | Duty | Configuration |
|---|---|---|---|---|
| ESP32-S3 (WiFi + BLE active) | 200 mA | 500 mA | Continuous | All |
| Motor driver + motor | `[DR-1: register row 2]` | `[DR-1: register row 2]` | Intermittent `[DR-1: register row 3]` | Intercept only |
| Sensors + pull-ups | 5 mA | 5 mA | Continuous | Intercept, standalone only |
| **Total** | `[DR-1]` | `[DR-1]` | — | — |

The motor's current (start inrush and running current with the worm under
brake load) has not been measured; the 800 mA / ~2 s figure is an
assumption. Rail sizing is a DR-1 output (audit decision D3): the
recommendation is a **separate motor rail**, likely with a **MOSFET H-bridge
driver** (current limit and fault output, e.g. DRV8833 or TB6612) rather than
the L9110S on the SoC's 3.3V rail, plus bulk capacitance near the SoC
(audit H-03).

### 3.5 USB-C Power and Programming

- **Phase 0/1 (breadboard observer) and Phase 2 (HR-only):** USB-C is the
  only power source. There is no buck converter to disconnect, and the
  logger can be flashed at any time without rewiring.
- **Buck-powered boards (passive tap and intercept):** the USB port feeds
  the ESP32's VBUS, and the onboard LDO produces a second 3.3V source on the
  same rail.

> [!WARNING]
> On a buck-powered board, **disconnect the buck converter's output** (or
> its 9V input) before plugging in USB-C. Two regulators on one 3.3V rail can
> cause droop or overshoot. Routine updates on installed boards use **manual
> OTA** (enabled and tested in PR-04a), so USB is a recovery path only.

---

## 4. Connectors

### 4.1 Connector Table

| Ref | Type | Side | Mates With | Configuration | Purpose |
|---|---|---|---|---|---|
| **H1** | JST-SM 9-pin pair | Loom | Bike 9-pin plug and console 9-pin socket | 0/1 | Y / back-probe harness (main loom) |
| **H2** | JST 3-pin pair | Loom | Encoder 3-pin plug and console 3-pin socket | 0/1 | Y / back-probe harness (encoder loom) |
| **J1** | JST-SM 9-pin socket | Bike frame | 9-pin plug from lower wiring hub | Passive, Intercept | All bike-side signals |
| **J2** | JST-SM 9-pin plug | Console | 9-pin socket on console PCB | Passive, Intercept | Pass-through to console |
| **J3** | JST 3-pin socket | Bike frame | 3-pin plug from resistance knob | Passive, Intercept | Encoder signals |
| **J4** | JST 3-pin plug | Console | 3-pin socket on console PCB | Passive, Intercept | Encoder pass-through |
| **J5** | 30-pin female headers ×2 | Board | ESP32-S3-Nano pin headers | Passive, Intercept | ESP32 carrier socket |
| **J6** | 2-pin screw terminal | Motor | Hookup wire to motor driver output | Intercept | Motor connection |

> [!NOTE]
> In Phase 0/1 there are no soldered boards. All taps come from the H1/H2
> Y / back-probe harness (§5.1.1). Never clip onto JST-SM contacts.

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

The cadence colour mapping is unconfirmed (see
[`ex3_specification.md`](ex3_specification.md) §7 vs §8); Phase 0 register
row 7 resolves it.

### 4.3 J3 — Encoder Loom (Knob Side, 3-Pin JST)

| Pin | Wire Colour | Signal Name | Function |
|---|---|---|---|
| 1 | Red | ENC_VCC | Encoder common (3.3V reference) |
| 2 | Brown | ENC_CHA | Encoder Channel A (quadrature) |
| 3 | Orange | ENC_CHB | Encoder Channel B (quadrature) |

---

## 5. Phase 0/1 — Breadboard Observer (PR-01, PR-02)

> **Goal:** Read every signal on the bike without changing anything.
> The bike operates normally; the taps are high-impedance and the logger
> cannot back-power the console's lines.

### 5.1 What You Need

| Ref | Item | Qty |
|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 |
| — | USB-C cable + power bank (or a laptop on battery) | 1 |
| H1 | JST-SM 9-pin mating pair → Y / back-probe harness | 1 pair |
| H2 | JST 3-pin mating pair → Y / back-probe harness | 1 pair |
| R1a, R2a | 30 kΩ resistor | 2 |
| R1b, R2b | 10 kΩ resistor | 2 |
| RS1–RS8 | 22 kΩ resistor (10–47 kΩ acceptable) | 8 |
| C2 | 100 nF capacitor (optional, wiper noise) | 1 |
| — | Breadboard, hookup wire | 1 |

Plus the bench equipment in §2.1. No buck converter, no L9110S, no
R3/R4/R5. The console provides all sensor power — the logger only observes.

#### 5.1.1 Y / Back-Probe Harness

Build the harness from the H1 and H2 mating pairs so that it plugs in
between the bike looms and the console exactly as the J1–J4 boards will:

- Every conductor passes straight through, plug to socket (9 + 3 wires).
- Each of the 10 tap conductors has a short tail to the breadboard.
  White (+9V) and Brown (GND) also get tails: Brown is the logger ground;
  White is for scope probing only and **never** goes to a GPIO.
- Leave an insertion point on the Red or Black motor conductor for the
  current shunt (register row 2), or enough slack for a DC clamp.
- Label every tail with its colour and loom (9-pin or 3-pin); the encoder
  loom reuses Red, Brown and Orange.

### 5.2 Motor-Drive Characterisation Gate

> [!CAUTION]
> **Complete this before connecting the motor-sense taps (GPIO 6/7).** The
> other taps may be connected immediately. This gate **characterises** the
> drive; it does not expect a particular answer.

A DMM measured **3.3V** on the motor wires, but a DMM shows the *average*
voltage — it cannot distinguish 3.3V DC from 9V PWM at ~37% duty cycle.
The ÷4 divider (§5.4) keeps the ESP32 pin below 3.3V for any drive up to
about 13V, so **either result lets logging proceed**. The captures are
evidence for Phase 0 register row 1 and feed DR-1 (sense network, relay
logic, motor rail).

**Setup:** DC coupling, 5V/div. Scope ground to Brown (via the harness).
CH1 on Red (motor +), CH2 on Black (motor −). If the scope has math, also
display CH1 − CH2.

**Captures (save each as an evidence file, §5.2.2):**

| # | Condition | Timebase | What to record |
|---|---|---|---|
| 1 | At rest (no knob movement, bike powered) | 10 ms/div | Both wires: driven low (brake), floating (coast), or other |
| 2 | Knob one click **up** | 10 ms/div | Drive duration, which wire is high, levels |
| 3 | Knob one click **down** | 10 ms/div | As #2, opposite direction |
| 4 | Knob up, during drive | µs/div (e.g. 10 µs/div) | Flat DC, or PWM (frequency, duty, peak) |
| 5 | Knob down, during drive | µs/div | As #4 |
| 6 | Drive end (either direction) | 10 ms/div, then µs/div | Braking, coasting, back-EMF excursions below GND |

```
Flat DC drive:                      PWM drive:
    ┌────┐                              ┌┐ ┌┐ ┌┐ ┌┐
 V  │    │                           V  ││ ││ ││ ││
    │    │                              ││ ││ ││ ││
────┘    └──────────── 0V          ─────┘└─┘└─┘└─┘└──── 0V
```

**Outcomes:**

- **Flat DC (≈3.3V):** the divider output is ≈0.825V, at the ESP32-S3's
  V_IL maximum — not a valid logic HIGH. Motor sense is sampled as analog on
  ADC1.
- **PWM (any amplitude up to ~13V):** the divider still protects the pin
  (9V ÷ 4 = 2.25V). Record frequency, duty and peak; burst capture
  (1–5 kHz) and threshold-based drive detection handle it in firmware.
- **Peak above ~12V (not expected):** stop, and resize the divider before
  connecting GPIO 6/7.

#### 5.2.1 Motor Current (register row 2)

With the shunt (or clamp) in the motor lead, capture start inrush and
running current in both directions, including a move toward cam-max with
the brake loaded. Use single-shot triggering on the drive onset.

#### 5.2.2 Evidence Files

Store captures under `docs/measurements/phase0/` using the naming
convention in the [Phase 0 register](measurements/phase0/README.md).

### 5.3 Schematic

The pin assignments below mirror the Phase 0/1 pin map, whose authoritative
copy is SPEC §4.4.1 ([`../SPEC.md`](../SPEC.md)). If the two ever differ,
SPEC §4.4.1 wins and this section is corrected.

| GPIO | Header | Signal | Tap network |
|---|---|---|---|
| 1 | A0 | Blue: pot wiper | RS1 22 kΩ series (+ optional C2 on the ESP32 side) |
| 2 | A1 | Yellow: pot outer HI | RS2 22 kΩ series |
| 3 | A2 | Green: pot outer LO | RS3 22 kΩ series |
| 4 | A3 | Grey: cadence return | RS4 22 kΩ series |
| 5 | D2 | Orange: cadence | RS5 22 kΩ series |
| 6 | D3 | Red: motor + | 30 kΩ / 10 kΩ ÷4 divider (R1a/R1b) |
| 7 | D4 | Black: motor − | 30 kΩ / 10 kΩ ÷4 divider (R2a/R2b) |
| 8 | D5 | Encoder Ch A (Brown) | RS6 22 kΩ series |
| 9 | D6 | Encoder Ch B (Orange) | RS7 22 kΩ series |
| 10 | D7 | Encoder VCC (Red) | RS8 22 kΩ series |

These are all ten ADC1 channels (ADC2 is unreliable while WiFi is on).
There are **10 tap signals**; White (+9V) and Brown (GND) are power, not
taps. Every tap pin is configured with **no internal pull-up or
pull-down**.

```
═══════════════════════════════════════════════════════════════════════════════
           PHASE 0/1 — BREADBOARD OBSERVER SCHEMATIC
═══════════════════════════════════════════════════════════════════════════════

POWER
─────

Laptop (on battery) or power bank ── USB-C ──► U1 ESP32 (VBUS → onboard LDO)
Brown wire (9-pin, GND) ── harness tail ─────► U1 ESP32 GND   (COMMON GND)
White wire (9-pin, +9V) ── harness tail ─────► scope probe only, never a GPIO


MOTOR SENSE (9-pin loom — AFTER the characterisation gate, §5.2)
────────────────────────────────────────────────────────────────

Red wire (pin 1, motor+) ── tail ──► R1a (30kΩ) ──┬──► U1 GPIO 6 (D3, ADC1)
                                                   │
                                               R1b (10kΩ)
                                                   │
                                                  GND

Black wire (pin 2, motor−) ── tail ──► R2a (30kΩ) ──┬──► U1 GPIO 7 (D4, ADC1)
                                                     │
                                                 R2b (10kΩ)
                                                     │
                                                    GND


SENSOR TAPS (9-pin loom — 22kΩ series resistor on every direct tap)
───────────────────────────────────────────────────────────────────

Blue wire   (pin 4, pot wiper)      ── tail ── RS1 22k ──┬──► U1 GPIO 1 (A0, ADC1)
                                                         └── C2 (100nF) ── GND
                                                              (optional)
Yellow wire (pin 3, pot outer HI)   ── tail ── RS2 22k ─────► U1 GPIO 2 (A1, ADC1)
Green wire  (pin 5, pot outer LO)   ── tail ── RS3 22k ─────► U1 GPIO 3 (A2, ADC1)
Grey wire   (pin 7, cadence return) ── tail ── RS4 22k ─────► U1 GPIO 4 (A3, ADC1)
Orange wire (pin 6, cadence)        ── tail ── RS5 22k ─────► U1 GPIO 5 (D2, ADC1 / edge)


ENCODER TAPS (3-pin loom — 22kΩ series resistor on every direct tap)
────────────────────────────────────────────────────────────────────

Enc Brown wire  (pin 2, Ch A) ── tail ── RS6 22k ──► U1 GPIO 8  (D5, ADC1 / edge)
Enc Orange wire (pin 3, Ch B) ── tail ── RS7 22k ──► U1 GPIO 9  (D6, ADC1 / edge)
Enc Red wire    (pin 1, VCC)  ── tail ── RS8 22k ──► U1 GPIO 10 (D7, ADC1)

All tap pins: no internal pull-up, no internal pull-down.
═══════════════════════════════════════════════════════════════════════════════
```

### 5.4 Motor Sense Voltage Dividers (R1a/R1b, R2a/R2b)

The motor is driven bidirectionally by the console's H-bridge — Red goes HIGH
while Black goes LOW (forward), then the reverse for backward. Back-EMF when
the motor stops can also push wires below ground.

The 30kΩ + 10kΩ divider is **protection and scaling only**, not a
logic-level interface:

```
Motor wire ──► R1a/R2a (30kΩ) ──┬──► GPIO (reads divided voltage, ADC1)
                                │
                            R1b/R2b (10kΩ)
                                │
                               GND

Division ratio: 10k / (30k + 10k) = 1/4   → pin stays ≤ 3.3V up to ~13V drive
```

| Motor wire voltage | GPIO sees | Current through divider | Safe? |
|---|---|---|---|
| 3.3V (DC drive) | 0.825V | 0.08 mA | ⚠️ At V_IL max. Read as analog. |
| 0V (idle) | 0V | 0 mA | ✅ Clean LOW |
| 9V (if PWM) | 2.25V | 0.23 mA | ✅ Below 3.3V max |
| −5V (back-EMF spike) | −1.25V | 0.17 mA | ✅ Current-limited by 30kΩ into the ESD diode |

The 30kΩ series resistance limits current to <0.3 mA under any plausible
condition, so these two taps need no extra series resistor. Cost: 4
resistors, ~£0.10.

> [!NOTE]
> Sampling: the PR-01 SignalLogger uses the ESP-IDF **ADC continuous (DMA)**
> driver on all ten ADC1 channels — a decimated ride log (min/mean/max at
> 10–20 Hz plus a µs-timestamped event log) and triggered 1–5 kHz burst
> captures. See [`implementation-strategy.md`](implementation-strategy.md)
> Phase 0. Phase 3+ sampling is sized at DR-1 from the §5.2 captures.

### 5.5 Direct-Tap Series Resistors (RS1–RS8) and the No-Pulls Rule

The eight direct taps (every tap except GPIO 6/7) each have a **4.7 kΩ series
resistor**, placed at the harness end of the tail.

> [!IMPORTANT]
> **Bench-tested 2026-10-03:** The original 22 kΩ value exceeded the ESP32-S3
> ADC continuous-mode source-impedance limit (Espressif recommends < 10 kΩ).
> At 83 kHz aggregate sample rate the S/H capacitor could not settle through
> 22 kΩ during brief reed-switch closures (~50 ms). Replaced with 4.7 kΩ +
> 100 nF ceramic cap on the ESP32 side. Cadence detection confirmed at
> 74–120 RPM.

**Why — back-powering.** Each ESP32 pad has ESD clamp diodes to its 3.3V
supply. If the ESP32 is unpowered (USB unplugged, or during boot/brown-out)
while the console is on, the console's 3.3V lines — pot reference (Yellow),
wiper (Blue), encoder VCC and channels, cadence — would drive current
through those diodes into the ESP32's rail and try to power it. That loads
the pot reference and the encoder supply, which are exactly what register
rows 4, 6, 8 and 10 measure, and can disturb the console. 4.7 kΩ limits any
such current to about 0.7 mA, which is still low enough to avoid measurably
loading the console's lines.

**No pulls.** Every tap pin is configured with **no internal pull-up or
pull-down**. An internal pull (~45 kΩ) on a cadence or encoder pin would bias
the console's own line and change what is being measured.

**Decoupling capacitor (C1).** A 100 nF ceramic capacitor is placed between
each ADC signal pin and GND on the ESP32 side of the series resistor. This
provides a low-impedance charge reservoir for the ADC's sample-and-hold
capacitor, ensuring accurate readings even with the 4.7 kΩ source impedance.

### 5.6 Phase 0 Observation-Only Taps

These taps exist to characterise the bike's signals. Whether each one
survives onto the Phase 3 boards is a DR-1 decision.

| GPIO | Signal | What We're Measuring | Register row |
|---|---|---|---|
| GPIO 2 (A1) | Yellow — pot outer HI | Console's pot reference voltage and stiffness | #4, #6 |
| GPIO 3 (A2) | Green — pot outer LO | Console's pot low reference (GND?) | #4, #6 |
| GPIO 4 (A3) | Grey — cadence return | Reed switch circuit return path | #7 |
| GPIO 10 (D7) | Encoder VCC (Red, 3-pin) | Console's encoder supply voltage | #8, #10 |

These results let DR-1 design the standalone supplies (R3, R4, R5, the
Green–GND link) with evidence instead of assumptions.

### 5.7 Build Checklist

- [ ] **Bench ready** (§2.1): scope (≥2 ch, single-shot), motor-current
      measurement, Y / back-probe harness, series resistors
- [ ] Build the H1/H2 Y / back-probe harness (§5.1.1); verify continuity
      plug-to-socket on all 12 conductors
- [ ] Set up the breadboard with the ESP32-S3-Nano; power it from USB-C
      (power bank or laptop on battery)
- [ ] Tie ESP32 GND to the Brown tail
- [ ] Fit RS1–RS8 (22 kΩ) in series with every direct tap
- [ ] Build the motor-sense dividers (R1a/R1b, R2a/R2b)
- [ ] Flash Phase 0 firmware via USB-C; confirm every tap pin is configured
      with no internal pulls
- [ ] **GATE: motor-drive characterisation** (§5.2) — all six captures saved
- [ ] Connect the motor-sense tails (Red → R1a, Black → R2a)
- [ ] Connect the sensor and encoder tails
- [ ] Verify all **10 tap signals** reading on the web dashboard

### 5.8 Test Points

Expected readings are the current assumptions; the Phase 0 register records
what is actually measured.

| Signal | GPIO | Expected Reading | Register row |
|---|---|---|---|
| Pot wiper | GPIO 1 | ~1160–1700 mV (varies with resistance position) | #4 |
| Pot outer HI | GPIO 2 | ~3300 mV (console reference) | #6 |
| Pot outer LO | GPIO 3 | ~0 mV (console ground) | #6 |
| Cadence return | GPIO 4 | TBD | #7 |
| Cadence | GPIO 5 | 3.3V idle, 0V on magnet pulse | #7 |
| Motor+ sense | GPIO 6 | 0V idle; ≈0.825V if 3.3V DC drive forward, PWM otherwise | #1 |
| Motor− sense | GPIO 7 | 0V idle; ≈0.825V if 3.3V DC drive reverse, PWM otherwise | #1 |
| Encoder Ch A | GPIO 8 | Toggles 0 ↔ 3.3V per click | #8 |
| Encoder Ch B | GPIO 9 | Brief ~1.5–2V pulse per click (below V_IH; capture as analog) | #8 |
| Encoder VCC | GPIO 10 | ~3300 mV (console supply) | #8, #10 |

---

## 6. Phases 2–4 — Non-Intercept Configurations

### 6.1 Phase 2 — HR-Only (USB) (PR-03)

> **Goal:** Pair an HR armband and re-broadcast to the Echelon app.
> No bike wiring needed — this is purely BLE.

| Ref | Item | Qty |
|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 |
| — | USB-C cable | 1 |

The ESP32 is powered via USB-C. No buck converter, no harness, no
breadboard circuits.

> [!TIP]
> Phase 2 can be developed in parallel with Phase 1 since it needs no bike
> hardware at all.

### 6.2 Phase 3 — Passive Tap Board (PR-04a) `[DR-1]`

> **Status: placeholder.** This section is written at DR-1 from Phase 0
> evidence. Nothing here is built before DR-1 is approved.

> **Goal:** A permanent, pass-through-only board installed in the console
> housing. It delivers UC1–UC3 (telemetry, HR relay, recording) as the
> default daily-use product and hosts PR-05 and PR-06.

#### 6.2.1 Fixed Design Intent

- **J1–J4 pass-through.** All 9 main-loom and all 3 encoder-loom conductors
  run straight through, J1→J2 and J3→J4. **No motor split** — the console
  drives the motor exactly as stock.
- **High-impedance taps only.** Series-resistor taps and the ÷4 motor-sense
  dividers, as in Phase 0 (§5.4, §5.5). No pull-ups, pull-downs, reference
  supplies or Green–GND link: the board never sources current into a bike
  line.
- **Powered from the buck** (MP1584EN, from White/Brown via J1, §3), not USB.
- **Manual OTA** enabled and tested before the board is sealed in the
  housing; `autoCheck` is forced to `false` after config load and after
  factory reset until framework #226 lands.
- **Pin map:** the sense-only subset of the provisional Phase 3+ map (§7.8):
  wiper GPIO 1, cadence GPIO 5, motor sense +/− GPIO 6/7, encoder A/B
  GPIO 8/9. No encoder-VCC sense. GPIO 10/17 are unused on this board.

#### 6.2.2 Decisions DR-1 Must Make

| # | Decision | Evidence |
|---|---|---|
| 1 | Which Phase 0 observation taps (Yellow, Green, Grey, encoder VCC) are kept | Register #4, #6, #7, #8 |
| 2 | Series-resistor value per tap, and whether C1/C2 filtering is fitted | WO0 spike (ADC settling), register #4, #7 |
| 3 | Motor-sense sampling scheme (analog level vs PWM detection) and rate | Register #1, §5.2 captures |
| 4 | Encoder Ch B conditioning (the pulse is below V_IH) and whether `EncoderModule` (PCNT) can decode it | WO0 spike Q3, register #8 |
| 5 | SoC supply: external buck, or 9V direct to the board's VIN; trimpot fixed or replaced | Register #11, audit H-03 |
| 6 | Behaviour at power-on: boot-order back-power risk and the console's start-up sweep | Register #13 |
| 7 | Bulk capacitance and brown-out margin on the SoC rail | Register #2 (if shared supply), audit H-03 |
| 8 | Board size, placement and cable routing in the console housing | Physical survey |
| 9 | Final BOM, schematic, net list, test points and assembly steps for this section | All of the above |

#### 6.2.3 Verification (from PR-04a)

- A stock ride with the board fitted is indistinguishable from no board.
- All sensor taps read correctly.
- The board fits in the console housing.
- Manual OTA succeeds with the board installed.

---

## 7. Phase 5+ — Intercept Board (PR-07)

> [!CAUTION]
> **PROVISIONAL — superseded at DR-1, do not build.** Everything in §7 and
> §8 is a pre-measurement design. DR-1 rewrites it from Phase 0 evidence
> (motor driver and rail — decision D3; standalone supplies — audit H-02;
> fail-safe — audit H-05). No part of it is soldered before DR-1 is approved.

> **Goal:** The intercept board sits between the bike frame and the Echelon
> console and splits the motor wires (MITM), adding Override and ERG.

### 7.1 What You Need (Additions to the Passive Tap Board)

| Ref | Item | Qty | Purpose |
|---|---|---|---|
| U2 | L9110S motor driver module `[DR-1: D3]` | 1 | Drives the resistance motor |
| J6 | 2-pin screw terminal | 1 | Motor output connection |
| R3 | 10kΩ resistor | 1 | Cadence pull-up (standalone only) |
| R4 | 100Ω resistor | 1 | Pot reference (standalone only) |
| R5 | 10Ω resistor | 1 | Encoder power (standalone only) |
| R6, R7 | 10kΩ resistor | 2 | Pull-downs on A-IA / A-IB: driver held off through reset and boot |
| SW1–SW4 | Jumpers (minimum) or high-side / analog switches | 4 | Switch R3, R4, the Green–GND link and R5 |
| C1 | 100 nF capacitor | 1 | Cadence debounce |
| C2 | 100 nF capacitor | 1 | Pot ADC filter |

**What changes from the passive tap board:**
- Add the motor driver and a motor rail `[DR-1: D3]`
- **Split the motor wires** (J1 pins 1–2 no longer connect to J2 pins 1–2)
- Add switchable standalone supplies (R3, R4, R5 and the Green–GND link via
  SW1–SW4), **open by default**
- Add motor drive outputs on **GPIO 10 / GPIO 17** (D7 / D8)
- GPIO 6/7 stay **sense-only** (console motor sense). GPIO 6/7 are never
  both driver outputs and sense inputs.

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
│              │  Buck Converter│  (+ motor rail [DR-1])         │
│              └───────────────┘                                 │
└──────────────────────────────────────────────────────────────┘
```

**No factory wires are cut.** The board plugs into existing JST connectors.
Removing the board restores the bike to stock.

### 7.3 Motor Circuit — The MITM

On this board the motor is driven by the ESP32's motor driver, never
directly by the Echelon console.

#### 7.3.1 Signal Routing

```
STOCK BIKE (no board):
    Console motor output ──────────────────────────── Motor
    (Red/Black on 9-pin)                              (in actuator hub)


WITH INTERCEPT BOARD:
    Console motor output (J2 pins 1/2)
        ──► R1a/R2a (30kΩ) ──┬──► ESP32 GPIO 6 / GPIO 7 (ADC1, SENSE ONLY)
                             │
                         R1b/R2b (10kΩ)
                             │
                            GND
        (read, but NOT connected to the motor)

    ESP32 GPIO 10 / GPIO 17 (D7 / D8, digital outputs)
        ──► L9110S A-IA / A-IB (control inputs)
                    │
                    ▼
            L9110S A-OA / A-OB (power outputs)
                    │
                    ▼
            Motor (J1 pins 1/2, in actuator hub)
```

#### 7.3.2 Why the Motor Wires Are Split

In the stock bike, the console drives the motor directly via pins 1–2
of the 9-pin loom. The intercept board breaks this connection:

- **Bike side** (J1 pins 1–2): Motor wires connect to **L9110S outputs**
  (A-OA, A-OB), not to J2.
- **Console side** (J2 pins 1–2): Console's motor drive signals connect to
  **ESP32 ADC1 inputs** (GPIO 6/7) via **voltage dividers** (R1a/R1b,
  R2a/R2b).

The console *thinks* it's driving the motor. Actually, its motor signals are
read by the ESP32, which decides whether to relay them (Intercept Mode) or
ignore them (Override Mode).

#### 7.3.3 Console Motor Sense — Voltage Dividers (R1a/R1b, R2a/R2b)

The same 30kΩ/10kΩ ÷4 divider as Phase 0 (§5.4) scales the console's motor
drive for GPIO 6/7. It is not a pull-down and not a logic-level interface;
the sensed voltage is read as analog on ADC1.

Two additional benefits on the intercept board:

1. **Defined idle state**: the divider's R1b/R2b (10kΩ to GND) hold GPIO 6/7
   at 0V when the console is not driving.

2. **Clean disconnect**: In standalone Override (UC6), the console JST is
   unplugged and R1b/R2b hold GPIO 6/7 at GND — no floating inputs.

> [!WARNING]
> **Fail-safe requirement:** In Intercept Mode, if the ESP32 crashes, the motor is orphaned. A fail-safe mechanism (e.g. NC relay or analog switch bypass) is recommended. Strategy deferred to `[DR-1: decision H-05]`.

#### 7.3.4 L9110S Truth Table

| GPIO 10 (A-IA) | GPIO 17 (A-IB) | L9110S A-OA | L9110S A-OB | Motor |
|---|---|---|---|---|
| LOW | LOW | LOW | LOW | **Stopped** (brake) |
| HIGH | LOW | HIGH | LOW | **Forward** (increase resistance) |
| LOW | HIGH | LOW | HIGH | **Reverse** (decrease resistance) |
| HIGH | HIGH | HIGH | HIGH | **Stopped** (not used) |

Driver inputs must be LOW through reset and boot (audit M-01); R6 and R7
(10kΩ to GND on A-IA / A-IB) hold them there. The driver choice itself is
`[DR-1: D3]`.

### 7.4 Sensor Circuits

The standalone supplies — R3, R4, R5 and the Green–GND link — are each
behind a switch (SW1–SW4). **Default: all open.** They are closed only in
standalone Override with the console disconnected (UC6). Minimum
implementation is jumpers; preferred is a high-side or analog switch under
ESP32 control. Values and switch type are `[DR-1: decision H-02]`.

| Switch | Connects | Default | Closed when |
|---|---|---|---|
| SW1 | R3 (10kΩ) cadence pull-up to 3V3 | Open | Standalone Override (console disconnected) |
| SW2 | R4 (100Ω) 3V3 to POT_REF_HI (Yellow) | Open | Standalone Override (console disconnected) |
| SW3 | POT_REF_LO (Green) to GND | Open | Standalone Override (console disconnected) |
| SW4 | R5 (10Ω) 3V3 to ENC_VCC | Open | Standalone Override (console disconnected) |

#### 7.4.1 Cadence Sensor

```
                    ┌──── SW1 ── R3 (10kΩ) ──── 3V3 RAIL
                    │
J1 Pin 6 (Orange) ──┼──── C1 (100nF) ──── GND
  CADENCE           │
                    ├──── U1 GPIO 5 (INPUT, interrupt FALLING)
                    │
                    └──── J2 Pin 6 (pass-through to console)
```

The reed switch is a normally-open contact. When the pedal magnet passes:
- Switch closes → CADENCE pulled to GND → GPIO 5 reads LOW (FALLING edge)
- Switch opens → the pull-up returns CADENCE to 3.3V → GPIO 5 reads HIGH

With the console connected, the console supplies the pull-up and SW1 is
open. With the console disconnected (UC6), SW1 closes and R3 provides the
pull-up. Whether the console's pull-up and R3 may safely coexist is register
row 7 / DR-1.

**C1 (100 nF)** provides hardware debounce, filtering contact bounce from the
reed switch, together with the software lockout.

#### 7.4.2 Resistance Potentiometer

```
3V3 RAIL ── SW2 ── R4 (100Ω) ──┐
                                │
J1 Pin 3 (Yellow) ──────────────┼──── J2 Pin 3 (pass-through to console)
  POT_REF_HI                    │
                                │
              ┌──────── POT ────────┐
              │     (inside hub)    │
              │                     │
J1 Pin 4 (Blue) ──┬──── C2 (100nF) ──── GND
  POT_WIPER       │
                  ├──── U1 GPIO 1 (ADC1_CH0, 12-bit, 11dB atten)
                  │
                  └──── J2 Pin 4 (pass-through to console)

J1 Pin 5 (Green) ───────┬──── J2 Pin 5 (pass-through to console)
  POT_REF_LO            │
                        └──── SW3 ──── GND
```

**Pot reference by scenario:**

| Scenario | SW2 / SW3 | POT_REF_HI Source | POT_REF_LO | Wiper Range |
|---|---|---|---|---|
| **Console connected** | Open / Open | Console (via J2) | Console (via J2) | ~1160–1700 mV (register #4) |
| **Console disconnected** | Closed / Closed | 3V3 rail via R4 (100Ω) | GND via SW3 | May differ — **recalibrate** |

The teardown measured 1.16–1.7V *across* Yellow and Green, which suggests
the console biases the pot. Driving Yellow or grounding Green while the
console is connected could change what the console reads and break UC1
("identical to stock"); that is why SW2 and SW3 are open by default
(audit H-02, register #6).

**C2 (100 nF)** filters ADC noise on the wiper signal.

> [!IMPORTANT]
> When switching between console connected and disconnected, the pot
> reference changes. **Run calibration** via the portal after changing the
> console connection state.

#### 7.4.3 Rotary Encoder (Resistance Knob)

```
3V3 RAIL ── SW4 ── R5 (10Ω) ──┐
                               │
J3 Pin 1 (Red) ────────────────┼──── J4 Pin 1 (pass-through to console)
  ENC_VCC

J3 Pin 2 (Brown) ──────────────┬──── J4 Pin 2 (pass-through to console)
  ENC_CHA                      │
                               └──── U1 GPIO 8 (INPUT, PCNT / interrupt)

J3 Pin 3 (Orange) ─────────────┬──── J4 Pin 3 (pass-through to console)
  ENC_CHB                      │
                               └──── U1 GPIO 9 (INPUT, PCNT / interrupt)
```

**R5 (10Ω)** feeds the encoder only in standalone mode (SW4 closed). With
the console connected SW4 is open, so there is no rail-to-rail link and no
back-feed path into the console's 3.3V rail (register #10).

> [!NOTE]
> In standalone mode ENC_A and ENC_B need pull-downs, or they float once the
> console is unplugged. Use the ESP32's internal pull-downs, enabled only in
> standalone Override, or external parts switched with SW4 `[DR-1]`.
> Decoding uses the framework `EncoderModule` (PCNT); Ch B conditioning is
> `[DR-1]` (register #8).

### 7.5 Standalone Operation (Console Disconnected)

When the Echelon console is disconnected (UC6), the sensors lose the
console's excitation. The intercept board provides switchable replacements:

| Sensor | Stock Power Source | Intercept Board Replacement | Switch | Default |
|---|---|---|---|---|
| Cadence reed switch | Console pull-up | R3: 10kΩ pull-up from 3V3 | SW1 | Open; closed in standalone only |
| Pot reference (HI) | Console | R4: 100Ω from 3V3 to POT_REF_HI | SW2 | Open; closed in standalone only |
| Pot reference (LO) | Console | POT_REF_LO to GND | SW3 | Open; closed in standalone only |
| Encoder | Console 3.3V | R5: 10Ω from 3V3 to ENC_VCC | SW4 | Open; closed in standalone only |

Values, switch type and whether any path may stay closed with the console
connected are `[DR-1: decision H-02]`, from register rows 6, 7 and 10.

### 7.6 Complete Schematic

```
═══════════════════════════════════════════════════════════════════════════════
     PHASE 5+ — INTERCEPT BOARD COMPLETE SCHEMATIC (PROVISIONAL [DR-1])
═══════════════════════════════════════════════════════════════════════════════

J1 (9-PIN JST-SM, BIKE SIDE)                 J2 (9-PIN JST-SM, CONSOLE SIDE)
┌──────────────────────────┐                 ┌──────────────────────────┐
│ 1  Red    MOTOR_POS ─────┼──► U2 A-OA     │ 1  Red  ──► R1a (30k) ──┤
│   (to L9110S output)     │                 │           ──┬── GPIO 6 │
│                          │                 │           R1b (10k)     │
│                          │                 │             └── GND     │
│ 2  Black  MOTOR_NEG ─────┼──► U2 A-OB     │ 2  Black ──► R2a (30k) ──┤
│   (to L9110S output)     │                 │           ──┬── GPIO 7 │
│                          │                 │           R2b (10k)     │
│                          │                 │             └── GND     │
│ 3  Yellow POT_REF_HI ────┼── pass-thru ───┼── 3                     │
│     └── R4 (100Ω) ◄─SW2─ 3V3             │                         │
│ 4  Blue   POT_WIPER ─────┼── pass-thru ───┼── 4                     │
│     ├── C2 (100nF) ── GND│                 │                         │
│     └── GPIO 1 (ADC)     │                 │                         │
│ 5  Green  POT_REF_LO ────┼── pass-thru ───┼── 5                     │
│     └── SW3 ── GND       │                 │                         │
│ 6  Orange CADENCE ───────┼── pass-thru ───┼── 6                     │
│     ├── R3 (10k) ◄─SW1─ 3V3              │                         │
│     ├── C1 (100nF) ── GND│                 │                         │
│     └── GPIO 5 (INT)     │                 │                         │
│ 7  Grey   CADENCE_RTN ───┼── pass-thru ───┼── 7                     │
│ 8  White  V9_SUPPLY ─────┼── pass-thru ───┼── 8                     │
│     └── U3 VIN           │                 │                         │
│ 9  Brown  GND ───────────┼── pass-thru ───┼── 9                     │
│     └── COMMON GND       │                 │                         │
└──────────────────────────┘                 └──────────────────────────┘
  SW1–SW4: open by default; closed only in standalone Override [DR-1: H-02]


J3 (3-PIN JST, ENCODER)                     J4 (3-PIN JST, CONSOLE)
┌──────────────────────────┐                 ┌──────────────────────────┐
│ 1  Red    ENC_VCC ───────┼── pass-thru ───┼── 1                     │
│     └── R5 (10Ω) ◄─SW4─ 3V3              │                         │
│ 2  Brown  ENC_CHA ───────┼── pass-thru ───┼── 2                     │
│     └── GPIO 8 (INT)     │                 │                         │
│ 3  Orange ENC_CHB ───────┼── pass-thru ───┼── 3                     │
│     └── GPIO 9 (INT)     │                 │                         │
└──────────────────────────┘                 └──────────────────────────┘


U1 — WAVESHARE ESP32-S3-NANO                U2 — L9110S H-BRIDGE [DR-1: D3]
┌──────────────────────────────┐            ┌───────────────────────┐
│                              │            │                       │
│  Pin 2 (3V3) ◄── 3V3 RAIL   │            │  VCC ◄── MOTOR RAIL   │
│  GND ◄── COMMON GND         │            │         [DR-1: D3]    │
│                              │            │  GND ◄── COMMON GND  │
│  GPIO 1  (A0) ◄── POT_WIPER │            │                       │
│  GPIO 5  (D2) ◄── CADENCE   │            │  A-IA ◄── GPIO 10    │
│  GPIO 6  (D3) ◄── CONS_M+   │            │  A-IB ◄── GPIO 17    │
│  GPIO 7  (D4) ◄── CONS_M−   │            │                       │
│  GPIO 8  (D5) ◄── ENC_CHA   │            │  A-OA ──► MOTOR_POS  │
│  GPIO 9  (D6) ◄── ENC_CHB   │            │  A-OB ──► MOTOR_NEG  │
│  GPIO 10 (D7) ──► L9110S_IA │            └───────────────────────┘
│  GPIO 17 (D8) ──► L9110S_IB │
│                              │            U3 — MP1584EN BUCK CONVERTER
│  USB-C (recovery only)       │            ┌───────────────────────┐
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
| **V9_SUPPLY** | J1 pin 8 (White) | J2 pin 8 (pass-through), U3 VIN, motor rail `[DR-1: D3]` |
| **GND** | J1 pin 9 (Brown) | J2 pin 9, U1 GND, U2 GND, U3 GND, R1b, R2b, C1, C2, SW3 |
| **3V3** | U3 VOUT | U1 pin 2 (3V3), SW1 (→ R3), SW2 (→ R4), SW4 (→ R5) |
| **MOTOR_RAIL** | `[DR-1: D3]` | U2 VCC |

#### Motor Nets (Split Path — the MITM)

| Net Name | Source | Destination | Notes |
|---|---|---|---|
| **MOTOR_POS** | J1 pin 1 (Red, bike) | U2 A-OA (output) | Bike's motor + wire → driver output, NOT to console |
| **MOTOR_NEG** | J1 pin 2 (Black, bike) | U2 A-OB (output) | Bike's motor − wire → driver output, NOT to console |
| **CONSOLE_MTR_POS** | J2 pin 1 (Red, console) | R1a (30kΩ) → R1b (10kΩ) to GND, tap to U1 GPIO 6 | Console motor + → ESP32 ADC1 via divider (sense only) |
| **CONSOLE_MTR_NEG** | J2 pin 2 (Black, console) | R2a (30kΩ) → R2b (10kΩ) to GND, tap to U1 GPIO 7 | Console motor − → ESP32 ADC1 via divider (sense only) |
| **L9110S_IA** | U1 GPIO 10 | U2 A-IA (input) | ESP32 controls motor forward |
| **L9110S_IB** | U1 GPIO 17 | U2 A-IB (input) | ESP32 controls motor reverse |

#### Sensor Nets (Tap + Pass-Through)

| Net Name | J1 Pin | → J2 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **POT_REF_HI** | 3 (Yellow) | Pass-through | — | R4 (100Ω) from 3V3 via SW2 (open by default) |
| **POT_WIPER** | 4 (Blue) | Pass-through | U1 GPIO 1 (ADC) | C2 (100 nF to GND) |
| **POT_REF_LO** | 5 (Green) | Pass-through | — | GND via SW3 (open by default) |
| **CADENCE** | 6 (Orange) | Pass-through | U1 GPIO 5 (interrupt) | R3 (10kΩ) to 3V3 via SW1 (open by default), C1 (100 nF to GND) |
| **CADENCE_RTN** | 7 (Grey) | Pass-through | — | — |

#### Encoder Nets (Tap + Pass-Through)

| Net Name | J3 Pin | → J4 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **ENC_VCC** | 1 (Red) | Pass-through | — | R5 (10Ω) from 3V3 via SW4 (open by default) |
| **ENC_CHA** | 2 (Brown) | Pass-through | U1 GPIO 8 | Pull-down in standalone only `[DR-1]` |
| **ENC_CHB** | 3 (Orange) | Pass-through | U1 GPIO 9 | Pull-down in standalone only `[DR-1]` |

The Phase 3+ direct taps keep a series resistor as in Phase 0 (§5.5); the
value per tap is `[DR-1]` (§6.2.2 #2).

### 7.8 ESP32-S3-Nano Pin Map (Phase 3+) — PROVISIONAL `[DR-1]`

This map mirrors SPEC §4.4.2. It applies to the intercept board; the passive tap board
uses its sense-only subset (no GPIO 10/17). **GPIO 6/7 are sense inputs
only; they are never driver outputs.** The Phase 0/1 map is SPEC §4.4.1.

| Header | Label | GPIO | Direction | Net Name | Function |
|---|---|---|---|---|---|
| Left | 3V3 | — | Power In | 3V3 | 3.3V supply from buck converter |
| Left | GND | — | Power | GND | Common ground |
| Left | A0 | GPIO 1 | Input (ADC1) | POT_WIPER | Resistance position (12-bit ADC) |
| Left | D2 | GPIO 5 | Input (INT) | CADENCE | Reed switch pulse (FALLING edge) |
| Left | D3 | GPIO 6 | Input (ADC1) | CONSOLE_MTR_POS | Console motor sense + (analog) |
| Left | D4 | GPIO 7 | Input (ADC1) | CONSOLE_MTR_NEG | Console motor sense − (analog) |
| Left | D5 | GPIO 8 | Input (PCNT/INT) | ENC_CHA | Encoder channel A |
| Left | D6 | GPIO 9 | Input (PCNT/INT) | ENC_CHB | Encoder channel B |
| Right | D7 | GPIO 10 | Output | L9110S_IA | Motor forward drive (intercept only) |
| Right | D8 | GPIO 17 | Output | L9110S_IB | Motor reverse drive (intercept only) |

There is no encoder-VCC sense in Phase 3+.

### 7.9 Wire Routing Summary

#### 7.9.1 Wires That Pass Straight Through (7 of 9 on main loom)

| J1 Pin | Colour | Signal | ESP32 Tap | Standalone supply |
|---|---|---|---|---|
| 3 | Yellow | Pot outer (REF_HI) | — | R4 via SW2 (open by default) |
| 4 | Blue | Pot wiper | GPIO 1 ADC (high-Z) | — |
| 5 | Green | Pot outer (REF_LO) | — | GND via SW3 (open by default) |
| 6 | Orange | Cadence | GPIO 5 interrupt (high-Z) | R3 via SW1 (open by default) |
| 7 | Grey | Cadence return | — | — |
| 8 | White | +9V supply | Buck converter VIN | — |
| 9 | Brown | GND | Common ground | — |

#### 7.9.2 Wires That Are Split (2 of 9 — the MITM)

| J1 Pin | Colour | Bike Side (J1) Goes To | Console Side (J2) Goes To |
|---|---|---|---|
| 1 | Red | L9110S A-OA (driven from GPIO 10 via A-IA) | ESP32 GPIO 6 (sense) via R1a (30kΩ) / R1b (10kΩ) divider |
| 2 | Black | L9110S A-OB (driven from GPIO 17 via A-IB) | ESP32 GPIO 7 (sense) via R2a (30kΩ) / R2b (10kΩ) divider |

#### 7.9.3 Encoder Loom (All 3 wires pass through)

| J3 Pin | Colour | Signal | ESP32 Tap | Standalone supply |
|---|---|---|---|---|
| 1 | Red | Encoder 3.3V | — | R5 via SW4 (open by default) |
| 2 | Brown | Channel A | GPIO 8 | — |
| 3 | Orange | Channel B | GPIO 9 | — |

---

## 8. Assembly Notes (Intercept Board)

> [!CAUTION]
> **PROVISIONAL — superseded at DR-1, do not build.** See the banner at §7.

### 8.1 Build Sequence

1. **Prepare the perfboard.** Mark out component positions. The board should
   fit inside the console housing alongside the Echelon motherboard.

2. **Solder female header strips (J5)** for the ESP32 carrier socket. Verify
   the ESP32 seats and can be removed.

3. **Mount the motor driver (U2)** on the perfboard. Solder its supply
   (motor rail, `[DR-1: D3]`) and GND, and wire A-IA to GPIO 10 and A-IB to
   GPIO 17.

4. **Mount the MP1584EN module (U3).** Solder VIN and GND. **Do not connect
   VOUT yet.** Power from 9V and adjust the trimpot to 3.3V output first.

5. **Wire the 3.3V rail** from U3 VOUT to the U1 3V3 pad and to the SW1,
   SW2 and SW4 inputs. Fit SW1–SW4 **open**.

6. **Solder passive components:** R1a/R1b, R2a/R2b, R3, R4, R5, C1, C2.

7. **Build the voltage dividers** for motor sense:
   - J2 pin 1 → R1a (30kΩ) → junction → GPIO 6
   - Junction → R1b (10kΩ) → GND
   - J2 pin 2 → R2a (30kΩ) → junction → GPIO 7
   - Junction → R2b (10kΩ) → GND

8. **Wire the JST connectors.** Use colour-matched hookup wire. The 7
   pass-through wires run J1→J2 directly. The 2 motor wires split as
   described in §7.9.2.

9. **Test with a multimeter** before powering:
   - Continuity: each pass-through wire J1 pin N → J2 pin N (pins 3–9)
   - No continuity: J1 pin 1 → J2 pin 1 (motor split)
   - No continuity: J1 pin 2 → J2 pin 2 (motor split)
   - Divider: R1a+R1b from J2 pin 1 to GND (~40kΩ total)
   - Divider: R2a+R2b from J2 pin 2 to GND (~40kΩ total)
   - With SW1–SW4 open: no path from 3V3 to J1 pins 3, 6 or J3 pin 1; no
     path from J1 pin 5 to GND
   - 3.3V rail: no shorts to GND

10. **Power-on test** (ESP32 not installed):
    - Connect 9V to J1 pins 8/9
    - Verify 3.3V on the rail and at U1 3V3 pad
    - Verify the motor rail `[DR-1: D3]`

11. **Install ESP32.** Flash firmware via USB-C with the buck converter
    disconnected (§3.5), then reconnect the buck and verify boot via serial
    or WiFi AP. Subsequent updates use manual OTA.

### 8.2 Test Points

Solder short pin headers at these locations for easy multimeter access:

| Test Point | Net | Expected Voltage |
|---|---|---|
| TP1 | 3V3 RAIL | 3.30V ± 0.05V |
| TP2 | V9_SUPPLY | 8.5–9.5V (from PSU) |
| TP3 | POT_WIPER | ~1160–1700 mV (varies with position) |
| TP4 | CADENCE | 3.3V idle, 0V on pulse |
| TP5 | CONSOLE_MTR_POS (at GPIO 6) | 0V idle; drive level per register #1 (read as analog) |
| TP6 | CONSOLE_MTR_NEG (at GPIO 7) | 0V idle; drive level per register #1 (read as analog) |

### 8.3 Physical Dimensions

| Component | Approx Size | Notes |
|---|---|---|
| Perfboard | 70 × 50 mm | Main carrier board |
| ESP32-S3-Nano | 49.8 × 18.2 mm | Mounted via female headers (removable) |
| L9110S module | 28 × 21 mm | Soldered or socketed on perfboard `[DR-1: D3]` |
| MP1584EN module | 22 × 17 mm | Soldered on perfboard (trimpot accessible) |

The console housing has space alongside the Echelon motherboard for a board
of this size. Route JST cables through the existing cable entry points.

---

## 9. Safety Notes

> [!CAUTION]
> - **Never exceed 3.3V on any ESP32 GPIO pin.** The 9V rail (White) must
>   only connect to the buck converter VIN or a scope probe. Verify wiring
>   before power-on.
> - **Complete the motor-drive characterisation gate (§5.2)** before
>   connecting the motor-sense taps in Phase 0. The dividers protect the
>   ESP32 up to about 13V drive; stop if a larger peak is seen.
> - **Every direct tap has a series resistor and no internal pulls** (§5.5),
>   so the logger cannot back-power or bias the console's lines.
> - **Phase 0/1 run from USB-C only**, with ESP32 GND tied to Brown. Prefer a
>   power bank or a laptop on battery to avoid ground loops through a mains
>   charger.
> - **On buck-powered boards, disconnect the buck converter output before
>   plugging in USB-C** to avoid two regulators on one rail.
> - **Motor current is unmeasured** (register #2). Size wiring, driver and
>   rail at DR-1; do not assume 800 mA.
> - **The L9110S has internal flyback clamp diodes**; a replacement driver
>   chosen at DR-1 must provide equivalent protection.
> - **Pot wiper is a high-impedance signal.** Keep the wire run from J1 pin 4
>   to GPIO 1 as short as possible to minimise noise pickup. C2 (100 nF)
>   must be placed close to the ESP32 ADC pin.
