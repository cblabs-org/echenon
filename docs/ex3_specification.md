# Echelon EX-3 Hardware Specification

**A complete hardware reference for the Echelon EX-3 Smart Connect indoor bike internals.**

*Based on first-hand teardown, multimeter measurements, and physical inspection.
Source investigation: [echelon-teardown/conversation.md](echelon-teardown/conversation.md)*

---

## 1. Architecture Overview

The EX-3's electrical system is divided into three functional zones connected by
wiring looms:

```
┌──────────────────────────────────────────────────────────────────┐
│                    HANDLEBAR CONSOLE                             │
│                                                                  │
│  ┌────────────────┐     ┌──────────────────────────────────┐     │
│  │  Switch Board  │────▶│         Main Motherboard         │     │
│  │  (4 buttons    │ 5p  │  • Bluetooth SoC                 │     │
│  │   + 1 LED)     │ JST │  • Buck converter (9V→5V→3.3V)   │     │
│  └────────────────┘     │  • USB-A charging port           │     │
│                         │  • 6-pin JTAG header             │     │
│                         └──────┬──────────────┬────────────┘     │
│                                │ 9-pin        │ 3-pin            │
│                                │ JST-SM       │ JST              │
└────────────────────────────────┼──────────────┼──────────────────┘
                                 │              │
                    ─ ─ ─ ─ ─ ─ ─│─ ─ ─ ─ ─ ─ ─ │─ ─ ─ FRAME
                                 │              │
┌────────────────────────────────┼──────────────┼──────────────────┐
│                    LOWER BODY  │              │                  │
│                                ▼              ▼                  │
│                    ┌───────────────────────────────┐             │
│                    │    Central Wiring Hub         │             │
│  9V DC in ────────▶│  ┌────────────────────────-─┐ │             │
│  (power supply)    │  │ Resistance Potentiometer │ │             │
│                    │  │ (position feedback)      │ │             │
│                    │  ├────────────────────────-─┤ │             │
│                    │  │ Resistance DC Motor      │ │             │
│                    │  │ (actuator, 3.3V)         │ │             │
│                    │  └─────────────────────────-┘ │             │
│                    └──────────┬────────────────────┘             │
│                               │                                  │
│                    ┌──────────▼──────────┐                       │
│                    │   Cadence Sensor    │                       │
│                    │  (magnetic reed sw) │                       │
│                    └───────────────────-─┘                       │
│                                                                  │
│  ┌───────────────────────┐                                       │
│  │ Resistance Knob       │──── 3-pin JST ────▶ Console           │
│  │ (rotary encoder,3.3V) │                                       │
│  └───────────────────────┘                                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. Power Supply

| Parameter | Value |
|---|---|
| **Manufacturer** | YINGJIAO |
| **Model** | YS25-0903000 |
| **Input** | 100–240V AC, 50/60 Hz, 0.7A max |
| **Output** | 9.0V DC, 3.0A (27W) |
| **Polarity** | Centre positive |
| **Manufacture date** | 2020-06 |
| **Certifications** | FCC, CE, UL, RoHS, IP20 |

The 9V DC rail enters the bike frame and routes through the central wiring hub
up to the console motherboard via the 9-pin loom (white and brown wires).

![Power supply label](echelon-teardown/attachments/image_cabb80.jpg)

---

## 3. Console Motherboard (Handlebar Unit)

### 3.1 Board Identification

| Field | Value |
|---|---|
| **Board designation** | ECHEX-3 |
| **Manufacturer** | CHANG YOU |
| **PCB revision** | S1001O-A03 |
| **PCB date code** | 2020/05/06 |
| **UL file** | E156176 |
| **Substrate** | 94V0GW808 (FR-4) |
| **Serial number (unit)** | 2021031226 (manufactured 2021-04-16) |

### 3.2 Key Components (Front)

| Component | Description |
|---|---|
| **Electrolytic capacitors** | 470µF 25V RVT (×1), 100µF 25V RVT (×3) — part of the buck converter |
| **Inductor** | Shielded 330µH power inductor — switching regulator energy storage |
| **Power resistors** | Multiple large through-hole resistors (current limiting / load balancing) |
| **Bluetooth SoC** | Red-capped RF module (bottom-left, shielded) — handles BLE protocol |
| **Microcontroller** | 8-pin DIP IC (marked "2210B02") — likely the main MCU or motor driver |
| **USB-A port** | Top of board — 5V charging output for tablet |
| **Buzzer** | Piezo buzzer (bottom-left) |
| **Voltage regulation** | Buck converter circuit steps 9V → 5V (USB) and → 3.3V (logic/BLE) |

### 3.3 Connectors (Rear)

| Connector | Type | Wires | Function |
|---|---|---|---|
| **9-pin black** | JST-SM (9-pin) | 9 wires (all slots populated) | Main loom — power, sensors, motor |
| **3-pin white** | JST 3-pin | Red, brown, orange | Resistance control knob (rotary encoder) |
| **5-pin white** | JST 5-pin | Ribbon cable | Auxiliary switch board |
| **6-pin header** | Pin header (unpopulated) | — | JTAG / programming debug port |

### 3.4 Debug / Programming Interfaces

Two debug headers are visible on the PCB:

1. **6-pin header** — positioned away from the main power/sensor inputs. Likely
   the primary JTAG or UART programming header for the board's main MCU.
2. **Pads near the BLE module** — likely a dedicated SWD or UART port for
   flashing firmware onto the Bluetooth SoC.

### 3.5 Photos

| View | Image |
|---|---|
| Console rear — connectors entering casing | ![Console rear](echelon-teardown/attachments/image_cabecb.jpg) |
| Console rear — close-up of PCB sockets | ![PCB sockets](echelon-teardown/attachments/image_cabf0c.jpg) |
| 9-pin and 3-pin connectors (wire side) | ![Connectors wire side](echelon-teardown/attachments/image_cac24b.jpg) |
| Connectors — alternate angle | ![Connectors alt](echelon-teardown/attachments/image_cac26c.jpg) |
| PCB rear (solder side) — connectors and traces | ![PCB rear](echelon-teardown/attachments/image_cace65.jpg) |
| PCB front (component side) — full board | ![PCB front](echelon-teardown/attachments/image_cace8c.jpg) |
| PCB front — close-up of MCU, BLE module, and 5-pin header | ![PCB detail](echelon-teardown/attachments/image_cb2022.jpg) |

---

## 4. Auxiliary Switch Board

A small daughter board mounted behind the handlebar controls, connected to the
main motherboard via the 5-pin JST header.

| Field | Value |
|---|---|
| **PCB revision** | S1001O-B03 |
| **PCB date code** | 20180529 |
| **UL file** | E156176 (shared with main board) |
| **Substrate** | 94V0GW808 (FR-4) |

### 4.1 Components

| Component | Quantity | Description |
|---|---|---|
| **Tactile switches** | 4 (S1–S4) | Momentary pushbuttons — bike controls |
| **LED** | 1 | Status indicator |

The 4 switches and 1 LED share 5 wires. The switches likely share a common
ground (or VCC) and may use a resistor ladder network to send distinct voltage
values back to a single analog pin on the main board, minimising the wire count.

### 4.2 Photos

| View | Image |
|---|---|
| Switch board front — 4 tactile switches and LED | ![Switch front](echelon-teardown/attachments/image_cb27e1.jpg) |
| Switch board rear — traces and components | ![Switch rear](echelon-teardown/attachments/image_cb283e.jpg) |
| Switch board installed — ribbon cable to main PCB | ![Switch installed](echelon-teardown/attachments/image_cb2806.jpg) |

---

## 5. Resistance Control Knob (Rotary Encoder)

The red resistance knob on the handlebars is an **incremental quadrature rotary
encoder** operating on 3.3V logic. It connects to the console motherboard via
the 3-pin JST connector, and its signals are routed through the frame (not the
main 9-pin loom).

### 5.1 Connector

| Pin | Wire Colour | Function |
|---|---|---|
| 1 (outer) | Red | Common (3.3V reference) |
| 2 (middle) | Brown | Channel A — toggle signal |
| 3 (outer) | Orange | Channel B — pulse signal |

### 5.2 Measured Behaviour

| Measurement | Observation |
|---|---|
| **Red ↔ Brown** (common to Ch A) | Alternating state: each click toggles between 0V and 3.3V |
| **Red ↔ Orange** (common to Ch B) | Momentary pulse: each click produces a brief spike (~1.5–2V) then returns to 0V |

The two channels pulse 90° out of phase (standard quadrature encoding). The
motherboard decodes both the **amount** and **direction** of rotation (CW =
increase resistance, CCW = decrease). The console translates this digital input
into motor drive commands sent down the 9-pin loom to the resistance actuator.

### 5.3 Photo

| View | Image |
|---|---|
| Resistance knob cavity — encoder and brake cable visible | ![Resistance knob](echelon-teardown/attachments/image_cac669.jpg) |

---

## 6. Central Wiring Hub (Lower Resistance Actuator Assembly)

The central wiring hub is a sealed unit located in the lower frame near the
flywheel. It is the junction point for all lower-body electrical components and
contains both the resistance actuator and position sensor.

### 6.1 Components Inside the Hub

#### Resistance Potentiometer (Position Feedback)

| Parameter | Value |
|---|---|
| **Type** | Rotary potentiometer (continuous rotation) |
| **Total resistance** | ~5.4 kΩ per 180° sweep |
| **Behaviour** | Two 180° sweeps per full rotation: 0→5.4kΩ, click-off, 0→5.4kΩ, click-off |
| **Measured voltage** | 1.16V – 1.7V across the two outer terminals (Yellow ↔ Green) |
| **Purpose** | Absolute position feedback — the console always knows the exact physical position of the magnetic brake carriage |

The dual 180° sweep pattern acts as a continuous-rotation absolute encoder,
allowing the motherboard to track the resistance arm position through its full
range of motion.

#### Resistance DC Motor (Actuator)

| Parameter | Value |
|---|---|
| **Type** | Small DC motor |
| **Operating voltage** | 3.3V |
| **Purpose** | Drives the resistance adjustment pin, physically moving the magnetic brake carriage |
| **Control** | Driven by the motherboard in response to rotary encoder input from the resistance knob |

The motor shaft is mechanically coupled to the same pin as the potentiometer,
so as the motor turns to adjust resistance, the potentiometer simultaneously
reports the new position back to the console — forming a closed feedback loop.

### 6.2 Connectors at the Hub

Three connectors exit the central hub:

| Connector | Wires | Destination |
|---|---|---|
| **9-pin JST-SM** (black) | All 9 wire colours | Up to console motherboard |
| **2-pin JST** (white, small) | Grey, Brown | Down to cadence reed switch at pedal crank |
| **2-pin** (power in) | Red→White, Black→Brown | 9V DC power supply input |

### 6.3 Photos

| View | Image |
|---|---|
| Actuator assembly — potentiometer and motor visible, heavily potted | ![Actuator overview](echelon-teardown/attachments/image_cb32e8.jpg) |
| Potentiometer close-up — wiper and terminals visible through potting | ![Potentiometer](echelon-teardown/attachments/image_cb3307.jpg) |
| Potentiometer terminals — detail | ![Pot terminals](echelon-teardown/attachments/image_cb3327.jpg) |
| Hub with all wiring — motor leads, pot wires, and loom exiting | ![Hub wiring](echelon-teardown/attachments/image_cb3362.jpg) |
| Wiring junction — 9-pin loom, cadence, and power connections visible | ![Wiring junction](echelon-teardown/attachments/image_cb3763.jpg) |

> [!NOTE]
> All solder joints inside the hub are heavily potted with silicone sealant to
> prevent fracturing under the heavy vibrations of indoor cycling sessions.

---

## 7. Cadence Sensor

| Parameter | Value |
|---|---|
| **Type** | Magnetic reed switch |
| **Location** | Pedal crank |
| **Signal** | 3.3V logic — digital pulse, one pulse per pedal revolution |
| **Wiring** | 2-wire (Grey + Brown from the 9-pin loom) |
| **Connection** | Via white JST connector on the central wiring hub |

---

## 8. Main Wiring Loom — Complete 9-Pin Pinout

The main wiring loom uses a **JST-SM 9-pin** connector carrying all sensor
data, actuator control, and power between the lower body and the console. All
nine slots are populated.

| Pin | Wire Colour | Component / Function | Electrical Notes |
|---|---|---|---|
| 1 | **Red** | Resistance motor (+) | Actuator control; 3.3V on a DMM (waveform unconfirmed, Phase 0 register row 1) |
| 2 | **Black** | Resistance motor (−) | Actuator control; 3.3V on a DMM (waveform unconfirmed, Phase 0 register row 1) |
| 3 | **Yellow** | Potentiometer (outer terminal) | Resistance measurement reference |
| 4 | **Blue** | Potentiometer (wiper) | Variable signal; sweeps 0–5.4 kΩ per 180° rotation |
| 5 | **Green** | Potentiometer (outer terminal) | Resistance measurement reference. 1.16–1.7V across Yellow↔Green |
| 6 | **Orange** | Cadence sensor | Reed switch circuit; 3.3V logic signal |
| 7 | **Grey** | Cadence sensor | Reed switch circuit; 3.3V logic signal |
| 8 | **White** | Main power (+) | +9V DC supply routed up to the console motherboard |
| 9 | **Brown** | Main power (GND) | System ground |

### 8.1 Signal Grouping

```
9-Pin JST-SM Loom
├── Power (2 wires)
│   ├── White ─── +9V DC
│   └── Brown ─── GND
├── Resistance Motor (2 wires)
│   ├── Red ───── Motor +  (3.3V)
│   └── Black ─── Motor −  (3.3V)
├── Resistance Potentiometer (3 wires)
│   ├── Yellow ── Outer terminal
│   ├── Blue ──── Wiper (variable)
│   └── Green ─── Outer terminal
└── Cadence Sensor (2 wires)
    ├── Orange ── Reed switch signal (3.3V)
    └── Grey ──── Reed switch signal (3.3V)
```

---

## 9. Resistance Knob Loom — 3-Pin Pinout

Separate from the main 9-pin loom. Connects the handlebar-mounted rotary
encoder directly to the console motherboard.

| Pin | Wire Colour | Function |
|---|---|---|
| 1 | **Red** | Common (3.3V) |
| 2 | **Brown** | Channel A (quadrature signal) |
| 3 | **Orange** | Channel B (quadrature signal) |

Connector type: JST 3-pin (white housing).

---

## 10. Voltage Summary

All data signals operate at **3.3V logic** levels. The only higher voltage in
the system is the 9V DC power rail from the external supply.

| Rail | Voltage | Source | Consumers |
|---|---|---|---|
| **Main power** | 9V DC | External YINGJIAO PSU | Console motherboard (input) |
| **USB charging** | 5V | Buck converter on motherboard | USB-A port → tablet |
| **Logic / BLE** | 3.3V | Second-stage regulator on motherboard | Bluetooth SoC, MCU, all sensor/actuator signals |

> [!IMPORTANT]
> **All sensor signals run at 3.3V, and the resistance motor measured 3.3V on
> a DMM.** A DMM reads the average, so the motor drive waveform (3.3V DC or
> higher-voltage PWM) is unconfirmed; it is characterised by oscilloscope in
> Phase 0 (register row 1, [`measurements/phase0/README.md`](measurements/phase0/README.md)).
> The only confirmed higher voltage is the 9V DC power rail from the external supply.

---

## 11. Reference Photos Index

All photos are stored in [`echelon-teardown/attachments/`](echelon-teardown/attachments/).

| Photo | Description | Section |
|---|---|---|
| `image_cabb80.jpg` | Power supply label (YINGJIAO YS25-0903000) | §2 |
| `image_cabecb.jpg` | Console rear — wiring loom entry point | §3 |
| `image_cabf0c.jpg` | Console rear — PCB connector sockets through casing | §3 |
| `image_cac24b.jpg` | 9-pin and 3-pin connectors (wire side, colours visible) | §3 |
| `image_cac26c.jpg` | Connectors alternate angle — both plugs | §3 |
| `image_cac669.jpg` | Resistance knob cavity — rotary encoder and brake cable | §5 |
| `image_cace65.jpg` | Main PCB rear (solder side) — traces and connectors | §3 |
| `image_cace8c.jpg` | Main PCB front (component side) — full board | §3 |
| `image_cb2022.jpg` | PCB close-up — MCU IC, BLE module, 5-pin header area | §3 |
| `image_cb27e1.jpg` | Switch board front — 4 tactile switches (S1–S4) + LED | §4 |
| `image_cb2806.jpg` | Switch board rear — PCB traces | §4 |
| `image_cb283e.jpg` | Switch board installed — ribbon cable to main PCB | §4 |
| `image_cb32e8.jpg` | Actuator assembly overview — potentiometer + motor | §6 |
| `image_cb3307.jpg` | Potentiometer close-up — wiper visible through potting | §6 |
| `image_cb3327.jpg` | Potentiometer terminals — detail shot | §6 |
| `image_cb3362.jpg` | Actuator hub — all wiring and motor leads visible | §6 |
| `image_cb3763.jpg` | Wiring junction — 9-pin loom, cadence, and power splits | §6 |
