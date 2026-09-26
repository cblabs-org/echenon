# Echelon EX-3 → ESP32 FTMS Bridge: Research Report

## 1. Your Board: Waveshare ESP32-S3-Nano

| Spec | Detail |
|---|---|
| **CPU** | Espressif ESP32-S3R8 — dual-core Xtensa LX7 @ 240 MHz |
| **Flash** | 16 MB Quad-SPI NOR |
| **PSRAM** | 8 MB Octal-SPI (integrated in package) |
| **Bluetooth** | **Bluetooth 5.0 LE** + Mesh |
| **Wi-Fi** | 2.4 GHz 802.11 b/g/n |
| **USB** | **USB-C** (native USB OTG, CDC serial, JTAG debug) |
| **Logic Level** | **3.3V only — NOT 5V tolerant** |
| **USB Power In** | 5V via USB-C (regulated to 3.3V internally) |
| **VIN Power In** | 6–21V DC (onboard buck converter) |
| **Form Factor** | Arduino Nano footprint (30-pin, 2.54mm headers) |
| **GPIOs** | 24 accessible (22 on headers + 2 underside pads) |
| **ADC** | 12-bit SAR, 16 channels (10× ADC1 + 6× ADC2) |

> [!IMPORTANT]
> **ADC2 is unusable when Bluetooth is active.** You must use **ADC1 pins only** for reading the resistance potentiometer. This is a hard silicon constraint — ADC2 hardware is shared with the RF baseband controller.

---

## 2. Pinout — What Matters for This Project

### Recommended Pin Assignments

| Function | Header Label | ESP32-S3 GPIO | ADC | Notes |
|---|---|---|---|---|
| **Resistance (Pot wiper)** | **A0** | GPIO 1 | ADC1_CH0 ✅ | Prime analog input — safe with BLE active |
| **Cadence (Reed switch)** | **D2** | GPIO 5 | ADC1_CH4 | All GPIOs support interrupts; use `FALLING` edge |
| **Common Ground** | **GND** | — | — | Left header pin 4 or right header pin 29 |
| **Power (from bike USB)** | **USB-C** | — | — | 5V in → internal 3.3V regulator |

### Pins to Avoid

| Pin | Why |
|---|---|
| **A4, A5, A6, A7, D8, D9** | ADC2 — **broken when BLE is active** |
| **A2 (GPIO 3)** | Strapping pin (JTAG config) — external voltage at boot can interfere |
| **D0/D1 (GPIO 43/44)** | UART0 TX/RX — reserved for serial debug if needed |

### Alternative Safe Analog Pins
If A0 is inconvenient, **A1 (GPIO 2)** and **A3 (GPIO 4)** are both ADC1 and fully safe. Pins **D3–D7** (GPIO 6–10) are also on ADC1 if needed.

---

## 3. ADC Gotchas — Reading the Resistance Potentiometer

The ESP32-S3 ADC has known non-linearity at the extremes:

| Voltage Range | Behaviour |
|---|---|
| **0–80 mV** | Dead zone — reads as 0 |
| **100 mV – 2850 mV** | ✅ Linear and accurate |
| **2900–3300 mV** | Saturates at 4095 — top ~12% of rotation is a dead zone |

### Mitigations

> [!TIP]
> **Always use `analogReadMilliVolts(pin)`** instead of raw `analogRead()`. The ESP32-S3 has factory-burned eFuse calibration curves that `analogReadMilliVolts()` applies automatically for much better linearity.

1. **Hardware option**: Add a series resistor (1.5–2.2kΩ) on the high side of the pot to clamp max wiper voltage to ~2.8V
2. **Software option**: Treat any reading above ~2850 mV as "max resistance" — simpler, works fine for 32 discrete resistance levels
3. **Filtering**: Place a **100 nF ceramic cap** between A0 and GND for noise suppression; average 16–32 samples in software (or use an EMA filter)

---

## 4. GitHub FTMS Projects — Comparative Analysis

### Full Comparison Matrix

| Project | ⭐ | Active? | Build | BLE Stack | S3 Support | Sensor Type | Power Calc | HRM Relay |
|---|---|---|---|---|---|---|---|---|
| **SmartSpin2k** | ~280 | ✅ **Yes** (Sep 2026) | **PlatformIO** | **NimBLE** | ✅ **Yes** (`S3release`) | BLE client to bike + ext. PM | Power tables (`.ptab`) | ✅ **Yes** |
| **Echelon-CYD-Controller** | ~15 | ✅ Active (2024–25) | Arduino/PIO | NimBLE | Partial (CYD board) | BLE client to Echelon | Echelon resistance curve | No |
| **snowzach/echbt** | ~58 | ❌ Dormant (2021) | Arduino IDE | NimBLE | No | BLE client to Echelon | Peloton regression formula | No |
| **eiffelpeter/esp32_ftms_indoor_bike** | Low | ⚠️ Dormant (2023–24) | Arduino IDE | BlueDroid | No | **Reed switch + Potentiometer** | Quadratic model | No |
| **kswiorek/ble-ftms** | ~58 | ❌ Archived (2020) | Arduino IDE | BlueDroid | No | HX711 strain gauge + gyro | Torque × angular velocity | No |
| **jamesjmtaylor/esp32-ftms-server** | ~20 | ❌ Inactive (2021) | Arduino IDE | BlueDroid | No | Hall-effect tachometer | Fluid trainer cubic curve | No |
| **dbsqp/bluetooth-reebok-57e** | Low | ❌ Inactive (2022) | Arduino IDE | BlueDroid | No | Reed switch + PWM duty tap | Quadratic model | No |

---

### Detailed Breakdown of the Key Projects

#### 🏆 SmartSpin2k — The Gold Standard
- **Repo**: [github.com/doudar/SmartSpin2k](https://github.com/doudar/SmartSpin2k)
- **Why it leads**: 5+ years of development, active daily commits, full PlatformIO build, NimBLE throughout, **native ESP32-S3 build target** (`env:S3release`)
- **Architecture**: Acts as a BLE multiplexer — connects as a *Central* to external BLE power meters, cadence sensors, and heart rate straps, then re-broadcasts everything as a unified FTMS *Peripheral*
- **Power calibration**: 2D lookup tables (`.ptab` CSV files) mapping resistance × cadence → watts; web UI for uploading custom curves
- **Full FTMS**: Indoor Bike Data (`0x2AD2`), Control Point (`0x2AD9`) for ERG mode, plus Cycling Power (`0x1818`) and CSC (`0x1816`)
- **HRM relay**: Ingests BLE HRM data and injects it into the FTMS packet — solves the Apple TV 2-device Bluetooth limit
- **Caveat**: Designed primarily as a BLE-to-BLE bridge (reads other BLE devices), not for direct GPIO sensor wiring. The motor/stepper control code is irrelevant to your use case

#### 🔧 eiffelpeter/esp32_ftms_indoor_bike — Best Architecture Match
- **Repo**: [github.com/eiffelpeter/esp32_ftms_indoor_bike](https://github.com/eiffelpeter/esp32_ftms_indoor_bike)
- **Why it's relevant**: **Exactly your hardware topology** — reed switch for cadence + potentiometer for resistance → FTMS broadcast
- **Power model**: Bivariate quadratic: $P(R, \text{RPM}) = A(R) \cdot \text{RPM}^2 + B(R) \cdot \text{RPM} + C$
- **Signal processing**: EMA filter on reed switch pulse intervals for stable RPM
- **Caveat**: Uses old BlueDroid BLE stack (not NimBLE), targets classic ESP32 only — needs porting

#### 🔌 snowzach/echbt — Echelon-Specific Protocol Knowledge
- **Repo**: [github.com/snowzach/echbt](https://github.com/snowzach/echbt)
- **Why it's relevant**: Contains the **Echelon-to-Peloton power regression formula** in `power.h` — the empirical constants for mapping Echelon's 32 resistance levels to real watts
- **Key formula**:
  - Peloton equivalent: $R_{\text{peloton}} = 1.8 \times R_{\text{echelon}} + 12$
  - Power: $P = (0.000185 \cdot R_{\text{peloton}}^{2.7} + 0.1) \times (\text{RPM}/60)^2 \times 60$
- **Caveat**: BLE client only (reads bike's Bluetooth, doesn't tap GPIO sensors), outputs to OLED display

---

## 5. Echelon EX-3 Internal Sensor Hardware

### What's Inside the Console

| Sensor | Type | Wiring | Signal |
|---|---|---|---|
| **Cadence** | Magnetic reed switch at pedal crank | 2-wire (GND + Signal) | Digital pulse per revolution |
| **Resistance** | 10kΩ linear rotary potentiometer on red knob | 3-wire (VCC + Wiper + GND) | Analog voltage proportional to knob position |

### The Wire-Splice Architecture

```
           ┌─────────────────────────────────────────────┐
           │         ECHELON EX-3 INTERNALS               │
           │                                               │
           │  [Reed Switch]          [10kΩ Potentiometer]  │
           │   GND ─┬─ Signal        VCC ─┬─ Wiper ─┬─ GND│
           │        │    │                │    │     │     │
           │        │    │    T-Tap       │    │     │     │
           │        │    ├────────┐       │    ├─────┤     │
           │        │    │        │       │    │     │     │
           │        │    ▼        ▼       │    ▼     │     │
           │   [Echelon PCB]  [ESP32-S3]  │ [ESP32]  │     │
           │   (unchanged)    D2   GND    │   A0    GND    │
           │                              │                │
           │         3.3V from ESP32 ─────┘  (if factory   │
           │         (or factory VCC)         VCC is 3.3V) │
           └───────────────────────────────────────────────┘
                              │
                    ESP32-S3 broadcasts
                   BLE FTMS (0x1826) to
                  Strava / Zwift / Garmin
```

> [!WARNING]
> **Before connecting anything**, measure the factory VCC on the potentiometer with a multimeter. If it reads **5V** instead of 3.3V, the wiper can output up to 5V — this will **fry your ESP32-S3**. In that case, add a 2:1 voltage divider (10kΩ + 10kΩ) between the wiper tap and pin A0.

---

## 6. Power Calculation Strategy

Since the Echelon EX-3 has no strain gauge, power must be **estimated** from resistance level + cadence RPM. Three viable approaches:

### Option A: Echelon/Peloton Regression (Quick Start)
Use the empirical constants from `snowzach/echbt`:

```
echelon_level = map(adc_reading, ADC_MIN, ADC_MAX, 1, 32)
peloton_equiv = 1.8 × echelon_level + 12
power_watts   = (0.000185 × peloton_equiv^2.7 + 0.1) × (RPM / 60)² × 60
```

**Pros**: Proven formula, quick to implement  
**Cons**: Accuracy depends on your specific bike matching the original calibration data

### Option B: Quadratic Model (eiffelpeter approach)
$$P(R, \text{RPM}) = (a_1 R + a_2) \cdot \text{RPM}^2 + (b_1 R + b_2) \cdot \text{RPM} + c$$

**Pros**: Smooth, tuneable  
**Cons**: Requires manual coefficient fitting

### Option C: 2D Lookup Table (SmartSpin2k approach)
A 32 × 8 matrix (32 resistance levels × 8 cadence nodes at 40–110 RPM) with bilinear interpolation.

**Pros**: Most accurate, can be calibrated against real power pedals  
**Cons**: Requires access to a reference power meter for calibration

> [!TIP]
> **Recommended approach**: Start with **Option A** (the Echelon regression formula) to get running quickly. Later, if you acquire power pedals (e.g. Favero Assioma), you can record reference data and build a proper lookup table.

---

## 7. Recommended Firmware Strategy

### Start From: `eiffelpeter/esp32_ftms_indoor_bike`
This project matches your exact sensor topology (reed switch + potentiometer → FTMS). However, it needs porting:

| What | Current State | What You Need |
|---|---|---|
| BLE stack | BlueDroid | **NimBLE** (saves ~150 KB RAM, required for dual-role HRM relay) |
| Target | Classic ESP32 | **ESP32-S3** (different ADC channels, USB-CDC serial) |
| Power formula | Generic quadratic | **Echelon-specific** regression from `echbt/power.h` |
| HRM relay | Not implemented | Borrow architecture from **SmartSpin2k** |
| Build system | Arduino IDE | **PlatformIO** |

### Pull From Other Projects

| Feature | Source Project |
|---|---|
| NimBLE FTMS service implementation | SmartSpin2k |
| Echelon power regression constants | snowzach/echbt (`power.h`) |
| HRM relay / BLE multiplexer pattern | SmartSpin2k |
| Reed switch debouncing + EMA filter | eiffelpeter/esp32_ftms_indoor_bike |
| Potentiometer ADC reading pattern | eiffelpeter/esp32_ftms_indoor_bike |

### PlatformIO Configuration for Your Waveshare Board

```ini
[env:waveshare_esp32_s3_nano]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_build.flash_mode = qio
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1

upload_protocol = esptool
monitor_speed = 115200

lib_deps =
    h2zero/NimBLE-Arduino
```

> [!CAUTION]
> **Do NOT use `board = arduino_nano_esp32`** — the Waveshare board does not have the Arduino DFU bootloader. PlatformIO would try to upload via `dfu-util` which will fail. Use `esp32-s3-devkitc-1` with `upload_protocol = esptool`.

---

## 8. BLE FTMS Packet Structure (What Your ESP32 Will Broadcast)

For Zwift/Strava/Garmin to recognise your bike as a smart trainer, you need to implement:

| GATT Characteristic | UUID | Type | Content |
|---|---|---|---|
| **Fitness Machine Feature** | `0x2ACC` | Read | Capability bitmask (cadence, resistance, power, HR) |
| **Indoor Bike Data** | `0x2AD2` | Notify @ 1–2 Hz | Flags + cadence + resistance + power + heart rate |
| **Control Point** | `0x2AD9` | Write/Indicate | ERG mode, target resistance (optional for v1) |

### Indoor Bike Data Packet Layout (`0x2AD2`)

```
Byte 0-1: Flags (uint16)
  Bit 2 = 1 → Cadence present (uint16, 0.5 RPM resolution)
  Bit 5 = 1 → Resistance present (sint16)
  Bit 6 = 1 → Power present (sint16, 1W resolution)
  Bit 9 = 1 → Heart Rate present (uint8, 1 BPM)

Byte 2+:  Data fields in order of set flag bits
```

### HRM Relay Architecture
The ESP32-S3 with NimBLE can run as **both Central and Peripheral simultaneously**:
- **Central**: Connects to your chest strap / Echelon Beat armband, reads HR notifications
- **Peripheral**: Broadcasts FTMS with HR embedded in the Indoor Bike Data packet
- **Result**: Zwift/Garmin sees power + cadence + HR over a **single BLE connection**

---

## 9. Next Steps

When you're ready to build, the path is:

1. **Set up PlatformIO project** in the project root with the config above
2. **Implement core firmware**: ADC reading (potentiometer), interrupt counting (reed switch), NimBLE FTMS server
3. **Flash & bench-test** on your desk — verify FTMS advertisement shows up in a BLE scanner app (nRF Connect)
4. **Open the bike console** — measure factory voltages with a multimeter
5. **Wire-splice** with Posi-Taps and connect the ESP32
6. **Calibrate** the power curve against known resistance levels

> [!NOTE]
> Want me to scaffold the PlatformIO project and write the initial firmware? I can create a working codebase that reads both sensors, calculates power using the Echelon regression formula, and broadcasts BLE FTMS — ready to flash to your Waveshare board.
