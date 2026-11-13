# Echenon — Hardware Build Guide

**Phase-by-phase circuit designs for the Echenon hardware configurations.**

*This document is organized by development phase. Each phase has a
schematic, parts list, and build checklist so you can build incrementally,
from a bench logger on USB power to a permanent board in the console.*

*For stock EX-3 hardware details (teardown), see
[`ex3_specification.md`](ex3_specification.md).
For development strategy and firmware, see
[`implementation-strategy.md`](implementation-strategy.md).
The authoritative pin maps are SPEC §4.4.1 (Phase 0/1) and §4.4.2 (Phase 3+:
v1 rows frozen at DR-1, v2 rows provisional until DR-2) ([`../SPEC.md`](../SPEC.md)); the
Phase 0 exit checklist is the Phase 0 register,
[`measurements/phase0/README.md`](measurements/phase0/README.md).*

---

## 1. Design Overview

Echenon ships as two releases (owner decision 2026-10-12): **v1 "Passive"**
(UC1–UC3, no resistance control of any kind) and **v2 "Active"** (UC4–UC6,
provisional). The system uses four hardware configurations, each tied to the
programme phases (numbering as in the programme plan and SPEC §1.5):

| # | Configuration | Phases (PRs) | What You Build | Bike wiring |
|---|---|---|---|---|
| 1 | **Breadboard observer** | 0 Observe (PR-01), 1 Calibrate (PR-02) | ESP32 on a breadboard, powered from USB-C. Y / back-probe harness on the looms, series-resistor taps, ÷4 motor-sense dividers. | Read-only. The bike is unmodified. |
| 2 | **HR-only (USB)** | 2 HR relay (PR-03) | Just the ESP32 on USB-C. BLE only. | None |
| 3 | **Passive tap board** (**v1**) | 3 Passive tap + FTMS telemetry (PR-04a, PR-05), 4 Session / upload (PR-06), 7 v1 Integration (PR-09) | Permanent board in the console housing: J1–J4 pass-through, high-impedance taps, ESP32 on the bike's 9 V via VIN or on USB-C, never both (mechanical `SW0` `VIN`/`USB-C` interlock, no buck), all ten PR-01 taps, UART0 reserved (`WITH_OTA` deferred post-beta in `v0.3.0-beta.1` — USB-C updates only; see standalone builder guide [`beta-build-guide.md`](beta-build-guide.md)). **Design frozen at DR-1** (§6.2). | Pass-through. No wires split. |
| 4 | **Active intercept board** (**v2**) | 5 (PR-07, PR-10), 6 (PR-08, PR-11, PR-12), 7 v2 Integration (PR-13) | Perfboard with ESP32, H-bridge, NC bypasses, encoder emulation, switchable standalone supplies with a pot-reference replica. Motor wires split. **PROVISIONAL — redesigned at DR-2** (§7–§8). | Motor split (MITM). |

PR-05 (FTMS telemetry) needs no new hardware: the breadboard observer is
enough to develop it; the passive tap board is the daily-use host once built.

DR-1 (v1 Passive Design Freeze) sits between Phase 0 and every soldered board.
No hardware is soldered or permanently installed until DR-1 is approved. The
v2 active board is frozen separately at DR-2.

**UART0 reservation (all boards).** GPIO 43 (TX, header D1) and GPIO 44 (RX,
header D0) are **reserved on every board and pin map** for a future ANT+
add-on (e.g. an nRF51422 module over UART). The ESP32-S3 has no ANT radio, so
ANT+ is not supported without it. Leave both pins unconnected and their header
pads accessible. Console and logging use native USB-CDC.

```
Phase 0/1            Phase 2          Phase 3–4, 7 (v1)      Phase 5–7 (v2)
Breadboard observer  HR-only          Passive tap board      Active intercept board
┌───────────────┐    ┌──────────┐     ┌──────────────────┐   ┌──────────────────────┐
│ ESP32 (USB-C) │    │ ESP32    │     │ ESP32 on VIN     │   │ ESP32  H-bridge/PWM  │
│ ÷4 dividers   │    │ (USB-C)  │     │ VIN/USB selector │   │ NC bypasses          │
│ series-R taps │    │          │     │ J1–J4 pass-thru  │   │ J1–J4, J6            │
│ Y harness     │    │          │     │ hi-Z taps, OTA   │   │ pot-ref replica      │
└───────────────┘    └──────────┘     │ UART0 reserved   │   │ Motor split (MITM)   │
                                      │ FROZEN (DR-1)    │   │ PROVISIONAL [DR-2]   │
                                      └──────────────────┘   └──────────────────────┘
```

---

## 2. Master Bill of Materials

Every component across all configurations. ✓ = needed in that
configuration. DR-1 values are resolved (v1 column); `[DR-2]` = the part or
its value is decided at DR-2 (v2 column).

| Ref | Item | Qty | 0/1 Observer | 2 HR | 3 Passive (v1, PR-04a) | 5 Intercept (v2, PR-07) | Status |
|---|---|---|---|---|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 | ✓ | ✓ | ✓ | ✓ | Owned |
| U2 | L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | — | — | — | ✓ `[DR-2: driver]` | Owned (5-pack) |
| U3 | ~~GTIWUNG MP1584EN DC-DC Adjustable Buck Converter Module~~ | — | — | — | — (removed from v1: ESP32 on VIN, DR-1 §2.3) | — `[DR-2]` | Owned (10-pack) |
| SW0 | VIN/USB power selector on the 9 V line to the Waveshare VIN pin (9 V disconnected in the USB position; part designed in PR-04a WO1, ≥ 1 A) | 1 | — | — | ✓ | ✓ `[DR-2]` | To purchase |
| D1 | Diode on the VIN path (Schottky, ≥ 1 A), **only if** PR-04a WO1 finds the Waveshare does not isolate VIN from VBUS | 0–1 | — | — | conditional | `[DR-2]` | To purchase |
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
| RS1–RS3 | 22 kΩ series resistor on the pot taps (GPIO 1, 2, 3); exact value | 3 | ✓ | — | ✓ | `[DR-2]` | To purchase |
| RS4–RS8 | 4.7 kΩ series resistor on the cadence taps (GPIO 4, 5) and encoder taps (GPIO 8, 9, 10) | 5 | ✓ | — | ✓ | `[DR-2]` | To purchase |
| R3 | 10 kΩ resistor (cadence pull-up, standalone only) | 1 | — | — | — | ✓ `[DR-2]` | To purchase |
| R4 | 100 Ω resistor (pot reference, standalone only) | 1 | — | — | — | ✓ `[DR-2]` | To purchase |
| R5 | 10 Ω resistor (encoder power, standalone only) | 1 | — | — | — | ✓ `[DR-2]` | To purchase |
| R6, R7 | 10 kΩ resistors (reset-safe pull-downs on driver inputs A-IA / A-IB, GPIO 10 / 17) | 2 | — | — | — | ✓ `[DR-2: driver]` | To purchase |
| SW1–SW4 | Jumpers (minimum) or high-side / analog switches for R3, R4, R5 and the Green–GND link | 4 | — | — | — | ✓ `[DR-2]` | To purchase |
| C1 | 100 nF ceramic capacitor, ESP32 side of the series resistor on GPIO 4, 5, 8, 9 and 10 (cadence and encoder filters) | 5 | ✓ | — | ✓ | `[DR-2]` | To purchase |
| C2 | 100 nF ceramic capacitor (pot wiper ADC filter) | 1 | optional (none recorded in B01) | — | — (not fitted, DR-1) | `[DR-2]` | To purchase |
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
| Series resistors | RS1–RS3 (22 kΩ, pot), RS4–RS8 (4.7 kΩ + 100 nF, cadence and encoder) and the ÷4 dividers (30 kΩ / 10 kΩ) | Safe, non-loading taps (§5.4, §5.5) |
| Multimeter | Any DMM | Continuity, static voltages (#6, #10) |

---

## 3. Power Supply

In Phase 0/1 the logger is powered **from USB-C only** (§5.3).

> **v1 power path (DR-1 §2.3, approved with conditions 2026-10-12).** The
> passive tap board is powered from the bike's 9 V (White/Brown) **or** from
> USB-C, **never both at once**. A **VIN/USB power selector (SW0)** on the 9 V
> line feeds the Waveshare **VIN** pin (6–21 V, register row #11; onboard
> MP2322 regulator) in the VIN position and disconnects 9 V in the USB
> position. **The external MP1584EN buck is removed from v1.** **PR-04a WO1**
> reads the Waveshare VBUS/VIN power path from the board schematic before any
> soldering and designs the selector, adding a diode (D1) on the VIN path if the
> board does not isolate VIN from VBUS (§6.2.3). The MP1584EN material in
> §3.2–§3.4 is retained as history and as input to the v2 driver decision
> (DR-2).

### 3.1 Source

The bike's 9V DC power supply (YINGJIAO YS25-0903000, 9V/3A) enters the
frame and travels up to the console via the 9-pin loom (White = +9V,
Brown = GND). The passive tap and intercept boards tap this 9V rail.

### 3.2 Regulation

A GTIWUNG MP1584EN adjustable buck converter steps 9V down to 3.3V.
**Decided at DR-1:** the Waveshare VIN pin takes 9 V directly (register
row 11: VIN 6–21 V, MP2322 regulator), so v1 has no external buck. The
MP1584EN design below is history, kept as input to DR-2.

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
> replace it with fixed resistors `[DR-2]` (audit H-03; only if v2 uses a buck).

### 3.3 Rail Distribution

On the passive tap board, the 3.3V rail powers only U1. On the intercept
board it is distributed as follows (**PROVISIONAL `[DR-2]`**):

```
3.3V RAIL ──┬──► U1 ESP32-S3-Nano pin 2 (3V3) — direct to SoC
            │
            ├──► SW1 ─► R3 (10kΩ) ──► Cadence pull-up (standalone only)
            │
            ├──► SW2 ─► R4 (100Ω) ──► Pot reference (standalone only)
            │
            └──► SW4 ─► R5 (10Ω) ──► Encoder power (standalone only)

MOTOR RAIL [DR-2: driver] ──► U2 motor driver supply (separate from the SoC rail)
```

### 3.4 Current Budget

| Consumer | Typical | Peak | Duty | Configuration |
|---|---|---|---|---|
| ESP32-S3 (WiFi + BLE active) | 200 mA | 500 mA | Continuous | All |
| Motor driver + motor | ≤ 150 mA steady (register row 2) | ≤ 175 mA reverse inrush (row 2) | Intermittent; ~16 s per half-revolution (row 3) | Intercept only (v2) |
| Sensors + pull-ups | 5 mA | 5 mA | Continuous | Intercept, standalone only (v2) |
| **Total, v1 passive board** | 200 mA | 500 mA (at 3.3 V, from VIN or USB) | — | — |
| **Total, v2** | `[DR-2]` | `[DR-2]` | — | — |

The v1 board draws only the ESP32's current; the taps draw well under 1 mA.
The motor current was measured in Phase 0 (A02-20261011: ≤ 175 mA peak,
≤ 150 mA steady, far below the old 800 mA assumption). Rail sizing for v2 is a
DR-2 output (audit decision D3): the recommendation is a **separate motor rail**, likely with a **MOSFET H-bridge
driver** (current limit and fault output, e.g. DRV8833 or TB6612) rather than
the L9110S on the SoC's 3.3V rail, plus bulk capacitance near the SoC
(audit H-03).

### 3.5 USB-C Power and Programming

- **Phase 0/1 (breadboard observer) and Phase 2 (HR-only):** USB-C is the
  only power source. There is no buck converter to disconnect, and the
  logger can be flashed at any time without rewiring.
- **VIN-powered boards (v1 passive tap; v2 active):** the 9 V line feeds VIN
  through the SW0 selector, and the USB port feeds VBUS. Both go through the
  Waveshare's own power path to the onboard regulator.

> [!WARNING]
> A VIN-powered board runs from **+9 V VIN (`J1-8`) or USB-C, NEVER both simultaneously** (DR-1 §4,
> condition 2.3). **Set the mechanical `SW0` interlock to the USB position (+9 V disconnected) before
> plugging in USB-C, and unplug USB-C completely before switching `SW0` to VIN.** PR-04a WO1 confirms from the Waveshare schematic
> whether VIN and VBUS are isolated and adds a diode on the VIN path if they
> are not (§6.2.3). Note that in **`v0.3.0-beta.1`**, `WITH_OTA` is deferred post-beta pending upstream framework hardening; all beta firmware flashes happen over **USB-C** with `SW0` isolating `VIN`.

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

The cadence colour mapping (see
[`ex3_specification.md`](ex3_specification.md) §7 vs §8) was resolved by
Phase 0 register row 7: Orange is the cadence signal (pulled up by the
console), Grey is the 0 V return.

### 4.3 J3 — Encoder Loom (Knob Side, 3-Pin JST)

| Pin | Position | Wire Colour | Signal Name | Function |
|---|---|---|---|---|
| 1 | Left | Black | ENC_CHA | Encoder Channel A (quadrature) |
| 2 | Centre | Orange | ENC_VCC | Encoder common (3.3V reference) |
| 3 | Right | Red | ENC_CHB | Encoder Channel B (quadrature) |

> [!NOTE]
> Wire colours corrected 2026-10-04 after bench verification. Original
> mapping (Red=VCC, Brown=CHA, Orange=CHB) was based on visual inspection;
> what appeared brown is actually black. Physical connector confirmed:
> VCC on centre pin, signals on outer pins.

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
| RS1–RS3 | 22 kΩ resistor (pot taps) | 3 |
| RS4–RS8 | 4.7 kΩ resistor (cadence and encoder taps) | 5 |
| C (tap filters) | 100 nF ceramic capacitor (GPIO 4, 5, 8, 9, 10) | 5 |
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
| 2 | A1 | Yellow: pot outer reference | RS2 22 kΩ series (measured 0 V, §5.8) |
| 3 | A2 | Green: pot outer reference | RS3 22 kΩ series (measured ≈ 1.67 V, §5.8) |
| 4 | A3 | Grey: cadence return | RS4 4.7 kΩ series + 100 nF |
| 5 | D2 | Orange: cadence | RS5 4.7 kΩ series + 100 nF |
| 6 | D3 | Red: motor + | 30 kΩ / 10 kΩ ÷4 divider (R1a/R1b) |
| 7 | D4 | Black: motor − | 30 kΩ / 10 kΩ ÷4 divider (R2a/R2b) |
| 8 | D5 | Black: Encoder Ch A | RS6 4.7 kΩ series + 100 nF (PCNT input) |
| 9 | D6 | Red: Encoder Ch B | RS7 4.7 kΩ series + 100 nF (PCNT input) |
| 10 | D7 | Orange: Encoder VCC | RS8 4.7 kΩ series + 100 nF (ADC1 CH9, steady 3.3 V) |

These are the tap signals; White (+9V) and Brown (GND) are power, not
taps. Every tap pin is configured with **no internal pull-up or
pull-down**. Note that GPIO 8 and 9 were released from continuous ADC to PCNT
hardware decoding (`EncoderModule`), leaving 8 ADC1 channels.

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


SENSOR TAPS (9-pin loom — series resistor on every direct tap)
──────────────────────────────────────────────────────────────

Blue wire   (pin 4, pot wiper)      ── tail ── RS1 22k ──┬──► U1 GPIO 1 (A0, ADC1)
                                                         └── C2 (100nF) ── GND
                                                              (optional)
Yellow wire (pin 3, pot outer ref)  ── tail ── RS2 22k ─────► U1 GPIO 2 (A1, ADC1, 0V)
Green wire  (pin 5, pot outer ref)  ── tail ── RS3 22k ─────► U1 GPIO 3 (A2, ADC1, 1.67V)
Grey wire   (pin 7, cadence return) ── tail ── RS4 4.7k ─┬──► U1 GPIO 4 (A3, ADC1)
                                                         └── C1 (100nF) ── GND
Orange wire (pin 6, cadence)        ── tail ── RS5 4.7k ─┬──► U1 GPIO 5 (D2, ADC1 / edge)
                                                         └── C1 (100nF) ── GND


ENCODER TAPS (3-pin loom — 4.7kΩ series + 100 nF to GND on every tap)
────────────────────────────────────────────────────────────────

Enc Black wire  (pin 1, Ch A) ── tail ── RS6 4.7k ─┬──► U1 GPIO 8  (D5, PCNT)
                                                   └── 100nF ── GND
Enc Red wire    (pin 3, Ch B) ── tail ── RS7 4.7k ─┬──► U1 GPIO 9  (D6, PCNT)
                                                   └── 100nF ── GND
Enc Orange wire (pin 2, VCC)  ── tail ── RS8 4.7k ─┬──► U1 GPIO 10 (D7, ADC1)
                                                   └── 100nF ── GND

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
> Phase 0. **DR-1 (v1):** the motor-sense taps are sampled as ADC1 analog at
> ≥ 1 kHz for monitoring only; no clamp diodes are needed (worst single-ended
> excursion −0.14 V at the wire, A01-20261012).

### 5.5 Direct-Tap Series Resistors (RS1–RS8) and the No-Pulls Rule

The direct taps use series resistors at the harness end of each tail:
- **Potentiometer taps (RS1–RS3)**: 22 kΩ series resistors. The pot signals are slow-moving DC, so the ADC sample-and-hold settling is not an issue; readings are stable and linear.
- **Cadence taps (RS4, RS5)**: 4.7 kΩ series resistors + 100 nF ceramic capacitors to GND on the ESP32 side.
- **Encoder taps (RS6–RS8)**: 4.7 kΩ series resistors + 100 nF ceramic capacitors to GND on the ESP32 side (A, B and VCC).

> [!IMPORTANT]
> **Bench-tested 2026-10-03 (B01):** The original 22 kΩ value exceeded the ESP32-S3
> ADC continuous-mode source-impedance limit (Espressif recommends < 10 kΩ).
> At 83 kHz aggregate sample rate the S/H capacitor could not settle through
> 22 kΩ during brief reed-switch closures (~20–50 ms). Replaced with 4.7 kΩ +
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
each fast signal pin and GND on the ESP32 side of the series resistor. This
provides a low-impedance charge reservoir for the ADC's sample-and-hold
capacitor, ensuring accurate readings even with the 4.7 kΩ source impedance.

### 5.6 Phase 0 Observation-Only Taps

These taps exist to characterise the bike's signals. **DR-1: all four are
kept on the v1 passive tap board** as diagnostics monitors (DR-1 §4,
condition 2.7). Whether the v2 active board keeps them is `[DR-2]`.

| GPIO | Signal | What We're Measuring | Register row |
|---|---|---|---|
| GPIO 2 (A1) | Yellow — pot outer ref | Console's pot reference voltage (measured 0 V) | #4, #6 |
| GPIO 3 (A2) | Green — pot outer ref | Console's pot reference voltage (measured ≈ 1.67 V active source, ~2.4 kΩ Z_out) | #4, #6 |
| GPIO 4 (A3) | Grey — cadence return | Reed switch circuit return path (measured 0 V) | #7 |
| GPIO 10 (D7) | Orange — Encoder VCC (3-pin centre) | Console's encoder supply voltage (measured 3.3 V steady) | #8, #10 |

These results let DR-2 design the v2 standalone supplies (R3, R4, R5, the
Green–GND link) with evidence instead of assumptions; the v1 board fits none
(DR-1 §2.9). Note: the measured pot polarity (Yellow=0V, Green=1.67V) is
opposite to original assumptions, so the standalone circuit is redesigned at
DR-2 (pot-reference replica).

### 5.7 Build Checklist

- [ ] **Bench ready** (§2.1): scope (≥2 ch, single-shot), motor-current
      measurement, Y / back-probe harness, series resistors
- [ ] Build the H1/H2 Y / back-probe harness (§5.1.1); verify continuity
      plug-to-socket on all 12 conductors
- [ ] Set up the breadboard with the ESP32-S3-Nano; power it from USB-C
      (power bank or laptop on battery)
- [ ] Tie ESP32 GND to the Brown tail
- [ ] Fit series resistors: RS1–RS3 (22 kΩ for pot), RS4–RS5 (4.7 kΩ + 100 nF for cadence), RS6–RS8 (4.7 kΩ + 100 nF for encoder)
- [ ] Build the motor-sense dividers (R1a/R1b, R2a/R2b)
- [ ] Flash Phase 0 firmware via USB-C; confirm every tap pin is configured
      with no internal pulls
- [ ] **GATE: motor-drive characterisation** (§5.2) — all six captures saved
- [ ] Connect the motor-sense tails (Red → R1a, Black → R2a)
- [ ] Connect the sensor and encoder tails
- [ ] Verify all tap signals reading on the web dashboard

### 5.8 Test Points

Expected readings reflect actual measurements from bench characterisation (B01, B03, B04).

| Signal | GPIO | Measured / Confirmed Reading | Register row |
|---|---|---|---|
| Pot wiper | GPIO 1 | 159–1321 counts (~130–1070 mV, varies with level 1–32; B03 linear fit: 39.1 × level + 97) | #4 |
| Pot outer ref (Yellow) | GPIO 2 | 0 V (always, unexpected console reference topology) | #6 |
| Pot outer ref (Green) | GPIO 3 | ≈ 1.66–1.67 V (≈ 2060–2084 counts, active source) | #6 |
| Cadence return | GPIO 4 | 0 V | #7 |
| Cadence | GPIO 5 | 3.3V idle (4095 counts), ~0.17V (~217 counts) on reed closure | #7 |
| Motor+ sense | GPIO 6 | 0V idle; ~0.74V (~922 counts) during forward drive (÷4 divider) | #1 |
| Motor− sense | GPIO 7 | 0V idle; ~0.73V (~912 counts) during reverse drive (÷4 divider) | #1 |
| Encoder Ch A (Black) | GPIO 8 | 0 ↔ 3.3V quadrature, decoded cleanly via PCNT (0% bounce, 100% detection) | #8 |
| Encoder Ch B (Red) | GPIO 9 | 0 ↔ 3.3V quadrature, decoded cleanly via PCNT (0% bounce, 100% detection) | #8 |
| Encoder VCC (Orange) | GPIO 10 | 3.3V steady (4095 counts) | #8, #10 |

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

### 6.2 Phase 3 — Passive Tap Board (PR-04a) — v1

> **Status: FROZEN at DR-1** (approved with conditions, 2026-10-12). This is
> the v1 build specification. **Soldering may start only after PR-04a WO1 has
> committed the power-path check and the selector design (§6.2.3).**

> **Goal:** A permanent, pass-through-only board installed in the console
> housing. It delivers UC1–UC3 (telemetry, HR relay, recording) as the
> default daily-use product and hosts PR-05 and PR-06.

#### 6.2.1 Design

The v1 board is **the PR-01 detection circuit made permanent** (DR-1 §4,
condition 2.8): the same ten taps, on the same GPIOs, with the same values as
the Phase 0/1 rig (§5.3; SPEC §4.4.1–§4.4.2).

- **Connectors (Y-harness pass-through).** J1 (JST-SM 9-pin socket, bike) →
  J2 (JST-SM 9-pin plug, console) and J3 (JST 3-pin socket, knob) → J4 (JST
  3-pin plug, console). All 9 + 3 conductors run straight through. **No motor
  split** — the console drives the motor exactly as stock. J5 carries the
  ESP32-S3-Nano. No J6.
- **Tap network (exact values, all ten taps kept):**

| GPIO | Header | Signal (wire) | Part(s) | Network |
|---|---|---|---|---|
| 1 | A0 | Pot wiper (Blue, J1-4) | RS1 | 22 kΩ series |
| 2 | A1 | Pot outer ref (Yellow, J1-3), monitor | RS2 | 22 kΩ series |
| 3 | A2 | Pot outer ref (Green, J1-5), monitor | RS3 | 22 kΩ series |
| 4 | A3 | Cadence return (Grey, J1-7), monitor | RS4, C1 | 4.7 kΩ series + 100 nF to GND (ESP32 side) |
| 5 | D2 | Cadence (Orange, J1-6) | RS5, C1 | 4.7 kΩ series + 100 nF to GND (ESP32 side) |
| 6 | D3 | Motor + (Red, J1-1) | R1a, R1b | 30 kΩ series / 10 kΩ to GND (÷4) |
| 7 | D4 | Motor − (Black, J1-2) | R2a, R2b | 30 kΩ series / 10 kΩ to GND (÷4) |
| 8 | D5 | Encoder Ch A (Black, J3-1) | RS6, C1 | 4.7 kΩ series + 100 nF to GND (ESP32 side, PCNT) |
| 9 | D6 | Encoder Ch B (Red, J3-3) | RS7, C1 | 4.7 kΩ series + 100 nF to GND (ESP32 side, PCNT) |
| 10 | D7 | Encoder VCC (Orange, J3-2), monitor | RS8, C1 | 4.7 kΩ series + 100 nF to GND (ESP32 side) |
| — | GND | Brown (J1-9) | — | Common ground |
| — | VIN | White (J1-8) | SW0 (+ D1 if needed) | 9 V via the VIN/USB selector |

- **No pulls, no supplies.** Every tap pin is an input with no internal
  pull-up or pull-down. No R3/R4/R5, no Green–GND link, no wiper capacitor
  (C2): the board never sources current into a bike line (DR-1 §2.9).
- **No outputs.** GPIO 17 and every other unlisted GPIO stay unconnected.
- **UART0 reserved:** GPIO 43/44 unconnected, header pads accessible (§1).
- **Power: +9 V (`J1-8` VIN) or USB-C, NEVER both simultaneously** (DR-1 §4, condition 2.3), enforced via
  the mechanical **SW0** `VIN`/`USB-C` interlock (§6.2.3). No external buck converter.
- **OTA (`WITH_OTA` deferred in `v0.3.0-beta.1`).** In the `v0.3.0-beta.1` beta release, `WITH_OTA` is deferred post-beta pending upstream framework hardening; beta firmware flashes over **USB-C** with `SW0` in the USB (`VIN` disconnected) position. Once `WITH_OTA` is enabled post-beta, manual OTA (`autoCheck = false` enforced after config load and factory reset) provides updates without opening the console housing.
- **No resistance control, no interception** (v1). The stock console runaway
  cannot be prevented by this board; the firmware detects and reports it
  (SPEC §5.2).

#### 6.2.2 DR-1 Decisions

| # | Decision | Outcome (DR-1, 2026-10-12) | Evidence |
|---|---|---|---|
| 1 | Which Phase 0 observation taps (Yellow, Green, Grey, encoder VCC) are kept | **All kept** as monitors (all ten PR-01 taps; owner condition 2.7) | Register #4, #6, #7, #8 |
| 2 | Series-resistor value per tap, and whether C1/C2 filtering is fitted | **PR-01 values unchanged** (table above); `C1 ×5` (`100 nF` to GND on `GPIO 4, 5, 8, 9, 10`); no C2 | B01, B03, A04-20261012 |
| 3 | Motor-sense sampling scheme and rate | **ADC1 analog at ≥ 1 kHz**, monitoring only; direction = whichever wire is high; no clamp diodes | Register #1, A01-20261012 |
| 4 | Encoder Ch B conditioning and PCNT decoding | **None needed**; `EncoderModule` (PCNT), `stepsPerDetent = 4`, 4 detents per level | B04, A08-20261012 |
| 5 | SoC supply | **VIN or USB, never both**; mechanical `SW0` `VIN`/`USB-C` interlock; D1 if not isolated; buck removed; PR-04a WO1 checks the power path first | Register #11, audit H-03 |
| 6 | Behaviour at power-on | The console powers on at level 1 with no sweep (register #13); the board only observes, and the series taps limit back-powering while the ESP32 is unpowered | Register #13, §5.5 |
| 7 | Bulk capacitance and brown-out margin | The v1 board powers only the ESP32 (≤ 500 mA peak) from the Waveshare regulator; no motor load on its rail. Any extra bulk capacitance is a PR-04a build choice | Register #2, audit H-03 |
| 8 | Board size, placement and cable routing | **A PR-04a check**: housing fit is verified before the board is sealed | Physical survey (PR-04a) |
| 9 | Final BOM, schematic and build steps | §6.2.4–§6.2.6 (see also standalone builder guide [`beta-build-guide.md`](beta-build-guide.md)) | All of the above |

#### 6.2.3 Power Selection (PR-04a WO1, before soldering)

```
White +9 V (J1-8) ──► SW0 selector ──[ D1, only if needed ]──► U1 VIN (6–21 V) ──► MP2322 ──► 3V3
                        VIN position: 9 V connected
                        USB position: 9 V disconnected
USB-C ────────────────────────────────────────────────────► U1 VBUS
Brown GND (J1-9) ─────────────────────────────────────────► U1 GND
```

1. **Read the power path.** From the Waveshare ESP32-S3-Nano schematic, record
   how VBUS and VIN reach the regulator, and whether the board isolates them
   (no back-feed from VIN into the USB host, none from VBUS into the 9 V line).
2. **Design the selector.** SW0 disconnects 9 V from VIN in the USB position.
   If step 1 shows the board does not isolate VIN from VBUS, add **D1**
   (Schottky, ≥ 1 A) on the VIN path.
3. **Commit the evidence** (schematic extract, finding, selector part and
   wiring) before any soldering.
4. **Rule of use:** set SW0 to USB before plugging in USB-C; never connect both
   sources. Measured behaviour in both positions is recorded after the build
   (§6.2.7).

#### 6.2.4 BOM Delta (v1 board vs the Phase 0/1 rig)

| Change | Parts |
|---|---|
| **Removed** | U3 MP1584EN buck (not used in v1); H1/H2 Y-harness (replaced by J1–J4 on the board); breadboard |
| **Added** | SW0 VIN/USB selector (part from WO1); **D1** diode on the VIN path, only if WO1 finds no isolation; J1–J4 connectors; J5 header strips; perfboard; hookup wire |
| **Unchanged** | U1; RS1–RS3 (22 kΩ); RS4–RS8 (4.7 kΩ); C1 ×5 (100 nF on GPIO 4, 5, 8, 9, 10); R1a/R1b, R2a/R2b (30 kΩ / 10 kΩ) |
| **Not fitted** | C2; R3, R4, R5; SW1–SW4; U2; R6, R7; J6 |

#### 6.2.5 Housing Fit (PR-04a check)

Board size, placement and cable routing in the console housing are checked in
PR-04a before the board is sealed, with the J1–J4 cables reaching the stock
connectors without strain and the USB-C port and SW0 reachable (or reachable
by opening the housing) for recovery.

#### 6.2.6 Build Checklist

- [ ] **WO1 power-path check committed** (§6.2.3): Waveshare VBUS/VIN path read
      from the schematic, selector designed, D1 decision recorded. **No
      soldering before this.**
- [ ] Housing survey: board outline, placement and cable routing fit (§6.2.5)
- [ ] Fit J5 headers; seat U1 only after the power checks below
- [ ] Wire J1 → J2 and J3 → J4 straight through; verify continuity on all 12
      conductors, and no shorts between adjacent pins
- [ ] Fit RS1–RS3 (22 kΩ), RS4–RS8 (4.7 kΩ), C1 ×5 on GPIO 4, 5, 8, 9, 10, and the
      ÷4 dividers R1a/R1b, R2a/R2b, exactly as in §6.2.1
- [ ] Leave GPIO 17, 43 and 44 (and every unlisted pin) unconnected
- [ ] Fit SW0 (and D1 if required); with U1 out, check: VIN position → 9 V at
      the VIN pad; USB position → 0 V at the VIN pad
- [ ] Seat U1; flash over USB-C with SW0 in the USB position; confirm every tap
      pin is configured with no internal pulls
- [ ] Switch to VIN (USB unplugged); confirm boot and all ten taps on the
      dashboard against §5.8
- [ ] Note: in `v0.3.0-beta.1`, `WITH_OTA` is deferred post-beta (updates via USB-C with `SW0` in USB position); once enabled post-beta, test manual OTA (and rollback) before permanently sealing the housing
- [ ] Ride test: stock behaviour unchanged (§6.2.7)

#### 6.2.7 Verification (from PR-04a)

- A stock ride with the board fitted is indistinguishable from no board.
- All ten taps read correctly against §5.8 and the PR-02 calibration.
- The board fits in the console housing.
- In `v0.3.0-beta.1`, USB-C updates with `SW0` isolating `VIN` succeed (`WITH_OTA` manual OTA deferred post-beta).
- Runs from VIN (SW0 in the VIN position, USB-C unplugged) and from USB (SW0 in the USB
  position, 9 V disconnected); the WO1 power-path finding is documented.
- GPIO 43/44 are unconnected and reserved.

---

## 7. Phase 5+ — Intercept Board (PR-07) — v2

> [!CAUTION]
> **v2 — PROVISIONAL, do not build.** Everything in §7 and §8 is a
> pre-measurement design for the **v2 Active** release. DR-2 rewrites it
> (motor driver: H-bridge + PWM, DRV8833 class; NC fail-safe bypasses and
> runaway protection; encoder emulation; standalone supplies with a
> pot-reference replica of the console's 1.66 V / 2.4 kΩ Green source, e.g.
> ≈ 4.7 kΩ / 4.7 kΩ from 3.3 V). Power is the v1 VIN path (§3), not the buck
> drawn below. No part of it is soldered before DR-2 is approved.

> **Goal:** The intercept board sits between the bike frame and the Echelon
> console and splits the motor wires (MITM), adding Override and ERG.

### 7.1 What You Need (Additions to the Passive Tap Board)

| Ref | Item | Qty | Purpose |
|---|---|---|---|
| U2 | L9110S motor driver module `[DR-2: D3]` | 1 | Drives the resistance motor |
| J6 | 2-pin screw terminal | 1 | Motor output connection |
| R3 | 10kΩ resistor | 1 | Cadence pull-up (standalone only) |
| R4 | 100Ω resistor | 1 | Pot reference (standalone only) |
| R5 | 10Ω resistor | 1 | Encoder power (standalone only) |
| R6, R7 | 10kΩ resistor | 2 | Pull-downs on A-IA / A-IB: driver held off through reset and boot |
| SW1–SW4 | Jumpers (minimum) or high-side / analog switches | 4 | Switch R3, R4, the Green–GND link and R5 |
| C1 | 100 nF capacitor | 1 | Cadence debounce |
| C2 | 100 nF capacitor | 1 | Pot ADC filter |

**What changes from the passive tap board:**
- Add the motor driver and a motor rail `[DR-2: D3]`
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
│              │  Buck Converter│  (+ motor rail [DR-2])         │
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
> **Fail-safe requirement:** In Intercept Mode, if the ESP32 crashes, the motor is orphaned. A fail-safe mechanism (e.g. NC relay or analog switch bypass) is recommended. Strategy deferred to `[DR-2: decision H-05]`.

#### 7.3.4 L9110S Truth Table

| GPIO 10 (A-IA) | GPIO 17 (A-IB) | L9110S A-OA | L9110S A-OB | Motor |
|---|---|---|---|---|
| LOW | LOW | LOW | LOW | **Stopped** (brake) |
| HIGH | LOW | HIGH | LOW | **Forward** (increase resistance) |
| LOW | HIGH | LOW | HIGH | **Reverse** (decrease resistance) |
| HIGH | HIGH | HIGH | HIGH | **Stopped** (not used) |

Driver inputs must be LOW through reset and boot (audit M-01); R6 and R7
(10kΩ to GND on A-IA / A-IB) hold them there. The driver choice itself is
`[DR-2: D3]`.

### 7.4 Sensor Circuits

The standalone supplies — R3, R4, R5 and the Green–GND link — are each
behind a switch (SW1–SW4). **Default: all open.** They are closed only in
standalone Override with the console disconnected (UC6). Minimum
implementation is jumpers; preferred is a high-side or analog switch under
ESP32 control. Values and switch type are `[DR-2: decision H-02]`.

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
row 7 / DR-2.

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
> standalone Override, or external parts switched with SW4 `[DR-2]`.
> Decoding uses the framework `EncoderModule` (PCNT); Ch B conditioning is
> `[DR-2]` (register #8).

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
connected are `[DR-2: decision H-02]`, from register rows 6, 7 and 10.

### 7.6 Complete Schematic

```
═══════════════════════════════════════════════════════════════════════════════
     PHASE 5+ — INTERCEPT BOARD COMPLETE SCHEMATIC (PROVISIONAL [DR-2])
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
  SW1–SW4: open by default; closed only in standalone Override [DR-2: H-02]


J3 (3-PIN JST, ENCODER)                     J4 (3-PIN JST, CONSOLE)
┌──────────────────────────┐                 ┌──────────────────────────┐
│ 1  Red    ENC_VCC ───────┼── pass-thru ───┼── 1                     │
│     └── R5 (10Ω) ◄─SW4─ 3V3              │                         │
│ 2  Brown  ENC_CHA ───────┼── pass-thru ───┼── 2                     │
│     └── GPIO 8 (INT)     │                 │                         │
│ 3  Orange ENC_CHB ───────┼── pass-thru ───┼── 3                     │
│     └── GPIO 9 (INT)     │                 │                         │
└──────────────────────────┘                 └──────────────────────────┘


U1 — WAVESHARE ESP32-S3-NANO                U2 — L9110S H-BRIDGE [DR-2: D3]
┌──────────────────────────────┐            ┌───────────────────────┐
│                              │            │                       │
│  Pin 2 (3V3) ◄── 3V3 RAIL   │            │  VCC ◄── MOTOR RAIL   │
│  GND ◄── COMMON GND         │            │         [DR-2: D3]    │
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
| **V9_SUPPLY** | J1 pin 8 (White) | J2 pin 8 (pass-through), U3 VIN, motor rail `[DR-2: D3]` |
| **GND** | J1 pin 9 (Brown) | J2 pin 9, U1 GND, U2 GND, U3 GND, R1b, R2b, C1, C2, SW3 |
| **3V3** | U3 VOUT | U1 pin 2 (3V3), SW1 (→ R3), SW2 (→ R4), SW4 (→ R5) |
| **MOTOR_RAIL** | `[DR-2: D3]` | U2 VCC |

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
| **ENC_CHA** | 2 (Brown) | Pass-through | U1 GPIO 8 | Pull-down in standalone only `[DR-2]` |
| **ENC_CHB** | 3 (Orange) | Pass-through | U1 GPIO 9 | Pull-down in standalone only `[DR-2]` |

The Phase 3+ direct taps keep a series resistor as in Phase 0 (§5.5). The v1
values are frozen in §6.2.1; the v2 values are `[DR-2]`.

### 7.8 ESP32-S3-Nano Pin Map (Phase 3+) — PROVISIONAL `[DR-2]` (v2)

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
| — | D1 (TX) | GPIO 43 | Reserved | UART0_TX | **Reserved** for a future ANT+ add-on (all boards) |
| — | D0 (RX) | GPIO 44 | Reserved | UART0_RX | **Reserved** for a future ANT+ add-on (all boards) |

There is no encoder-VCC sense in Phase 3+. **DR-2 draft outputs** (replace
the L9110S rows once DR-2 closes): encoder out A/B GPIO 17/18, encoder bypass
GPIO 21, driver IN1/IN2 GPIO 10/11, motor bypass GPIO 47, optional motor-cut
relay GPIO 38. Power: VIN via SW0 (§3) rather than the 3V3 pin from a buck.

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
| 1 | Black | Channel A | GPIO 8 (PCNT) | — |
| 2 | Orange | Encoder 3.3V | — | R5 via SW4 (open by default) |
| 3 | Red | Channel B | GPIO 9 (PCNT) | — |

---

## 8. Assembly Notes (Intercept Board)

> [!CAUTION]
> **PROVISIONAL — rewritten at DR-2, do not build.** See the banner at §7.

### 8.1 Build Sequence

1. **Prepare the perfboard.** Mark out component positions. The board should
   fit inside the console housing alongside the Echelon motherboard.

2. **Solder female header strips (J5)** for the ESP32 carrier socket. Verify
   the ESP32 seats and can be removed.

3. **Mount the motor driver (U2)** on the perfboard. Solder its supply
   (motor rail, `[DR-2: D3]`) and GND, and wire A-IA to GPIO 10 and A-IB to
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
    - Verify the motor rail `[DR-2: D3]`

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
| L9110S module | 28 × 21 mm | Soldered or socketed on perfboard `[DR-2: D3]` |
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
> - **VIN-powered boards run from 9 V or USB-C, never both** (§3.5, §6.2.3):
>   set SW0 to the USB position before plugging in USB-C.
> - **Motor current** was measured in Phase 0 (register #2: ≤ 175 mA peak,
>   ≤ 150 mA steady). The v2 wiring, driver and rail are sized at DR-2.
> - **The L9110S has internal flyback clamp diodes**; a replacement driver
>   chosen at DR-2 must provide equivalent protection.
> - **Pot wiper is a high-impedance signal.** Keep the wire run from J1 pin 4
>   to GPIO 1 as short as possible to minimise noise pickup. C2 (100 nF)
>   must be placed close to the ESP32 ADC pin.
