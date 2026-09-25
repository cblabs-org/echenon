# Echenon — Firmware Specification

**An open training computer for the Echelon EX-3 indoor bike.**

---

## 1. Project Overview

### 1.1 Purpose

Echenon transforms an Echelon EX-3 Smart Connect indoor bike into a
self-contained, open training computer by intercepting the bike's electrical
signals via a **Man-in-the-Middle (MITM) intercept board** and providing:

- A **session overlay** — a floating training computer UI showing live power,
  cadence, heart rate, charts, and session controls
- **Automatic session recording** with post-ride upload to Strava, Garmin
  Connect, and other fitness platforms
- A **BLE FTMS smart trainer broadcast** compatible with Zwift, TrainerRoad,
  Peloton Digital, and any FTMS-aware fitness app
- **Heart rate monitor relay** — connects to an HR strap and re-broadcasts to
  the Echelon tablet, the FTMS stream, and the session recording simultaneously
- **Automatic resistance control (ERG mode)** — FTMS Control Point support
  enables apps like Zwift and Xert to set resistance directly, with the ESP32
  driving the bike's resistance motor via closed-loop position control

Echenon operates in two modes:

- **Intercept Mode** — The Echelon console is connected. All signals pass
  through; the ESP32 reads them transparently and relays motor commands. The
  Echelon app runs unchanged. Echenon operates silently alongside.
- **Override Mode** — The Echelon console is disconnected. The ESP32 directly
  controls the resistance motor, reads the rotary encoder for manual input,
  and provides full ERG mode via FTMS Control Point. FTMS is the sole
  training interface.

For complete hardware documentation of the EX-3's internal sensors, wiring
looms, connectors, and actuators, see
[`docs/ex3_specification.md`](docs/ex3_specification.md). For the phase-by-phase
circuit build guide (breadboard schematics, intercept board, net list, assembly),
see [`docs/hardware_specification.md`](docs/hardware_specification.md). For the
iterative development roadmap and phase gates, see
[`docs/implementation-strategy.md`](docs/implementation-strategy.md). For
validated operating scenarios and acceptance criteria, see
[`docs/use_case_analysis.md`](docs/use_case_analysis.md).

### 1.2 Design Principles

1. **Non-destructive** — No factory wires are cut. The intercept board plugs
   into the existing JST connectors between the bike frame and console. Unplug
   to restore the bike to stock.
2. **Two operating modes** — **Intercept Mode** passes all signals through to
   the Echelon console (coexist). **Override Mode** operates standalone without
   the console (full control).
3. **Hands-free daily use** — Recording starts when you pedal and uploads when
   you stop. No buttons, no apps to open.
4. **Three independent outputs** — The session overlay, BLE FTMS broadcast, and
   cloud upload are completely decoupled. Each works independently of the others.
5. **Open ecosystem** — FTMS is always broadcasting. Data uploads to your choice
   of platforms. Nothing is locked down.
6. **Full signal control** — The intercept board sits between all sensors and
   the console. Every signal passes through the ESP32, which can read, relay,
   or override any of them.
7. **Better than stock** — Closed-loop motor positioning with potentiometer
   feedback provides faster, more precise resistance changes than the factory
   board. Override Mode supports up to 100 resistance levels (vs Echelon's 32).
8. **Original implementation** — Written from first principles, public
   specifications, and permissively licensed references.
9. **Hardware-portable** — All hardware-specific configuration (pin map, sensor
   characteristics, partition table, SDK settings) lives in a single target
   directory under `hardware/`. Supporting a different ESP32 board or a
   different bike is a new target directory + PlatformIO environment, not a
   code change. Firmware modules use `#if HAS_*` guards so only applicable
   modules compile for each target.

### 1.3 Built On

Echenon is built as a module on the
[ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation),
inheriting WiFi management, the web portal, encrypted credential storage, OTA
updates, the module composition architecture, and the new **BLE Manager**
infrastructure module (merged in framework `≥ v0.0.12-dev.82`).

Uses `esp-nimble-cpp` (`h2zero/esp-nimble-cpp@^2.1.0`) for BLE stack.


### 1.4 Out of Scope (v1)

- On-device physical display
- Custom PCB manufacture (v1 uses perfboard or protoboard)
- Automatic mode switching (Intercept ↔ Override is a software configuration)
- Portal-native HR-zone auto-resistance (v2 — for v1, external apps provide
  HR-zone-based ERG via FTMS Control Point)

---

## 2. Licensing & Attribution

### 2.1 Project License

Apache License 2.0, compatible with all dependencies.

### 2.2 Dependencies

| Library | License | Usage |
|---|---|---|
| [ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation) | Apache-2.0 | Framework (WiFi, web, config, OTA, auth, BLE) |
| [h2zero/esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Apache-2.0 | BLE stack (via Foundation BLEManager) |
| Espressif Arduino Core for ESP32 | Apache-2.0 | Hardware abstraction |

### 2.3 Reference Implementations

The BLE FTMS GATT service structure references the MIT-licensed
[jamesjmtaylor/esp32-ftms-server](https://github.com/jamesjmtaylor/esp32-ftms-server)
and [dbsqp/bluetooth-reebok-57e](https://github.com/dbsqp/bluetooth-reebok-57e).
MIT copyright notices are preserved in adapted source files.

The power calculation model is derived from the physics of magnetic eddy-current
braking and publicly documented power curve data. The BLE FTMS packet format
follows the Bluetooth SIG Fitness Machine Service v1.0 public specification.

---

## 3. System Architecture

### 3.1 Three Independent Output Paths

Echenon has three output paths. They are **completely independent** — each works
regardless of whether the others are active, connected, or configured.

```
 Sensors → PowerEngine → SessionManager
                               │
               ┌───────────────┼───────────────┐
               ▼               ▼               ▼

          📊 OVERLAY      📡 BLE FTMS     ☁️ UPLOAD
          (always)        (always on)     (post-session)

     Session control    Broadcasts to    Pushes completed
     Live metrics       any app that     sessions to one
     Charts & graphs    connects, or     or more cloud
     HR zones           to nobody        services

     Floats over        Zwift            Strava ──► Xert*
     ANY app:           TrainerRoad      Garmin Connect
     - Echelon          Xert EBC         TrainingPeaks
     - Zwift            Peloton          Intervals.icu
     - Xert EBC         Wahoo SYSTM
     - Peloton          Garmin watch
     - YouTube          Apple Watch
     - Nothing          (or nobody)

* Xert auto-syncs from Strava — no direct Echenon integration needed.
  Connect Xert to Strava once in Xert Online settings.
```

| Path | What it does | When | Knows about the others? |
|---|---|---|---|
| **Overlay** | Live training computer UI — session control, metrics, charts | Always available via web portal | No |
| **FTMS** | Broadcasts standard BLE fitness data | Always advertising | No |
| **Upload** | Pushes finished sessions to cloud platforms | After session ends | No |

### 3.2 High-Level Block Diagram

```
┌────────────────────────── ECHELON EX-3 BIKE FRAME ──────────────────────────┐
│                                                                              │
│  [Reed Switch]    [Potentiometer]    [DC Motor]          [Rotary Encoder]    │
│  (cadence)        (position)         (resistance)        (red knob)         │
│  Orange + Grey    Yellow/Blue/Green  Red + Black         Red/Brown/Orange   │
│       │                │                 │                    │              │
│  via 9-pin JST-SM                   via 9-pin JST-SM    via 3-pin JST      │
└───────┼────────────────┼─────────────────┼────────────────────┼─────────────┘
        │                │                 │                    │
┌───────┼────────────────┼─────────────────┼────────────────────┼─────────────┐
│       │         ECHENON INTERCEPT BOARD  │                    │              │
│       │                │                 │                    │              │
│       │  ┌─────────────┼─────────────────┼────────────────────┼───────┐     │
│       │  │             │      ESP32-S3-NANO                   │       │     │
│       │  │             │                 │                    │       │     │
│       │  │  D2 (GPIO 5) ◄── Cadence ────┼─ PASS-THROUGH ──────────────┐    │
│       │  │  A0 (GPIO 1) ◄── Pot wiper ──┼─ PASS-THROUGH ──────────┐  │    │
│       │  │  D5 (GPIO 8) ◄── Encoder A ──┼─────────────────────┘   │  │    │
│       │  │  D6 (GPIO 9) ◄── Encoder B ──┼─────────────────────    │  │    │
│       │  │                               │                    │   │  │    │
│       │  │  D7 (GPIO 10) ◄── Console Motor+ sense ────────── │ ──│──│─┐  │
│       │  │  D8 (GPIO 17) ◄── Console Motor- sense ────────── │ ──│──│─┤  │
│       │  │       │                                            │   │  │ │  │
│       │  │  [Software: Intercept=relay, Override=ignore]      │   │  │ │  │
│       │  │       │                                            │   │  │ │  │
│       │  │  D3 (GPIO 6) ──► L9110S A-IA ──┐                  │   │  │ │  │
│       │  │  D4 (GPIO 7) ──► L9110S A-IB ──┤                  │   │  │ │  │
│       │  │                                │                   │   │  │ │  │
│       │  │  ┌──────────────────────────┐  │                   │   │  │ │  │
│       │  │  │     ECHENON FIRMWARE     │  │                   │   │  │ │  │
│       │  │  │                          │  │                   │   │  │ │  │
│       │  │  │  CadenceSensor ─────┐    │  │                   │   │  │ │  │
│       │  │  │  ResistanceSensor ─────┤    │  │                   │   │  │ │  │
│       │  │  │  ResistanceEncoder ─┤    │  │                   │   │  │ │  │
│       │  │  │         ┌───────────┘    │  │                   │   │  │ │  │
│       │  │  │         ▼                │  │                   │   │  │ │  │
│       │  │  │  PowerEngine ──► SessionMgr │                   │   │  │ │  │
│       │  │  │         │          │  │  │  │                   │   │  │ │  │
│       │  │  │  HRMClient ► HRMRepeater │  │                   │   │  │ │  │
│       │  │  │                    │  │  │  │                   │   │  │ │  │
│       │  │  │  InterceptCtrl ────┤  │  │  │                   │   │  │ │  │
│       │  │  │  ERGController ────┤  │  │  │                   │   │  │ │  │
│       │  │  │  ResistanceMotor ──┘  │  │  │                   │   │  │ │  │
│       │  │  │         ┌─────────────┘  │  │                   │   │  │ │  │
│       │  │  │         ▼     ▼     ▼    │  │                   │   │  │ │  │
│       │  │  │  📊 Overlay  📡 FTMS  ☁️ Upload                │   │  │ │  │
│       │  │  └──────────────────────────┘  │                   │   │  │ │  │
│       │  └────────────────────────────────┘                   │   │  │ │  │
│       │                                                       │   │  │ │  │
│  ┌────┼───────────────────────────────────────────────────────┤   │  │ │  │
│  │    │            L9110S H-BRIDGE                             │   │  │ │  │
│  │    │  VCC ◄── 3.3V    A-OA ──► Motor+ (Red)               │   │  │ │  │
│  │    │  GND ◄── GND     A-OB ──► Motor- (Black)             │   │  │ │  │
│  └────┼───────────────────────────────────────────────────────┘   │  │ │  │
│       │                                                           │  │ │  │
│  ┌────┼── 9V→3.3V Buck (MP1584EN) ──┬──► ESP32 3V3 pin (direct)  │  │ │  │
│  │    │                              ├──► L9110S VCC              │  │ │  │
│  │    │                              └──► Sensor power rail       │  │ │  │
│  └────┼───────────────────────────────────────────────────────────┘  │ │  │
│       │                                                              │ │  │
│  ═════╪═══════ 9-pin JST-SM (console side) ══════════════════════════╧═╧═╡  │
│       │         7 wires pass through (power + cadence + pot)          │  │
│       │         2 wires to console motor sense GPIO inputs            │  │
│  ═════╪═══════ 3-pin JST (console side) ═════════════════════════════════╡  │
│                 3 wires pass through (encoder)                           │
└──────────────────────────────────────────────────────────────────────────┘
         │              │                │
    WiFi (HTTP)    BLE FTMS        WiFi (HTTPS)
         │         (0x1826)              │
         ▼         + HR (0x180D)         ▼
   [Floating       + Control Point  [Strava API]
    Dashboard]     (0x2AD9)         [Garmin API]
                       │            [TrainingPeaks]
                       ▼
                  [Zwift / TR /
                   Xert EBC]
```

**Signal routing summary:**

| 9-Pin Wire | Colour | Bike → ESP32 | Bike → Console | Motor Path |
|---|---|---|---|---|
| Power +9V | White | Via buck converter | Pass-through | — |
| GND | Brown | Common | Pass-through | — |
| Cadence | Orange | GPIO 5 (tap) | Pass-through | — |
| Cadence | Grey | GND reference | Pass-through | — |
| Pot outer | Yellow | — | Pass-through | — |
| Pot wiper | Blue | GPIO 1 ADC (tap) | Pass-through | — |
| Pot outer | Green | — | Pass-through | — |
| **Motor+** | **Red** | **L9110S A-OA → motor** | **GPIO 6 (ADC1 analog sense)** | Split |
| **Motor−** | **Black** | **L9110S A-OB → motor** | **GPIO 7 (ADC1 analog sense)** | Split |

Seven wires pass straight through to the console. The two motor wires are
**split** — the bike-side motor connects to the L9110S outputs; the
console-side motor signals route to ESP32 GPIO inputs (with 10kΩ pull-downs).

### 3.3 Data Flow

1. **Every pedal revolution**: Reed switch triggers a hardware interrupt on
   GPIO 5. The ISR records the timestamp for cadence calculation.
2. **Every 50 ms**: ADC samples the resistance potentiometer wiper on GPIO 1,
   applies an EMA filter, and maps the voltage to resistance level (1–32 in
   Intercept Mode, 1–100 in Override Mode).
3. **Continuously**: BLE Central client receives HR notifications from the
   paired armband. The HR value is:
   - Re-broadcast via BLE HR Service for the Echelon tablet app
   - Embedded in the FTMS Indoor Bike Data notifications
   - Stored in the session recording buffer
4. **Every 500 ms** (2 Hz):
   - Power engine calculates watts from cadence + resistance
   - FTMS server sends a BLE notification (consumed by Zwift, or nobody)
   - Session manager appends a data point to the ride buffer
   - Web overlay receives a WebSocket update with live metrics
5. **Motor control loop** (20 Hz, every 50 ms):
   - **Intercept Mode**: Read console motor sense pins (GPIO 10/11), mirror
     state to L9110S outputs (GPIO 6/7). Console drives motor via ESP32 relay.
   - **Override Mode**: `InterceptController` selects the active command
     source — encoder click (manual), ERG target (FTMS Control Point), or
     web overlay command. Position controller drives motor toward target
     potentiometer value, stops when within dead zone.
6. **On encoder click** (interrupt-driven):
   - Quadrature decoder on GPIO 8/9 determines direction and increment.
   - In Override Mode: adjusts target resistance level, triggers motor.
   - In Intercept Mode: encoder signals pass through to console; ESP32
     reads them for telemetry but does not act on them.
7. **On FTMS Control Point write** (Override Mode only):
   - ERG controller receives target watts from Zwift/Xert/TrainerRoad.
   - Looks up required resistance level from the power table given current
     cadence.
   - Sets new motor target position, continuously adjusts as cadence changes.
8. **On session end**: Upload manager generates a TCX file and pushes it to
   each configured cloud service.

### 3.4 Module Decomposition

**Echenon application modules:**

| Module | Responsibility | Foundation Interface |
|---|---|---|
| `EchenonModule` | Composition root — owns all sub-components | `ITickable`, `IConfigurable`, `IWebContributor`, `IDataSource` |
| `CadenceSensor` | Interrupt-driven reed switch, RPM calculation | — |
| `ResistanceSensor` | ADC sampling of actuator potentiometer, EMA filter, level mapping | — |
| `ResistanceEncoder` | Quadrature decode of the red resistance knob (3-pin JST) | — |
| `ResistanceMotor` | L9110S H-bridge driver, direction/PWM control, position feedback loop | — |
| `InterceptController` | Mode manager (Intercept ↔ Override), console motor relay, signal routing | — |
| `ERGController` | FTMS Control Point handler, target watts → resistance level closed-loop | — |
| `PowerEngine` | Watts calculation, calibration coefficients | — |
| `FTMSServer` | FTMS GATT service, Indoor Bike Data + Control Point | `IBLEServiceProvider` |
| `HRMClient` | BLE Central → connect to HR strap | `IBLEClientProvider` |
| `HRMRepeater` | Re-broadcast HR as BLE HR Service | `IBLEServiceProvider` |
| `SessionManager` | Auto/manual start-stop, ride data buffering | — |
| `RideStore` | PSRAM ring buffer for data points | — |
| `SessionOverlay` | Web dashboard — live metrics, charts, controls | `IWebContributor` |
| `UploadManager` | Post-session TCX generation and cloud upload | `ITickable`, `IConfigurable`, `IWebContributor` |
| `StravaUploader` | Strava OAuth 2.0 + activity upload | `IUploader`, `IDataSource` |

Note: `HRMClient` publishes HR events instead of directly calling `_hrmRepeater.publishHr()`.


### 3.5 Foundation Integration

**Module registration (ADR-16: Static Scope):**

```cpp
// main.cpp
#include "AppFoundation.h"
#include "echenon/EchenonModule.h"
#include "BLEManager/BLEManager.hpp"

static AppFoundation app;
static EchenonModule echenon;
static BLEManager bleManager;

void setup() {
    bleManager.registerServiceProvider(&ftmsServer);
    bleManager.registerServiceProvider(&hrmRepeater);
    bleManager.registerClientProvider(&hrmClient);
    app.registerModule(&bleManager);
    app.registerModule(&echenon);
    app.begin();
    bleManager.begin(app);
}
```

Redactor must be re-registered on every token refresh, and `RegisterResult` must be checked.
Use `esp-nimble-cpp@^2.1.0` library and pin framework submodule to `≥ v0.0.12-dev.82`.
The framework's `IDataSource::fetchUpdate()` returns `UpdateStatus` (not `FetchResult`), and `IWebContributor::registerRoutes()` takes `PsychicHttpServer&` (not `WebServer&`).


### 4.1 Target Board: Waveshare ESP32-S3-Nano

| Spec | Value |
|---|---|
| SoC | ESP32-S3R8 (dual-core Xtensa LX7 @ 240 MHz) |
| Flash | 16 MB Quad-SPI |
| PSRAM | 8 MB Octal-SPI |
| Bluetooth | BLE 5.0 (NimBLE via Foundation BLEManager) |
| WiFi | 2.4 GHz 802.11 b/g/n |
| USB | Type-C (native USB-CDC for serial/debug) |
| Logic level | **3.3V — NOT 5V tolerant** |
| Form factor | Arduino Nano footprint (30-pin, 2.54mm headers) |

### 4.2 Target Deployment Environment

| Component | Detail |
|---|---|
| **Tablet** | Lenovo Tab P12 (12.7" 3K screen, Android) |
| **Floating window** | Android's freeform/split-screen PiP mode — Chrome can be resized to a small overlay window on top of full-screen apps |
| **Bike placement** | Tablet on the bike's handlebar tablet holder |
| **ESP32 placement** | On the intercept board inside the bike's console housing, powered by 9V rail via MP1584EN buck converter (9V→3.3V) |

The Lenovo P12 runs both the Echelon class app (full-screen) and Chrome
showing `echenon.local` (floating window) simultaneously. This is a native
Android capability — no root or special app required.

### 4.3 Hardware Configuration Architecture

The firmware's hardware layer is configured per **target** — a specific
board × bike combination. Each target is a directory under `hardware/`
containing everything that varies between physical setups:

```
hardware/
└── ex3-waveshare-nano/              ← one directory per physical setup
    ├── config.h                     pin map, bike characteristics, module flags
    ├── partitions.csv               flash partition table for this board
    └── sdkconfig.defaults           SDK overrides (PSRAM, USB, coex flags)
```

Unify hardware-config model: Use `-DBOARD_HEADER=...board.h`, `env:esp32s3nano` base, `um_nanos3` board ID, octal-PSRAM sdkconfig.

```c
// hardware/ex3-waveshare-nano/config.h

// ── Identity ──
#define TARGET_NAME             "EX-3 on Waveshare ESP32-S3-Nano"
#define BIKE_NAME               "Echelon EX-3"
#define BOARD_NAME              "Waveshare ESP32-S3-Nano"

// ── Signal map (what signals exist + which GPIO) ──
#define HAS_CADENCE             1
#define PIN_CADENCE             5         // D2, reed switch

#define HAS_RESISTANCE_POT     1
#define PIN_POT_WIPER           1         // A0, ADC1_CH0

#define HAS_MOTOR_CONTROL       1
#define PIN_MOTOR_FWD           6         // D3, PWM capable
#define PIN_MOTOR_REV           7         // D4, PWM capable
#define BIKE_MOTOR_VOLTAGE      [DR-1: measure stock drive voltage]

#define HAS_MOTOR_SENSE         1
#define PIN_MOTOR_SENSE_P       6        // ADC1
#define PIN_MOTOR_SENSE_N       7        // ADC1
// Motor sense is analog at >= 1kHz, not digital. Divider is for protection only.

#define HAS_ENCODER             1
#define PIN_ENCODER_A           8         // D5, interrupt
#define PIN_ENCODER_B           9         // D6, interrupt
```


### 4.4 Pin Assignments (Echelon EX-3 × Waveshare ESP32-S3-Nano)

The following table is the cross-product of the current board and bike configs:

| Function | Header | GPIO | Direction | Type | Notes |
|---|---|---|---|---|---|
| Cadence input | D2 | GPIO 5 | Input | Digital interrupt (FALLING) | ADC1_CH4, no strapping conflicts |
| Pot wiper (resistance position) | A0 | GPIO 1 | Input | ADC1 Channel 0, 12-bit | Safe with BLE + WiFi active |
| L9110S A-IA (motor forward) | D3 | GPIO 6 | Output | PWM | Motor direction control |
| L9110S A-IB (motor reverse) | D4 | GPIO 7 | Output | PWM | Motor direction control |
| Encoder Channel A | D5 | GPIO 8 | Input | Digital interrupt (CHANGE) | Quadrature decode |
| Encoder Channel B | D6 | GPIO 9 | Input | Digital interrupt (CHANGE) | Quadrature decode |
| Console motor+ sense | ADC1 | GPIO 6 | Input | Analog (ADC1) | Reads console's motor drive signal |
| Console motor− sense | ADC1 | GPIO 7 | Input | Analog (ADC1) | Reads console's motor drive signal |
| Encoder VCC sense | ADC1 | GPIO 4 | Input | Analog (ADC1) | |
| Common ground | GND | — | — | Power | Left header pin 4 |
| Power input | 3V3 | Pin 2 | — | 3.3V from buck converter | Direct to SoC, bypasses onboard LDO |


### 4.5 Echelon EX-3 Internal Hardware

For complete hardware documentation of the EX-3's internal sensors, wiring
looms, connectors, actuators, and PCB details, see
[`docs/ex3_specification.md`](docs/ex3_specification.md).

**Key facts for firmware design:**

| Component | EX-3 Spec Detail |
|---|---|
| **Cadence sensor** | 2-wire magnetic reed switch at pedal crank. One pulse per revolution. 3.3V logic (Orange + Grey on 9-pin loom) |
| **Resistance potentiometer** | Rotary pot inside the lower actuator hub — **not** on the red knob. ~5.4 kΩ per 180° sweep. Measured 1.16–1.7V across terminals (Yellow/Blue/Green on 9-pin loom) |
| **Resistance motor** | 3.3V DC motor inside the actuator hub. Drives the magnetic brake carriage. Works at up to 9V but nominal is 3.3V (Red + Black on 9-pin loom) |
| **Resistance knob** | Incremental quadrature rotary encoder on the handlebars. 3.3V logic, two channels (Red/Brown/Orange on separate 3-pin JST) |
| **Main wiring loom** | 9-pin JST-SM connector carrying all signals between frame and console |
| **Encoder loom** | Separate 3-pin JST connector from the red knob to the console |

### 4.6 Intercept Board Wiring Plan

The intercept board sits between the bike's two JST connectors and the Echelon
console. No factory wires are cut — connectors plug in and out to restore stock.

Note: R3, R4, R5 and the Green-to-GND link must be **switchable** (jumpers minimum, high-side switch preferred). Values not final until DR-1.
ESP32 internal or external pull-downs needed on ENC_A/ENC_B for standalone mode.

**Connectors on the intercept board:**
(Omitted full table for brevity, but same physical layout)


### 4.7 Voltage Safety

> **ESP32-S3 ADC pins accept max 3.3V. Exceeding this causes permanent damage.**

All EX-3 sensor signals operate at 3.3V logic.
Motor sense must be read as analog on ADC1, it is not safe for digital input due to voltage levels.
The only voltage above 3.3V in the system is the 9V DC power rail, which is
handled by the buck converter on the intercept board and never touches any
GPIO pin.


### 4.8 Parts List

| Item | Qty | Status | Cost |
|---|---|---|---|
| Waveshare ESP32-S3-Nano | 1 | Owned | — |
| L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | Owned | — |
| GTIWUNG MP1584EN adjustable buck converter module (9V→3.3V) | 1 | Owned (10-pack) | — |
| JST-SM 9-pin socket + plug pair | 1 | To purchase | £3 |
| JST 3-pin socket + plug pair | 1 | To purchase | £1 |
| Perfboard / protoboard (70×50mm) | 1 | To purchase | £2 |
| 10kΩ pull-downs (R1, R2) for console motor sense | 2 | To purchase | £0.10 |
| 10kΩ resistor (R3: cadence pull-up — standalone sensor power) | 1 | To purchase | £0.05 |
| 100Ω resistor (R4: pot reference — contention protection) | 1 | To purchase | £0.05 |
| 10Ω resistor (R5: encoder power — contention protection) | 1 | To purchase | £0.05 |
| 100 nF ceramic capacitors (C1: cadence debounce, C2: pot filter) | 2 | To purchase | £0.20 |
| 30-pin female header strips (ESP32 carrier) | 2 | To purchase | £1 |
| Screw terminal blocks (2-pin, motor output) | 1 | To purchase | £0.50 |
| Hookup wire (22 AWG, assorted colours) | — | To purchase | £3 |


---|---|---|---|
| Waveshare ESP32-S3-Nano | 1 | Owned | — |
| L9110S Dual-Channel H-Bridge Motor Driver Module | 1 | Owned (5-pack, Amazon order #206-0379283-8022750, Dec 2024) | — |
| GTIWUNG MP1584EN adjustable buck converter module (9V→3.3V) | 1 | Owned (10-pack) | — |
| JST-SM 9-pin socket + plug pair | 1 | To purchase | £3 |
| JST 3-pin socket + plug pair | 1 | To purchase | £1 |
| Perfboard / protoboard (70×50mm) | 1 | To purchase | £2 |
| 10kΩ resistors (R1, R2: console motor sense pull-downs) | 2 | To purchase | £0.10 |
| 10kΩ resistor (R3: cadence pull-up — standalone sensor power) | 1 | To purchase | £0.05 |
| 100Ω resistor (R4: pot reference — contention protection) | 1 | To purchase | £0.05 |
| 10Ω resistor (R5: encoder power — contention protection) | 1 | To purchase | £0.05 |
| 100 nF ceramic capacitors (C1: cadence debounce, C2: pot filter) | 2 | To purchase | £0.20 |
| 30-pin female header strips (ESP32 carrier) | 2 | To purchase | £1 |
| Screw terminal blocks (2-pin, motor output) | 1 | To purchase | £0.50 |
| Hookup wire (22 AWG, assorted colours) | — | To purchase | £3 |

For detailed component designators (R1–R5, C1–C2, U1–U3, J1–J6) and circuit
connections, see [`docs/hardware_specification.md`](docs/hardware_specification.md).

---

## 5. Sensor Acquisition

### 5.1 Cadence Sensor

One active-low pulse per pedal revolution. Cadence from inter-pulse interval.

- GPIO 5 (D2): `INPUT` mode, hardware interrupt on `FALLING` edge
- Software debounce lockout: **25 ms**
- RPM: $\text{RPM} = 60{,}000{,}000 \div \Delta t_{\mu s}$
- Timeout: 3 seconds of no pulse → cadence = 0
- Hardware debounce: 100 nF capacitor (C1) between D2 and GND
- Pull-up: 10kΩ (R3) from 3.3V rail to cadence signal — provides the pull-up
  voltage for the reed switch. Works in parallel with the console's own pull-up
  when connected, and provides standalone power when the console is disconnected.

### 5.2 Resistance Position Sensor

- State that the actuator is a continuously rotating cam, not a linear slider. The pot sweeps through its range non-monotonically over a full revolution.
- Pot voltage does not uniquely identify a resistance — the cam half must be tracked.
- `[DR-1: measure wiper range over full revolution]`
- Two ADC paths: a fast unfiltered path (≥200 Hz) for the control loop, and a display EMA path (20 Hz, α=0.15) for the UI.
- Click-off detection: a discontinuity in the ADC stream marks half-revolution boundaries.


### 5.3 Resistance Level Mapping

Replaced linear mV-to-level formula with angle-based mapping within the working half-revolution. "100 evenly spaced levels" is a goal to validate in Phase 3b, not a spec fact.
The calibration method must find the resistance extremes by sweeping through a full revolution.


### 5.4 Resistance Encoder

Replaced the hand-rolled ISR decoder with a reference to the framework's `EncoderModule` (PCNT hardware peripheral with glitch filter). Use `BoardEncoder` descriptor in the board header. Record counts-per-detent from Phase 0.


### 5.5 Resistance Motor Control

Rewrite the controller architecture:
- Track cam angle (0–360°) across the full revolution. Combine pot sweep with a half-revolution counter updated on click-off events and motor drive direction.
- Working range: the half-revolution from cam-min to cam-max. Never cross an extreme.
- Approach direction: choose by angle, add brake-and-approach profile (PWM slow-down).
- Settle/retry: hysteresis and settle limit. Command unsatisfied after ~1 full revolution → stop, flag angle invalid, re-home.
- Run-time bound: `[DR-1: measure from Phase 0 Appendix A #3]`.
- Reset-safe driver inputs: 10kΩ pull-downs on IA and IB.
- Pot plausibility: stuck/open-circuit/out-of-range → stop motor and fault.
- IRAM safety: motor stop/start decisions must be IRAM-safe for immunity to SPI-flash erase stalls.
- Defer specific loop rate, dead zone, and PWM profile values to `[DR-1]`.


### 5.6 Operating Modes

The `InterceptController` manages the operating mode.

#### Intercept Mode (Echelon Console Connected)

All signals pass through to the console.
Fail-safe discussion: Recommend NC relay/analog-switch bypass (audit H-05 option a). Start-up race condition requires fail-safe state to pass through by default. `[DR-1: decision H-05]`.

#### Override Mode (Echelon Console Disconnected or Ignored)

The ESP32 directly controls resistance.
Override Mode standalone start-up: Store last known half-revolution in NVS. At boot, verify with a small move. Never run a full homing sweep while there is cadence. Override must not desynchronise the console.

**ERG Mode State Machine:**
States: `ACTIVE`, `SUSPENDED_BY_RIDER`, `RELEASED`.
A knob turn suspends ERG. Define Fitness Machine Status notifications on each transition.
Add cadence smoothing and setpoint deadband.


## 6. Power Calculation Engine

### 6.1 Physical Basis

Magnetic eddy-current brake. Neodymium magnets adjacent to aluminium flywheel.
Power ∝ cadence² at fixed resistance.

### 6.2 Mathematical Model

Re-indexed on cam angle within the working half-revolution.

$$P(R, \text{RPM}) = A(R) \times \text{RPM}^2 + B(R) \times \text{RPM}$$

$$A(R) = a_2 R^2 + a_1 R + a_0 \qquad B(R) = b_1 R + b_0$$

Placeholder coefficients (`[DR-1: re-derive from real data]` as these give ~27W at R=20/80rpm vs 191W expected).

Power clamped: $\max(0, P)$.


### 6.3 Calibration

Three calibration tiers are available, from coarsest to most precise:

#### Tier 1 — Physics Defaults (Zero Effort)

The default coefficients (§6.2) are placeholder defaults with known-bad values. They will be updated based on Phase 0 measurements `[DR-1: re-derive from real data]`.

#### Tier 2 — Community Power Matrix (Recommended)

A **power lookup table** mapping (resistance level × cadence) → watts, derived
from publicly documented empirical measurements of Echelon EX-series bikes. Note: There is a UK/EU database right risk for imported tables.

The web portal **Calibration** page provides a dropdown to select from
pre-built power matrices.

#### Tier 3 — Reference Power Meter (Most Precise)

For users with a reference power meter, the portal provides a guided calibration workflow. Power data acquisition WO is added to PR-02.

#### Potentiometer Two-Point Calibration (Required, All Tiers)

Regardless of power calibration tier, the potentiometer range must be
calibrated once so the ADC voltage maps correctly to resistance levels.
Maps to levels 1–32 in Intercept, 1–100 in Override.
This is triggered via the web portal Calibration page: find extremes by sweeping through a full revolution.


### 6.4 NVS Storage

| Key | Type | Default | Description |
|---|---|---|---|
| `cal_mv_min` | uint16 | `[DR-1]` | ADC mV at minimum resistance |
| `cal_mv_max` | uint16 | `[DR-1]` | ADC mV at maximum resistance |
| `cal_pot_done` | bool | false | Potentiometer calibration completed |
| `cal_power_tier` | uint8 | 1 | Active power tier (1=physics, 2=matrix, 3=reference) |
| `cal_a0` – `cal_b1` | float × 5 | see §6.2 | Polynomial coefficients (tiers 1 & 3) |
| `cal_matrix_id` | string | `""` | Selected pre-built matrix ID (tier 2) |
| `cal_matrix_custom` | blob | — | Custom CSV matrix uploaded via portal (tier 2) |
| `op_mode` | uint8 | 0 | Operating mode (0=Intercept, 1=Override) |
| `op_max_levels` | uint8 | 32 | Max resistance levels (32 in Intercept, 100 in Override) |
| `motor_dead_zone` | uint16 | `[DR-1]` | Motor position dead zone in mV |


---|---|---|---|
| `cal_mv_min` | uint16 | 1160 | ADC mV at minimum resistance |
| `cal_mv_max` | uint16 | 1700 | ADC mV at maximum resistance |
| `cal_pot_done` | bool | false | Potentiometer calibration completed |
| `cal_power_tier` | uint8 | 1 | Active power tier (1=physics, 2=matrix, 3=reference) |
| `cal_a0` – `cal_b1` | float × 5 | see §6.2 | Polynomial coefficients (tiers 1 & 3) |
| `cal_matrix_id` | string | `""` | Selected pre-built matrix ID (tier 2) |
| `cal_matrix_custom` | blob | — | Custom CSV matrix uploaded via portal (tier 2) |
| `op_mode` | uint8 | 0 | Operating mode (0=Intercept, 1=Override) |
| `op_max_levels` | uint8 | 32 | Max resistance levels (32 in Intercept, 100 in Override) |
| `motor_dead_zone` | uint16 | 5 | Motor position dead zone in mV |

---

## 7. BLE FTMS Server

### 7.1 Overview

The FTMS server is one of the three independent output paths. It broadcasts
standard BLE fitness data continuously. Any compatible app (Zwift, TrainerRoad,
Garmin, etc.) can connect at any time — or nothing connects. The session
recording and overlay work regardless.

Implements the **Bluetooth SIG Fitness Machine Service v1.0** (`0x1826`).

The MIT-licensed [esp32-ftms-server](https://github.com/jamesjmtaylor/esp32-ftms-server)
and [bluetooth-reebok-57e](https://github.com/dbsqp/bluetooth-reebok-57e) serve
as GATT structure references.

### 7.2 Foundation BLE Integration

`FTMSServer` implements `IBLEServiceProvider` from the Foundation's BLEManager module. It also implements `ITickable` for its own 2 Hz notification timer.

`BLEManager` is instantiated statically at file scope in `main.cpp`. There is no `getBLEManager()` accessor on `Application`. The composition pattern is:

```cpp
// main.cpp — static composition
#include "BLEManager/BLEManager.hpp"
static BLEManager bleManager;

void setup() {
    bleManager.registerServiceProvider(&ftmsServer);
    bleManager.registerServiceProvider(&hrmRepeater);
    bleManager.registerClientProvider(&hrmClient);
    app.registerModule(&bleManager);
    app.begin(config);
    bleManager.begin(app);
}
```

Provider registration is pre-lock only. After `bleManager.begin(app)`, `_locked = true` and registration is rejected.

**Built-in DeviceInfoServiceProvider (DIS, `0x180A`).** The framework auto-creates DIS with manufacturer, model, serial, firmware/hardware/software revision. Echenon MUST NOT create a duplicate `0x180A` service.

`ScanInhibit` is an RAII token: `auto token = bleManager.inhibitScanning("strava_upload");` — move-only, thread-safe. Releasing the last token resumes reconnect schedules.

All provider callbacks (`onPeerConnected`, `onNotification`, etc.) are dispatched on the main loop via queue — no mutex needed inside providers.


### 7.3 GATT Services

**Fitness Machine Service (`0x1826`):**

| Characteristic | UUID | Properties |
|---|---|---|
| Fitness Machine Feature | `0x2ACC` | Read |
| Indoor Bike Data | `0x2AD2` | Notify @ 2 Hz |
| Supported Resistance Level Range | `0x2AD6` | Read |
| Supported Power Range | `0x2AD8` | Read |
| Fitness Machine Control Point | `0x2AD9` | Write, Indicate |
| Fitness Machine Status | `0x2ADA` | Notify |

**Fitness Machine Feature flags (`0x2ACC`):**

| Feature | Bit | Set? | Notes |
|---|---|---|---|
| Cadence Supported | 1 | Yes | |
| Resistance Level Supported | 7 | Yes | |
| Power Measurement Supported | 14 | Yes | |
| Heart Rate Measurement Supported | 10 | Yes | If HRM connected |
| Target Resistance Level Config Supported (Target Settings) | 2 (byte 4) | Yes | Enables Set Target Resistance |
| Target Power Config Supported (Target Settings) | 3 (byte 4) | Yes | Enables Set Target Power (ERG mode) |

Clear target-setting bits in the Feature characteristic while in Intercept Mode.

**FTMS provider must implement `IBLEServiceProvider`:**
```cpp
class FTMSServer : public IBLEServiceProvider, public ITickable {
    const char* getProviderId() const override { return "ftms"; }
    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                                uint16_t charUuid16, bool notify, bool indicate) override;
    void tick(uint32_t currentMillis) override;
};
```

**Advertising with priority-based packing:**
```cpp
void FTMSServer::contributeAdvertising(BLEAdvertisingBuilder& adv) {
    adv.addServiceUuid16(0x1826, BLEAdvertisingBuilder::Priority::Required);
    adv.addServiceData16(0x1826, serviceData, sizeof(serviceData),
                         BLEAdvertisingBuilder::Priority::Required);
}
```
Note: 31-byte legacy advertising only, no extended advertising. `Required` elements must fit or a precondition fault is raised.

**Add Fitness Machine Status characteristic (`0x2ADA`, Notify)** — needed for ERG state machine notifications.


### 7.4 FTMS Control Point (`0x2AD9`)

The Control Point characteristic enables FTMS clients (Zwift, Xert, TrainerRoad)
to command resistance changes. Only processed in **Override Mode** — in
Intercept Mode, the console controls resistance, so Control Point writes
respond with result code `0x05` (Control Not Permitted) and no motor action
occurs. The FTMS client should treat Echenon as a read-only data source in
Intercept Mode.

**Supported opcodes:**

| Opcode | Name | Parameter | ESP32 Action |
|---|---|---|---|
| `0x00` | Request Control | — | Required. Control Point commands return `0x05` without it. |
| `0x04` | Set Target Resistance Level | uint8 (0.1 resolution) | FTMS v1.0 defines UINT8 with 0.1 resolution (max 25.5); some clients send SINT16. Map to level 1–100, set motor target |
| `0x05` | Set Target Power | sint16 (watts) | ERG mode: look up required resistance for current cadence, set motor target |
| `0x11` | Zwift SIM Mode | — | To be spec'd for v1.1. Spec it now so FTMS work leaves room. |
| `0x01` | Reset | — | Clear ERG target, return to manual mode |
| `0x07` | Start/Resume | — | Accepted (no-op — recording is auto-managed) |
| `0x08` | Stop/Pause | — | Accepted (no-op) |

**Response format (Indicate):**

```
[0x80] [request_opcode] [result_code]
```

Result codes: `0x01` = Success, `0x02` = Not Supported, `0x03` = Invalid
Parameter, `0x05` = Control Not Permitted (Intercept Mode).

**Fitness Machine Status notifications:** Included for ERG state transitions.


### 7.5 Indoor Bike Data Packet (`0x2AD2`)

Little-endian, notified at 2 Hz:

```
Offset  Size     Field                  Flags Bit  Resolution
0       uint16   Flags                  —          Bitmask
2       uint16   Instantaneous Speed    0 (=0)     0.01 km/h
4       uint16   Instantaneous Cadence  2          0.5 RPM
6       sint16   Resistance Level       5          1
8       sint16   Instantaneous Power    6          1 Watt
10      uint8    Heart Rate             9          1 BPM (if HRM connected)
```

Speed derived from cadence: `speed = rpm × 10` (0.01 km/h units). This is a
synthetic value — the bike is stationary. Strava uses it for estimated distance.

**Notification Pattern:**
- Track subscription state via `onSubscriptionChanged(handle, charHandle, charUuid16, notify, indicate)`.
- In `tick()`, notify only if subscribed and not paused.


### 7.6 Notification Rate & Safety

FTMS notifications are sent at **2 Hz** (every 500 ms). This rate must NOT
exceed **4 Hz** — higher rates increase BLE connection event density and radio
utilisation, competing with WiFi for the shared radio (see §8.5). FTMSServer
only notifies when at least one subscriber exists (`onSubscriptionChanged()`).

HR re-broadcast notifications follow the strap's native rate (typically 1 Hz).

### 7.7 App Compatibility

The following apps form a test matrix for standard BLE FTMS indoor bike services.

| App | Platform | Notes |
|---|---|---|
| **Zwift** | Windows, Mac, iOS, Android | Virtual cycling world |
| **TrainerRoad** | iOS, Android, Windows, Mac | Structured power workouts |
| **Xert EBC** | iOS, Android, Karoo | SMART workouts, live MPA |
| **Peloton Digital** | iOS, Android | Instructor classes with power overlay (FTMS support unverified) |
| **Wahoo SYSTM** | iOS, Android, Mac | Video workouts with power targets |
| **Rouvy** | iOS, Android, Windows, Mac | Augmented reality routes |
| **MyWhoosh** | iOS, Android, Windows, Mac | Free virtual cycling |
| **Kinomap** | iOS, Android | Real-route video rides |
| **Garmin devices** | Edge / Forerunner / Fenix | Records as Indoor Cycling activity |
| **Apple Watch** | watchOS | Live workout tracking |


### 7.8 Multi-Client BLE Connections

The BLE module is configured for up to **3 simultaneous Peripheral
connections** (with 1 reserved for Central). This enables scenarios like:

- Xert EBC (for coaching) + Zwift (for visuals) connected simultaneously
- Echelon tablet (HR service) + Zwift (FTMS) connected simultaneously
- Garmin watch + phone app connected simultaneously

**Add Echenon-specific sdkconfig override:**
```ini
# hardware/ex3-waveshare-nano/sdkconfig.defaults (appended after ble.defaults)
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3
CONFIG_BT_CTRL_BLE_MAX_ACT=5
```
Cost: +1 connection slot (~3 KB DRAM) + 1 activity slot (828 B DRAM). Total BLE DRAM: ~73 KB.

**Device name:** Pick `Echenon` as the base name. The framework auto-appends a `-XXXX` suffix from the Bluetooth MAC via `BLEAdvertisingBuilder::pack()` when `nameSuffix = true`. The suffix disambiguates multiple Echenon devices in a gym or multi-bike household.

Note: `BLEManager` manages connection lifecycle, peer tracking, and MTU negotiation. Do not duplicate.


---

## 8. Heart Rate Monitor Relay

### 8.1 Overview

The ESP32 connects to an HR device as a BLE Central, then outputs the heart
rate through all three paths simultaneously.

**Compatible devices:** Any standard BLE device advertising the Heart Rate
Service (`0x180D`) — chest straps (Polar H10, Garmin HRM-Pro), optical
armbands (Wahoo TICKR Fit, Scosche Rhythm+, CooSpo HW808), and HR watches
in broadcast mode. The parser supports the full `0x2A37` specification:
uint8 and uint16 HR formats, optional sensor contact status, optional
RR-intervals, and optional energy expended fields.

**Primary tested device:** Echelon Beat Advanced Armband (model **HW901**,
OEM Shenzhen GWSTAI, BLE company `0x0F05`, FCC ID `2AWD4HW901`).
Advertises as `EBEAT-NNNNNNN`. See
[`docs/echelon-beat-ble-validation.md`](docs/echelon-beat-ble-validation.md)
for the full GATT dump and comparative test results.

| Beat characteristic | Value | Firmware impact |
|---|---|---|
| HR format | uint8 only | Parser handles both — not locked to uint8 |
| Sensor contact flags | Not supported (flags always `0x00`) | Must not assume all devices lack this — see §8.4 |
| RR-intervals | Not present | Parser extracts when present; Beat simply won't send them |
| Energy expended | Not present | No `0x2A39` Control Point on Beat |
| Body Sensor Location | `0x04` (Hand) | HRMRepeater reads actual value from device, does not hardcode |
| Battery Service | `0x180F` with Read + Notify | Exposed in portal when available |
| Pairing | Not required (open connection) | Opportunistic bonding — bonds if peer supports SMP |
| Max connections | 1 Central only | ESP32 takes the connection exclusively |
| Notification rate | ~1 Hz | Typical for optical sensors; chest straps may be faster |
| Auto-sleep | Stops advertising ~30–60s after losing skin contact | Undocumented by Echelon; causes connection drop |
| No-contact signal | Sends BPM = 0 (not sensor contact flags) | Firmware handles both methods — see §8.4 |

```
[HR Strap / Echelon Beat Armband]
        │
        │  BLE Central (HRMClient → IBLEClientProvider)
        ▼
┌─────────────────────────────────────────────┐
│               ESP32 HR Relay                │
│                                             │
│  1. BLE HR Service (0x180D)                 │──► Echelon app (tablet)
│     HRMRepeater → IBLEServiceProvider       │    shows HR during class
│                                             │
│  2. FTMS Indoor Bike Data (0x2AD2)          │──► Zwift / TrainerRoad
│     HR embedded in FTMS packet              │    HR in FTMS stream
│                                             │
│  3. Session recording buffer                │──► Upload to Strava
│     HR stored per data point                │    HR in TCX file
└─────────────────────────────────────────────┘
```

### 8.2 Foundation BLE Integration for HR

**HRMClient must implement `IBLEClientProvider`:**

```cpp
class<ctrl94> : public IBLEClientProvider, public ITickable {
    const char* getProviderId() const override { return "hr_client"; }
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }
    
    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override {
        if (max > 0) { out[0] = 0x180D; return 1; }
        return 0;
    }
    
    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override {
        if (max > 0) { 
            out[0] = { 0x180D, 0x2A37, false, true };
            return 1;
        }
        return 0;
    }
    
    void onConnected(const BLEStoredDevice& device) override;
    void onNotification(const BLENotification& notification) override;
    void onDisconnected(BLEDisconnectReason reason) override;
    void tick(uint32_t currentMillis) override;
};
```

**HR Measurement parsing** — the parser handles the full `0x2A37` specification,
not just the subset used by the Beat:

```cpp
void HRMClient::onNotification(const BLENotification& n) {
    if (n.charUuid16 == 0x2A19) {  // Battery Level
        _batteryPct = n.data[0];  // 0–100%
        return;
    }
    if (n.charUuid16 != 0x2A37 || n.length < 2) return;

    // --- Parse flags byte (Bluetooth SIG Heart Rate Measurement) ---
    uint8_t flags = n.data[0];
    bool     hrIs16Bit          = flags & 0x01;
    bool     contactSupported   = flags & 0x04;  // bit 2
    bool     contactDetected    = flags & 0x02;  // bit 1 (only valid if bit 2 set)
    bool     energyPresent      = flags & 0x08;
    bool     rrPresent          = flags & 0x10;

    // --- Extract HR value ---
    uint16_t bpm;
    size_t   idx = 1;
    if (hrIs16Bit) {
        if (n.length < 3) return;
        bpm = n.data[1] | (n.data[2] << 8);
        idx = 3;
    } else {
        bpm = n.data[1];
        idx = 2;
    }

    // --- No-contact detection (two methods, device-dependent) ---
    // Method 1: BLE sensor contact flags (Polar H10, Garmin HRM-Pro, etc.)
    //   → contactSupported && !contactDetected means off-skin
    // Method 2: BPM = 0 (Echelon Beat HW901, many Chinese ODM armbands)
    //   → firmware sends 0 BPM when optical sensor loses pulse
    if (bpm == 0 || (contactSupported && !contactDetected)) {
        _contactState = HRContactState::NO_CONTACT;
        _hrmRepeater.publishHr(0);  // suppress — don't relay invalid HR
        return;
    }

    // --- Valid HR reading ---
    _contactState = HRContactState::ACTIVE;
    _lastValidHrMs = n.receivedAtMs;
    _hrmRepeater.publishHr(bpm);

    // --- Optional: skip energy expended field ---
    if (energyPresent && idx + 2 <= n.length) idx += 2;

    // --- Optional: extract RR-intervals (for future TCX enrichment) ---
    if (rrPresent) {
        while (idx + 2 <= n.length) {
            uint16_t rr_raw = n.data[idx] | (n.data[idx+1] << 8);
            float rr_ms = rr_raw / 1024.0f * 1000.0f;
            _lastRrMs = rr_ms;  // store for session recording if available
            idx += 2;
        }
    }
}
```

**HR contact state** — three-state model derived from hardware validation:

```
                    ┌─────────────┐
     Contact made   │   ACTIVE    │  BPM > 0, contact detected (or unsupported)
    ┌──────────────►│             │  → relay to FTMS/TCX/re-broadcast
    │               └──────┬──────┘
    │                      │ BPM = 0 OR contact flags say off-skin
    │               ┌──────▼──────┐
    │               │ NO_CONTACT  │  Suppress HR relay, keep connection alive
    │  Contact back │             │  → armband may regain contact any moment
    ├──────────────►└──────┬──────┘
    │                      │ Connection lost (device auto-sleep or out of range)
    │               ┌──────▼──────┐
    │               │DISCONNECTED │  onDisconnected() fires
    │               │             │  → trigger reconnect backoff (§8.4)
    └──────────────►└─────────────┘
```

**HRMRepeater must implement `IBLEServiceProvider`:**

```cpp
class HRMRepeater : public IBLEServiceProvider {
    const char* getProviderId() const override { return "hr_repeater"; }
    bool registerServices(NimBLEServer& server) override; // creates 0x180D + 0x2A37
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override {
        adv.addServiceUuid16(0x180D, BLEAdvertisingBuilder::Priority::Normal);
    }
    void publishHr(uint8_t bpm);
};
```
Advertising priority for HR: Normal priority so it spills into scan response (FTMS UUID takes primary PDU space).

**Stale data detection:** 10-second timeout. If no notification for 10s, `publishHr(0)`. On disconnect, `publishHr(0)`.

**Discovery and pairing:** Use `bleManager.startDiscovery("hr_client", 30000)` for user-initiated scan, `bleManager.pair("hr_client", addr)` to pair, `bleManager.forget("hr_client")` to unpair. Paired device persisted in `BLEDeviceRegistry` (NVS).

**Portal:** BLEManager provides a built-in BLE portal tab with device cards, scan, pair/forget UI. No custom HR portal page needed.


### 8.3 Tablet BLE Connections

The tablet maintains two independent BLE connections:

| Connection | Target | Purpose |
|---|---|---|
| 1 | Factory Echelon PCB | Bike metrics for class UI |
| 2 | Echenon ESP32 | Heart rate (re-broadcast from armband) |

> **Single-connection constraint:** Many consumer HR monitors (including the
> Echelon Beat) support only **one BLE Central** at a time. When the ESP32
> connects to the armband, the Echelon Fit app on the tablet **cannot** connect
> to it directly. This is by design — the ESP32 takes the HR connection
> exclusively and re-broadcasts via `HRMRepeater`, which the tablet connects
> to instead. Users should ensure no other app (Echelon Fit, Zwift, etc.) is
> holding the HR monitor connection before pairing via the ESP32 portal.
> Higher-end monitors that support multiple Centrals (e.g., Polar H10) will
> work with the ESP32 without this constraint.

A zero-code spike is added as a PR-03 entry condition: use nRF Connect advertiser to emulate a `0x180D` device named `Echenon-TEST`, check if the Echelon app lists and displays HR.


### 8.4 Reconnect Policy

**Two-tier no-contact detection** — the firmware handles both BLE-standard
and device-specific mechanisms for detecting when HR data is no longer valid:

| Tier | Trigger | Latency | Applies to |
|---|---|---|---|
| **Instant** | `BPM = 0` from device (Echelon Beat, Chinese ODM armbands) | **< 1 second** | Devices that send zero when optical sensor loses pulse |
| **Instant** | Sensor contact flags: `contactSupported && !contactDetected` | **< 1 second** | Devices that implement BLE HR contact status (Polar, Garmin, Wahoo) |
| **Timeout** | No valid notification for **10 seconds** | **10 seconds** | All devices — catches connection drops, radio interference, device crashes |

When no-contact is detected (by either tier):
- HR Service stops notifying (0 BPM) — HRMRepeater suppresses
- FTMS packet clears bit 9 (HR not present)
- Session records HR = 0

On valid HR resuming (BPM > 0, contact restored, or reconnect), notifications
resume immediately.

**Auto-sleep behaviour** (device-dependent, documented for Echelon Beat):

Some HR monitors (including the Beat) stop BLE advertising entirely after
~30–60 seconds without skin contact. This causes a full connection drop
(`onDisconnected()` fires), not just a stale timeout. The reconnect backoff
handles re-acquisition when the user puts the device back on. This is
distinct from simply losing radio range. The Beat auto-sleeps after 30-60s off-skin, so the slow phase is the **common** case.

**Three-tier HR gap criteria:**

| Criterion | Limit | What it measures |
|---|---|---|
| **(a) Stale detection** | HR absent within **12s** of last valid notification | App-level policy — how fast we mark HR as unavailable (10s threshold + 2s grace) |
| **(b) Steady state** | No gap **> 2.5s** while connected and on-skin | Radio coexistence jitter at a ~1 Hz strap. Measured in the PR-07 coexistence test. |
| **(c) Recovery** | Reconnected **≤ slow-retry interval + 5 s** (≤ 20 s) | Using the framework's `BLEReconnectPolicy`. |

**Reconnect policy** (managed by `BLEReconnectPolicy`):
- **Direct connect** to stored address using the radio in the background.
- Fast phase: default delays `{2, 4, 8, 16, 32}` seconds (5 attempts).
- Slow phase: **set to 15 s** via `setSlowRetryMs(15000)`.
- Indefinite slow retry while `AppState::Running` — no "max 5 attempts, give up."
- Phase transitions: `Idle → Fast → Slow → (never Exhausted while Running)`.
- `BLEReconnectPolicy` suspends during `ScanInhibit` (e.g. HTTPS upload) automatically and during OTA.


### 8.5 Radio Coexistence Policy (WiFi + BLE)

> [!WARNING]
> The ESP32-S3 has a **single 2.4 GHz radio** shared between WiFi and BLE via
> time-division coexistence. Poor coex management causes FTMS notification jitter,
> HR update drops, portal timeouts, and Strava upload failures. This section
> defines binding policy for all BLE and WiFi code.

**ESP-IDF coexistence configuration (`sdkconfig.defaults`):**

```
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y
CONFIG_ESP_COEX_EXTERNAL_COEXIST_ENABLE=n
# Enable power management under coexistence (allows radio sleep between events)
CONFIG_ESP_COEX_POWER_MANAGEMENT=y
```

**DRAM budget:** ~69 KB for BLE (35–40 KB controller blob + 25–30 KB NimBLE host) in internal DRAM only. With Echenon's 3-connection override: ~73 KB. BLE memory **MUST** use internal DRAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=y`); PSRAM causes DMA panics on ESP32-S3.

**Coexistence preference:** `PREFER_WIFI` is mandatory. Do NOT call `esp_coex_preference_set()` in application code. `PREFER_BT` starves WiFi; `PREFER_BALANCE` causes `AUTH_EXPIRE` timeouts.

**Scanning discipline (two distinct operations):**

*Discovery scan* — general scan to find new devices:

| Rule | Rationale |
|---|---|
| **User-initiated only** | Only scan when the user opens the BLE pairing page in the portal or triggers via MCP. Never scan autonomously for new devices. |
| **Scan window ≤ 50% of scan interval** | Framework enforces scan window ≤ 50% of interval normally, ≤ 25% if any peripheral connection exists. Application code must not bypass this. |
| **Duration bounded to 30s** | Auto-stop after 30s. User can re-trigger. Never scan indefinitely. |
| **Stop scanning before WiFi-heavy operations** | Before Strava upload, OTA download, or large portal asset loads — call `NimBLEDevice::getScan()->stop()` first. |
| **Not allowed during CONFIGURING/AP mode** | WiFi must be responsive for portal setup. |

*Reconnect scan* — automatic reconnection to a known, previously-paired device:

| Rule | Rationale |
|---|---|
| **Direct connect to stored address** | Uses the persisted `{address, addressType, name}` identity. Much cheaper than a general scan — no scan at all if the address is known. Falls back to accept-list-filtered scan if direct connect fails. |
| **Exponential backoff** | 2s → 4s → 8s → 16s → 32s, max 5 attempts (~62s total). |
| **Conservative duty cycle when connections active** | If FTMS clients are connected, reconnect scan uses interval=160ms, window=40ms (25% duty) to minimise radio contention. |
| **Give up after max attempts** | After 5 failures, HR = 0 (stale data protection, §8.4). User can re-trigger pairing from the BLE portal tab. |
| **Not subject to the 30s cap** | Reconnect is a bounded retry sequence, not an open-ended scan. |

**Phase-aware behaviour:**

| Foundation Phase | WiFi State | BLE Behaviour |
|---|---|---|
| **BOOT** | Off | BLE not started |
| **CONFIGURING** (AP mode) | AP active, portal serving | **No BLE at all.** BLE starts on entering RUNNING only. User configures WiFi first via the portal. |
| **RUNNING** (STA mode) | Connected | Normal operation. Discovery scan only when user opens BLE pairing page. Reconnect to known strap is automatic with backoff. FTMS notifications and HR relay take priority over WiFi data transfers. |

**Upload/OTA radio budget:**

- **Strava upload:** `auto token = bleManager.inhibitScanning("strava_upload");` held for the duration of the HTTPS transfer. While the token is alive, no discovery
  or reconnect scans start, and running scans stop. Connections and advertising
  are unaffected. The DataManager's backoff handles any deferred fetches
  gracefully.
- **OTA download:** BLEManager automatically pauses on OTA progress, cleanly disconnects peers.
- **OTA boot-time safety:** The Foundation's `OTAUpdateManager` with
  `autoCheck=true` runs a full update **including reboot** as soon as WiFi
  connects after boot — which is exactly when a rider powers on the trainer.
  Until upstream [issue #226](https://github.com/matthewmcneill/esp32-app-foundation/issues/226)
  lands (adds `checkOnBoot` + runtime "busy" guard), Echenon runs with
  **`autoCheck=false`**. OTA is manual-only via the System portal tab.

**NimBLE connection parameter tuning:**

> [!NOTE]
> Android connection parameter timing: Do not call `updateConnParams()` immediately on connect — Android centrals disconnect with GATT CONN TIMEOUT (Error 147/133).
> In the **Peripheral** role (FTMS, HR re-broadcast), the ESP32 can only
> *request* preferred connection parameters.

```cpp
// Peripheral (FTMS + HR Service) — PREFERRED parameters only
// The Central (Zwift etc.) decides the actual interval.
// We request 15–30ms for responsive notifications, but the Central may override.
// supervision_timeout: 400 (4s) → tolerates brief WiFi bursts
pServer->setPreferredConnParams(12, 24, 0, 400);  // requested, not guaranteed

// Central (HR strap) — we control these directly
// conn_interval: 30–75ms → relaxed, HR updates are ~1 Hz
// supervision_timeout: 600 (6s) → tolerates WiFi uploads
pClient->updateConnParams(24, 60, 0, 600);         // applied by us (with appropriate delay)
```

**Verification requirement (PR-07 integration test):**

The following coexistence scenario must be tested at **maximum configuration**
and documented:

1. **3 Peripheral connections** (FTMS notifying at 2 Hz on at least one, plus
   the Echelon tablet and one additional app — e.g. nRF Connect × 2 + Zwift)
2. **1 Central connection** (HR strap connected and relaying at ~1 Hz)
3. Portal `/overlay` open via WebSocket (2 Hz push)
4. Trigger Strava upload (or simulate with a large HTTP POST)
5. **Run for 5 minutes continuously**
6. Measure free heap with WiFi + BLE both active `[DR-1]`
7. **Pass criteria:**
   - FTMS notifications: no gap >600ms (allows one missed 500ms cycle)
   - HR relay (steady-state): no gap >2.5s at a ~1 Hz strap
   - HR relay (dropout + reconnect): reconnected within 35s of strap returning
   - WebSocket: no disconnect during upload
   - Upload: completes within DataManager deadline (15s)


### 9.1 Overview

The session manager records ride data independently of the overlay and FTMS
paths. It buffers data points in PSRAM during the ride and hands the completed
session to the upload manager when the ride ends.

### 9.2 Session Lifecycle

| State | Trigger | Action |
|---|---|---|
| **Idle** | Boot / after upload | Waiting for activity |
| **Recording** | First cadence pulse (auto) or "Start" button (manual) | Buffering data points |
| **Paused** | Cadence = 0 during recording | Continue recording zero-power points |
| **Ending** | No cadence for 180s (auto) or "Stop" button (manual) | Finalise session |
| **Uploading** | Session ended | Hand to upload manager |

Auto-start is the default. Rest intervals (cadence = 0) do NOT end the session
— zero-power data points are recorded. The 3-minute auto-end timeout ensures
only a genuine dismount triggers session completion.
There is a wall-clock anchor (first NTP-synced time, back-compute start) to ensure correct real-world timestamping. Ghost session prevention requires N seconds of sustained cadence to start. Crash recovery is done via append-only compressed checkpoint every 30-60s to LittleFS. Trailing zeros are trimmed before TCX generation.


### 9.3 Ride Data Buffer

```cpp
struct __attribute__((packed)) RideDataPoint {
    uint32_t timestamp_ms;   // millis() since session start
    uint16_t cadence_rpm;    // 0.5 RPM resolution
    int16_t  power_watts;    // Instantaneous power
    uint8_t  resistance;     // Level 1–100 (Override) or 1–32 (Intercept)
    uint8_t  heart_rate;     // BPM (0 if unavailable)
};  // 10 bytes per point
```

Internal buffer rate: **2 Hz** (every 500 ms).
At 2 Hz for 60 minutes: 7,200 points × 10 bytes = **72 KB**. The 8 MB PSRAM
can buffer over 100 hours.


### 9.4 TCX Generation

**Execution model:** TCX generation is part of the **DataManager pipeline**,
not `tick()`. When a session ends, the `SessionManager` queues a priority
fetch via `DataManager::requestPriorityFetch()`. The `StravaUploader`
(implementing `IDataSource`) picks this up on the next DataManager cycle:

1. `SessionManager` publishes `session/ended` on EventBus
2. `StravaUploader::fetchUpdate(ctx)` detects pending session
3. **Phase 1** — generates TCX by streaming XML to a LittleFS file, adding gzip compression
4. **Phase 2** — split pipeline: refresh token → upload → poll status
5. Both phases run inside the same `fetchUpdate()` call, bounded by
   `ctx.deadline` with `isExpired()` checks between chunks

This runs entirely off the main tick loop — `DataManager` provides:
- TWDT unsubscription (`TwdtFetchGuard`)
- Deadline-bounded transport (15s total budget)
- Health state machine with exponential backoff on failure
- Diagnostic visibility via portal, MCP, and EventBus

The TCX is streamed directly to a LittleFS file (no DOM tree in memory).
Includes: activity type, lap summary, per-trackpoint power/cadence/HR, and
watts in the TPX extension namespace.
Size estimate: ~550–720 KB uncompressed, ~30–50 KB as `tcx.gz`.

**Critical: 1 Hz trackpoint output.** The TCX file MUST contain exactly one
`<Trackpoint>` per elapsed second, regardless of the internal 2 Hz buffer
rate. Adjacent 2 Hz samples are averaged to produce each 1-second trackpoint.

This is required for compatibility with advanced analytics platforms such as
**Xert**, which computes second-by-second Maximal Power Available (MPA) using
continuous differential equations over the power stream. Averaged, smoothed,
or sub-1 Hz data corrupts MPA calculations, produces false breakthrough
detections, and distorts training load distribution.

**Rules for TCX output:**
- One `<Trackpoint>` per elapsed second — no gaps, no skipped seconds
- When coasting (0 watts), output `<Watts>0</Watts>` — do NOT drop the
  trackpoint or repeat stale values, so recovery calculations are accurate
- Timestamps in ISO 8601 UTC format

```xml
<Trackpoint>
  <Time>2026-09-17T21:00:00Z</Time>
  <HeartRateBpm><Value>145</Value></HeartRateBpm>
  <Cadence>90</Cadence>
  <Extensions>
    <ns3:TPX>
      <ns3:Watts>265</ns3:Watts>
    </ns3:TPX>
  </Extensions>
</Trackpoint>
```


### 9.5 Offline Resilience

If WiFi is unavailable when a session ends:
1. TCX written to LittleFS
2. Retry queue maintained in NVS
3. Queued uploads retry on next WiFi connection
4. Up to 5 sessions queued

Queue sizing is validated against a 2.75 MiB partition: gzipped sessions fit, but uncompressed sessions do not.


### 9.6 Manual Download

The web portal exposes a download endpoint for manual file retrieval:

```
GET /api/session/download?id=<session_id>&format=tcx
```

This allows riders to download the TCX file directly from the ESP32 and
manually upload it to any platform (Xert, TrainingPeaks, Golden Cheetah, etc.)
if cloud sync is unavailable or not configured.
Because the TCX download contains HR (health data), it requires authentication.


---

## 10. Upload Services

### 10.1 Overview

The upload manager is the third independent output path. When a session ends,
it pushes the recorded data to each configured cloud service. It has no
knowledge of the overlay or FTMS — it receives a completed `RideSession`
object and uploads it.

### 10.2 Extensible Architecture

Each cloud integration implements a common interface:

```cpp
namespace echenon {
class IUploader {
public:
    virtual ~IUploader() = default;
    virtual const char* name() = 0;          // "Strava", "Garmin Connect"
    virtual const char* icon() = 0;          // Emoji or icon class
    virtual bool isConfigured() = 0;         // OAuth tokens present?
    virtual bool upload(const RideSession& session) = 0;
    virtual void registerRoutes(PsychicHttpServer& server) = 0;  // Settings page
};
}
```

The unified execution model relies exclusively on `IDataSource`. `IUploader` is only for describing capabilities.

The upload manager iterates enabled uploaders after each session:

```cpp
for (auto* uploader : _uploaders) {
    if (uploader->isConfigured()) {
        bool ok = uploader->upload(session);
        // Log result, retry on failure
    }
}
```


### 10.3 Settings Pages

Each uploader contributes its own section to the **Upload Services** settings
page (via `IWebContributor`):

```
┌──────────────────────────────────────────────┐
│  Upload Services                             │
│                                              │
│  ── Strava ──                                │
│  Status: ✅ Connected (matthew@...)          │
│  Auto-upload: ☑ Enabled                      │
│  Last upload: 17 Sep 2026, 19:45             │
│  [ Disconnect ]                              │
│                                              │
│  ── Garmin Connect ──                        │
│  Status: ⚪ Not configured                   │
│  [ Connect Garmin ]                          │
│                                              │
│  ── Intervals.icu ──                         │
│  Status: ⚪ Not configured                   │
│  [ Enter API Key ]                           │
└──────────────────────────────────────────────┘
```

This page is separate from the overlay — it's a settings concern, not a
training-session concern.

### 10.4 Strava Integration (v1)

**OAuth 2.0 flow:**
1. Bring-your-own API app credential model. User enters API credentials on the Upload Services settings page.
2. Browser redirects to Strava's OAuth consent page
3. User authorizes → Strava redirects back with auth code
4. ESP32 exchanges code for access + refresh tokens (HTTPS)
5. Tokens stored encrypted via Foundation `KeyManager`
6. Tokens registered with `Redactor::instance().registerSecret()` to prevent
   accidental exposure in serial logs

**Token refresh:** Automatic before each upload (access tokens expire after
6 hours).

**Upload execution model (ADR-17 compliance):**

The Strava HTTPS upload (TLS handshake + multipart POST) takes 2–10 seconds,
far exceeding the 10 ms tick budget. `StravaUploader` implements `IDataSource`
so the framework's `DataManager` manages the upload lifecycle:

```cpp
class StravaUploader : public IDataSource {
    UpdateStatus fetchUpdate(const FetchContext& ctx) override {
        // DataManager provides:
        // - ctx.client: pre-configured TLS WiFiClient
        // - ctx.deadline: bounded timeout with isExpired() checks
        // - TWDT unsubscription (TwdtFetchGuard)
        // - Automatic health state tracking & backoff on failure

        if (!_pendingSession) return UpdateStatus::NoData;

        // Upload model: async POST /uploads + GET /uploads/{id} poll
        // Split into separate fetches. Treat duplicate as success.
        
        HTTPClient http;
        http.begin(*ctx.client, "https://www.strava.com/api/v3/uploads");
        // ... multipart POST with TCX payload ...
        // Check ctx.deadline.isExpired() between chunks

        return ok ? UpdateStatus::Success : UpdateStatus::Error;
    }
};
```

This gives the upload:
- **Bounded transport** — the framework's 3-layer timeout system prevents
  hung connections from triggering the TWDT
- **Health state machine** — failed uploads degrade gracefully with
  exponential backoff (30s → 15 min cap) rather than retrying in a tight loop
- **TWDT safety** — `DataManager` unsubscribes from the watchdog during fetch
- **Diagnostic visibility** — upload health appears in the portal Status tab,
  MCP `get_data_sources` tool, and EventBus (`data.source.health`)


### 10.5 Downstream Platform Sync

Some platforms auto-ingest data from Strava, requiring no direct Echenon
integration:

| Platform | Sync Method | Setup |
|---|---|---|
| **Xert** | Automatic Strava webhook | Connect Xert to Strava once in Xert Online → Sync settings |
| **Relive** | Automatic Strava sync | Connect in Relive settings |
| **VeloViewer** | Automatic Strava sync | Connect in VeloViewer settings |

When Echenon uploads a ride to Strava, Xert automatically pulls the full
1 Hz power stream within seconds, runs its MPA model, checks for
breakthroughs, and updates the athlete's fitness signature — all without
any action from the rider or any Xert-specific code on the ESP32.

### 10.6 Future Direct Integrations (Post-v1)

| Platform | Auth Type |
|---|---|
| Garmin Connect | OAuth 2.0 |
| TrainingPeaks | OAuth 2.0 |
| Intervals.icu | API key |

---

## 11. Session Overlay

### 11.1 Overview

The session overlay is a lightweight web-based training computer UI. It is
completely independent of FTMS and upload — it doesn't know or care what's
consuming the BLE data or where sessions are uploaded. It is purely a live
monitor and session controller.

Designed to run as a **floating window** on the Android tablet (Lenovo P12),
overlaid on top of whatever app is running the class — Echelon, Zwift,
Peloton, YouTube, or nothing at all.

Accessible at `http://echenon.local/overlay` (or device IP). There is an mDNS fallback via serial log, QR code, and IP on AP captive page. Verify `echenon.local` on P12 Chrome during PR-01.


### 11.2 Live Ride View

Updated in real-time via WebSocket:

```
┌──────────────────────────────────────────────┐
│  ● REC  00:32:15             [■ Stop]       │
│                                              │
│  ⚡ 187W        🔄 82 rpm        ❤️ 145 bpm │
│  ◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼░░░░░░░░░░░░ 56/100    │
│                                              │
│  ┌────────────────────────────────────────┐  │
│  │  Power (last 5 min)                   │  │
│  │  250W ┤         ╱╲                    │  │
│  │  200W ┤    ╱╲  ╱  ╲  ╱╲              │  │
│  │  150W ┤╱╲╱  ╲╱    ╲╱  ╲╱╲           │  │
│  │  100W ┤                    ╲──        │  │
│  │       └────────────────────────────   │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  ❤️ Zone 3 (Tempo)    Avg: 156W   Max: 243W │
└──────────────────────────────────────────────┘
```

**Elements:**
- Session state (● REC / ○ Idle) with elapsed time
- Start / Stop button (manual override of auto-start/stop)
- Live power, cadence, heart rate — large, glanceable
- Resistance level bar (1–32 in Intercept, 1–100 in Override)
- ERG target indicator (when FTMS Control Point active)
- Rolling 5-minute power chart
- HR zone indicator (Z1–Z5, colour-coded)
- Running averages and peaks

### 11.3 Post-Ride Summary

Displayed after session ends:

- Duration, estimated distance
- Average and max: power, cadence, heart rate
- Time-in-zone chart (HR zones and/or power zones)
- Power curve (best 5s, 30s, 1m, 5m, 20m averages)
- Upload status (auto-uploaded to Strava ✓ / pending / failed)
- "Discard" option (delete session without uploading)

### 11.4 Technical Implementation

**Portal tabs vs standalone overlay:**

The Echenon web UI has two distinct surfaces:

1. **Portal tabs** — Calibration, Upload Services, BLE, Session History.
   These are standard `IWebContributor` tabs inside the framework's SPA.
   Each is a JavaScript file that injects its own markup into the SPA's
   tab container, registered via `registerTabs()`. **No separate HTML files.**

2. **Session overlay** — A standalone page at `/overlay` designed for
   floating-window use on the Lenovo tablet. This is NOT a portal tab —
   it has its own entry point with a minimal, purpose-built HTML page
   optimised for the small floating window viewport.

**Asset delivery — gzip+PROGMEM pipeline (canonical):**

All JavaScript assets use the framework's **gzip+PROGMEM pipeline** —
source files in `web/`, compiled by `portalBuilder.py` at build time to
gzip-compressed `PROGMEM` byte arrays in `include/portal_assets/`.
Served via `serveGzipAsset()` with zero DRAM impact and built-in ETag
caching.

> **Do NOT put consumer HTML/JS in `data/www/`.** That directory is reserved
> for the framework's SPA skeleton (`index.html`, `app.js`). Consumer assets
> go through the gzip+PROGMEM pipeline via `web/`.

```cpp
// SessionOverlay.cpp — registerRoutes()
#include "utils/serveGzipAsset.hpp"
#include "portal_assets/overlay_js.hpp"      // from web/overlay.js
#include "portal_assets/overlay_html.hpp"    // from web/overlay.html

void SessionOverlay::registerRoutes(PsychicHttpServer& server) {
    // Standalone overlay page (not a portal tab)
    server.on("/overlay", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
        return serveGzipAsset(req, res,
            overlay_html_gz, overlay_html_gz_len, overlay_html_gz_etag,
            "text/html");
    });
    server.on("/overlay.js", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
        return serveGzipAsset(req, res,
            overlay_js_gz, overlay_js_gz_len, overlay_js_gz_etag,
            "application/javascript");
    });

    // REST API
    server.on("/api/session/start", HTTP_POST, ...);
    server.on("/api/session/stop",  HTTP_POST, ...);
    server.on("/api/session/status", HTTP_GET, ...);
    server.on("/api/session/history", HTTP_GET, ...);

    // WebSocket for live 2 Hz push
    _ws = server.websocket("/ws/session");
    _ws->onOpen([this](PsychicWebSocketClient* client) { ... });
}

void SessionOverlay::registerTabs(ITabRegistry& registry) {
    // Session History tab in the main portal
    registry.addTab("Session", nullptr, 60, kSessionIcon, "/session.js");
}
```

**Real-time data:** WebSocket push at 2 Hz via `PsychicHttp` WebSocket
endpoint `/ws/session`. The overlay JS opens the socket on page load.

**Charts:** Lightweight `<canvas>` drawing — no Chart.js or similar heavy
libraries. The overlay must load fast in a small floating window.

**DRAM tuning (framework best practice):**

WebSocket frame buffers consume DRAM. With PSRAM available on the ESP32-S3,
the overlay's data structures (rolling chart history, session summary) should
be allocated in PSRAM to keep DRAM free for the WiFi/BLE stack:

```cpp
// Allocate chart history in PSRAM
_chartBuffer = (PowerPoint*)ps_malloc(CHART_POINTS * sizeof(PowerPoint));
```

The WebSocket JSON payloads themselves are small (~100 bytes per 2 Hz update)
and can remain in DRAM. The foundation's `DeepStackExecutor` is not needed
here since overlay requests are lightweight.

---

### 11.5 Trust Boundaries

- Read-only live data: no auth.
- Mutations/downloads/motor commands: require `AuthManager`.
- BLE Control Point: any Central in range can Request Control (accepted risk, documented).

## 12. Build System

### 12.1 Project Structure

```
Echenon/
├── .gitmodules
├── platformio.ini
├── docs/
│   ├── ex3_specification.md              EX-3 stock hardware reference
│   ├── hardware_specification.md         Intercept board circuit design
│   ├── implementation-strategy.md        Iterative dev roadmap (phases 0–6)
│   ├── use_case_analysis.md              6 validated use cases
│   └── echelon-teardown/                 Raw teardown photos + notes
├── lib/
│   └── esp32-app-foundation/        (git submodule)
├── hardware/
│   └── ex3-waveshare-nano/              Target: EX-3 on Waveshare S3-Nano
│       ├── config.h                     Pin map, bike characteristics, module flags
│       ├── partitions.csv               Flash partition table
│       └── sdkconfig.defaults           SDK config overrides
├── src/
│   ├── main.cpp
│   └── echenon/
│       ├── EchenonModule.h / .cpp
│       ├── CadenceSensor.h / .cpp
│       ├── ResistanceSensor.h / .cpp
│       ├── ResistanceEncoder.h / .cpp
│       ├── ResistanceMotor.h / .cpp
│       ├── InterceptController.h / .cpp
│       ├── ERGController.h / .cpp
│       ├── PowerEngine.h / .cpp
│       ├── FTMSServer.h / .cpp
│       ├── HRMClient.h / .cpp
│       ├── HRMRepeater.h / .cpp
│       ├── SessionManager.h / .cpp
│       ├── RideStore.h / .cpp
│       ├── SessionOverlay.h / .cpp
│       ├── TCXGenerator.h / .cpp
│       ├── UploadManager.h / .cpp
│       ├── StravaUploader.h / .cpp
│       ├── IUploader.h
│       ├── Config.h
│       └── Calibration.h / .cpp
├── web/                                 (gzip+PROGMEM pipeline source)
│   ├── overlay.html                     → portal_assets/overlay_html.hpp
│   ├── overlay.js                       → portal_assets/overlay_js.hpp
│   ├── overlay.css                      → portal_assets/overlay_css.hpp
│   ├── session.js                       → portal_assets/session_js.hpp
│   ├── calibration.js                   → portal_assets/calibration_js.hpp
│   └── upload_settings.js               → portal_assets/upload_settings_js.hpp
├── include/
│   └── portal_assets/                   (auto-generated by portalBuilder.py)
├── SPEC.md
├── README.md
└── LICENSE
```

**Build pipeline:** `portalBuilder.py` scans `web/`, gzip-compresses each
file, and emits `include/portal_assets/<name>_<ext>.hpp` headers with
`PROGMEM` byte arrays + ETag. Served via `serveGzipAsset()` in
`registerRoutes()`. The `data/www/` directory is reserved for the framework's
SPA skeleton — consumer apps MUST NOT place files there.

### 12.2 platformio.ini

```ini
[env:ex3-waveshare-nano]
extends = env:esp32s3nano
board_build.partitions = hardware/ex3-waveshare-nano/partitions.csv
board_build.cmake_extra_args =
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;hardware/esp32s3nano/sdkconfig.defaults;hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"
build_flags =
    ${env:esp32s3nano.build_flags}
    ${ble.build_flags}
    -DBOARD_HEADER=\"hardware/ex3-waveshare-nano/board.h\"
lib_deps =
    ${env:esp32s3nano.lib_deps}
    ${ble.lib_deps}
```

The `env:esp32s3nano` base environment (provided by the Foundation) already
configures the Waveshare board, PSRAM, USB-CDC, and core library dependencies.
Echenon extends it, overriding only the board header, partition table, and
adding BLE support via the framework's `[ble]` mix-in. The Echenon `sdkconfig.defaults` includes `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3` and `CONFIG_BT_CTRL_BLE_MAX_ACT=5`.


### 12.3 board.h / config.h

```cpp
#pragma once

// --- Sensor Pins ---
#define PIN_CADENCE_SENSOR    5    // D2 — GPIO 5, interrupt-capable
#define PIN_RESISTANCE_ADC    1    // A0 — GPIO 1, ADC1_CH0

// --- Motor Driver Pins (L9110S H-Bridge) ---
#define PIN_L9110S_IA         6    // D3 — GPIO 6, motor forward (PWM)
#define PIN_L9110S_IB         7    // D4 — GPIO 7, motor reverse (PWM)

// --- Rotary Encoder Pins (Resistance Knob) ---
#define PIN_ENCODER_A         8    // D5 — GPIO 8, quadrature channel A
#define PIN_ENCODER_B         9    // D6 — GPIO 9, quadrature channel B

// --- Console Motor Sense Pins (MITM Intercept) ---
#define PIN_MOTOR_SENSE_P     6    // ADC1, console motor+ sense
#define PIN_MOTOR_SENSE_N     7    // ADC1, console motor- sense

// --- Feature Flags ---
#define WITH_BLE                    // Foundation BLEManager
#define WITH_BLE_FTMS               // FTMS server + Control Point
#define WITH_BLE_HRM_RELAY          // HRM Central + re-broadcast
#define WITH_STRAVA                 // Strava API integration
#define WITH_MOTOR_CONTROL          // L9110S motor driver + position loop
#define WITH_ENCODER                // Quadrature rotary encoder input
#define WITH_ERG_MODE               // FTMS Control Point → ERG controller
// #define WITH_DISPLAY_U8G2        // No physical display
// #define WITH_HOMEASSISTANT       // Not used
```


--- Sensor Pins ---
#define PIN_CADENCE_SENSOR    5    // D2 — GPIO 5, interrupt-capable
#define PIN_RESISTANCE_ADC    1    // A0 — GPIO 1, ADC1_CH0

// --- Motor Driver Pins (L9110S H-Bridge) ---
#define PIN_L9110S_IA         6    // D3 — GPIO 6, motor forward (PWM)
#define PIN_L9110S_IB         7    // D4 — GPIO 7, motor reverse (PWM)

// --- Rotary Encoder Pins (Resistance Knob) ---
#define PIN_ENCODER_A         8    // D5 — GPIO 8, quadrature channel A
#define PIN_ENCODER_B         9    // D6 — GPIO 9, quadrature channel B

// --- Console Motor Sense Pins (MITM Intercept) ---
#define PIN_CONSOLE_MOTOR_POS 6    // A1 — GPIO 6, ADC1, console motor+ analog sense
#define PIN_CONSOLE_MOTOR_NEG 7    // A2 — GPIO 7, ADC1, console motor− analog sense

// --- Feature Flags ---
#define WITH_BLE                    // Foundation BLEManager
#define WITH_BLE_FTMS               // FTMS server + Control Point
#define WITH_BLE_HRM_RELAY          // HRM Central + re-broadcast
#define WITH_STRAVA                 // Strava API integration
#define WITH_MOTOR_CONTROL          // L9110S motor driver + position loop
#define WITH_ENCODER                // Quadrature rotary encoder input
#define WITH_ERG_MODE               // FTMS Control Point → ERG controller
// #define WITH_DISPLAY_U8G2        // No physical display
// #define WITH_HOMEASSISTANT       // Not used
```

---

## 13. Testing & Verification

Test phases map to the validated use cases in
[`docs/use_case_analysis.md`](docs/use_case_analysis.md).

Add native test requirement for every pure-logic module (googletest on `env:native`).
Build environment name: use `ex3-waveshare-nano` consistently.

| # | Test | Expected Result |
|---|---|---|
| 6.6 | **Motor boundary:** ERG demands high resistance at low cadence. | ERG target near cam-max settles without crossing the extreme. |
| 6.7 | **ERG Acceptance:** | `[DR-1: setpoint reached within X s without oscillation; position error ≤ 1 level]` |
