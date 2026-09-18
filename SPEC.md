# Echenon — Firmware Specification

**An open training computer for the Echelon EX-3 indoor bike.**

---

## 1. Project Overview

### 1.1 Purpose

Echenon transforms an Echelon EX-3 Smart Connect indoor bike into a
self-contained, open training computer by reading the bike's physical sensors
via a non-invasive wire splice and providing:

- A **session overlay** — a floating training computer UI showing live power,
  cadence, heart rate, charts, and session controls
- **Automatic session recording** with post-ride upload to Strava, Garmin
  Connect, and other fitness platforms
- A **BLE FTMS smart trainer broadcast** compatible with Zwift, TrainerRoad,
  Peloton Digital, and any FTMS-aware fitness app
- **Heart rate monitor relay** — connects to an HR strap and re-broadcasts to
  the Echelon tablet, the FTMS stream, and the session recording simultaneously

The Echelon app, tablet, and instructor-led classes continue to function
normally. Echenon runs silently alongside, capturing the data that the Echelon
ecosystem refuses to export.

### 1.2 Design Principles

1. **Non-invasive** — No factory wires are cut. Posi-Tap connectors can be
   removed to restore the bike to stock.
2. **Coexist with Echelon** — Wire-splice reads signals in parallel with the
   factory PCB. The Echelon app runs unchanged.
3. **Hands-free daily use** — Recording starts when you pedal and uploads when
   you stop. No buttons, no apps to open.
4. **Three independent outputs** — The session overlay, BLE FTMS broadcast, and
   cloud upload are completely decoupled. Each works independently of the others.
5. **Open ecosystem** — FTMS is always broadcasting. Data uploads to your choice
   of platforms. Nothing is locked down.
6. **Future-proof** — Raw electrical signals are read before the Echelon PCB
   processes them. No firmware update can lock this out.
7. **Original implementation** — Written from first principles, public
   specifications, and permissively licensed references.

### 1.3 Built On

Echenon is built as a module on the
[ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation),
inheriting WiFi management, the web portal, encrypted credential storage, OTA
updates, the module composition architecture, and the new **BLE Manager**
infrastructure module.

### 1.4 Out of Scope (v1)

- Automated resistance control (ERG mode / motorised knob)
- FTMS Control Point write handling
- On-device physical display
- Echelon BLE protocol interception (bypassed via wire splice)

---

## 2. Licensing & Attribution

### 2.1 Project License

Apache License 2.0, compatible with all dependencies.

### 2.2 Dependencies

| Library | License | Usage |
|---|---|---|
| [ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation) | Apache-2.0 | Framework (WiFi, web, config, OTA, auth, BLE) |
| [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Apache-2.0 | BLE stack (via Foundation BLEManager) |
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
┌────────────────────────────── ECHELON EX-3 ──────────────────────────────┐
│                                                                          │
│  [Reed Switch]                  [10kΩ Pot]              [Bike USB Port]  │
│   GND ── Signal                 VCC ── Wiper ── GND        5V out       │
│      │      │                    │       │       │            │          │
│      │      ├── T-Tap ─┐        │       ├─ T-Tap ─┐         │          │
│      │      ▼          │        │       ▼         │         │          │
│  [Echelon PCB]         │   [Echelon PCB]         │         │          │
│  (unchanged)           │   (unchanged)           │         │          │
│                        │                         │         │          │
│         ┌──────────────┴─────────────────────────┴─────────┴────┐     │
│         │              WAVESHARE ESP32-S3-NANO                   │     │
│         │                                                        │     │
│         │  D2 (GPIO 5) ◄── Cadence pulse                        │     │
│         │  A0 (GPIO 1) ◄── Resistance voltage                   │     │
│         │  GND         ◄── Common ground                        │     │
│         │  USB-C       ◄── 5V power                             │     │
│         │                                                        │     │
│         │  ┌────────────────────────────────────────────────┐   │     │
│         │  │              ECHENON FIRMWARE                   │   │     │
│         │  │                                                 │   │     │
│         │  │  CadenceSensor ──┐                              │   │     │
│         │  │                  ├─► PowerEngine ──► SessionMgr │   │     │
│         │  │  ResistanceADC ──┘                     │  │  │  │   │     │
│         │  │                                        │  │  │  │   │     │
│         │  │  HRMClient ──► HRMRepeater ────────────┘  │  │  │   │     │
│         │  │                                           │  │  │   │     │
│         │  │          ┌────────────────────────────────┘  │  │   │     │
│         │  │          │              ┌───────────────────┘  │   │     │
│         │  │          ▼              ▼                      ▼   │     │
│         │  │    📊 Overlay    📡 FTMSServer          ☁️ Upload  │     │
│         │  │    (WebSocket)   (BLE Peripheral)    (HTTP POST)  │     │
│         │  └─────────────────────────────────────────────────┘   │     │
│         └────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────────┘
         │              │                │
    WiFi (HTTP)    BLE FTMS        WiFi (HTTPS)
         │         (0x1826)              │
         ▼         + HR (0x180D)         ▼
   [Floating         │             [Strava API]
    Dashboard]       ▼             [Garmin API]
                [Zwift / TR /      [TrainingPeaks]
                 Echelon app
                 for HR]
```

### 3.3 Data Flow

1. **Every pedal revolution**: Reed switch triggers a hardware interrupt on
   GPIO 5. The ISR records the timestamp for cadence calculation.
2. **Every 50 ms**: ADC samples the resistance potentiometer on GPIO 1, applies
   an EMA filter, and maps the voltage to resistance level 1–32.
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
5. **On session end**: Upload manager generates a TCX file and pushes it to
   each configured cloud service.

### 3.4 Module Decomposition

**Echenon application modules:**

| Module | Responsibility | Foundation Interface |
|---|---|---|
| `EchenonModule` | Composition root — owns all sub-components | `ITickable`, `IConfigurable`, `IWebContributor`, `IDataSource` |
| `CadenceSensor` | Interrupt-driven reed switch, RPM calculation | — |
| `ResistanceADC` | ADC sampling, EMA filter, level mapping | — |
| `PowerEngine` | Watts calculation, calibration coefficients | — |
| `FTMSServer` | FTMS GATT service, Indoor Bike Data notifications | `IBLEServiceProvider` |
| `HRMClient` | BLE Central → connect to HR strap | `IBLEClientProvider` |
| `HRMRepeater` | Re-broadcast HR as BLE HR Service | `IBLEServiceProvider` |
| `SessionManager` | Auto/manual start-stop, ride data buffering | — |
| `RideStore` | PSRAM ring buffer for data points | — |
| `SessionOverlay` | Web dashboard — live metrics, charts, controls | `IWebContributor` |
| `UploadManager` | Post-session TCX generation and cloud upload | `ITickable`, `IConfigurable`, `IWebContributor` |
| `StravaUploader` | Strava OAuth 2.0 + activity upload | `IUploader`, `IDataSource` |

**Foundation-provided infrastructure:**

| Module | What Echenon Uses It For |
|---|---|
| `BLEManager` | NimBLE lifecycle, advertising, HRM scan/pairing portal, reconnection |
| `WifiManager` | AP setup, STA connection, mDNS (`echenon.local`) |
| `WebServer` | Portal chrome, REST API, WebSocket for overlay |
| `ConfigManager` | NVS persistence (calibration, session settings) |
| `KeyManager` | Encrypted storage (Strava OAuth tokens) |
| `AuthManager` | Portal login security |
| `OTAUpdateManager` | Firmware updates without opening the console |

**Coupling audit (Service Module Pattern):**

| Rule | Echenon Compliance |
|---|---|
| No stored `Application*` member | ✅ ADR-39 Transient Service Locator — narrow refs only |
| No polling of application state in `tick()` | ✅ Sensor loop is service-intrinsic |
| No sibling module concrete references | ✅ Sub-components communicate via shared data structs |
| Orchestration stays upstream | ✅ `EchenonModule` is the composition root; sub-components are services |
| EventBus for inter-module notifications | ✅ Session events published via EventBus, not direct calls |

### 3.5 Foundation Integration

**Module registration (ADR-16: Static Scope):**

All module instances MUST be declared in static or global scope. The ESP32
lacks an MMU — stack-allocated modules produce dangling pointers when `setup()`
returns.

```cpp
// main.cpp
#include "AppFoundation.h"
#include "echenon/EchenonModule.h"

static AppFoundation app;
static EchenonModule echenon;

void setup() {
    app.registerModule(&echenon);
    app.begin();
}

void loop() {
    app.tick();
}
```

**Service Locator pattern (ADR-39):**

`EchenonModule` uses the **Transient Service Locator** pattern — it receives
`Application&` in `begin()`, extracts the narrow service references it needs,
and does NOT store `Application*` as a member. This is critical for testability
and avoids the Orchestrator Anti-Pattern.

```cpp
void EchenonModule::begin(Application& app) {
    // ADR-39: Extract narrow refs, do NOT store Application*
    _ble       = app.getBLEManager();
    _config    = app.getConfigManager();
    _eventBus  = app.getEventBus();

    // Sensor modules (internal, no framework dependency)
    _cadence.begin();
    _resistance.begin();
    _power.begin(_config);  // Loads calibration from NVS

    // BLE modules — register with Foundation BLEManager
    _ble->registerServiceProvider(&_ftmsServer);   // IBLEServiceProvider
    _ble->registerServiceProvider(&_hrmRepeater);  // IBLEServiceProvider
    _ble->registerClientProvider(&_hrmClient);     // IBLEClientProvider

    // Session & upload
    _session.begin();
    _uploadManager.begin(_config, app.getKeyManager());

    // Register OAuth tokens for log redaction (security)
    auto& redactor = Redactor::instance();
    redactor.registerSecret(_uploadManager.getAccessToken());
    redactor.registerSecret(_uploadManager.getRefreshToken());
}
```

**Tick budget (ADR-17):**

All `tick()` work MUST complete within **10 ms**. The Task Watchdog Timer
(TWDT) will panic and reboot if the main loop blocks for >5 seconds.

Operations that violate the tick budget:
- ADC multisampling (16× `analogReadMilliVolts()`) — ~2 ms ✅ safe
- Power calculation + FTMS notify — <1 ms ✅ safe
- WebSocket push to overlay — <1 ms ✅ safe
- **TCX generation** — potentially 50–200 ms ❌ must be deferred
- **HTTPS upload to Strava** — 2–10 seconds ❌ must be deferred

Both deferred operations are handled by the framework's **DataManager**
via the `IDataSource` interface. `DataManager` runs fetch cycles on a
background task with TWDT guard and deadline tokens — never in `tick()`.
See §9.4 and §10.4 for the pipeline design.

---

## 4. Hardware

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
| **ESP32 placement** | Mounted inside the bike's console housing, powered by internal 5V rail |

The Lenovo P12 runs both the Echelon class app (full-screen) and Chrome
showing `echenon.local` (floating window) simultaneously. This is a native
Android capability — no root or special app required.

### 4.3 Pin Assignments

| Function | Header | GPIO | Type | Notes |
|---|---|---|---|---|
| Cadence input | D2 | GPIO 5 | Digital interrupt (FALLING) | ADC1_CH4, no strapping conflicts |
| Resistance input | A0 | GPIO 1 | ADC1 Channel 0, 12-bit | Safe with BLE + WiFi active |
| Common ground | GND | — | Power | Left header pin 4 |
| Power input | USB-C | — | 5V from bike USB | Onboard regulator → 3.3V |

**Why these pins:**
- A0 is on **ADC1** — ADC2 produces invalid readings when WiFi or BLE is active
- D2 supports hardware interrupts on all edge types
- Both pins free of strapping-pin conflicts

### 4.4 Echelon EX-3 Internal Sensors

**Cadence sensor** — 2-wire magnetic reed switch at the pedal crank. One pulse
per revolution. Factory PCB provides pull-up voltage.

**Resistance sensor** — 3-wire 10kΩ linear rotary potentiometer on the red
resistance knob (VCC, wiper, GND).

### 4.5 Wiring Plan

All connections use **Posi-Tap** screw-on connectors (22–24 AWG). No wires cut.

| Step | Connection | From | To |
|---|---|---|---|
| 1 | Common ground | Pot GND wire (T-Tap) | ESP32 **GND** pin |
| 2 | Cadence signal | Reed switch signal wire (T-Tap) | ESP32 **D2** pin |
| 3 | Resistance signal | Pot wiper wire (T-Tap) | ESP32 **A0** pin |
| 4 | 5V power | Internal 5V rail (T-Tap) | ESP32 **VBUS** pin (pin 27) |

**Power options:**

| Method | How | Pros | Cons |
|---|---|---|---|
| **Direct tap (recommended)** | T-Tap the internal 5V power rail that feeds the bike's USB port. Wire to VBUS (pin 27) + GND. | Cleanest install, frees USB port for tablet charging, no cable clutter | One extra T-Tap |
| USB cable (fallback) | Short USB-A→C cable from the bike's USB port to the ESP32's USB-C connector | Simplest — just plug in | Occupies the USB port, cable clutter inside console |

The 5V internal rail is the same supply that powers the bike's USB-A charging
port. Tapping upstream of the port means the ESP32 draws directly from the
bike's power supply (~150 mA with WiFi + BLE active) while leaving the USB
port free for charging the Lenovo tablet during rides.

The ESP32-S3-Nano's onboard regulator converts the 5V input on VBUS to the
3.3V required by the SoC.

> **Note:** When using the direct power tap, the USB-C port remains available
> for programming/debugging. Disconnect the 5V T-Tap before connecting USB-C
> to a computer to avoid backfeeding.

### 4.6 Voltage Safety Check (Mandatory)

> **ESP32-S3 ADC pins accept max 3.3V. Exceeding this causes permanent damage.**

1. Wake the bike (pedal). Multimeter on pot VCC and GND wires.
2. **~3.3V**: Wire wiper directly to A0.
3. **~5V**: Install 2:1 voltage divider (two 10kΩ resistors) between wiper
   and A0.
4. Repeat for cadence signal pull-up — if 5V, add divider or level-shifter.

### 4.7 Parts List

| Item | Qty | Cost |
|---|---|---|
| Waveshare ESP32-S3-Nano | 1 (owned) | — |
| Posi-Tap connectors (Red, 20–22 AWG) | 4 | £6 |
| Jumper wires (male-to-female, 20cm) | 5 | £2 |
| 100 nF ceramic capacitors | 2 | £0.20 |
| 10kΩ resistors (if divider needed) | 2 | £0.20 |

---

## 5. Sensor Acquisition

### 5.1 Cadence Sensor

One active-low pulse per pedal revolution. Cadence from inter-pulse interval.

- GPIO 5 (D2): `INPUT` mode, hardware interrupt on `FALLING` edge
- Software debounce lockout: **25 ms**
- RPM: $\text{RPM} = 60{,}000{,}000 \div \Delta t_{\mu s}$
- Timeout: 3 seconds of no pulse → cadence = 0
- Hardware debounce: 100 nF capacitor between D2 and GND

### 5.2 Resistance Sensor

Potentiometer wiper voltage proportional to knob position.

- GPIO 1 (A0): ADC1 Channel 0, 12-bit, 11 dB attenuation
- Sample rate: 20 Hz (every 50 ms)
- Each sample: average of 16 consecutive `analogReadMilliVolts()` calls
- EMA filter: $\text{filtered} = 0.15 \times \text{sample} + 0.85 \times \text{filtered}$
- Hardware filter: 100 nF capacitor between A0 and GND
- Dead zone handling: <80 mV → min, >2850 mV → max

### 5.3 Resistance Level Mapping

Two-point calibration (NVS):
$$\text{level} = 1 + 31 \times \frac{mV - mV_{\min}}{mV_{\max} - mV_{\min}}$$
$$\text{level} = \text{clamp}(\text{round}(\text{level}),\ 1,\ 32)$$

---

## 6. Power Calculation Engine

### 6.1 Physical Basis

Magnetic eddy-current brake. Neodymium magnets adjacent to aluminium flywheel.
Power ∝ cadence² at fixed resistance.

### 6.2 Mathematical Model

$$P(R, \text{RPM}) = A(R) \times \text{RPM}^2 + B(R) \times \text{RPM}$$

$$A(R) = a_2 R^2 + a_1 R + a_0 \qquad B(R) = b_1 R + b_0$$

| Coefficient | Default | Role |
|---|---|---|
| $a_0$ | 0.0001 | Base quadratic (friction) |
| $a_1$ | 0.000045 | Linear growth with resistance |
| $a_2$ | 0.0000035 | Acceleration at high resistance |
| $b_0$ | −0.05 | Static friction offset |
| $b_1$ | 0.01 | Linear term growth |

Power clamped: $\max(0, P)$.

### 6.3 Calibration

Three calibration tiers are available, from coarsest to most precise:

#### Tier 1 — Physics Defaults (Zero Effort)

The default coefficients (§6.2) are derived from magnetic brake physics and
published power curves for 32-level resistance spin bikes. This provides a
**reasonable estimate** suitable for general fitness tracking but is not
calibrated to the specific EX-3 unit.

#### Tier 2 — Community Power Matrix (Recommended)

A **power lookup table** mapping (resistance level × cadence) → watts, derived
from publicly documented empirical measurements of Echelon EX-series bikes.

The web portal **Calibration** page provides a dropdown to select from
pre-built power matrices:

```
┌──────────────────────────────────────────────┐
│  Power Calibration                           │
│                                              │
│  Source: [ Echelon EX-3 Community Table  ▼]  │
│          ────────────────────────────────     │
│          Physics Defaults                    │
│        ► Echelon EX-3 Community Table        │
│          Echelon EX-5s Community Table        │
│          Custom (upload CSV)                 │
│          Reference Power Meter               │
│                                              │
│  Preview:                                    │
│  ┌──────────────────────────────────────┐    │
│  │  R\RPM │  60  │  70  │  80  │  90  │    │
│  │  ──────┼──────┼──────┼──────┼──────│    │
│  │   10   │  45  │  62  │  82  │ 105  │    │
│  │   15   │  72  │  99  │ 131  │ 168  │    │
│  │   20   │ 105  │ 144  │ 191  │ 245  │    │
│  │   25   │ 144  │ 198  │ 262  │ 336  │    │
│  └──────────────────────────────────────┘    │
│                                              │
│  [ Apply ]                                   │
└──────────────────────────────────────────────┘
```

**How it works:**
- The matrix is stored as a 32 × N lookup table in NVS (resistance levels 1–32,
  cadence bins at 5 RPM intervals from 0–150 RPM)
- At runtime, bilinear interpolation between the four nearest table entries
  produces smooth, continuous power output
- Pre-built tables are compiled into the firmware as `const` arrays in PROGMEM
- Users can upload a custom CSV matrix via the portal for other bike models

**Sources for community tables:**
- Publicly shared spreadsheets from cycling forums and Reddit (r/echelon,
  r/pelotoncycle) where users have measured power at known resistance/cadence
  combinations using reference power meters
- The [echbt](https://github.com/snowzach/echbt) project's published EX-3
  power curves (mathematical formulas are not copyrightable — the underlying
  measured data points are facts)
- Cross-referenced against multiple independent measurements for consistency

**Custom CSV format:**
```csv
resistance,rpm_60,rpm_65,rpm_70,rpm_75,rpm_80,...
1,12,14,17,20,24,...
2,15,18,22,26,31,...
...
32,380,420,465,515,570,...
```

#### Tier 3 — Reference Power Meter (Most Precise)

For users with a reference power meter (e.g., Favero Assioma pedals, Stages
crank arm), the portal provides a guided calibration workflow:

1. Ride at a known resistance level at steady cadence for 30 seconds
2. Enter the reference power reading from the external meter
3. Repeat at 3–5 different resistance/cadence combinations
4. A least-squares fit updates the polynomial coefficients (§6.2) in NVS

This produces the most accurate per-unit calibration but requires external
hardware most users won't have.

#### Potentiometer Two-Point Calibration (Required, All Tiers)

Regardless of power calibration tier, the potentiometer range must be
calibrated once so the ADC voltage maps correctly to resistance levels 1–32.
This is triggered via the web portal Calibration page:

1. Live ADC bar on screen. Turn knob to minimum → click "Set Min"
2. Turn knob to maximum → click "Set Max". Values saved to NVS.

### 6.4 NVS Storage

| Key | Type | Default | Description |
|---|---|---|---|
| `cal_mv_min` | uint16 | 100 | ADC mV at minimum resistance |
| `cal_mv_max` | uint16 | 2800 | ADC mV at maximum resistance |
| `cal_pot_done` | bool | false | Potentiometer calibration completed |
| `cal_power_tier` | uint8 | 1 | Active power tier (1=physics, 2=matrix, 3=reference) |
| `cal_a0` – `cal_b1` | float × 5 | see §6.2 | Polynomial coefficients (tiers 1 & 3) |
| `cal_matrix_id` | string | `""` | Selected pre-built matrix ID (tier 2) |
| `cal_matrix_custom` | blob | — | Custom CSV matrix uploaded via portal (tier 2) |

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

`FTMSServer` implements `IBLEServiceProvider` from the Foundation's BLEManager
module ([RFC #225 v2](https://github.com/matthewmcneill/esp32-app-foundation/issues/225)).
It also implements `ITickable` for its own 2 Hz notification timer:

```cpp
class FTMSServer : public IBLEServiceProvider, public ITickable {
    const char* getProviderId() const override { return "ftms"; }

    // One-shot GATT registration — called before lock, never again (C3)
    bool registerServices(NimBLEServer& server) override;

    // Contribute FTMS service data + UUID to the advertising packet
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override {
        const uint8_t sd[3] = { 0x01 /*available*/, 0x20, 0x00 /*Indoor Bike*/ };
        adv.addServiceUuid16(0x1826);
        adv.addServiceData16(0x1826, sd, sizeof sd);
        adv.setAppearance(kIndoorBikeAppearance);
    }

    // Track subscribers — only notify when someone is listening
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                               uint16_t charUuid16, bool notify, bool indicate) override;

    // OTA/admin pause — stop the 2 Hz timer
    void onStackPaused() override;
    void onStackResumed() override;

    // Connection lifecycle
    void onPeerConnected(const BLEPeerInfo& peer) override;
    void onPeerDisconnected(BLEConnHandle handle, BLEDisconnectReason reason) override;

    // 2 Hz notification timer (ITickable)
    void tick(uint32_t now) override;
};
```

BLEManager handles:
- NimBLE initialisation and stack lifecycle
- Advertising: packs contributions from all providers, owns Flags and device
  name (`"Echenon-XXXX"` with per-unit MAC suffix in the scan response)
- Connection lifecycle and preferred parameter negotiation
- Pausing/resuming BLE during OTA (subscribes to `ota.progress`)

FTMSServer handles:
- Creating the FTMS GATT service and characteristics in `registerServices()`
- Contributing FTMS service data to the advertisement
- Assembling and sending Indoor Bike Data notifications at 2 Hz (only when
  subscribers > 0, paused during OTA)

### 7.3 GATT Services

**Fitness Machine Service (`0x1826`):**

| Characteristic | UUID | Properties |
|---|---|---|
| Fitness Machine Feature | `0x2ACC` | Read |
| Indoor Bike Data | `0x2AD2` | Notify @ 2 Hz |
| Fitness Machine Status | `0x2ADA` | Notify |

**Device Information Service (`0x180A`):**

| Characteristic | UUID | Value |
|---|---|---|
| Manufacturer Name | `0x2A29` | `"Echenon"` |
| Model Number | `0x2A24` | `"EX3-v1"` |
| Firmware Revision | `0x2A26` | Build version |

### 7.4 Indoor Bike Data Packet (`0x2AD2`)

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

### 7.5 Notification Rate & Safety

FTMS notifications are sent at **2 Hz** (every 500 ms). This rate must NOT
exceed **4 Hz** — higher rates increase BLE connection event density and radio
utilisation, competing with WiFi for the shared radio (see §8.5). FTMSServer
only notifies when at least one subscriber exists (`onSubscriptionChanged()`).

HR re-broadcast notifications follow the strap's native rate (typically 1 Hz).

### 7.6 FTMS-Compatible Apps

The following apps are known to work with standard BLE FTMS indoor bike
services. Any of these can connect to `"Echenon"` for live power and cadence:

| App | Platform | Notes |
|---|---|---|
| **Zwift** | Windows, Mac, iOS, Android | Virtual cycling world |
| **TrainerRoad** | iOS, Android, Windows, Mac | Structured power workouts |
| **Xert EBC** | iOS, Android, Karoo | SMART workouts, live MPA |
| **Peloton Digital** | iOS, Android | Instructor classes with power overlay |
| **Wahoo SYSTM** | iOS, Android, Mac | Video workouts with power targets |
| **Rouvy** | iOS, Android, Windows, Mac | Augmented reality routes |
| **MyWhoosh** | iOS, Android, Windows, Mac | Free virtual cycling |
| **Kinomap** | iOS, Android | Real-route video rides |
| **Garmin devices** | Edge / Forerunner / Fenix | Records as Indoor Cycling activity |
| **Apple Watch** | watchOS | Live workout tracking |

### 7.7 Multi-Client BLE Connections

The BLE module is configured for up to **3 simultaneous Peripheral
connections** (`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4`, with 1 reserved for
Central). This enables scenarios like:

- Xert EBC (for coaching) + Zwift (for visuals) connected simultaneously
- Echelon tablet (HR service) + Zwift (FTMS) connected simultaneously
- Garmin watch + phone app connected simultaneously

**Device name:** BLEManager advertises as `"Echenon-XXXX"` where `XXXX` is the
last 4 hex digits of the device's BT MAC address (e.g. `"Echenon-1A2B"`). The
name is placed in the **scan response** (not the primary AD) to maximise space
for service data in the primary packet. The suffix disambiguates multiple
Echenon devices in a gym or multi-bike household.

---

## 8. Heart Rate Monitor Relay

### 8.1 Overview

The ESP32 connects to an HR device as a BLE Central, then outputs the heart
rate through all three paths simultaneously.

**Primary tested device:** Echelon Beat Advanced Armband Heart Rate Monitor
(included with EX-3 bundle or sold separately). Any standard BLE device
advertising the Heart Rate Service (`0x180D`) is compatible — chest straps
(Polar H10, Garmin HRM-Pro), optical armbands (Wahoo TICKR Fit, Scosche
Rhythm+), and HR watches in broadcast mode.

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

### 8.2 Foundation BLE Integration

**HRMClient** implements `IBLEClientProvider` ([RFC #225 v2](https://github.com/matthewmcneill/esp32-app-foundation/issues/225)).
It declares what it wants; BLEManager handles scanning, connecting, subscribing:

```cpp
class HRMClient : public IBLEClientProvider {
    const char* getProviderId() const override { return "hrm"; }
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }

    // Declare target service — BLEManager filters discovery results
    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override {
        if (max) out[0] = 0x180D; return 1;
    }

    // Declare subscription — BLEManager subscribes after connecting
    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override {
        if (max) out[0] = { 0x180D, 0x2A37, /*indicate*/false, /*required*/true };
        return 1;
    }

    // Data delivered on the main loop — no raw NimBLE types
    void onConnected(const BLEStoredDevice& device) override;
    void onNotification(const BLENotification& n) override {
        uint8_t bpm = parseHrMeasurement(n.data, n.length);
        _hrmRepeater.publishHr(bpm);           // relay to re-broadcast
        _lastHrMs = n.receivedAtMs;            // stale detection (§8.4)
    }
    void onDisconnected(BLEDisconnectReason reason) override;
};
```

**HRMRepeater** implements `IBLEServiceProvider` — the HR service always exists
(static GATT, per Q7). It sends 0 BPM / stops notifying if no strap is connected:

```cpp
class HRMRepeater : public IBLEServiceProvider {
    const char* getProviderId() const override { return "hr-repeater"; }
    bool registerServices(NimBLEServer& server) override;  // creates 0x180D, 0x2A37
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override {
        adv.addServiceUuid16(0x180D);
    }
    void publishHr(uint8_t bpm);  // called by HRMClient; notifies if subscribed
};
```

BLEManager handles:
- Discovery scan (user-initiated from the BLE portal tab, bounded to 30s)
- Automatic reconnection to stored device (direct connect with exponential
  backoff: 2/4/8/16/32s fast phase, then 60s slow background retry)
- Device identity: `{address, addressType, name}` owned by BLEManager,
  persisted via ConfigManager. Opportunistic bonding (bonds if peer supports
  SMP, never requires it).
- **HRM pairing page** in the BLE portal tab (scan, select, pair, forget)

Echenon modules handle:
- Parsing HR measurement notifications (`onNotification()` on the main loop)
- Re-broadcasting to the HR Service characteristic via `publishHr()`
- Injecting HR into FTMS packets and session data

### 8.3 Tablet BLE Connections

The tablet maintains two independent BLE connections:

| Connection | Target | Purpose |
|---|---|---|
| 1 | Factory Echelon PCB | Bike metrics for class UI |
| 2 | Echenon ESP32 | Heart rate (re-broadcast from armband) |

### 8.4 Stale Data Protection

If no HR update received for **10 seconds** (stale threshold):
- HR Service stops notifying (0 BPM)
- FTMS packet clears bit 9 (HR not present)
- Session records HR = 0

On reconnect, notifications resume immediately.

**Three-tier HR gap criteria:**

| Criterion | Limit | What it measures |
|---|---|---|
| **(a) Stale detection** | HR absent within **12s** of last notification | App-level policy — how fast we mark HR as unavailable (10s threshold + 2s grace) |
| **(b) Steady state** | No gap **> 2.5s** while connected | Radio coexistence jitter at a ~1 Hz strap. Measured in the PR-07 coexistence test. |
| **(c) Recovery** | Reconnected within **35s** of strap returning to range | Fast-phase backoff: 2+4+8+16 = 30s for 4 attempts + connection time. After the fast phase, slow 60s background retry continues indefinitely. |

**Reconnect policy** (managed by BLEManager):
- **Direct connect** to stored address (not a general scan) — cheap on radio
- Fast phase: 2s, 4s, 8s, 16s, 32s exponential backoff
- Slow phase: one attempt every 60s, indefinitely while RUNNING
- `ScanInhibit` tokens suspend the schedule; it resumes afterwards
- No mid-ride re-pair needed — the device always comes back

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

**Scanning discipline (two distinct operations):**

*Discovery scan* — general scan to find new devices:

| Rule | Rationale |
|---|---|
| **User-initiated only** | Only scan when the user opens the BLE pairing page in the portal or triggers via MCP. Never scan autonomously for new devices. |
| **Scan window ≤ 50% of scan interval** | E.g. interval=100ms, window=50ms. Leaves radio time for WiFi. |
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

- **Strava upload:** StravaUploader acquires a `ScanInhibit` RAII token from
  BLEManager before starting the upload. While the token is alive, no discovery
  or reconnect scans start, and running scans stop. Connections and advertising
  are unaffected. The DataManager's backoff handles any deferred fetches
  gracefully.
- **OTA download:** OTA is a large sustained WiFi transfer. BLE advertising
  continues (low cost) but **FTMS notifications should pause** — the user is
  not riding during OTA. HRM scan/reconnect should be suppressed.
- **OTA boot-time safety:** The Foundation's `OTAUpdateManager` with
  `autoCheck=true` runs a full update **including reboot** as soon as WiFi
  connects after boot — which is exactly when a rider powers on the trainer.
  Until upstream [issue #226](https://github.com/matthewmcneill/esp32-app-foundation/issues/226)
  lands (adds `checkOnBoot` + runtime "busy" guard), Echenon runs with
  **`autoCheck=false`**. OTA is manual-only via the System portal tab.

**NimBLE connection parameter tuning:**

> [!NOTE]
> In the **Peripheral** role (FTMS, HR re-broadcast), the ESP32 can only
> *request* preferred connection parameters — the **Central** (Zwift, TrainerRoad,
> head unit) sets the actual interval. In the **Central** role (HR strap),
> the ESP32 sets the parameters directly.

```cpp
// Peripheral (FTMS + HR Service) — PREFERRED parameters only
// The Central (Zwift etc.) decides the actual interval.
// We request 15–30ms for responsive notifications, but the Central may override.
// supervision_timeout: 400 (4s) → tolerates brief WiFi bursts
pServer->setPreferredConnParams(12, 24, 0, 400);  // requested, not guaranteed

// Central (HR strap) — we control these directly
// conn_interval: 30–75ms → relaxed, HR updates are ~1 Hz
// supervision_timeout: 600 (6s) → tolerates WiFi uploads
pClient->updateConnParams(24, 60, 0, 600);         // applied by us
```

**FTMS + WebSocket overlay — concurrent operation analysis:**

During a ride, the user always has both active simultaneously. This is the
primary use case, not an edge case:

| Stream | Protocol | Rate | Payload | Radio time per event |
|---|---|---|---|---|
| FTMS Indoor Bike Data | BLE notify | 2 Hz (500ms) | 11 bytes | ~2.5ms (1M PHY, single conn event) |
| HR relay notification | BLE notify | ~1 Hz (1000ms) | 4 bytes | ~1.5ms |
| WebSocket overlay push | WiFi TCP | 2 Hz (500ms) | ~100 bytes JSON | ~1–2ms |
| HR re-broadcast | BLE notify | ~1 Hz (1000ms) | 4 bytes | ~1.5ms |

**Steady-state radio budget (notification payloads only):**

> [!NOTE]
> This budget counts only notification payload transmissions, not the underlying
> BLE connection events. Each connection fires a connection event every interval
> (e.g. ~67/s at 15ms) even with no data. WiFi beacon reception, TCP ACKs, and
> AP traffic add further overhead. The real duty cycle is several times the figure
> below. **The measured coexistence test (see verification section) is the
> arbiter — not this arithmetic.**

- BLE: ~2.5ms × 2 (FTMS) + ~1.5ms × 2 (HR in + HR out) = **~8ms per second**
- WiFi: ~2ms × 2 (WebSocket) = **~4ms per second**
- Total notification payload: ~12ms of radio time per second = **~1.2% duty cycle**

Despite connection event overhead increasing the real duty cycle, the
coexistence controller has substantial headroom for these small, infrequent
payloads. The risk is **not** sustained throughput — it's **collision timing**
and burst scenarios.

**The collision risk — synchronized 2 Hz ticks:**

If FTMS notification and WebSocket push fire at the same instant (both on 500ms
timers), they compete for the radio in the same millisecond window. The coex
arbiter will delay one — typically WiFi yields to BLE (BLE has higher default
priority in `CONFIG_ESP_COEX`). This causes:

- WebSocket frame delayed by 2–3ms (imperceptible to user)
- No data loss — TCP retransmits if needed, BLE notification is fire-and-forget

**Application-level timer offset (not a radio-level mitigation):**

> [!NOTE]
> A notification queued at app time `t` goes over the air at the *next
> connection event*, which is clocked by the BLE controller, not `millis()`.
> WiFi transmit timing depends on the TCP stack and the AP. The coexistence
> arbiter operates per event type and priority — WiFi can preempt BLE, not
> only the reverse. **Staggering application timers does not phase-lock the
> radio.** The offset below is an application-level scheduling preference
> that avoids unnecessary CPU contention in the tick loop, not a coexistence
> guarantee.

```cpp
// In EchenonModule::tick()
uint32_t now = millis();

// FTMS fires at t=0, 500, 1000, 1500 ...
if (now - _lastFTMS >= 500) {
    _lastFTMS = now;
    _ftmsServer.update(watts, rpm, hr);
}

// WebSocket fires at t=250, 750, 1250, 1750 ... (offset by 250ms)
if (now - _lastWSPush >= 500 && (now % 500) >= 250) {
    _lastWSPush = now;
    _overlay.pushData(watts, rpm, hr, resistance, sessionState);
}
```

**Burst scenarios (overlay page load):**

| Scenario | WiFi burst | BLE impact | Mitigation |
|---|---|---|---|
| Initial `/overlay` load | ~15 KB gzipped (1–2 TCP segments) | FTMS may miss 1 notification (~500ms gap) | Acceptable — user is opening the app, not mid-sprint |
| Portal tab navigation | ~5 KB per tab script | Negligible (~1 TCP segment) | gzip+PROGMEM makes this fast |
| Strava upload | ~10–50 KB TCX over HTTPS | 2–3 FTMS notifications may jitter | Upload runs post-ride (session ended) — FTMS irrelevant |
| OTA update | ~1.5 MB sustained | FTMS will stutter badly | User is not riding during OTA — pause FTMS notifications |

The only scenario where FTMS + WiFi burst overlap during an active ride is the
initial overlay page load — and that's a one-time 1–2 second event at the start
of the session before the user begins pedalling.

**Verification requirement (PR-07 integration test):**

The following coexistence scenario must be tested at **maximum configuration**
and documented:

1. **3 Peripheral connections** (FTMS notifying at 2 Hz on at least one, plus
   the Echelon tablet and one additional app — e.g. nRF Connect × 2 + Zwift)
2. **1 Central connection** (HR strap connected and relaying at ~1 Hz)
3. Portal `/overlay` open via WebSocket (2 Hz push)
4. Trigger Strava upload (or simulate with a large HTTP POST)
5. **Run for 5 minutes continuously**
6. **Pass criteria:**
   - FTMS notifications: no gap >600ms (allows one missed 500ms cycle)
   - HR relay (steady-state): no gap >2.5s at a ~1 Hz strap
   - HR relay (dropout + reconnect): reconnected within 35s of strap returning
   - WebSocket: no disconnect during upload
   - Upload: completes within DataManager deadline (15s)
7. **Measurement points (defined):**
   - FTMS gap: time between consecutive Indoor Bike Data notifications
     received by the Central (nRF Connect log)
   - HR relay latency (<50ms budget, p99): measured using
     `BLENotification.receivedAtMs` — from *host callback receive* to
     *re-broadcast notification queued on HRMRepeater characteristic*.
     Over-the-air latency (external Central's connection interval) is
     outside our control and excluded.
   - HR gap: time between consecutive HR Measurement notifications received
     by the Echelon tablet (or nRF Connect as proxy)

---

## 9. Session Recording

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

### 9.3 Ride Data Buffer

```cpp
struct RideDataPoint {
    uint32_t timestamp_ms;   // millis() since session start
    uint16_t cadence_rpm;    // 0.5 RPM resolution
    uint8_t  resistance;     // Level 1–32
    int16_t  power_watts;    // Instantaneous power
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
3. **Phase 1** — generates TCX by streaming XML to a LittleFS file
4. **Phase 2** — uploads the TCX file to Strava via HTTPS
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

### 9.6 Manual Download

The web portal exposes a download endpoint for manual file retrieval:

```
GET /api/session/download?id=<session_id>&format=tcx
```

This allows riders to download the TCX file directly from the ESP32 and
manually upload it to any platform (Xert, TrainingPeaks, Golden Cheetah, etc.)
if cloud sync is unavailable or not configured.

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
class IUploader {
public:
    virtual ~IUploader() = default;
    virtual const char* name() = 0;          // "Strava", "Garmin Connect"
    virtual const char* icon() = 0;          // Emoji or icon class
    virtual bool isConfigured() = 0;         // OAuth tokens present?
    virtual bool upload(const RideSession& session) = 0;
    virtual void contributeSettings(WebServer& server) = 0;  // Settings page
};
```

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
1. User clicks "Connect Strava" on the Upload Services settings page
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
    FetchResult fetchUpdate(const FetchContext& ctx) override {
        // DataManager provides:
        // - ctx.client: pre-configured TLS WiFiClient
        // - ctx.deadline: bounded timeout with isExpired() checks
        // - TWDT unsubscription (TwdtFetchGuard)
        // - Automatic health state tracking & backoff on failure

        if (!_pendingSession) return FetchResult::NoData;

        HTTPClient http;
        http.begin(*ctx.client, "https://www.strava.com/api/v3/uploads");
        // ... multipart POST with TCX payload ...
        // Check ctx.deadline.isExpired() between chunks

        return ok ? FetchResult::Success : FetchResult::Error;
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

Accessible at `http://echenon.local/dashboard` (or device IP).

### 11.2 Live Ride View

Updated in real-time via WebSocket:

```
┌──────────────────────────────────────────────┐
│  ● REC  00:32:15             [■ Stop]       │
│                                              │
│  ⚡ 187W        🔄 82 rpm        ❤️ 145 bpm │
│  ◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼░░░░░░░░░░░░ 18/32     │
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
- Resistance level bar (1–32)
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

## 12. Build System

### 12.1 Project Structure

```
Echenon/
├── .gitmodules
├── platformio.ini
├── lib/
│   └── esp32-app-foundation/        (git submodule)
├── hardware/
│   └── echenon/
│       ├── board.h
│       └── partitions.csv
├── src/
│   ├── main.cpp
│   └── echenon/
│       ├── EchenonModule.h / .cpp
│       ├── CadenceSensor.h / .cpp
│       ├── ResistanceADC.h / .cpp
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
[env:echenon]
extends               = env:esp32s3nano
board_build.partitions = hardware/echenon/partitions.csv
board_build.cmake_extra_args =
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;hardware/echenon/sdkconfig.defaults"

build_flags =
    ${env:esp32s3nano.build_flags}
    -DBOARD_ID=\"echenon\"
    -DBOARD_HEADER=\"hardware/echenon/board.h\"
    -DWITH_BLE

lib_deps =
    ${env:esp32s3nano.lib_deps}
    h2zero/NimBLE-Arduino@^2.1.0
```

The `env:esp32s3nano` base environment (provided by the Foundation) already
configures the Waveshare board, PSRAM, USB-CDC, and core library dependencies.
Echenon extends it, overriding only the board header, partition table, and
adding BLE support.

### 12.3 board.h

```cpp
#pragma once

// --- Sensor Pins ---
#define PIN_CADENCE_SENSOR    5    // D2 — GPIO 5, interrupt-capable
#define PIN_RESISTANCE_ADC    1    // A0 — GPIO 1, ADC1_CH0

// --- Feature Flags ---
#define WITH_BLE                    // Foundation BLEManager
#define WITH_BLE_FTMS               // FTMS server
#define WITH_BLE_HRM_RELAY          // HRM Central + re-broadcast
#define WITH_STRAVA                 // Strava API integration
// #define WITH_DISPLAY_U8G2        // No physical display
// #define WITH_HOMEASSISTANT       // Not used
```

---

## 13. Testing & Verification

### 13.1 Phase 1 — Desk Test (No Bike)

1. Flash firmware via USB-C.
2. Connect to `Echenon-Setup` WiFi AP → configure home WiFi.
3. Open `http://echenon.local` → verify portal with overlay, BLE, and
   upload settings tabs.
4. Open **nRF Connect** → verify `"Echenon"` advertising FTMS (`0x1826`)
   and HR (`0x180D`).
5. Connect → verify 2 Hz notifications on `0x2AD2` with zeroed data.
6. Simulate cadence (short D2 to GND) → observe changes in overlay and
   BLE notifications.

### 13.2 Phase 2 — Sensor Integration (Console Open)

1. Complete wire splice with console cover removed.
2. Web overlay: cadence matches manual count, resistance tracks knob 1–32.
3. Run potentiometer calibration via portal Calibration page.
4. Pair HR strap via portal BLE page → verify HR in overlay and FTMS data.

### 13.3 Phase 3 — End-to-End

1. **Echelon coexistence:** Run Echelon class. Pair `"Echenon"` as HR device
   in Echelon app. Verify HR on tablet + ESP32 not interfering with bike.
2. **Auto-session:** Pedal → overlay shows "REC". Stop 3 minutes → session
   ends, upload triggered.
3. **Strava:** Connect via portal. Ride. Verify activity in Strava with power,
   cadence, HR.
4. **FTMS consumer:** Zwift → pair `"Echenon"` → verify live power/cadence.
5. **Floating window:** Chrome on Lenovo P12 → overlay as PiP over Echelon
   class.

### 13.4 Known Limitations

- Power estimated, not measured. Accuracy improves with reference power meter
  calibration.
- Speed synthetic (from cadence). Strava distance approximate.
- No FTMS Control Point (ERG mode). Zwift treats as power source only.
- Echelon app may show slightly different cadence/resistance values.

---

## Appendix A: User Journeys

### Journey 1 — Daily Echelon Class (Hands-Free)

> Ride an instructor class. Data appears in Strava automatically.

1. Put on HR armband.
2. Pedal → bike wakes → ESP32 boots → connects to armband → connects to WiFi.
3. **Tablet:** Open Echelon app → select class → pair bike (factory BLE).
4. In Echelon HR settings → pair `"Echenon"` as heart rate device.
5. Optionally: open `echenon.local` as floating window for live overlay.
6. **Start pedalling** → Echenon auto-starts recording.
7. Ride the class. HR on Echelon tablet. Live power/charts in overlay.
8. Class ends → stop pedalling → dismount.
9. 3 minutes → session auto-ends → uploads to Strava.
10. Open Strava later: full ride with power curve, cadence, HR zones.

**No buttons. No apps to start. Just ride.**

---

### Journey 2 — Manual Session Control

> Exclude warm-up, explicit start/stop.

1–5. Same as Journey 1.
6. Warm-up on bike. Overlay shows live data, status "Idle".
7. Tap **"Start"** in overlay.
8. Ride the class.
9. Tap **"Stop"**. Review post-ride summary.
10. Summary auto-uploads (or tap "Discard" to skip).

---

### Journey 3 — Zwift Virtual Ride

> No instructor class. Virtual cycling world.

1. Pedal → ESP32 boots.
2. **Tablet:** Open Zwift → pair `"Echenon"` (FTMS power + cadence).
3. HR from FTMS data (or pair strap separately in Zwift).
4. Ride virtual course — avatar speed from live power.
5. End ride in Zwift → Zwift uploads to Strava.
6. (Echenon's internal recording also runs. Disable auto-upload to avoid
   Strava duplicate, or let Strava's duplicate detection handle it.)

---

### Journey 4 — Structured Training (TrainerRoad / SYSTM)

> Power-based workout with fixed-wattage targets.

1. Pedal → ESP32 boots.
2. **Tablet:** Open training app → pair `"Echenon"` (FTMS).
3. App shows: **"Target: 180W — Current: 165W — INCREASE"**
4. Manually turn red knob until power matches.
5. Complete workout → app uploads to Strava.

---

### Journey 4b — Xert SMART Workout (Advanced Training)

> Dynamic workout with real-time physiological tracking. Xert's SMART
> intervals adapt to your current fatigue — no FTP test required.

1. Pedal → ESP32 boots.
2. **Tablet:** Open **Xert EBC app** → pair `"Echenon"` (FTMS).
3. Select a SMART workout from the Xert library.
4. EBC shows live **MPA gauge** (your maximum available power right now)
   and dynamic targets: **"Ride at 250W until MPA drops to 320W, then
   recover until MPA reaches 95%"**
5. Manually adjust the red knob to match power targets.
6. If you push hard enough to exceed your predicted MPA → **Breakthrough
   detected** → Xert auto-raises your fitness signature (no FTP test!).
7. End ride → activity syncs to Xert via Strava (automatic).

**Why Xert is different:** TrainerRoad prescribes "4×5 min at 280W" — the
same for a fresh day and a tired day. Xert's intervals dynamically shorten
or lengthen based on your real-time fatigue, ensuring the intended
physiological strain is achieved regardless of how you're feeling.

**Note:** Since we don't implement FTMS Control Point (ERG mode), Xert EBC
runs in **Open mode** — it coaches with dynamic targets, you adjust the knob.
If ERG mode is added in a future version, Xert EBC can send automatic
resistance commands via the FTMS Control Point characteristic.

**Dual-app setup** (optional): Run Xert EBC for coaching + Zwift for visuals
simultaneously. Both connect to `"Echenon"` via BLE (the BLE module supports
multiple simultaneous Central connections).

---

### Journey 5 — First-Time Setup

> One-time configuration after flashing.

1. Flash firmware via USB-C.
2. Power on → `Echenon-Setup` WiFi AP appears.
3. Phone/tablet → connect to AP → captive portal.
4. Enter home WiFi credentials → ESP32 connects.
5. Navigate to `http://echenon.local`.
6. **Calibration tab:**
   - Potentiometer calibration: turn knob min → "Set" → turn max → "Set".
   - Power source: select "Echelon EX-3 Community Table" from dropdown
     (or "Physics Defaults" if no table available for your model).
7. **BLE tab:** Click "Scan" → select HR strap → paired.
8. **Upload Services tab:** Click "Connect Strava" → authorize.
9. Done. Everything automatic from now on.

---

### Journey 6 — Free Ride (No App)

> Just pedal. No class, no virtual world. Maybe watching YouTube or Netflix.

1. Put on HR armband. Pedal → ESP32 boots.
2. **Tablet:** Open whatever you want — YouTube, Netflix, a film, nothing.
3. Optionally: open `echenon.local` as floating window for live power/HR.
4. Pedal → Echenon auto-starts recording.
5. Ride as long as you want. The overlay shows your live data.
6. Stop → 3 minutes → session auto-ends → uploads to Strava.

**This is the simplest possible use case.** No fitness apps involved at all.
Echenon records your ride autonomously. Open Strava later to see your workout.

---

### Journey 7 — OTA Firmware Update

> Update Echenon firmware without opening the bike console.

> [!NOTE]
> Automatic OTA on boot is **disabled** (`autoCheck=false`) until upstream
> [#226](https://github.com/matthewmcneill/esp32-app-foundation/issues/226)
> adds a runtime "busy" guard. Without it, OTA could trigger mid-warmup.

1. Navigate to `http://echenon.local` → **System** tab.
2. Click "Check for Updates" to manually check.
3. If an update is available: review changelog → click "Update".
4. Progress bar shows upload and verification. ESP32 reboots.
5. Dashboard reconnects automatically. New firmware running.

No USB cable, no laptop, no console disassembly. The ESP32 stays wired inside
the bike permanently.
