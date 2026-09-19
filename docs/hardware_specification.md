# Echenon Intercept Board — Hardware Specification

**Complete circuit design for the MITM intercept board.**

*This document specifies every electrical connection between the EX-3 bike
sensors, the Echelon console, and the ESP32-S3 intercept board. For EX-3 stock
hardware details, see [`ex3_specification.md`](ex3_specification.md).*

---

## 1. Design Summary

The intercept board sits between the bike's frame wiring and the Echelon
handlebar console. It passes all signals through while tapping them for the
ESP32, and interposes the L9110S motor driver in the resistance motor circuit.

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

---

## 2. Bill of Materials

| Ref | Item | Qty | Status |
|---|---|---|---|
| U1 | Waveshare ESP32-S3-Nano | 1 | Owned |
| U2 | L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | Owned (5-pack) |
| U3 | GTIWUNG MP1584EN DC-DC Adjustable Buck Converter Module | 1 | Owned (10-pack) |
| J1 | JST-SM 9-pin socket (female, bike side) | 1 | To purchase |
| J2 | JST-SM 9-pin plug (male, console side) | 1 | To purchase |
| J3 | JST 3-pin socket (female, encoder side) | 1 | To purchase |
| J4 | JST 3-pin plug (male, console side) | 1 | To purchase |
| J5 | 30-pin female header strips ×2 (ESP32 carrier) | 1 set | To purchase |
| J6 | 2-pin screw terminal block (motor output) | 1 | To purchase |
| R1 | 10kΩ resistor (console motor+ sense pull-down) | 1 | To purchase |
| R2 | 10kΩ resistor (console motor− sense pull-down) | 1 | To purchase |
| R3 | 10kΩ resistor (cadence pull-up — standalone power) | 1 | To purchase |
| R4 | 100Ω resistor (pot reference — contention protection) | 1 | To purchase |
| R5 | 10Ω resistor (encoder power — contention protection) | 1 | To purchase |
| C1 | 100 nF ceramic capacitor (cadence debounce) | 1 | To purchase |
| C2 | 100 nF ceramic capacitor (pot ADC filter) | 1 | To purchase |
| — | Perfboard / protoboard (70×50 mm) | 1 | To purchase |
| — | Hookup wire (22 AWG, assorted colours) | — | To purchase |

---

## 3. Power Supply

### 3.1 Source

The bike's 9V DC power supply (YINGJIAO YS25-0903000, 9V/3A) enters the
frame and travels up to the console via the 9-pin loom (White = +9V,
Brown = GND). The intercept board taps this 9V rail before passing it
through to the console.

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
            ├──► U2 L9110S VCC — motor driver supply
            │
            ├──► R3 (10kΩ) ──► Cadence pull-up (Net: CADENCE)
            │
            ├──► R4 (100Ω) ──► Pot reference (Net: POT_REF_HI, J1 pin 3)
            │
            └──► R5 (10Ω) ──► Encoder power (Net: ENC_VCC, J3 pin 1)
```

### 3.4 Current Budget

| Consumer | Typical | Peak | Duty |
|---|---|---|---|
| ESP32-S3 (WiFi + BLE active) | 200 mA | 500 mA | Continuous |
| L9110S + motor | 0 mA | 800 mA | Intermittent (repositioning, ~2 sec) |
| Sensors + pull-ups | 5 mA | 5 mA | Continuous |
| **Total** | **205 mA** | **1,305 mA** | — |

The MP1584EN is rated for 3A continuous — ample headroom. At 9V→3.3V with
90% switching efficiency, worst-case heat dissipation is ~0.5W.

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

| Ref | Type | Side | Mates With | Purpose |
|---|---|---|---|---|
| **J1** | JST-SM 9-pin socket | Bike frame | 9-pin plug from lower wiring hub | All bike-side signals: power, sensors, motor |
| **J2** | JST-SM 9-pin plug | Console | 9-pin socket on console PCB | Pass-through to Echelon console |
| **J3** | JST 3-pin socket | Bike frame | 3-pin plug from resistance knob | Encoder signals |
| **J4** | JST 3-pin plug | Console | 3-pin socket on console PCB | Encoder pass-through to console |
| **J5** | 30-pin female headers ×2 | Board | ESP32-S3-Nano pin headers | ESP32 carrier socket (removable) |
| **J6** | 2-pin screw terminal | Motor | Hookup wire to L9110S output | Motor connection from H-bridge |

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

## 5. Complete Schematic

### 5.1 Net List

Every electrical connection on the intercept board, grouped by function.

#### Power Nets

| Net Name | Source | Destinations |
|---|---|---|
| **V9_SUPPLY** | J1 pin 8 (White) | J2 pin 8 (pass-through), U3 VIN |
| **GND** | J1 pin 9 (Brown) | J2 pin 9, U1 GND, U2 GND, U3 GND, R1, R2, C1, C2 |
| **3V3** | U3 VOUT | U1 pin 2 (3V3), U2 VCC, R3, R4, R5 |

#### Motor Nets (Split Path — the MITM)

| Net Name | Source | Destination | Notes |
|---|---|---|---|
| **MOTOR_POS** | J1 pin 1 (Red, bike) | U2 A-OA (output) | Bike's motor + wire goes to L9110S output, NOT to console |
| **MOTOR_NEG** | J1 pin 2 (Black, bike) | U2 A-OB (output) | Bike's motor − wire goes to L9110S output, NOT to console |
| **CONSOLE_MTR_POS** | J2 pin 1 (Red, console) | U1 GPIO 10 via R1 (10kΩ to GND) | Console's motor + output → ESP32 reads as sense input |
| **CONSOLE_MTR_NEG** | J2 pin 2 (Black, console) | U1 GPIO 11 via R2 (10kΩ to GND) | Console's motor − output → ESP32 reads as sense input |
| **L9110S_IA** | U1 GPIO 6 | U2 A-IA (input) | ESP32 controls motor forward |
| **L9110S_IB** | U1 GPIO 7 | U2 A-IB (input) | ESP32 controls motor reverse |

#### Sensor Nets (Tap + Pass-Through)

| Net Name | J1 Pin | → J2 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **POT_REF_HI** | 3 (Yellow) | Pass-through | — | R4 (100Ω) from 3V3 rail (standalone power) |
| **POT_WIPER** | 4 (Blue) | Pass-through | U1 GPIO 1 (ADC) | C2 (100 nF to GND) |
| **POT_REF_LO** | 5 (Green) | Pass-through | — | Connected to GND (standalone power) |
| **CADENCE** | 6 (Orange) | Pass-through | U1 GPIO 5 (interrupt) | R3 (10kΩ to 3V3), C1 (100 nF to GND) |
| **CADENCE_RTN** | 7 (Grey) | Pass-through | — | — |

#### Encoder Nets (Tap + Pass-Through)

| Net Name | J3 Pin | → J4 (Console) | → ESP32 | Passive Components |
|---|---|---|---|---|
| **ENC_VCC** | 1 (Red) | Pass-through | — | R5 (10Ω) from 3V3 rail (standalone power) |
| **ENC_CHA** | 2 (Brown) | Pass-through | U1 GPIO 8 (interrupt) | — |
| **ENC_CHB** | 3 (Orange) | Pass-through | U1 GPIO 9 (interrupt) | — |

### 5.2 Schematic Diagram

```
═══════════════════════════════════════════════════════════════════════════
                         INTERCEPT BOARD SCHEMATIC
═══════════════════════════════════════════════════════════════════════════

J1 (9-PIN JST-SM, BIKE SIDE)          J2 (9-PIN JST-SM, CONSOLE SIDE)
┌───────────────────────┐              ┌───────────────────────┐
│ 1  Red    MOTOR_POS ──┼──────────────────────────► U2 A-OA   │ 1  Red  ──┼──► R1 (10k) ──► GND
│                       │              │   (to L9110S output)  │          ──┼──► U1 GPIO 10
│ 2  Black  MOTOR_NEG ──┼──────────────────────────► U2 A-OB   │ 2  Black──┼──► R2 (10k) ──► GND
│                       │              │   (to L9110S output)  │          ──┼──► U1 GPIO 11
│                       │              │                       │           │
│ 3  Yellow POT_REF_HI ─┼──────────── pass-through ───────────┼── 3      │
│                        │    └──► R4 (100Ω) ◄── 3V3          │           │
│ 4  Blue   POT_WIPER ──┼──────────── pass-through ───────────┼── 4      │
│                        │    └──► C2 (100nF) ──► GND          │           │
│                        │    └──► U1 GPIO 1 (ADC)             │           │
│ 5  Green  POT_REF_LO ─┼──────────── pass-through ───────────┼── 5      │
│                        │    └──► GND (standalone ref)         │           │
│ 6  Orange CADENCE ─────┼──────────── pass-through ───────────┼── 6      │
│                        │    └──► R3 (10k) ◄── 3V3            │           │
│                        │    └──► C1 (100nF) ──► GND          │           │
│                        │    └──► U1 GPIO 5 (interrupt)       │           │
│ 7  Grey   CADENCE_RTN ┼──────────── pass-through ───────────┼── 7      │
│                       │              │                       │           │
│ 8  White  V9_SUPPLY ──┼──────────── pass-through ───────────┼── 8      │
│                        │    └──► U3 VIN (buck converter)     │           │
│ 9  Brown  GND ─────────┼──────────── pass-through ───────────┼── 9      │
│                        │    └──► COMMON GND                  │           │
└───────────────────────┘              └───────────────────────┘


J3 (3-PIN JST, ENCODER SIDE)           J4 (3-PIN JST, CONSOLE SIDE)
┌───────────────────────┐              ┌───────────────────────┐
│ 1  Red    ENC_VCC ─────┼──────────── pass-through ───────────┼── 1      │
│                        │    └──► R5 (10Ω) ◄── 3V3           │           │
│ 2  Brown  ENC_CHA ─────┼──────────── pass-through ───────────┼── 2      │
│                        │    └──► U1 GPIO 8 (interrupt)       │           │
│ 3  Orange ENC_CHB ─────┼──────────── pass-through ───────────┼── 3      │
│                        │    └──► U1 GPIO 9 (interrupt)       │           │
└───────────────────────┘              └───────────────────────┘


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
```

### 5.3 ESP32-S3-Nano Pin Map

| Pin # | Header | Label | GPIO | Direction | Net Name | Function |
|---|---|---|---|---|---|---|
| 2 | Left | 3V3 | — | Power In | 3V3 | 3.3V supply from buck converter |
| 4 | Left | GND | — | Power | GND | Common ground |
| 6 | Left | A0 | GPIO 1 | Input (ADC) | POT_WIPER | Resistance position (12-bit ADC) |
| 10 | Left | D2 | GPIO 5 | Input (INT) | CADENCE | Reed switch pulse (FALLING edge) |
| 12 | Left | D3 | GPIO 6 | Output | L9110S_IA | Motor forward drive |
| 14 | Left | D4 | GPIO 7 | Output | L9110S_IB | Motor reverse drive |
| 16 | Left | D5 | GPIO 8 | Input (INT) | ENC_CHA | Encoder channel A (CHANGE) |
| 18 | Left | D6 | GPIO 9 | Input (INT) | ENC_CHB | Encoder channel B (CHANGE) |
| 20 | Right | D7 | GPIO 10 | Input | CONSOLE_MTR_POS | Console motor sense + |
| 22 | Right | D8 | GPIO 11 | Input | CONSOLE_MTR_NEG | Console motor sense − |

> [!NOTE]
> Pin numbers above are illustrative — verify against the Waveshare ESP32-S3-Nano
> datasheet for exact header positions. GPIO numbers are authoritative.

---

## 6. Motor Circuit Detail (The MITM)

This is the core innovation — the motor is **always** driven by the L9110S,
never directly by the Echelon console.

### 6.1 Signal Routing

```
STOCK BIKE (no intercept board):
    Console motor output ──────────────────────────── Motor
    (Red/Black on 9-pin)                              (in actuator hub)


WITH INTERCEPT BOARD:
    Console motor output ──► ESP32 GPIO 10/11 (sense inputs, 10kΩ pull-downs)
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

### 6.2 Why the Motor Wires Are Split

In the stock bike, the console sends 3.3V directly to the motor via pins 1–2
of the 9-pin loom. The intercept board breaks this connection:

- **Bike side** (J1 pins 1–2): Motor wires connect to **L9110S outputs**
  (A-OA, A-OB), not to J2.
- **Console side** (J2 pins 1–2): Console's motor drive signals connect to
  **ESP32 GPIO inputs** (GPIO 10/11) with 10kΩ pull-down resistors.

The console *thinks* it's driving the motor. Actually, its motor signals are
read by the ESP32, which decides whether to relay them (Intercept Mode) or
ignore them (Override Mode).

### 6.3 Pull-Down Resistors (R1, R2)

10kΩ pull-down resistors on GPIO 10 and GPIO 11 serve two purposes:

1. **Clean LOW when console disconnected**: In Override Mode or standalone
   operation (UC6), the console-side JST is unplugged. Without pull-downs,
   GPIO 10/11 would float and read random values. The pull-downs ensure
   they read LOW (no motor command).

2. **Defined idle state**: Even with the console connected, the pull-downs
   ensure a clean LOW when the console is not actively driving the motor.

### 6.4 L9110S Truth Table

| GPIO 6 (A-IA) | GPIO 7 (A-IB) | L9110S A-OA | L9110S A-OB | Motor |
|---|---|---|---|---|
| LOW | LOW | LOW | LOW | **Stopped** (brake) |
| HIGH | LOW | HIGH | LOW | **Forward** (increase resistance) |
| LOW | HIGH | LOW | HIGH | **Reverse** (decrease resistance) |
| HIGH | HIGH | HIGH | HIGH | **Stopped** (not used) |

---

## 7. Sensor Circuits

### 7.1 Cadence Sensor

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

### 7.2 Resistance Potentiometer

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

The potentiometer needs a voltage reference across its outer terminals to
produce a meaningful wiper signal. Two scenarios:

| Scenario | POT_REF_HI Source | POT_REF_LO | Wiper Range |
|---|---|---|---|
| **Console connected** | Console's 3.3V regulator (via J2 pass-through) | Console's GND | ~1160–1700 mV (measured stock range) |
| **Console disconnected** | Intercept board 3V3 via R4 (100Ω) | Intercept board GND | Range may differ — **recalibrate via portal** |

**R4 (100Ω series resistor)** protects against contention when both the
console and the intercept board are driving POT_REF_HI simultaneously:

- If both drive 3.3V: ~0 mA through R4, no effect. Safe.
- If voltages differ slightly (e.g., console at 3.28V, buck at 3.32V):
  R4 limits current to ~0.4 mA. No damage, minimal voltage error.
- If console is disconnected: R4 drops ~0.5 mV at 5 mA pot current.
  Negligible effect on ADC readings.

**C2 (100 nF)** filters ADC noise on the wiper signal. Combined with the
16× oversampling in firmware, this provides clean, stable readings.

> [!IMPORTANT]
> When switching between Intercept Mode (console connected) and Override Mode
> standalone (console disconnected), the potentiometer reference voltage
> changes. **Run the calibration wizard** via the portal after changing the
> console connection state.

### 7.3 Rotary Encoder (Resistance Knob)

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

**R5 (10Ω series resistor)** on the encoder's 3.3V supply line prevents
bus-fight between the intercept board's 3.3V and the console's 3.3V when
both are connected. At 10Ω with <10 mA encoder current, the voltage drop
is <100 mV — negligible. If only one source is present, R5 has no
meaningful effect.

The encoder's Channel A and Channel B signals are tapped by the ESP32 for
quadrature decoding while simultaneously passing through to the console for
its own encoder processing (in Intercept Mode).

---

## 8. Standalone Operation (Console Disconnected)

When the Echelon console is completely disconnected or removed (UC6), all
sensors must continue to function. The intercept board provides replacement
power for the three sensor circuits that normally depend on the console:

| Sensor | Stock Power Source | Intercept Board Replacement | Component |
|---|---|---|---|
| Cadence reed switch | Console pull-up (via 9-pin loom) | R3: 10kΩ pull-up from 3V3 rail | Always connected |
| Potentiometer reference | Console 3.3V (via Yellow/Green) | R4: 100Ω from 3V3 to POT_REF_HI; GND to POT_REF_LO | Always connected |
| Rotary encoder | Console 3.3V (via 3-pin JST Red) | R5: 10Ω from 3V3 to ENC_VCC | Always connected |

All three standalone power paths are **always connected** — they do not need
switching. The series resistors (R4, R5) and parallel pull-up (R3) coexist
safely with the console's own power when it is present (see §7 for analysis).

> [!NOTE]
> When transitioning to standalone operation, recalibrate the potentiometer
> via the portal. The wiper voltage range may shift slightly because the
> pot reference is now supplied through R4 (100Ω) rather than the console's
> direct 3.3V connection.

---

## 9. Wire Routing Summary

### 9.1 Wires That Pass Straight Through (7 of 9 on main loom)

These wires connect J1 directly to J2 with no interruption. The ESP32 taps
them via high-impedance connections (ADC input, interrupt input) but does not
modify or break the signal path.

| J1 Pin | Colour | Signal | ESP32 Tap |
|---|---|---|---|
| 3 | Yellow | Pot outer (REF_HI) | — (not read by ESP32) |
| 4 | Blue | Pot wiper | GPIO 1 ADC (high-Z) |
| 5 | Green | Pot outer (REF_LO) | — (not read by ESP32) |
| 6 | Orange | Cadence | GPIO 5 interrupt (high-Z) |
| 7 | Grey | Cadence return | — |
| 8 | White | +9V supply | Buck converter VIN |
| 9 | Brown | GND | Common ground |

### 9.2 Wires That Are Split (2 of 9 — the MITM)

These wires do NOT connect J1 to J2. Instead:

| J1 Pin | Colour | Bike Side (J1) Goes To | Console Side (J2) Goes To |
|---|---|---|---|
| 1 | Red | L9110S A-OA (motor power output) | ESP32 GPIO 10 + R1 10kΩ pull-down |
| 2 | Black | L9110S A-OB (motor power output) | ESP32 GPIO 11 + R2 10kΩ pull-down |

### 9.3 Encoder Loom (All 3 wires pass through)

| J3 Pin | Colour | Signal | ESP32 Tap |
|---|---|---|---|
| 1 | Red | Encoder 3.3V | — (not read, but powered via R5) |
| 2 | Brown | Channel A | GPIO 8 interrupt |
| 3 | Orange | Channel B | GPIO 9 interrupt |

---

## 10. Assembly Notes

### 10.1 Suggested Build Sequence

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

6. **Solder passive components:** R1–R5, C1, C2. These can go on the copper
   side of the perfboard.

7. **Wire the JST connectors.** Use colour-matched hookup wire. The 7
   pass-through wires run J1→J2 directly. The 2 motor wires split as
   described in §9.2.

8. **Test with a multimeter** before powering:
   - Continuity: each pass-through wire J1 pin N → J2 pin N
   - No continuity: J1 pin 1 → J2 pin 1 (motor split)
   - No continuity: J1 pin 2 → J2 pin 2 (motor split)
   - Resistance: R1 and R2 to GND (~10kΩ)
   - 3.3V rail: no shorts to GND

9. **Power-on test** (ESP32 not installed):
   - Connect 9V to J1 pins 8/9
   - Verify 3.3V on the rail
   - Verify 3.3V at U1 3V3 pad, U2 VCC, R3/R4/R5 destinations

10. **Install ESP32.** Flash firmware via USB-C (with buck converter
    disconnected). Then disconnect USB, reconnect buck converter, and verify
    boot via serial or WiFi AP.

### 10.2 Recommended Test Points

Solder short pin headers at these locations for easy multimeter access
during debugging:

| Test Point | Net | Expected Voltage |
|---|---|---|
| TP1 | 3V3 RAIL | 3.30V ± 0.05V |
| TP2 | V9_SUPPLY | 8.5–9.5V (from PSU) |
| TP3 | POT_WIPER | ~1160–1700 mV (varies with resistance position) |
| TP4 | CADENCE | 3.3V idle, 0V on pulse |
| TP5 | CONSOLE_MTR_POS | 0V idle, 3.3V when console drives motor |
| TP6 | CONSOLE_MTR_NEG | 0V idle, 3.3V when console drives motor |

### 10.3 Physical Dimensions

| Component | Approx Size | Notes |
|---|---|---|
| Perfboard | 70 × 50 mm | Main carrier board |
| ESP32-S3-Nano | 49.8 × 18.2 mm | Mounted via female headers (removable) |
| L9110S module | 28 × 21 mm | Soldered or socketed on perfboard |
| MP1584EN module | 22 × 17 mm | Soldered on perfboard (trimpot accessible) |

The console housing has space alongside the Echelon motherboard for a board
of this size. Route JST cables through the existing cable entry points.

---

## 11. Safety Notes

> [!CAUTION]
> - **Never exceed 3.3V on any ESP32 GPIO pin.** The 9V rail must only
>   connect to the buck converter VIN. Verify wiring before power-on.
> - **Disconnect buck converter output before plugging in USB-C** to avoid
>   contention between the buck converter and USB power.
> - **The motor can draw up to 800 mA.** Ensure hookup wire between J6 and
>   the L9110S output is rated for at least 1A (22 AWG is sufficient).
> - **The L9110S has internal flyback clamp diodes**, so no external diodes
>   are needed for the motor's inductive kickback.
> - **Pot wiper is a high-impedance signal.** Keep the wire run from J1 pin 4
>   to GPIO 1 as short as possible to minimise noise pickup. C2 (100 nF)
>   must be placed close to the ESP32 ADC pin.
