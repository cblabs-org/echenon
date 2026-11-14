# Echenon — Firmware Specification

**An open training computer for the Echelon EX-3 indoor bike.**

> **Status:** Draft, repaired 2026-09-25 (readiness review RR-01 to RR-03, RR-13).
> Values written as `[DR-1: …]` are decided at the Design Freeze (DR-1) from
> Phase 0 evidence and are not specified before then. Values written as
> `[PR-nn: …]` are tuned and recorded by the named PR. Values written as
> `[DR-2: …]` belong to the v2 Active Design Freeze.
>
> **DR-1 (v1 Passive) approved with conditions, 2026-10-12.** Every v1
> `[DR-1: …]` value is resolved in this revision. Values still open are
> `[PR-nn: …]` (tuned by the named PR) or `[DR-2: …]` (v2 only).
>
> **Releases (owner decision 2026-10-12):** **v1 "Passive"** and **v2 "Active"**
> (§1.1, §1.5). Sections or rows marked **v2** describe the provisional v2
> release and are not part of v1.

---

## 1. Project Overview

### 1.1 Purpose

Echenon turns an Echelon EX-3 Smart Connect indoor bike into a self-contained,
open training computer. It ships as **two releases**:

- **v1 "Passive"**: a permanent **passive tap board** that only reads the
  bike's signals, while every wire still runs unaltered between the bike frame
  and the Echelon console. **No resistance control of any kind.**
- **v2 "Active"** (provisional): an **active intercept board** (the "MITM"
  board) that takes the knob, the console's motor drive and the sensors in as
  signals and re-emits them, adding resistance control and console-off
  operation.

| Release | Board | What it adds | Use cases |
|---|---|---|---|
| **v1 Passive** | Passive tap board (PR-04a); ESP32 on the bike's 9 V via VIN or on USB-C, never both (VIN/USB selector) | Ride page, automatic recording and Strava upload, **read-only** FTMS telemetry (Indoor Bike Data) over BLE, heart-rate relay | UC1–UC3 |
| **v2 Active** | Active intercept board (PR-07): H-bridge + PWM motor drive, encoder emulation, NC fail-safe bypasses | Mode 1 relay/active: FTMS Control Point targets, ERG, resistance setting, portal-native HR-zone auto-resistance. Mode 2 console off (UC6) | UC4–UC6 + HR zones |

Echenon provides:

- A **ride page**: a standalone training-computer UI showing live power,
  cadence, heart rate, charts and session controls
- **Automatic session recording** with post-ride upload to Strava (other
  platforms post-v1)
- A **BLE FTMS broadcast** readable by Zwift, TrainerRoad, Xert EBC and any
  FTMS-aware app (**read-only in v1**: no Control Point targets)
- A **heart-rate relay**: Echenon connects to an HR strap and re-broadcasts to
  the Echelon tablet app, the FTMS stream and the session recording
- **v2:** **automatic resistance control (ERG)** and portal-native HR-zone
  auto-resistance, with the active board only: FTMS Control Point targets drive
  the resistance motor under closed-loop cam-angle control

**v2** — with the active board installed, Echenon has two operating modes
(details `[DR-2]`; §5.6 uses the earlier names Intercept and Override):

- **Mode 1, relay/active (Intercept)**: the console is connected. The ESP32
  takes the knob and the console's motor drive in as signals, emulates the
  encoder toward the console and drives the motor through an H-bridge
  ("console as signal source", `docs/mitm-encoder-design-notes.md` §11). The
  Echelon app runs unchanged.
- **Mode 2, console off (Override, UC6)**: the console is switched off or
  unplugged. The ESP32 drives the resistance motor from the knob, the web
  ride page or the FTMS Control Point, with a replica of the console's pot
  reference.

The v1 passive tap board has no motor path and no outputs; it only observes,
and FTMS is read-only (§7.3, §7.4). **ANT+ is not supported** in either
release (the ESP32-S3 has no ANT radio); UART0 is reserved for a future ANT+
add-on (§4.4).

Related documents:

- EX-3 internal sensors, looms, connectors and actuators:
  [`docs/ex3_specification.md`](docs/ex3_specification.md)
- Circuit build guide per phase:
  [`docs/hardware_specification.md`](docs/hardware_specification.md)
- Standalone v1 Passive Tap Board beta builder guide (`v0.3.0-beta.1`):
  [`docs/beta-build-guide.md`](docs/beta-build-guide.md)
- Development rationale:
  [`docs/implementation-strategy.md`](docs/implementation-strategy.md)
- Operating scenarios UC1–UC6:
  [`docs/use_case_analysis.md`](docs/use_case_analysis.md)
- Phase 0 measurement register:
  [`docs/measurements/phase0/README.md`](docs/measurements/phase0/README.md)
  (the "Phase 0 register", also called Appendix A in programme documents)

### 1.2 Design Principles

1. **Non-destructive**: no factory wire is cut. Both boards plug into the
   existing JST connectors between the bike frame and the console. Unplugging
   restores the bike to stock.
2. **Passive first, control in v2**: v1 observes only and never controls
   resistance. Motor control and interception are the v2 hardware upgrade,
   not a prerequisite.
3. **Hands-free daily use**: recording starts when the rider pedals and uploads
   when the rider stops. No buttons, no apps to open.
4. **Three independent outputs**: the ride page, the BLE FTMS broadcast
   and the cloud upload are decoupled. Each works without the others.
5. **Open ecosystem**: FTMS advertises continuously. Data uploads to the
   rider's own accounts. Nothing is locked down.
6. **Invisible taps**: the passive tap board reads the cadence, potentiometer,
   encoder and motor-drive signals through high-impedance taps (series
   resistors, no internal pulls). The console sees the same signals it sees
   without Echenon. v1 places the ESP32 in no signal path; only the v2 active
   board does.
7. **Measured, not assumed**: resistance control precision, speed and the size
   of any Override level scale are goals validated against measured data
   (`[DR-2]`), not claims made in advance.
8. **Original implementation**: written from first principles, public
   specifications and permissively licensed references.
9. **Hardware-portable**: all hardware-specific configuration (pin map, bike
   signal set, partition table, SDK settings) lives in one target directory
   under `hardware/`. A different ESP32 board or bike is a new target
   directory plus a PlatformIO environment. Firmware uses `#if HAS_*` guards
   for bike signals and `WITH_*` flags for compiled modules (§12.3).

### 1.3 Built On

Echenon is an application module on the
[ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation),
vendored as the git submodule `framework/`. It inherits WiFi management, the web
portal, encrypted credential storage, OTA updates, the DataManager background
fetch pipeline, the module composition architecture and the optional
**BLEManager** module.

- BLEManager exists from framework tag `v0.0.12-dev.82` onward. PR-01 WO1 bumps
  the submodule to `v0.0.12-dev.82` with BLE **off**, which also brings in the
  fix for framework issue #236 (WiFi fallback to an open AP).
- PR-03 turns BLE on (`WITH_BLE`, `h2zero/esp-nimble-cpp@^2.1.0`) once its
  entry gates close (§8.3).
- OTA is manual-only until framework issue #226 lands (§8.5). For **`v0.3.0-beta.1`**,
  `WITH_OTA` is intentionally omitted/disabled in `platformio.ini` while
  framework OTA undergoes stabilisation; beta firmware updates are performed
  over **USB-C** with the mechanical `SW0` `VIN`/`USB-C` interlock set to USB
  ([`docs/beta-build-guide.md`](docs/beta-build-guide.md)).

### 1.4 Out of Scope (v1)

**Out of scope for v1 "Passive":**

- **Any resistance control**: no FTMS Control Point targets, no ERG, no
  resistance setting from the portal or apps, no HR-zone auto-resistance
  (external or portal-native). These are v2
- **Any interception**: no encoder or motor interception, no outputs to the
  bike; the stock console runaway (A09-20261012 #3) can be detected but not
  prevented (§15.11)
- Standalone / console-off operation (UC6; v2 mode 2)
- **ANT+**: not supported. The ESP32-S3 has no ANT radio. A future hardware
  add-on (e.g. an nRF51422 ANT+ module) may connect over **UART0, GPIO 43 /
  44, reserved** on every board (§4.4)
- On-device physical display
- Custom PCB manufacture (v1 uses perfboard or protoboard)
- Direct upload to platforms other than Strava, Intervals.icu, and Wahoo Cloud (§10.5)

**Out of scope for v2 "Active"** (provisional):

- ANT+ (as v1; the UART0 reservation stands)
- Automatic mode switching between mode 1 and mode 2 `[DR-2]`
- On-device physical display; custom PCB manufacture

### 1.5 Delivery Phases

The programme (`.agents/skills/principal-architect-workspace/PROGRAM-PLAN.md`)
is canonical for sequencing. This specification uses its numbering.

**v1 "Passive"** (UC1–UC3):

| Phase | Work | PR | Hardware |
|---|---|---|---|
| 0 | Observe | PR-01 ✅ | Fly-wire logger on USB power (§4.4) |
| — | v1 Passive Design Freeze | **DR-1** | — |
| 1 | Calibrate | PR-02 (gated on DR-1) | Same fly-wire rig |
| 2 | HR relay | PR-03 | Same rig (BLE only) |
| 3 | Passive tap board (VIN or USB via a selector, manual OTA, UART0 reserved) | PR-04a | Passive tap board, permanent install |
| 3 | FTMS telemetry, read-only | PR-05 | Fly-wire rig is sufficient |
| 4 | Session + TCX + Strava | PR-06 | Any of the above |
| 4 | Diagnostics & run-data persistence (save-to-flash option, default off; paged query interface) | PR-14 | Any of the above |
| 7 | v1 integration and release (`1.0.0`) | PR-09 | Passive tap board |

**v2 "Active"** (provisional; decomposed after DR-2; UC4–UC6 + HR zones):

| Phase | Work | PR | Hardware |
|---|---|---|---|
| — | Active-design bench evidence | DR-2 prep | Bench rig only |
| — | v2 Active Design Freeze | **DR-2** | — |
| 5 | Active board + motor control + safety | PR-07 (after DR-2 and the v1 release) | Active intercept board |
| 5 | Encoder emulation + relay mode (mode 1) | PR-10 | Active intercept board |
| 6 | ERG + Control Point targets + resistance setting | PR-08 | Active intercept board |
| 6 | HR-zone auto-resistance (portal-native) | PR-11 | Active intercept board |
| 6 | Console-off standalone (mode 2, UC6) | PR-12 | Active intercept board + pot-reference replica |
| 7 | v2 integration and release | PR-13 | Active intercept board |

The decision records are
`.agents/skills/principal-architect-workspace/reviews/DR-1-design-freeze.md`
(v1) and `.agents/skills/principal-architect-workspace/reviews/DR-2-design-freeze.md`
(v2).

---

## 2. Licensing & Attribution

### 2.1 Project License

Apache License 2.0, compatible with all dependencies.

### 2.2 Dependencies

| Library | License | Usage |
|---|---|---|
| [ESP32 App Foundation](https://github.com/matthewmcneill/esp32-app-foundation) | Apache-2.0 | Framework (WiFi, web, config, OTA, auth, BLE) |
| [h2zero/esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) | Apache-2.0 | BLE stack (via Foundation BLEManager) |
| Espressif Arduino Core and ESP-IDF for ESP32 | Apache-2.0 | Hardware abstraction |

### 2.3 Reference Implementations

The BLE FTMS GATT service structure references the MIT-licensed
[jamesjmtaylor/esp32-ftms-server](https://github.com/jamesjmtaylor/esp32-ftms-server)
and [dbsqp/bluetooth-reebok-57e](https://github.com/dbsqp/bluetooth-reebok-57e).
MIT copyright notices are preserved in adapted source files.

The power model derives from the physics of magnetic eddy-current braking and
from data measured on the target bike. The BLE FTMS packet format follows the
Bluetooth SIG Fitness Machine Service v1.0 public specification.

---

## 3. System Architecture

### 3.1 Three Independent Output Paths

Echenon has three output paths. They are **completely independent**: each works
regardless of whether the others are active, connected or configured.

```
 Sensors → PowerEngine → SessionManager
                               │
               ┌───────────────┼───────────────┐
               ▼               ▼               ▼

          📊 RIDE PAGE    📡 BLE FTMS     ☁️ UPLOAD
          (always)        (always on)     (post-session)

     Session control    Broadcasts to    Pushes completed
     Live metrics       any app that     sessions to the
     Charts & graphs    connects, or     rider's Strava
     HR zones           to nobody        account

     Floats over        Zwift            Strava ──► Xert*
     ANY app:           TrainerRoad
     - Echelon          Xert EBC
     - Zwift            Wahoo SYSTM
     - Xert EBC         Garmin watch
     - YouTube          (or nobody)
     - Nothing

* Xert auto-syncs from Strava; no direct Echenon integration is needed.
```

| Path | What it does | When | Knows about the others? |
|---|---|---|---|
| **Ride page** | Live training-computer UI: session control, metrics, charts | Always available via the web portal | No |
| **FTMS** | Broadcasts standard BLE fitness data | Advertising whenever BLE is running | No |
| **Upload** | Pushes finished sessions to cloud platforms | After a session ends | No |

### 3.2 High-Level Block Diagram

The diagram shows the Phase 3+ boards. Pin numbers follow the Phase 3+ map
(§4.4.2): its v1 rows were frozen at DR-1 and are identical to the Phase 0/1
map (§4.4.1); its v2 rows stay provisional until DR-2.

```
┌──────────────────── ECHELON EX-3 BIKE FRAME ────────────────────
│  Reed switch     Potentiometer        DC motor       Rotary encoder
│  (cadence)       (cam position)       (resistance)   (red knob)
│  Orange + Grey   Yellow/Blue/Green    Red + Black    Black/Red/Orange
│  └──────────────── 9-pin JST-SM ────────────────┘    3-pin JST
└──────────────────────────┬───────────────────────────────┬──────
                           │ J1                            │ J3
┌──────────────────────────▼───────────────────────────────▼──────
│  ECHENON BOARD (passive tap board, or optional MITM board)
│
│  Pass-through J1 → J2 and J3 → J4 for every wire, except that the
│  MITM board splits Red/Black (motor) as shown below.
│
│  Taps into the ESP32-S3-Nano (series R, no internal pulls):
│    Blue  (pot wiper)        → A0 / GPIO 1   ADC1
│    Orange (cadence)         → D2 / GPIO 5   digital, interrupt
│    Encoder Ch A / Ch B      → D5 / GPIO 8, D6 / GPIO 9   PCNT (Black=A, Red=B)
│    Console motor + / −      → 30 kΩ/10 kΩ (÷4) → D3 / GPIO 6, D4 / GPIO 7
│                               ADC1 analog sense inputs only
│    v1 monitors: Yellow GPIO 2, Green GPIO 3, Grey GPIO 4, enc VCC GPIO 10
│
│  v2 active board only (Phase 5+, pins [DR-2]):
│    D7 / GPIO 10 ──► L9110S A-IA ─┐
│    D8 / GPIO 17 ──► L9110S A-IB ─┴─► A-OA / A-OB ──► bike motor Red / Black
│    Console-side Red / Black terminate in the ÷4 sense divider only.
│
│  Power (v1): White +9 V ──► VIN/USB selector ──► Waveshare VIN ──► 3V3
│         (VIN or USB-C, never both, §4.6; Phase 0/1 logger on USB-C)
│  UART0 GPIO 43/44 reserved (future ANT+ add-on)
│
│  Firmware:
│    CadenceSensor, ResistanceSensor, EncoderModule ──► PowerEngine
│    PowerEngine ──► SessionManager ──► Ride page / FTMS / Upload
│    HRMClient ──► EventBus ──► HRMRepeater, FTMSServer, SessionManager
│    InterceptController, ResistanceMotor, ERGController (v2 only)
└───────┬───────────────────────────┬───────────────────────────┬──
        │ WiFi (HTTP + WebSocket)   │ BLE                       │ WiFi (HTTPS)
        ▼                           ▼                           ▼
  Ride page                  FTMS 0x1826                  Strava API
  (echenon.local)             HR   0x180D
                              Control Point 0x2AD9 (v2)
                                    │
                                    ▼
                  Zwift / TrainerRoad / Xert EBC; Echelon app (HR)
```

**Signal routing summary (Phase 3+; tap column frozen for v1 at DR-1, motor-path column `[DR-2]`):**

| J1 wire | Colour | Bike → Console | Tap to ESP32 | Motor path (MITM board only) |
|---|---|---|---|---|
| Power +9 V | White | Pass-through | Waveshare VIN via the VIN/USB selector (§4.6) | — |
| GND | Brown | Pass-through | Common ground | Driver GND `[DR-2]` |
| Cadence | Orange | Pass-through | GPIO 5 via 4.7 kΩ + 100 nF | — |
| Cadence return | Grey | Pass-through | GPIO 4 via 4.7 kΩ + 100 nF (v1 monitor, measured 0 V) | — |
| Pot outer (0 V) | Yellow | Pass-through | GPIO 2 via 22 kΩ (v1 monitor, measured 0 V) | — |
| Pot wiper | Blue | Pass-through | GPIO 1 via 22 kΩ (ADC1) | — |
| Pot outer ref (1.67 V) | Green | Pass-through | GPIO 3 via 22 kΩ (v1 monitor, measured 1.67 V) | — |
| **Motor +** | **Red** | Passive: pass-through. MITM: console side terminates in the sense divider | **GPIO 6 via ÷4 divider (ADC1 sense)** | Bike side ← L9110S A-OA |
| **Motor −** | **Black** | Passive: pass-through. MITM: console side terminates in the sense divider | **GPIO 7 via ÷4 divider (ADC1 sense)** | Bike side ← L9110S A-OB |

On the passive tap board all nine wires pass straight through, and the
motor-sense taps observe the console's drive. On the MITM board the two motor
wires are **split**: the bike-side motor connects to the L9110S outputs, and
the console-side motor lines connect only to the sense divider. The divider's
10 kΩ lower leg holds each sense input low when the console is absent, so no
separate pull-down resistor is fitted. GPIO 6 and 7 are sense inputs in every
phase and are never driver outputs; the driver uses GPIO 10 and 17.

### 3.3 Data Flow

1. **Every pedal revolution**: the reed switch produces one falling edge on
   GPIO 5. The cadence ISR records the timestamp for the RPM calculation
   (§5.1).
2. **Resistance position, fast path**: the pot wiper on GPIO 1 is sampled by
   the ADC continuous (DMA) stream. **v1** uses the wiper only through the
   display path (level, internal scale and runaway detection, §5.2–§5.3).
   **v2:** the unfiltered fast path at `[DR-2: fast-path rate, target ≥ 200 Hz]`
   is converted to a **cam angle** within the current half-revolution, with
   half-revolution changes tracked at click-off discontinuities (§5.2). The
   motor controller consumes this path.
3. **Resistance position, display path**: an EMA-filtered copy at 20 Hz
   provides the angle and the display level for the UI (§5.3).
4. **Continuously**: `HRMClient` receives HR notifications as a BLE Central
   and publishes each sample on the EventBus. Subscribers re-broadcast it
   (`HRMRepeater`), embed it in FTMS Indoor Bike Data (`FTMSServer`) and store
   it in the session buffer (`SessionManager`).
5. **Every 500 ms** (2 Hz):
   - `PowerEngine` computes watts from cadence and cam angle (§6)
   - `FTMSServer` sends an Indoor Bike Data notification if anyone is
     subscribed
   - `SessionManager` appends a data point to the ride buffer
   - the ride page receives a WebSocket update
6. **Console motor sense (v1, monitoring only)**: GPIO 6/7 are sampled as
   ADC1 analog at ≥ 1 kHz through the ÷4 dividers; the drive direction is
   whichever wire is high. Echenon publishes motor-direction and seek events
   and never acts on the motor (§5.2).
   **v2 — Motor path (active board only)** at `[DR-2: control loop rate]`:
   - **Intercept Mode**: `InterceptController` samples the console's motor
     drive on GPIO 6/7 (analog, ADC1) and reproduces the drive direction, and
     the waveform where DR-2 requires it, on the driver inputs `[DR-2: pins]`.
   - **Override Mode**: `InterceptController` selects the active command
     source (knob, ERG target or web command). `ResistanceMotor` drives
     toward the target cam angle and stops inside the dead band (§5.5).
7. **On knob rotation**: the framework `EncoderModule` counts quadrature steps
   in hardware (PCNT) and publishes `encoder.rotate` events. In Override Mode
   the event adjusts the target; in Intercept Mode the console acts on the
   knob and Echenon records the event for telemetry only.
8. **v2 — On an FTMS Control Point target write** (Override Mode, PR-08): the
   `ERGController` converts target watts at the current smoothed cadence into
   a target cam angle using the power model, and keeps adjusting it as
   cadence changes (§5.6).
9. **On session end**: `SessionManager` hands the session to `UploadQueue`,
   which runs the upload pipeline inside the framework's DataManager (§9.4,
   §10).

### 3.4 Module Decomposition

**Echenon application modules:**

| Module | Responsibility | Foundation interface | First PR |
|---|---|---|---|
| `EchenonModule` | Composition root; owns all sub-components, exposes providers through accessors, and registers JSON-RPC MCP tools (`POST /api/mcp`: telemetry, calibration, recalibration, power table, FTMS smoothing, and `CalibrationRig` control) | `ITickable`, `IConfigurable`, `IWebContributor`, `IMCPContributor` | PR-01 / PR-09 |
| `CalibrationRig` (`WITH_CALIBRATION_RIG`) | Synthesizes quadrature encoder and cadence pulses for automated bench sweeps; controlled via `IMCPContributor` (`POST /api/mcp`: `echenon_rig_status`, `echenon_rig_command`) rather than raw HTTP endpoints | — (via `EchenonModule` `IMCPContributor`) | PR-02 / PR-09 |
| `SignalLogger` | Phase 0 capture: ride log and burst capture on all ten ADC1 channels, CSV export | — | PR-01 |
| `CadenceSensor` | Interrupt-driven reed switch, RPM calculation | — | PR-02 |
| `ResistanceSensor` | Wiper ADC fast and display paths, cam-angle and half-revolution tracking, display-level mapping | — | PR-02 |
| `PowerEngine` | Watts from cadence and cam angle; calibration tiers | — | PR-02 |
| `CalibrationWizard` | Portal page for passive (PR-02) and motor-driven (PR-07) calibration | — | PR-02 |
| `HRMClient` | BLE Central to the HR strap; parses 0x2A37; publishes HR on the EventBus | `IBLEClientProvider` | PR-03 |
| `HRMRepeater` | Re-broadcasts HR as the BLE Heart Rate Service; subscribes to HR events | `IBLEServiceProvider` | PR-03 |
| `FTMSServer` | FTMS GATT service: Indoor Bike Data, Feature, ranges (v1, read-only); Control Point and Status (**v2**, PR-08) | `IBLEServiceProvider`, `ITickable` | PR-05 / PR-08 |
| `SessionManager` | Auto/manual start-stop, ride buffering, checkpoints | — | PR-06 |
| `RideStore` | PSRAM buffer for data points | — | PR-06 |
| `TCXGenerator` | Streams 1 Hz TCX to a gzip file on LittleFS | — | PR-06 |
| `UploadQueue` | The only upload execution path: runs one pipeline stage per `fetchUpdate()` | `IDataSource` | PR-06 |
| `StravaUploader` | Describes the Strava service: BYOA OAuth, request building, response parsing | `IUploader` (Echenon), `IWebContributor` | PR-06 |
| `IntervalsIcuUploader` | Describes the Intervals.icu service: API-key auth, request building, response parsing | `IUploader` (Echenon), `IConfigurable` | PR-19 |
| `WahooUploader` | Describes the Wahoo Cloud service: BYOA OAuth 2.0, token refresh, multipart TCX upload | `IUploader` (Echenon), `IConfigurable` | PR-21 |
| `SessionOverlay` | REST API overlay for multi-sink upload management: `/api/upload/sinks`, `/api/upload/status`, `/api/intervals/*`, `/api/wahoo/*`, per-sink force-push/retry, session history with sync status | `IWebContributor` | PR-19 / PR-21 |
| `StoredRides` | Portal pill in the Echenon tab showing ride history with per-sink sync badges, TCX download, delete, and force-push/retry actions | — | PR-19 |
| `SessionUI` | Session REST API, authenticated TCX download and Session History tab (PR-06); standalone `/ride` page and `/ws/session` WebSocket push (PR-09) | `IWebContributor` | PR-06 / PR-09 |
| `DiagnosticsModule` | PSRAM-backed bounded event log with ring-buffer semantics; records boot, session, runaway, calibration and firmware-change events (§13) | `ITickable` | PR-14 |
| `RidePersistence` | Saves completed rides as gzip-compressed `.ride.gz` binary files to LittleFS with configurable cap and oldest-first eviction (§14) | — | PR-14 |
| `DiagnosticsRoutes` | REST API for event log queries, stored ride listing, CSV download and ride deletion (§13.4) | — | PR-14 |
| `InterceptController` (**v2**) | Mode manager (Intercept ↔ Override), console-drive relay, command source arbitration | — | PR-07 / PR-10 |
| `ResistanceMotor` (**v2**) | Motor driver (`[DR-2]`), cam-angle position controller, safety limits | — | PR-07 |
| `ERGController` (**v2**) | Control Point target handling, ERG state machine, Fitness Machine Status | — | PR-08 |

`HRMClient` has no reference to `HRMRepeater`; heart rate flows only through
the EventBus (§8.2).

**Foundation-provided infrastructure:**

| Module | What Echenon uses it for |
|---|---|
| `BLEManager` (`WITH_BLE`) | NimBLE lifecycle, advertising packing, HR strap discovery, pairing portal, reconnection, `ScanInhibit` |
| `EncoderModule` (`WITH_ENCODER`) | PCNT quadrature decoding of the red knob (PR-07) |
| `WifiManager` | AP setup, STA connection, mDNS hostname `echenon` |
| `WebServerManager` | Portal chrome, REST API, WebSocket for the ride page |
| `ConfigManager` | Persistence of every `IConfigurable` namespace |
| `KeyManager` | Encrypted storage of Strava client secret and tokens |
| `AuthManager` | Portal login; protects mutations, downloads and motor commands |
| `DataManager` | Background fetch task with deadlines and health state for `UploadQueue` |
| `EventBus` | HR samples, session events, encoder events |
| `OTAUpdateManager` (`WITH_OTA`) | Manual firmware updates without opening the console (from PR-04a; intentionally disabled in `v0.3.0-beta.1` while framework OTA undergoes stabilisation — beta updates use USB-C with the `SW0` `VIN`/`USB-C` interlock set to USB) |

**Coupling rules (Service Module Pattern):**

| Rule | Echenon compliance |
|---|---|
| No stored `Application*` member | `EchenonModule` receives narrow references (`ConfigManager&`, `EventBus&`, `KeyManager&`) through setters before registration, as the framework's template app does |
| No polling of application state in `tick()` | The sensor loop is service-intrinsic |
| No sibling concrete references between sub-components | HR and session events travel on the EventBus |
| Orchestration stays upstream | `EchenonModule` is the composition root; sub-components are services |
| Blocking work stays off `tick()` | TCX generation and HTTPS run inside `UploadQueue::fetchUpdate()` on the DataManager task |

### 3.5 Foundation Integration

**Composition (ADR-16, static scope).** Every module instance is declared at
static or global scope. `Application::begin(const FrameworkConfig&)` boots the
framework. BLE providers are owned by `EchenonModule` and exposed through
accessors. They are registered with `BLEManager` before `app.begin(config)`,
and `bleManager.begin(app)` follows it; this is the order given in the
framework's `docs/developer/bluetooth.md` at `v0.0.12-dev.82`. After
`bleManager.begin(app)` provider registration is locked and further
`registerServiceProvider()` / `registerClientProvider()` calls return `false`.

```cpp
// src/main.cpp
#include "AppFoundation.h"
#include "echenon/EchenonModule.h"

#ifdef WITH_BLE
#include "BLEManager/BLEManager.hpp"
static BLEManager bleManager;
#endif

#ifdef WITH_OTA
#include "OTAUpdateManager.hpp"
static OTAUpdateManager otaManager;
#endif

static Application   app;
static EchenonModule echenon;

void setup() {
    app.setAppName("Echenon");
    app.getWifiManager().setDefaultHostname("echenon");

    // Narrow references only (ADR-39); no Application* is stored.
    echenon.setConfigManager(&app.getConfigManager());
    echenon.setEventBus(&app.getEventBus());
    echenon.setKeyManager(&app.getKeyManager());
    app.registerModule(&echenon);
    app.registerModule(&echenon.uploadQueue());      // IDataSource (PR-06)

#ifdef WITH_BLE
    bleManager.setDefaultAdvertisedName("Echenon");  // advertises Echenon-XXXX
    bleManager.registerServiceProvider(&echenon.ftmsServer());
    bleManager.registerServiceProvider(&echenon.hrmRepeater());
    bleManager.registerClientProvider(&echenon.hrmClient());
    app.registerModule(&bleManager);
#endif

#ifdef WITH_OTA
    app.registerModule(&otaManager);
#endif

    FrameworkConfig config;
    app.begin(config);

#ifdef WITH_BLE
    bleManager.begin(app);                           // locks provider registration
#endif

#ifdef WITH_OTA
    otaManager.begin(app);
    otaManager.markAppValid();
#endif

    echenon.enforceManualOta();                      // §8.5: autoCheck = false
}

void loop() {
    app.tick();
    delay(1);   // yield to the idle task, which feeds the TWDT
}
```

Framework API facts relied on (checked against `v0.0.12-dev.82` headers):

- `IDataSource::fetchUpdate(const FetchContext&)` returns `UpdateStatus`
  (`Success`, `NoChange`, `RateLimited`, `FetchError`, `ParseError`,
  `AuthError`, `Skipped`). `FetchContext` carries a `DeadlineToken& deadline`
  and a framework-owned `WiFiClient* client`; the total budget per fetch is
  15 s (`FETCH_TOTAL_TIMEOUT_MS`).
- `IWebContributor::registerRoutes()` takes `PsychicHttpServer&`.
- `Redactor::registerSecret()` returns a `RegisterResult`, which callers
  check. Redactor storage is append-only.
- `Application` has no `getBLEManager()` accessor; `BLEManager` is composed
  explicitly in `main.cpp`.

**Tick budget (ADR-17).** Every `tick()` completes within **10 ms**; the Task
Watchdog panics if the main loop blocks for more than 5 s.

| Operation | Cost | Where it runs |
|---|---|---|
| Reading the ADC fast path from the continuous-mode DMA buffer | < 1 ms | `tick()` |
| Power calculation and FTMS notify | < 1 ms | `tick()` |
| WebSocket push to the ride page | < 1 ms | `tick()` |
| TCX generation (50–200 ms) | Deferred | `UploadQueue::fetchUpdate()` |
| HTTPS upload to Strava (2–10 s) | Deferred | `UploadQueue::fetchUpdate()` |

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
| Logic level | **3.3 V — NOT 5 V tolerant** |
| Form factor | Arduino Nano footprint (30-pin, 2.54 mm headers) |
| PlatformIO board ID | `um_nanos3` |

### 4.2 Target Deployment Environment

| Component | Detail |
|---|---|
| **Tablet** | Lenovo Tab P12 (12.7" 3K screen, Android) |
| **Floating window** | Android's freeform/split-screen PiP mode; Chrome can be resized to a small window on top of full-screen apps |
| **Bike placement** | Tablet on the bike's handlebar tablet holder |
| **ESP32 placement, Phase 0/1** | Fly-wire logger beside the bike, powered from USB-C (laptop or power bank) |
| **ESP32 placement, Phase 3+** | v1: on the passive tap board inside the console housing, powered from the bike's 9 V rail via the Waveshare VIN pin **or** from USB-C, never both, through a VIN/USB power selector (no external buck; §4.6). v2: on the active board, same power path `[DR-2]` |

The Lenovo P12 runs both the Echelon class app (full-screen) and Chrome showing
`echenon.local/ride` (split-screen or floating window) at the same time. This is
a native Android capability; no root or special app is required. The ride page
can also be installed as a PWA ("Add to Home Screen") for a standalone
full-screen experience.

### 4.3 Hardware Configuration Architecture

The hardware layer is configured per **target**: one board × bike combination.
Each target is one directory under `hardware/` holding everything that varies
between physical setups:

```
hardware/
└── ex3-waveshare-nano/          one directory per physical setup
    ├── board.h                  pin map, bike signal set (HAS_*), board identity
    ├── partitions.csv           flash partition table for this board
    └── sdkconfig.defaults       SDK overrides (PSRAM, flash, USB, NimBLE slots)
```

- **One header.** `board.h` is the single source of truth for pins and bike
  signals. It follows the framework's board contract
  (`framework/hardware/_common/board_contract.h`) and is authored from the
  framework's `framework/hardware/_TEMPLATE/`, not cloned from the reference
  `framework/hardware/esp32s3nano/board.h`, whose display and button pins
  collide with Echenon's tap pins (§12.3). No other pin or configuration
  header exists.
- **Two flag families.** `HAS_*` macros in `board.h` state that a bike signal
  exists and is wired. `WITH_*` build flags in `platformio.ini` select which
  framework and application modules compile (§12.3).
- **Build wiring.** The PlatformIO env passes
  `-DBOARD_HEADER=\"hardware/ex3-waveshare-nano/board.h\"`, the target's
  partition table and the target's `sdkconfig.defaults` (§12.2).

Firmware guards signal-dependent code with `HAS_*`:

```cpp
void EchenonModule::beginSensors() {
#if HAS_CADENCE
    _cadence.begin(PIN_CADENCE);
#endif
#if HAS_MOTOR_SENSE
    _intercept.beginSense(PIN_MOTOR_SENSE_P, PIN_MOTOR_SENSE_N);
#endif
}
```

**Adding a target** means a new directory under `hardware/` and a new
`[env:]` in `platformio.ini`. No source code changes are required.

**v1 passive board (DR-1).** The v1 passive tap board is the PR-01 detection
circuit made permanent: it keeps all ten taps on the same GPIOs with the same
tap networks (§4.4.1, §4.4.2). It therefore uses the existing
`ex3-waveshare-nano` target and `board.h` unchanged; no new target, pin or
`HAS_*` change is needed for v1 (§12.3). How the v2 active board is
expressed (a new target or a variant of this one) is `[DR-2]`.

| Target | Status | Notes |
|---|---|---|
| `ex3-waveshare-nano` | Primary | Echelon EX-3 on Waveshare ESP32-S3-Nano: PR-01 logger, PR-02 rig and the v1 passive tap board (one pin map) |
| (future targets) | — | New directory and env per setup |

### 4.4 Pin Assignments (Echelon EX-3 × Waveshare ESP32-S3-Nano)

#### 4.4.1 Phase 0/1 Map (authoritative)

This table is the single authoritative pin map for the Phase 0 logger (PR-01)
and Phase 1 calibration (PR-02). Every other document refers to it rather than
copying it. The ten tap signals were originally allocated one-to-one to GPIO 1–10.
Following WO4a/B04, GPIO 8 and 9 are dedicated to the ESP32 PCNT peripheral
(`EncoderModule`), leaving 8 ADC1 channels for continuous DMA capture
(CH0–CH6, CH9).

| GPIO | Header | Signal | Tap network |
|---|---|---|---|
| 1 | A0 | Blue: pot wiper | 22 kΩ series |
| 2 | A1 | Yellow: pot outer reference | 22 kΩ series (measured 0 V) |
| 3 | A2 | Green: pot outer reference | 22 kΩ series (measured ≈ 1.67 V) |
| 4 | A3 | Grey: cadence return | 4.7 kΩ series + 100 nF |
| 5 | D2 | Orange: cadence | 4.7 kΩ series + 100 nF |
| 6 | D3 | Red: motor + | 30 kΩ / 10 kΩ divider (÷4) |
| 7 | D4 | Black: motor − | 30 kΩ / 10 kΩ divider (÷4) |
| 8 | D5 | Black: Encoder Ch A | PCNT input (4.7 kΩ series + 100 nF) |
| 9 | D6 | Red: Encoder Ch B | PCNT input (4.7 kΩ series + 100 nF) |
| 10 | D7 | Orange: Encoder VCC | 4.7 kΩ series + 100 nF (ADC1 CH9, steady 3.3 V) |
| — | GND | Brown (bike GND) | Direct to ESP32 GND |

Rules for Phase 0/1:

- There are **ten tap signals** across the two looms. White (+9 V) and Brown
  (GND) are power, not taps; White is never connected to the logger.
- **Series resistors and capacitors**:
  - Potentiometer lines (GPIO 1, 2, 3) use **22 kΩ series resistors** (slow-moving DC signals).
  - Cadence lines (GPIO 4, 5) use **4.7 kΩ series + 100 nF ceramic capacitors** to GND on the ESP32 side (bench-verified in B01; the original 22 kΩ value exceeded the ESP32-S3 ADC continuous-mode source impedance limit for brief reed transients).
  - Encoder signals (GPIO 8, 9, 10) use wire colours **Black (Ch A)**, **Red (Ch B)**, and **Orange (VCC)** as measured in B01.
- GPIO 6 and 7 read the motor wires only through the 30 kΩ / 10 kΩ ÷4 divider,
  which is safe up to about 13 V at the wire.
- Every tap pin is configured with **no internal pull-up or pull-down** (hardware spec §5.5 no-pulls rule).
- The logger is **powered from USB-C**, with ESP32 GND tied to Brown. Use a
  power bank or a laptop running on battery, so that the USB ground does not
  form a mains ground loop through the bike's GND (hardware spec §5.3, §9).
  The MP1584EN buck is not used on any board (dropped at DR-1, §4.6).
- Capture uses the ESP-IDF **ADC continuous (DMA)** driver on 8 channels
  (CH0–CH6, CH9). GPIO 8 and 9 are assigned exclusively to PCNT hardware
  decoding (`EncoderModule`), achieving 0% bounce and 100% click detection (B04).
- GPIO 3 is an ESP32-S3 strapping pin (JTAG signal source). The PR-01 WO0
  spike confirms that the Green tap does not change boot behaviour.
- **Potentiometer Polarity (E-01 measured fact)**: B01 measured Yellow (GPIO 2)
  = 0 V and Green (GPIO 3) ≈ 1.67 V (≈ 2,084 ADC counts). This is reversed from
  the initial assumption that Yellow was HI (3.3 V) and Green was LO (0 V). Green
  acts as an active voltage source with source impedance ≈ 2.4 kΩ. The earlier
  standalone network (SW2/SW3, R4) is incorrect for this measured polarity.
  The v1 board fits no standalone network (DR-1 §2.9); its redesign (a
  pot-reference replica) is a v2 item, DR-2 §2.2/§2.9.

At DR-1 this map was also frozen as the **v1 passive tap board** map: same
GPIOs, same tap networks, same resistor values (§4.4.2).

#### 4.4.2 Phase 3+ Map — v1 FROZEN (DR-1) / v2 PROVISIONAL `[DR-2]`

> **v1 rows FROZEN at DR-1** (approved with conditions, 2026-10-12). Build the
> v1 passive tap board from them (hardware spec §6.2). **v2 rows are
> PROVISIONAL `[DR-2]`**: do not build v2 hardware from them before DR-2 closes.

**v1 passive tap board.** All ten PR-01 taps are kept, for diagnostics (DR-1
§4, condition 2.7), with the exact PR-01 tap networks. The v1 map is
therefore identical to §4.4.1.

| GPIO | Header | Signal | Tap network (exact values) | Direction | Release / board |
|---|---|---|---|---|---|
| 1 | A0 | Blue: pot wiper | 22 kΩ series | ADC1 input | v1 and v2 |
| 2 | A1 | Yellow: pot outer reference (monitor, measured 0 V) | 22 kΩ series | ADC1 input | v1; v2 `[DR-2]` |
| 3 | A2 | Green: pot reference (monitor, measured ≈ 1.67 V) | 22 kΩ series | ADC1 input | v1 (owner decision 2026-10-12); v2 `[DR-2]` |
| 4 | A3 | Grey: cadence return (monitor, measured 0 V) | 4.7 kΩ series + 100 nF to GND | ADC1 input | v1; v2 `[DR-2]` |
| 5 | D2 | Orange: cadence | 4.7 kΩ series + 100 nF to GND | ADC1 / digital input, interrupt | v1 and v2 |
| 6 | D3 | Red: console motor + sense | 30 kΩ / 10 kΩ divider (÷4) | ADC1 input | v1 and v2 |
| 7 | D4 | Black: console motor − sense | 30 kΩ / 10 kΩ divider (÷4) | ADC1 input | v1 and v2 |
| 8 | D5 | Black: encoder Ch A | 4.7 kΩ series + 100 nF to GND | Input, PCNT | v1 and v2 |
| 9 | D6 | Red: encoder Ch B | 4.7 kΩ series + 100 nF to GND | Input, PCNT | v1 and v2 |
| 10 | D7 | Orange: encoder VCC (monitor, steady 3.3 V) | 4.7 kΩ series + 100 nF to GND | ADC1 input (CH9) | v1. **v2 conflict:** the DR-2 draft uses GPIO 10 as driver IN1 `[DR-2]` |
| 17 | D8 | — | — | Unused | v1. v2 draft: encoder out A `[DR-2]` |
| 43 | D1 (TX) | **UART0 TX — reserved** (future ANT+ add-on) | — | Reserved; leave unconnected | v1 and v2 |
| 44 | D0 (RX) | **UART0 RX — reserved** (future ANT+ add-on) | — | Reserved; leave unconnected | v1 and v2 |
| — | VIN | 9 V from White via the VIN/USB power selector (VIN **or** USB, never both, §4.6) | — | Power | v1 and v2 |
| — | 3V3 | 3.3 V from the Waveshare onboard regulator | — | Power | v1 and v2 |
| — | GND | Common ground (Brown) | — | Power | v1 and v2 |

- **v1 has no outputs.** Every v1 pin is an input with no internal pull-up or
  pull-down. GPIO 17 is unused on the v1 board.
- **Exact values.** The tap values are those of the PR-01 rig and must not be
  changed on the v1 board: the B03 level fit (§5.2) is in ADC counts measured
  through the 22 kΩ wiper tap, and the cadence and encoder values are the
  bench-verified ones (B01, B04, A07-20261012).
- **Motor sense** needs no clamp diodes: the worst single-ended excursion
  measured is −0.14 V at the wire (−0.035 V at the GPIO), and the 30 kΩ leg
  limits any transient to well under 0.1 mA (A01-20261012).
- **UART0 (GPIO 43/44) is reserved on every board and pin map** for a future
  nRF51422 ANT+ module. No module may claim it. Console and logging use native
  USB-CDC.
- GPIO 6 and 7 are **always sense inputs**; they are never driver outputs.
- **v2 (draft, DR-2 §2A):** encoder out A/B GPIO 17/18, encoder bypass control
  GPIO 21, driver IN1/IN2 GPIO 10/11, motor bypass control GPIO 47, optional
  motor-cut relay GPIO 38, with reset-safe external pull-downs on the driver
  inputs (§5.5). DR-2 must resolve the GPIO 10 conflict with the v1
  encoder-VCC monitor and decide which v1 monitor taps the active board keeps.
- Encoder Ch B conditioning: Early teardown notes suggested an analog pulse of
  ~1.5–2 V on Ch B (A08). Following wire-colour correction, B04 confirmed PCNT
  decodes both channels cleanly with 0% bounce and 100% detection. A08 is
  superseded by B04; no analog conditioning is required.

### 4.5 Echelon EX-3 Internal Hardware

For complete documentation of the EX-3's sensors, looms, connectors, actuators
and PCB, see [`docs/ex3_specification.md`](docs/ex3_specification.md).

**Key facts for firmware design:**

| Component | EX-3 detail | Open question (Phase 0 register) |
|---|---|---|
| **Cadence sensor** | 2-wire magnetic reed switch at the crank; one pulse per revolution; 3.3 V logic (Orange + Grey) | Pull-up source and Grey's role (#7) |
| **Resistance potentiometer** | Rotary pot inside the lower actuator hub, driven by a continuously rotating cam, **not** by the red knob (Yellow/Blue/Green) | Wiper shape over one revolution, click-offs, extremes (#4, #5); reference stiffness (#6) |
| **Resistance motor** | DC motor inside the actuator hub, driving the magnetic brake carriage through the cam (Red + Black) | Drive voltage and waveform (#1); current (#2); run time per revolution (#3) |
| **Resistance knob** | Incremental quadrature rotary encoder on the handlebars (Red/Brown/Orange on a separate 3-pin JST) | Counts per detent and pull direction (#8) |
| **Main wiring loom** | 9-pin JST-SM carrying all signals between frame and console | — |
| **Encoder loom** | Separate 3-pin JST from the red knob to the console | — |
| **Console power-on behaviour** | Unknown | Calibration sweep, timing and reference (#13) |

### 4.6 Board Wiring Plan

Both Phase 3+ boards (v1 passive, **v2** active) sit between the bike's two JST
connectors and the Echelon console. No factory wire is cut; the connectors plug in and out to restore
stock. Pin numbers are from §4.4.2 (v1 rows frozen at DR-1; v2 rows
provisional `[DR-2]`).

**v1 passive tap board (DR-1 §2.8, frozen).** The board is the PR-01
detection circuit made permanent (DR-1 §4, condition 2.8; see
[`docs/beta-build-guide.md`](docs/beta-build-guide.md) alongside
[`docs/hardware_specification.md`](docs/hardware_specification.md) for
standalone DIY beta board construction):

- **Y-harness pass-through:** J1 → J2 (9-pin) and J3 → J4 (3-pin), every
  conductor straight through. No motor split, no J6, no encoder interception.
- **Tap network:** the PR-01 network with its exact values (§4.4.2): 22 kΩ on
  the three pot taps, 4.7 kΩ + 100 nF on the two cadence taps and the
  encoder-VCC tap, 4.7 kΩ + 100 nF on the encoder channels, 30 kΩ / 10 kΩ (÷4) on the
  two motor wires.
- **No R3, R4, R5 and no Green–GND link** (DR-1 §2.9): the board never sources
  current into a bike line.
- **Power:** VIN **or** USB, never both (below). **Manual OTA** for routine
  updates (§8.5, PR-04a; for **`v0.3.0-beta.1`**, `WITH_OTA` is intentionally
  disabled in `platformio.ini` and beta updates are performed over **USB-C**
  with the mechanical `SW0` `VIN`/`USB-C` interlock set to USB). UART0 pads left free.

**Connectors:**

| Connector | Type | Side | Purpose |
|---|---|---|---|
| J1 | JST-SM 9-pin socket | Bike (input) | Receives the main loom from the frame |
| J2 | JST-SM 9-pin plug | Console (output) | Passes signals through to the console |
| J3 | JST 3-pin socket | Knob (input) | Receives the rotary encoder from the red knob |
| J4 | JST 3-pin plug | Console (output) | Passes encoder signals through to the console |
| J5 | Pin header (30-pin) | — | Waveshare ESP32-S3-Nano carrier socket |
| J6 | Screw terminals | Motor output (**v2** only) | Motor driver output → bike motor (Red + Black) |

**J1 → J2 (9-pin) with ESP32 taps:**

| J1 pin | Wire | → J2 (console) | → ESP32 (v1 and v2) | → motor driver (MITM only, `[DR-2]`) | Standalone support (MITM, Override, `[DR-2]`) |
|---|---|---|---|---|---|
| 1 | Red (Motor +) | Passive: pass-through. MITM: to sense divider only | GPIO 6 via ÷4 divider (30 kΩ / 10 kΩ) | Bike side ← driver output A | — |
| 2 | Black (Motor −) | Passive: pass-through. MITM: to sense divider only | GPIO 7 via ÷4 divider (30 kΩ / 10 kΩ) | Bike side ← driver output B | — |
| 3 | Yellow (Pot outer ref, 0 V) | Pass-through | GPIO 2 via 22 kΩ (monitor) | — | Pot-reference replica `[DR-2]` |
| 4 | Blue (Pot wiper) | Pass-through | GPIO 1 via 22 kΩ | — | — |
| 5 | Green (Pot outer ref, 1.67 V) | Pass-through | GPIO 3 via 22 kΩ (monitor) | — | Pot-reference replica `[DR-2]` |
| 6 | Orange (Cadence) | Pass-through | GPIO 5 via 4.7 kΩ + 100 nF | — | R3 pull-up `[DR-2]` |
| 7 | Grey (Cadence return) | Pass-through | GPIO 4 via 4.7 kΩ + 100 nF (monitor) | — | Grey is the console's 0 V return (register #7); standalone handling `[DR-2]` |
| 8 | White (+9 V) | Pass-through | VIN via the VIN/USB power selector | — | — |
| 9 | Brown (GND) | Pass-through | Common GND | GND | — |

**J3 → J4 (3-pin encoder) with ESP32 taps:**

| J3 pin | Wire | → J4 (console) | → ESP32 (v1 and v2) | Standalone support (MITM, Override, `[DR-2]`) |
|---|---|---|---|---|
| 1 | Black (Ch A) | Pass-through | GPIO 8 via 4.7 kΩ + 100 nF (PCNT) | Pull direction from register #8 `[DR-2]` |
| 2 | Orange (VCC 3.3 V) | Pass-through | GPIO 10 via 4.7 kΩ + 100 nF (ADC1 CH9, monitor) | R5 encoder supply `[DR-2]` |
| 3 | Red (Ch B) | Pass-through | GPIO 9 via 4.7 kΩ + 100 nF (PCNT) | Pull direction from register #8 `[DR-2]` |

**Switchable standalone supports (v2).** R3 (cadence pull-up), R4 (pot reference),
R5 (encoder supply) and the Green–GND link exist only so that the sensors work
with the console disconnected (UC6). Each sits behind its own switch: SW1
(R3), SW2 (R4), SW3 (Green–GND) and SW4 (R5), as in hardware spec §7.5. The
switches are jumpers at minimum, a high-side or analog switch preferred. All
four are **open by default** and whenever the console is connected, so the
board never back-feeds the console's rails. Their values, the switch type and
whether each support is needed at all are DR-2 outputs (DR-2 §2.2 / §2.9,
including the pot-reference replica). The v1 passive tap board fits none of
them (DR-1 §2.9).

**Power (Phase 3+ boards, DR-1 §2.3 and §4 condition 2.3):**

```
9 V DC (White, J1 pin 8)            USB-C (VBUS)
    │                                    │
    └──► VIN/USB power selector ◄────────┘   VIN position: 9 V connected
              │                               USB position: 9 V disconnected
              └──► [diode, only if WO1 finds no isolation]
                        │
                        └──► Waveshare VIN (6–21 V, register #11) ──► onboard MP2322 ──► 3V3
                                  │
                                  ├──► ESP32-S3 (v1 and v2)
                                  ├──► Motor driver (v2 only, supply [DR-2])
                                  └──► Switchable standalone supports (v2 only, [DR-2])
```

- **Requirement: the board is powered from 9 V (VIN) or from USB-C, never
  both at once.** The external MP1584EN buck is **removed** from v1; the ESP32
  runs from the Waveshare onboard regulator.
- **Selector.** In the USB position the 9 V line is disconnected from VIN. The
  selector's part and wiring are designed in **PR-04a WO1**.
- **Verification (PR-04a WO1, before any soldering).** WO1 reads the
  Waveshare ESP32-S3-Nano VBUS/VIN power path from the board schematic and
  records whether the board isolates VIN from VBUS (no back-feed from VIN into
  the USB host, none from VBUS into the 9 V line). If it does not, a diode is
  added on the VIN path. The schematic finding and the selector design are
  committed as evidence before the board is soldered; the behaviour in both
  selector positions is then measured on the built board (§15.6).
- Until the selector is built and verified, the rule on any VIN-powered rig
  is: disconnect 9 V before connecting USB.

### 4.7 Voltage Safety

> **ESP32-S3 GPIO and ADC pins accept at most 3.3 V (absolute maximum 3.6 V).
> Exceeding this causes permanent damage.**

- The cadence, potentiometer and encoder signals operate at 3.3 V logic (see
  [`docs/ex3_specification.md` §10](docs/ex3_specification.md#10-voltage-summary)).
  They reach the ESP32 only through series resistors (§4.4).
- The motor wires may carry more than 3.3 V; their voltage and waveform are
  register row #1. They are read only as analog signals on ADC1 through the
  ÷4 divider, which keeps the pin within range up to about 13 V. They are
  never connected to a digital input directly. The divider needs no clamp
  diodes: the worst measured excursion is −0.14 V at the wire (A01-20261012).
- The 9 V rail reaches only the VIN pin (through the VIN/USB selector) and never
  touches a GPIO. VIN and USB are never connected at once (§4.6).
- When the ESP32 is unpowered while the console is powered, the series
  resistors limit back-powering current through the protection diodes. This
  is why Phase 0/1 uses USB power and why every direct tap has a series
  resistor.

### 4.8 Parts List

| Item | Qty | Phase | Status | Cost |
|---|---|---|---|---|
| Waveshare ESP32-S3-Nano | 1 | 0+ | Owned | — |
| 22 kΩ resistors (pot taps, GPIO 1–3; exact value, §4.4.2) | 3 | 0+ | To purchase | £0.15 |
| 4.7 kΩ resistors (cadence taps GPIO 4/5, encoder taps GPIO 8/9/10) | 5 | 0+ | To purchase | £0.25 |
| 100 nF ceramic capacitors (GPIO 4, 5, 8, 9 and 10 tap filters, ESP32 side of the series resistor) | 5 | 0+ | To purchase | £0.30 |
| 30 kΩ + 10 kΩ resistor pairs (÷4 motor-sense dividers) | 2 | 0+ | To purchase | £0.20 |
| JST-SM 9-pin socket + plug pair (Y / back-probe harness, then board) | 2 | 0+ | To purchase | £6 |
| JST 3-pin socket + plug pair (Y / back-probe harness, then board) | 2 | 0+ | To purchase | £2 |
| ~~GTIWUNG MP1584EN buck converter~~ | — | — | Removed from v1 (DR-1 §2.3: ESP32 on VIN) | — |
| VIN/USB power selector (9 V disconnected in the USB position; part designed in PR-04a WO1) | 1 | 3+ (v1) | To purchase | £1 |
| Diode on the VIN path (only if PR-04a WO1 finds the Waveshare does not isolate VIN from VBUS) | 0–1 | 3+ (v1) | Conditional | £0.20 |
| Perfboard / protoboard (70 × 50 mm) | 1 | 3+ | To purchase | £2 |
| 30-pin female header strips (ESP32 carrier) | 2 | 3+ | To purchase | £1 |
| Hookup wire (22 AWG, assorted colours) | — | 0+ | To purchase | £3 |
| **v2:** L9110S Dual-Channel H-Bridge Motor Driver Module (DR-2 draft: DRV8833-class H-bridge) | 1 | 5+ | Owned | — |
| **v2:** R6, R7: 10 kΩ resistors (driver input reset pull-downs) | 2 | 5+ | To purchase | £0.10 |
| **v2:** standalone network: pot-reference replica (≈ 4.7 kΩ / 4.7 kΩ), cadence pull-up, encoder supply | 3 | 5+ | Values `[DR-2]` | — |
| **v2:** switches for the standalone network (open by default) | 4 | 5+ | Type `[DR-2]` | — |
| **v2:** Screw terminal block (2-pin, motor output) | 1 | 5+ | To purchase | £0.50 |

**Bench equipment for Phase 0** (the PR-01 bench-ready entry condition):

- an oscilloscope with at least 2 channels and single-shot triggering;
- a way to measure motor current (a shunt with the scope, or a DC clamp);
- JST-SM 9-pin and JST 3-pin pairs made into a Y / back-probe harness, instead
  of alligator clips on JST contacts;
- the series resistors and dividers listed above.

The bench list, including example shunt and harness details, is hardware spec
§2.1; the full per-phase BOM with component designators is hardware spec §2
([`docs/hardware_specification.md`](docs/hardware_specification.md)).

---

## 5. Sensor Acquisition

### 5.1 Cadence Sensor

One active-low pulse per pedal revolution. Cadence comes from the inter-pulse
interval.

- GPIO 5 (D2): `INPUT` with no internal pull, hardware interrupt on `CHANGE`
  with leading-edge (`LOW`) timestamp commit (refined in PR-05 `WO-05.4` over
  single-edge `FALLING` to enforce open-release hysteresis)
- Two-layer debounce & break-bounce rejection (refined in PR-05 `WO-05.4`):
  - **Layer 1 (Hardware Glitch Filter):** ESP32-S3 IO-MUX pin glitch filter
    (`gpio_new_pin_glitch_filter` on `GPIO 5`) rejects sub-microsecond EMI spikes
  - **Layer 2 (Leading-Edge Commit + Open-Release Hysteresis):** Minimum inter-pulse
    period **`250 ms` (`MIN_HW_INTERVAL_US = 250,000 us`, 240 RPM ceiling)** plus
    mandatory **`50 ms` (`REARM_HIGH_US = 50,000 us`) continuous-HIGH release
    hysteresis** before re-arming for the next falling edge, eliminating mechanical
    reed-switch break-bounce (~80–120 ms after magnet passage)
- RPM: $\text{RPM} = 60{,}000{,}000 \div \Delta t_{\mu s}$
- Timeout: 3 s with no pulse → cadence = 0 (tracked via wraparound-safe signed
  `int32_t` `millis()` deltas and latched in `timedOut_` so 32-bit `micros()`
  rollover at 71.58 min cannot resurrect stale pulses)
- Hardware filter (v1 board, as on the PR-01 rig): 4.7 kΩ series + 100 nF from
  GPIO 5 to GND on the ESP32 side (a ~0.47 ms RC low-pass) that the console's
  line does not see. The tap does not distort the console's edge (fall time
  < 1 ms with and without the tap; high level 3.1 → 2.8 V; A07-20261012).
- Grey (cadence return) is monitored on GPIO 4 in v1 (measured 0 V).
- Pull-up: the console provides it when connected. **v2:** with the console
  disconnected (MITM board, Override), the switchable R3 provides it (§4.6).

### 5.2 Resistance Position Sensor

The actuator is a **continuously rotating cam**, not a linear slider. The pot
wiper tracks the cam and sweeps its range non-monotonically over a full
revolution, so the wiper voltage alone does not identify a resistance outside
the console's working range.

- **Measurement**: GPIO 1 (A0), ADC1 channel 0, read through the ADC
  continuous (DMA) driver, via the 22 kΩ tap.
- **Wiper profile over a revolution** (register #4, #5; A03, A04, B03): two
  symmetrical working ramps spanning 0–2120 counts, separated by two dead zones
  (a gap crossing of ~5°) and two non-working plateaus. The console works on
  one ramp from **165 counts (level 1) to 1327 counts (level 32)** (or
  **171.0–1333.0 counts** on the Phase 3 C07 320-point sweep), about 151°
  of shaft travel, with hysteresis < 15 counts.
- **v1 fundamental-first model (DR-1 §2.5/§2.11, refined in PR-02 & PR-05 `WO-05.4`):**
  the wiper is linear in cam rotation over the working range. Phase 0 fit:
  `wiper = 39.1 × level + 97` over 165–1327 counts (R² = 0.9989, B03). Phase 3
  automated sweep refinement (C03/C07-20261024): `WORKING_RANGE_MIN = 171`,
  `WORKING_RANGE_MAX = 1333` (`wiper = 37.48 × (level - 1) + 171.0`,
  R² = 0.9998). Rather than passing through a discrete 32-entry lookup table
  in the forward path, `ResistanceSensor::getFilteredWiperFloat()` exposes the
  unrounded EMA float wiper $w$, which maps directly to the normalized cam
  coordinate $\tilde{p} = \text{clamp}\!\big((w - \text{pot}_{\min}) / (\text{pot}_{\max} - \text{pot}_{\min}),\ 0,\ 1\big)$.
  v1 needs no cam-angle tracking. §5.3 maps $\tilde{p}$ to all external scales.
- **Display path** (v1 and v2): EMA at 20 Hz with α = 0.15
  ($\text{filtered} = 0.15 \times \text{sample} + 0.85 \times \text{filtered}$),
  retained in floating point (`getFilteredWiperFloat()`) for $\tilde{p}$, power,
  internal scale, display level, and runaway detection.
- **v2 — fast path**: unfiltered samples at `[DR-2: rate, target ≥ 200 Hz]`,
  used by the motor controller and by click-off detection.
- **v2 — click-off detection**: a discontinuity in the fast-path stream marks
  a half-revolution boundary. The detection threshold is `[DR-2]`.
- **v2 — cam angle**: `ResistanceSensor` combines the wiper position within the
  current sweep with a half-revolution counter. The counter changes on
  click-off events, in the direction implied by the motor drive (Intercept:
  the sensed console drive; Override: Echenon's own drive command).
- **Hardware filter**: none on the wiper in v1 (the PR-01 tap network has no
  wiper capacitor). A wiper filter on the v2 board is `[DR-2]`.

**Console motor sense (v1: monitoring only, DR-1 §2.1).** GPIO 6/7 read the
console's motor wires through the 30 kΩ / 10 kΩ (÷4) dividers, sampled as ADC1
analog at ≥ 1 kHz. The drive is flat bidirectional DC (±3.25 V differential,
no PWM, register #1); the drive direction is whichever wire is high. Echenon
uses it only to publish motor-direction and seek events (and to corroborate
runaway detection); it never acts on the motor in v1.

**Stock-runaway detection (v1, PR-02; known limitation).** A fast knob spin
past level 32 can leave the stock console's motor turning continuously
(A09-20261012 #3). A passive board **cannot prevent** this. v1 detects it: when
the display-path wiper stays **outside the working range (171–1333 counts with
20-count tolerance, i.e., `< 151` or `> 1353`) for more than
10 s** (`runaway_timeout_s` default = 10 s), Echenon publishes a
`runaway.detected` event on the EventBus and raises
a portal alert (PR-14 persists and surfaces it). The motor-sense taps showing
continuous drive corroborate the event. Prevention is a v2 item (DR-2 §2.4).

### 5.3 Resistance Level Mapping

**Fundamental-first hierarchy & canonical internal scale (owner decisions 2026-10-04 & 2026-10-26, DR-1 §2.11, PR-05 `WO-05.4`).**
The primary physical state of the resistance mechanism is the continuous
normalized cam coordinate $\tilde{p} \in [0.0, 1.0]$ derived directly from the
unrounded float wiper $w$ (`ResistanceSensor::getFilteredWiperFloat()`):

$$\tilde{p} = \text{clamp}\!\left(\frac{w - \text{pot}_{\min}}{\text{pot}_{\max} - \text{pot}_{\min}},\; 0.0,\; 1.0\right)$$

where $(\text{pot}_{\min}, \text{pot}_{\max})$ default to the Phase 3 C07 bounds
$[171.0, 1333.0]$ (`LevelMapping::WORKING_RANGE_MIN = 171`,
`WORKING_RANGE_MAX = 1333`) and are refined by `CalibrationWizard::fitLinearEndpoints(potMin, potMax)`
via Ordinary Least Squares (OLS) linear regression $\text{pot}(\ell) = a + b \cdot (\ell - 1)$
across all recorded calibration steps ($N \ge 2$). Fitting the linear cam
endpoints via OLS rather than interpolating piecewise through 32 individual
level entries prevents single-level bang-bang motor settling noise ($\pm 2$ ADC
counts) from injecting kinks into the continuous $\tilde{p}$ and power surface.

In compact 8-bit storage (`uint8_t`), resistance is represented on the canonical
internal scale by `i ∈ 0…254` ($i = \text{round}(254.0 \times \tilde{p})$), with
`255` (`LevelMapping::UNCALIBRATED`) reserved as the uncalibrated sentinel. Every
discretized external scale is a deterministic projection of $\tilde{p}$ (or `i`):

**Mappings (DR-1 §4, condition 2.11, finalized in PR-05):**

| Consumer | Scale | Mapping |
|---|---|---|
| **Echelon** (console, app, portal) | Continuous $\ell \in [1.0, 32.0]$ / Integer `L ∈ 1…32` | $\ell = 1.0 + 31.0\,\tilde{p}$, $L = \text{round}(\ell)$ (from internal `i`: $L = \text{round}(i \times 31 / 254) + 1$); exact endpoints `0 ↔ 1` and `254 ↔ 32` |
| **Peloton** | Resistance `R ∈ 0…100` | **By braking-curve power equivalence at reference cadence $\text{cad}_{\text{ref}} = 80\text{ RPM}$:** $R(\tilde{p}) = \text{clamp}\!\left(\text{round}\!\left(\left[\frac{\max(P(\tilde{p},\, 80) / 106.6667 - 0.1,\; 0)}{0.000185}\right]^{1/2.7}\right), 0, 100\right)$ (`PowerEngine::computePelotonResistanceFromNormalized`). Monotonic 1-to-1 with knob position (`L1 → 13`, `L32 → 42`) and valid at `0 RPM` (**delivered in PR-05**) |
| **FTMS** (Indoor Bike Data `0x2AD2`) | Resistance-level field (`sint16`) | Carries `L` (`1…32`); encoding in §7.5. v1 reports only and accepts **no targets** (PR-05) |
| **Garmin, Zwift, Strava** | Power, cadence, virtual speed, HR | Power (§6), cadence (§5.1), virtual speed $v(P)$ (§7.5) and HR (§8) over BLE (FTMS Indoor Bike Data, HR service) and in the TCX file (§9.4) |
| **Internal** (`RideStore`, logs, state) | `i ∈ 0…254` (`255` = uncalibrated) | $i = \text{round}(254.0 \times \tilde{p})$; inverse $\tilde{p} = i / 254.0$ preserves $8\times$ finer resolution than discrete `1…32` |

- **v2 — Override Mode.** The display scale has `[DR-2: N_override]` levels
  spanning the working range. A 100-level scale is a goal validated in PR-07
  and PR-08 against the achievable position error; it is not a specification
  fact. In v2, `i` is derived from cam angle θ between θ<sub>min</sub> and
  θ<sub>max</sub> `[DR-2]`.

### 5.4 Resistance Encoder

The red resistance knob is an incremental quadrature encoder on a separate
3-pin JST loom (wire colours: Black = Ch A, Red = Ch B, Orange = VCC).

- **Phase 0/1**: Ch A and Ch B were initially captured as analog on GPIO 8 and 9
  by the `SignalLogger`, together with encoder VCC on GPIO 10. Following WO4a/B04,
  GPIO 8 and 9 were released from ADC to the ESP-IDF PCNT hardware peripheral
  (`EncoderModule`), achieving 0% bounce and 100% detection without analog
  conditioning.
- **v1 and v2**: the framework `EncoderModule` (`WITH_ENCODER`)
  decodes the knob in the PCNT hardware peripheral with its glitch filter.
  `board.h` supplies a `BoardEncoder` descriptor `{ id, pinA, pinB,
  stepsPerDetent, reversed, accel }` with pins 8 and 9, `stepsPerDetent = 4`
  (confirmed in B04) and `reversed = false`. There are **4 detents per level
  (16 PCNT steps per level)** on manual knob turns (B03, confirmed by
  A08-20261012), giving 512 encoder steps over the full range (32 levels); in
  synthesized quadrature (`CalibrationRig`, C01-20261024), **8 quadrature edges
  (2 full Gray-code cycles) = 1 console level** with 100% reliability at 150 ms
  per edge. The console's own response
  is less regular going down on manual turns (6–10 detents per level observed, A08-20261012),
  so the level always comes from the wiper (§5.3), never from counting clicks.
  `EncoderModule` publishes `EncoderEvent` payloads on the EventBus topic
  `encoder.rotate`.
- **Encoder VCC** is monitored on GPIO 10 in v1 (steady 3.3 V, 4095 counts).
- PR-02 verified that the PCNT pin configuration enables no internal pulls on
  GPIO 8 and 9 (`EncoderModule` leaves internal pulls disabled); PR-07 re-verifies on the v2 board.

**In v1** (and in v2 Intercept Mode before PR-10 adds encoder emulation) the
encoder signals pass through to the console, which drives the motor. Echenon
records knob events for telemetry and monitoring only.

**v2 — in Override Mode** each knob event moves the target by one display level
and cancels or suspends any active ERG target (§5.6).

### 5.5 Resistance Motor Control

> **v2.** Not part of v1. Driver part and pins are `[DR-2]`.

Applies to the v2 active board only (PR-07). The L9110S H-bridge drives the
resistance motor from two GPIO outputs.

**L9110S module (datasheet values):**

| Parameter | Value |
|---|---|
| **Model** | L9110S dual-channel H-bridge |
| **Supply voltage** | 2.5–12 V (motor rail `[DR-2]`, register #1 and #11) |
| **Continuous current** | 800 mA per channel |
| **Peak current** | 1.5–2.0 A per channel |
| **Logic compatibility** | Directly compatible with 3.3 V GPIO |
| **Protection** | Internal flyback clamp diodes per output |

**Control truth table** (provisional pins, §4.4.2):

| GPIO 10 (A-IA) | GPIO 17 (A-IB) | Motor action |
|---|---|---|
| HIGH | LOW | Forward (direction mapped to cam angle at PR-07) |
| LOW | HIGH | Reverse |
| LOW | LOW | Stop |

**Position controller:**

- **Angle tracking.** The controller works on cam angle (0–360°) across the
  full revolution, from `ResistanceSensor` (§5.2).
- **Working range.** Targets lie in the half-revolution from cam-min to
  cam-max. The controller never drives the cam across either extreme.
- **Approach direction.** The controller chooses the direction by angle and
  applies a brake-and-approach profile (reduced PWM duty near the target).
  The profile values are `[DR-2]`.
- **Settle and retry.** Hysteresis and a settle limit prevent hunting. A
  command that is still unsatisfied after about one full revolution of drive
  stops the motor, marks the angle invalid and triggers re-homing.
- **Run-time bound.** Any single move is bounded to `[DR-2: register #3, ~16 s per half-revolution measured]`;
  exceeding it stops the motor and raises a fault.
- **Reset-safe driver inputs.** External 10 kΩ pull-downs on A-IA and A-IB
  hold the motor stopped while the ESP32 is in reset or booting.
- **Pot plausibility.** A stuck, open-circuit or out-of-range wiper stops the
  motor and raises a fault.
- **IRAM safety.** Motor stop decisions are IRAM-safe, so an SPI-flash erase
  stall cannot leave the motor running.
- **Tuning values.** Loop rate, dead band and PWM profile are `[DR-2]`.

**Motor-driven calibration (PR-07).** A full-revolution sweep locates the
click-offs and the resistance extremes and records θ<sub>min</sub> and
θ<sub>max</sub> (§6.3).

### 5.6 Operating Modes

> **v2.** Not part of v1: the v1 passive board has no operating mode. In v2,
> Intercept Mode corresponds to mode 1 (relay/active, with encoder emulation,
> PR-10) and Override Mode with the console absent to mode 2 (console off,
> PR-12); the detail is `[DR-2]`.

The `InterceptController` manages the operating mode on the MITM board. The
mode is a persisted software setting, not a physical switch. The passive tap
board has no operating mode; it is permanently observe-only.

#### Intercept Mode (Echelon Console Connected)

All signals except the motor pair pass through to the console, and Echenon
reads them.

| Signal | ESP32 action | Console | Motor path |
|---|---|---|---|
| Cadence | Tap and read (GPIO 5) | Receives signal normally | — |
| Potentiometer | Tap and read (GPIO 1, ADC1) | Receives signal normally | — |
| Encoder | Tap and read (GPIO 8/9) | Receives signal normally | — |
| Motor | Sense the console's drive on GPIO 6/7 (analog), reproduce it on GPIO 10/17 | Drives its motor outputs into the sense divider | Console → ESP32 relay → L9110S → motor |

- **FTMS in Intercept Mode.** Indoor Bike Data is broadcast normally.
  Control Point target opcodes are refused with `0x05` (Control Not
  Permitted), and the target-setting feature bits are cleared (§7.3, §7.4).
- **Multi-client BLE.** A typical session has the Echelon tablet connected to
  the factory PCB (bike metrics) and to `HRMRepeater` (HR), and one fitness
  app connected to `FTMSServer` (read-only telemetry).
- **Relay fidelity.** The relay reproduces direction and, if the console uses
  PWM, the duty cycle; register row #1 decides which is needed.
- **Fail-safe.** At power-up, in reset and on any firmware fault the motor
  path defaults to **console pass-through**, so the console always keeps
  control if Echenon is not running. The recommended mechanism is a
  normally-closed relay or analog-switch bypass across the split (audit H-05
  option a). The choice is `[DR-2: decision H-05, DR-2 §2.4]`, informed by register row
  #13 (console power-on behaviour).

#### Override Mode (Echelon Console Disconnected or Ignored)

The ESP32 controls resistance directly. The console's motor outputs reach only
the sense divider and are ignored.

| Signal | ESP32 action | Motor path |
|---|---|---|
| Cadence | Read directly (GPIO 5) | — |
| Potentiometer | Read directly (GPIO 1) as position feedback | — |
| Encoder | `encoder.rotate` events as manual input | Knob → target angle → controller → L9110S → motor |
| ERG target | Control Point target → `ERGController` → target angle | Target angle → controller → L9110S → motor |
| Web command | `POST /api/resistance` (authenticated) → target angle | Target angle → controller → L9110S → motor |

**Console-connected Override.** The encoder and pot signals still pass
through to the console. If the console is powered it sees knob clicks and
tries to drive the motor, but its drive reaches only the sense divider. No
hardware change is needed to block the console; the split achieves it. The
Echelon app reads the pot for its resistance display and never commands a
resistance. Override must not desynchronise the console: after Override
ends, the console's view of the cam position must still be valid (register
rows #9 and #13).

**Standalone start-up.** Echenon stores the last known half-revolution in
persisted configuration. At boot it verifies the stored half with a small
move. It never runs a full homing sweep while there is cadence.

**Command priority (highest first):**

1. **Knob** — always wins; suspends ERG (below). The rider always has
   physical control.
2. **FTMS Control Point** — target from Zwift, Xert or TrainerRoad.
3. **Ride page** — `POST /api/resistance`, authenticated, Override Mode
   only; returns `409` in Intercept Mode.

**ERG state machine (PR-08):**

| State | Entered when | Behaviour |
|---|---|---|
| `RELEASED` | Boot; Reset (`0x01`); control lost; no target set | No automatic resistance. Knob and web commands act directly. |
| `ACTIVE` | A target power or resistance is written by the controlling client | `ERGController` computes and tracks the target cam angle |
| `SUSPENDED_BY_RIDER` | The knob turns while `ACTIVE` | The knob sets resistance. Target writes are **stored, not applied**. |

- `SUSPENDED_BY_RIDER` latches until the client sends a new Request Control
  (`0x00`) or Reset (`0x01`), or until `[DR-2/PR-08: hold time]` expires.
  On leaving the latch by Request Control or timeout, the most recent stored
  target becomes active.
- **Fitness Machine Status** (`0x2ADA`) notifications: `0x08` Target Power
  Changed and `0x07` Target Resistance Level Changed on every applied target
  change, and `0xFF` Control Permission Lost when ERG moves to `RELEASED`
  because control was lost. PR-08 confirms the opcode values against FTMS
  v1.0 before implementation.
- **Cadence handling**: ERG uses smoothed cadence
  (`[DR-2/PR-08: smoothing constant]`), a setpoint deadband
  (`[DR-2/PR-08: watts]`) and a rate limit on target-angle changes
  (`[DR-2/PR-08: degrees per second]`), so cadence noise does not make the
  motor hunt.

```
FTMS Control Point → target_watts
                          │
                          ├── smoothed cadence (CadenceSensor)
                          ▼
                   Power model inverse (§6)
                   target_watts + cadence → target θ
                          │  deadband + rate limit
                          ▼
                   ResistanceMotor position controller
                          │
                          ▼
                   L9110S → motor → pot feedback → loop
```

Within the physical limits of the brake, ERG raises resistance when cadence
drops and lowers it when cadence rises, holding the requested power.

---

## 6. Power Calculation Engine

### 6.1 Physical Basis

Magnetic eddy-current brake: a linear multipole array of neodymium (NdFeB)
permanent magnets mounted on a pivoted calliper beside the outer aluminium rim
of the perimeter-weighted flywheel, driven by a $12\text{ V}$ DC gearmotor
through a $\Delta\theta_{\max} = 151^\circ$ ($2.635447\text{ rad}$) eccentric
cam coupled to a $10\text{ k}\Omega$ rotary potentiometer wiper.

As derived and verified against the 320-point Phase 3 empirical sweep in
[`docs/power-physics-model.md`](docs/power-physics-model.md) and
[`docs/measurements/phase3/C07-20261024-power-regression-analysis.md`](docs/measurements/phase3/C07-20261024-power-regression-analysis.md),
the physical braking power is governed by four coupled electromechanical
stages:

1. **Eccentric Cam Kinematics ($\tilde{p} \to \theta \to u$):** Level 1 is
   mechanically parked at Top Dead Center ($\theta_0 = 0^\circ$). Across
   normalized wiper travel $\tilde{p} \in [0, 1]$ (where
   $\tilde{p} = \frac{\ell - 1}{31}$ from calibrated continuous level
   $\ell \in [1.0, 32.0]$, or $\frac{\text{pot\_adc} - \text{pot}_{\min}}{\text{pot}_{\max} - \text{pot}_{\min}}$),
   the cam rotates through $\theta(\tilde{p}) = 2.635447\,\tilde{p}\text{ rad}$
   and advances the magnet calliper toward the flywheel rim by
   $u(\tilde{p}) = 1 - \cos(2.635447\,\tilde{p}) \in [0, 1.87462]$.
2. **Exponential Air-Gap Magnetic Flux Squared ($B^2(u)$):** Because magnetic
   flux density across air gap $d = d_0 - e\cdot u(\tilde{p})$ decays
   exponentially ($B(d) = B_0 e^{-\kappa d}$), the Lorentz eddy-current braking
   factor $B^2$ grows as $\exp(\beta\,u(\tilde{p}))$ with fitted exponent
   $\beta = 1.630327 \pm 0.013016$ (a $21.25\times$ magnetic braking force
   amplification from Level 1 to Level 32).
3. **Kloss Eddy-Current Inductive Rollover ($\tau_{\text{eddy}}(\omega)$):**
   The spinning aluminium flywheel rim has finite sheet resistance and
   eddy-current loop inductance, obeying the classical Kloss induction-brake
   torque equation $\tau_{\text{eddy}} \propto \frac{\text{cad}}{1 + (\text{cad}/\text{cad}_c)^2}$
   with critical skin-effect cadence $\text{cad}_c = 128.1495 \pm 1.2194\text{ RPM}$.
4. **Drivetrain Coulomb Friction ($c_{\text{fric}}\cdot\text{cad}$):** Constant
   mechanical drag from the poly-V belt and bearings adds linear power
   $c_{\text{fric}}\cdot\text{cad}$ with $c_{\text{fric}} = 0.114260 \pm 0.013530\text{ W/RPM}$
   ($1.091\text{ N}\cdot\text{m}$ crank friction torque).

### 6.2 Power Model (Owner Decision 2026-10-24, superseding DR-1 §2.6)

In Default mode (Tier 1), `PowerEngine` computes mechanical power directly from
the **4-parameter first-principles electromechanical formula (`Physics P3r`)**:

$$P(\tilde{p}, \text{cad}) = A \cdot \exp\!\Big(\beta \cdot \big(1 - \cos(2.635447 \cdot \tilde{p})\big)\Big) \cdot \frac{\text{cad}^2}{1 + (\text{cad} / \text{cad}_c)^2} + c_{\text{fric}} \cdot \text{cad}$$

with production constants:
- $A = 0.0048839\text{ W/RPM}^2$
- $\beta = 1.630327$
- $\text{cad}_c = 128.1495\text{ RPM}$
- $c_{\text{fric}} = 0.114260\text{ W/RPM}$
- $\Delta\theta_{\max} = 2.635447\text{ rad}$ ($151^\circ$)

Runtime evaluation rules:
- When $\text{cad} \le 0\text{ RPM}$, $P(\tilde{p}, \text{cad}) = 0\text{ W}$ identically.
- The normalized position input $\tilde{p} = \text{clamp}\!\left(\frac{w - \text{pot}_{\min}}{\text{pot}_{\max} - \text{pot}_{\min}}, 0.0, 1.0\right)$
  is computed directly from the unrounded float wiper fundamental
  $w = \text{ResistanceSensor::getFilteredWiperFloat}()$ and the OLS-fitted
  linear cam endpoints $[\text{pot}_{\min}, \text{pot}_{\max}]$
  (`CalibrationWizard::fitLinearEndpoints`, defaulting to C07 bounds
  $[171.0, 1333.0]$ before passive calibration), and passed to
  `PowerEngine::tickFromNormalized(pTilde, cadenceRpm)`. This provides smooth
  sub-count continuous resolution and eliminates both potentiometer end-stop
  offset across bikes and single-level calibration table kinks.
- Power is clamped to $[0, 2000]\text{ W}$.
- When Tier 3 (`RECALIBRATED`) is active (`RecalibrationUI`), Echenon fits the
  physical eddy-current gain $A$ (`params.a_scale`) via least-squares regression
  (`RecalibrationUI::applyToPhysicsParams`, synced via
  `EchenonModule::syncPhysicsCalibration()`) across the recorded reference points,
  preserving the smooth continuous `Physics P3r` curve while also keeping the
  **32 levels × 31 cadence bins** ($0\text{–}150\text{ RPM}$ in $5\text{ RPM}$
  steps) table updated for REST `/api/power-table` inspection and Tier 2 custom
  CSV/JSON overrides.
- **v2 (PR-08 ERG Mode):** Because `Physics P3r` is strictly monotonic and
  analytically invertible in closed form via $\ln$ and $\arccos$
  ([`docs/power-physics-model.md`](docs/power-physics-model.md) §5.5), the ERG
  controller can invert $(P_{\text{target}}, \text{cad}) \to \tilde{p}_{\text{target}}$
  directly without table search.

**Accuracy statement (owner decision 2026-10-24).** Across the 317 clean points
of the Phase 3 automated 32-level × 10-cadence sweep (`C07`), the 4-parameter
physical model `Physics P3r` achieves **$R^2 = 0.999522$** ($\text{RMSE} = 3.77\text{ W}$,
$\text{MAE} = 2.71\text{ W}$) from raw `pot_adc`, and **$R^2 = 0.999627$**
($\text{RMSE} = 3.33\text{ W}$, $\text{MAE} = 2.41\text{ W}$) on calibrated
level position $\tilde{p}$—well inside the PR-02 $\pm 15\%$ validation gate.

### 6.3 Calibration

#### Position Calibration (Required)

Calibration is split between a passive step and a motor-driven step.

- **Passive calibration (PR-02 & refined in PR-05 `WO-05.4`, all installs).** The
  `CalibrationWizard` guides the rider to step the console through its levels
  (or record any $\ge 2$ distinct levels), pausing at each. The firmware records
  the display-path wiper counts and the console level at each step in
  `cal_level_table`. At runtime (`CalibrationWizard::fitLinearEndpoints`), the
  firmware fits a straight line $\text{pot}(\ell) = a + b \cdot (\ell - 1)$ across
  all recorded steps via Ordinary Least Squares (OLS) linear regression to
  determine the bike's physical cam endpoints $\text{pot}_{\min} = a$ ($\ell = 1$)
  and $\text{pot}_{\max} = a + 31\,b$ ($\ell = 32$), normalizing
  $\tilde{p} = \text{clamp}\!\big((w - \text{pot}_{\min}) / (\text{pot}_{\max} - \text{pot}_{\min}), 0, 1\big)$
  for the physical power formula (§6.2) and all downstream scale projections (§5.3).
- **v2 — Motor-driven calibration (PR-07, active board).** A full-revolution sweep
  finds the click-offs and the cam extremes and records θ<sub>min</sub> and
  θ<sub>max</sub>, which bound the Override working range.

**v2:** pot calibration values differ between console-connected operation (the
console provides the pot reference) and standalone Override (the replica
reference provides it, `[DR-2]`). The firmware then keeps a separate calibration
for each reference source.

#### Power-Data Sources (owner decisions 2026-10-12 & 2026-10-24)

| Source | Status | Use |
|---|---|---|
| **First-principles electromechanical model (`Physics P3r`) fitted to Phase 3 C07 320-point sweep** | **Primary (Tier 1 Default)** | Evaluates continuous power directly from calibrated $\tilde{p}$ and cadence ($R^2 = 0.999522$, $\text{RMSE} = 3.77\text{ W}$) and seeds `DEFAULT_POWER_TABLE` |
| **Echelon app's displayed power**, read by the rider at a grid of level × cadence points and entered through the recalibration UI | **Tier 3 Override / Refinement** | Refines the per-level table or calibrates unit-specific offsets via `RecalibrationUI` |
| **Community / Custom Echelon power tables** | **Tier 2 Override** | Rider-uploaded CSV/JSON power table via `/api/power-table` |

The three tiers below govern how `PowerEngine` computes power at runtime, from
the built-in physical formula to unit-specific empirical overrides.

#### Tier 1 — First-Principles Physical Formula (`PowerTableTier::DEFAULT`)

In Tier 1 (the factory default), `PowerEngine::computePower(continuous_level, cadence_rpm)`
evaluates the 4-parameter electromechanical formula `Physics P3r` (§6.2)
directly in hardware floating point. In addition, the built-in reference table
(`ex3-default` in `src/echenon/PowerTableDefault.h` and
`tools/calibration-rig/ex3-default-power-table.csv`) is populated from
Echenon's 320-point empirical sweep (`docs/measurements/phase3/evidence/sweep_20261024.csv`)
and physical model so that portal previews (`GET /api/power-table`) and Tier 3
initial seeding have a complete $32 \times 31$ grid ($0\text{–}150\text{ RPM}$ in
$5\text{ RPM}$ steps) derived entirely from Echenon's own measurements.

#### Tier 2 — Power Matrix

A **power lookup table** mapping (console level × cadence) → watts, selected
or uploaded on the portal **Calibration** page:

```
┌──────────────────────────────────────────────┐
│  Power Calibration                           │
│                                              │
│  Source: [ Echelon EX-3 measured table   ▼]  │
│          ────────────────────────────────     │
│          Default table                       │
│        ► Echelon EX-3 measured table         │
│          Custom (upload CSV)                 │
│          Recalibrated (from readings)        │
│                                              │
│  Preview:                                    │
│  ┌──────────────────────────────────────┐    │
│  │  L\RPM │  60  │  70  │  80  │  90  │    │
│  │  ──────┼──────┼──────┼──────┼──────│    │
│  │   10   │  …   │  …   │  …   │  …   │    │
│  │   20   │  …   │  …   │  …   │  …   │    │
│  └──────────────────────────────────────┘    │
│                                              │
│  [ Apply ]                                   │
└──────────────────────────────────────────────┘
```

- Built-in tables are compiled into the firmware as `const` PROGMEM arrays,
  subject to the licence rule in Tier 1.
- Riders can upload a custom CSV:

```csv
level,rpm_60,rpm_65,rpm_70,rpm_75,rpm_80,...
1,12,14,17,20,24,...
2,15,18,22,26,31,...
...
32,380,420,465,515,570,...
```

#### Tier 3 — Recalibration from Readings (DR-1 §4, condition 2.6; refined in PR-05 `WO-05.4`)

The **recalibration UI** (PR-02 / PR-05) is the primary acquisition path:

1. The rider rides at a console level and a steady cadence.
2. The rider enters the **level** and the **power** read from the console,
   the Echelon app screen, or external power-meter pedals.
3. The firmware correlates the entry with its live readings over the same
   window (display-path wiper → `i`, cadence) and stores the point.
4. Repeating this over reference points updates both:
   - **Parametric Physics Model (`RecalibrationUI::applyToPhysicsParams`):** Solves
     a closed-form least-squares linear regression for the multiplicative
     eddy-current gain $A$ (`PowerPhysicsParams::a_scale`, clamped to
     $[0.25\,A_0, 4.0\,A_0]$) across all stored points (`EchenonModule::syncPhysicsCalibration()`),
     recalibrating the smooth continuous `Physics P3r` surface globally without
     kinks.
   - **Discrete $32 \times 31$ Table (`RecalibrationUI::applyToPowerTable`):** Updates
     the **32 levels (1–32) × 31 cadence bins (0–150 RPM in 5 RPM steps)** grid
     via nearest-bin cell replacement (`watts[level - 1][rpmBin]`) with a
     **5000 ms (5 s)** correlation averaging window (`RecalibrationUI::AVERAGING_WINDOW_MS`)
     and a maximum of **100** stored points (`RecalibrationUI::MAX_POINTS = 100`).

A level entry that disagrees with the wiper-derived level is flagged, and can
also be used to refine the wiper table (§6.3 passive calibration). The rider can
reset the recalibrated table to the default.

### 6.4 Persisted Settings

Calibration and mode settings are fields of the `echenon` configuration
namespace (`EchenonModule` implements `IConfigurable`); `ConfigManager`
persists them. PR-02 may refine key names and sizes; the semantics of the v1
keys are frozen at DR-1.

| Key | Type | Default | Description |
|---|---|---|---|
| `cal_level_table` | array[32] of uint16 | empty (B03 fit used) | Passive calibration: display-path wiper counts at each console level (PR-02) |
| `cal_pos_done` | bool | false | Passive position calibration completed |
| `cal_power_tier` | uint8 | 1 | Active power table (1 = default, 2 = selected or custom matrix, 3 = recalibrated) |
| `cal_matrix_id` | string | `""` | Selected built-in matrix (tier 2) |
| `cal_matrix_custom` | blob | — | Custom CSV matrix uploaded via the portal (tier 2) |
| `cal_power_table` | blob (32 × N watts) | copy of the active table | Working table updated by recalibration (tier 3) |
| `cal_recal_points` | array of {level, watts, rpm, i, time} | empty | Recalibration entries, bounded to `100` points (`RecalibrationUI::MAX_POINTS`) |
| `runaway_timeout_s` | uint16 | `10` | Stock-runaway detection time (seconds) outside 165–1327 counts (§5.2) |
| `save_rides_to_flash` | bool | `false` | Enable persistent ride storage to LittleFS (§14.2) |
| `diag_ride_cap_kb` | uint32 | `512` | Maximum total flash budget for persisted rides (KB, range 64–2048; §14.2) |
| `cal_level_table_sa` | array[32] | empty | **v2:** standalone-referenced level table (PR-07) |
| `cam_theta_min` | float | `[DR-2]` | **v2:** cam-min angle from the motor-driven sweep (PR-07) |
| `cam_theta_max` | float | `[DR-2]` | **v2:** cam-max angle from the motor-driven sweep (PR-07) |
| `cam_last_half` | uint8 | — | **v2:** last known half-revolution, verified at boot (§5.6) |
| `op_mode` | uint8 | 0 | **v2:** operating mode (0 = Intercept, 1 = Override); MITM board only |
| `op_override_levels` | uint8 | `[DR-2]` | **v2:** Override display scale size |
| `motor_dead_band` | float | `[DR-2]` | **v2:** position dead band, in degrees of cam angle |
| `erg_hold_s` | uint16 | `[DR-2/PR-08]` | **v2:** `SUSPENDED_BY_RIDER` hold time |

---

## 7. BLE FTMS Server

### 7.1 Overview

The FTMS server is one of the three independent output paths. It broadcasts
standard BLE fitness data continuously. Any compatible app can connect at any
time, or nothing connects; recording and the ride page work regardless.

It implements the **Bluetooth SIG Fitness Machine Service v1.0** (`0x1826`).
The MIT-licensed [esp32-ftms-server](https://github.com/jamesjmtaylor/esp32-ftms-server)
and [bluetooth-reebok-57e](https://github.com/dbsqp/bluetooth-reebok-57e) are
GATT structure references.

### 7.2 Foundation BLE Integration

`FTMSServer` implements `IBLEServiceProvider` from the framework's BLEManager
module and `ITickable` for its 2 Hz notification timer. It is owned by
`EchenonModule` and registered in `main.cpp` as shown in §3.5.

BLEManager behaviour relied on (from the `v0.0.12-dev.82` headers):

- **Pre-lock registration.** Providers register before
  `bleManager.begin(app)`; afterwards registration returns `false`.
- **Built-in Device Information Service.** BLEManager creates DIS (`0x180A`)
  itself. Echenon does not create a second `0x180A` service; manufacturer and
  model strings come from the framework's app identity.
- **Main-loop callbacks.** Provider callbacks (`onPeerConnected`,
  `onSubscriptionChanged`, `onNotification`, …) run on the main loop from
  `BLEManager::tick()`, so providers need no mutex for their own state.
- **Pause on OTA.** BLEManager calls `onStackPaused()` / `onStackResumed()`
  around OTA downloads; `FTMSServer` stops its notification timer while
  paused.
- **`ScanInhibit`.** `bleManager.inhibitScanning("reason")` returns a
  move-only RAII token; while any token is alive, no discovery or reconnect
  scan starts and running scans stop. Releasing the last token resumes the
  reconnect schedule. The call is thread-safe.
- **Manager-owned connection lifecycle.** BLEManager owns connection
  tracking, advertising packing, Flags and the device name. Echenon does not
  duplicate these.

### 7.3 GATT Services

**Fitness Machine Service (`0x1826`):**

| Characteristic | UUID | Properties | PR |
|---|---|---|---|
| Fitness Machine Feature | `0x2ACC` | Read | PR-05 (v1) |
| Indoor Bike Data | `0x2AD2` | Notify @ 2 Hz | PR-05 (v1) |
| Supported Resistance Level Range | `0x2AD6` | Read | **v2** (PR-08) — Omitted in v1 per FTMS v1.0 §3.1 Table 3.1 (`C` on Resistance Target Setting bit 2 = 0) |
| Supported Power Range | `0x2AD8` | Read | **v2** (PR-08) — Omitted in v1 per FTMS v1.0 §3.1 Table 3.1 (`C` on Power Target Setting bit 3 = 0) |
| Fitness Machine Control Point | `0x2AD9` | Write, Indicate | **v2** (PR-08) — Omitted in v1 per FTMS v1.0 §3.1 Table 3.1 (`C` on Target Setting Features = 0) |
| Fitness Machine Status | `0x2ADA` | Notify | **v2** (PR-08) — Omitted in v1 per FTMS v1.0 §3.1 Table 3.1 (`C` on Control Point `0x2AD9` presence) |

**Fitness Machine Feature (`0x2ACC`)** — Fitness Machine Features field
(bytes 0–3) and Target Setting Features field (bytes 4–7):

| Feature | Field, bit | Set? | Notes |
|---|---|---|---|
| Cadence Supported | Features, bit 1 | Yes | |
| Resistance Level Supported | Features, bit 7 | Yes | |
| Heart Rate Measurement Supported | Features, bit 10 | Yes | |
| Power Measurement Supported | Features, bit 14 | Yes | |
| Resistance Target Setting Supported | Target Settings, bit 2 | v1: **No**. **v2:** Override Mode only | Enables Set Target Resistance Level |
| Power Target Setting Supported | Target Settings, bit 3 | v1: **No**. **v2:** Override Mode only | Enables Set Target Power (ERG) |

**v1: Target Setting Features = 0** (the whole field is zero). FTMS is
read-only. **v2:** the target-setting bits are mode-dependent: clear in
Intercept Mode and in builds without ERG support, set only in Override Mode
with ERG compiled in `[DR-2]`.

```cpp
class FTMSServer : public IBLEServiceProvider, public ITickable {
public:
    const char* getProviderId() const override { return "ftms"; }
    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                               uint16_t charUuid16, bool notify, bool indicate) override;
    void onStackPaused() override;
    void onStackResumed() override;
    void tick(uint32_t currentMillis) override;
};

void FTMSServer::contributeAdvertising(BLEAdvertisingBuilder& adv) {
    static const uint8_t sd[3] = { 0x01 /*available*/, 0x20, 0x00 /*indoor bike*/ };
    adv.addServiceUuid16(0x1826, BLEAdvertisingBuilder::Priority::Required);
    adv.addServiceData16(0x1826, sd, sizeof sd, BLEAdvertisingBuilder::Priority::Required);
    adv.setAppearance(kIndoorBikeAppearance);
}
```

Advertising is 31-byte legacy only (the framework sets
`CONFIG_BT_NIMBLE_EXT_ADV=n`). `Required` elements that fit nowhere raise the
"BLE Advertising" precondition fault.

### 7.4 FTMS Control Point (`0x2AD9`)

> **v2.** v1 FTMS is read-only.

The Control Point lets FTMS clients command resistance.

- **v1 (PR-05):** no targets. Per Bluetooth SIG FTMS v1.0 §3.1 Table 3.1,
  `Fitness Machine Control Point (0x2AD9)` is Conditional (`C`) on any Target
  Setting Features bit being set in `0x2ACC` (bytes 4–7). Because v1 sets
  `Target Setting Features = 0x00000000`, `0x2AD9`, `0x2AD6`, `0x2AD8`, and
  `0x2ADA` are omitted in v1 (`FTMSData.h` citation).
- **v2 (PR-08):** implements the target opcodes and Fitness Machine Status
  below.

**Opcodes:**

| Opcode | Name | Parameter | Echenon action |
|---|---|---|---|
| `0x00` | Request Control | — | Grants control to the requesting client. Other opcodes return `0x05` until it succeeds. Also clears `SUSPENDED_BY_RIDER`. |
| `0x01` | Reset | — | Clears the ERG target; ERG → `RELEASED`; manual control |
| `0x04` | Set Target Resistance Level | FTMS v1.0: UINT8, 0.1 resolution. Some clients send SINT16; both lengths are accepted | Maps onto the Override display scale and sets the target angle |
| `0x05` | Set Target Power | SINT16, watts | ERG: target angle for the current smoothed cadence (§5.6) |
| `0x07` | Start or Resume | — | Accepted; no-op (recording is automatic) |
| `0x08` | Stop or Pause | — | Accepted; no-op |
| `0x11` | Set Indoor Bike Simulation Parameters | wind, grade, Crr, Cw | Returns `0x02` (Op Code Not Supported). Simulation mode is reserved for a later release; the v2 Control Point parser leaves room for it. |

**Response (Indicate):** `[0x80] [request_opcode] [result_code]`

Result codes: `0x01` Success, `0x02` Op Code Not Supported, `0x03` Invalid
Parameter, `0x04` Operation Failed, `0x05` Control Not Permitted.

**Access.** Any Central in radio range can Request Control. This is an
accepted, documented risk for a device used in the rider's own home (§11.4).

### 7.5 Indoor Bike Data Packet (`0x2AD2`)

Little-endian, notified at 2 Hz:

```
Offset  Size     Field                  Flags bit  Resolution
0       uint16   Flags                  —          Bitmask
2       uint16   Instantaneous Speed    0 (=0)     0.01 km/h
4       uint16   Instantaneous Cadence  2          0.5 RPM
6       sint16   Resistance Level       5          1 (display level)
8       sint16   Instantaneous Power    6          1 W
10      uint8    Heart Rate             9          1 BPM (only when HR is valid)
```

Speed is derived from instantaneous mechanical power $P$ via first-principles
flat-road cycling equilibrium ($75\text{ kg}$ rider + $9\text{ kg}$ bike =
$84\text{ kg}$, $C_d A = 0.40\text{ m}^2$, $\rho = 1.225\text{ kg/m}^3$,
$C_{rr} = 0.004$; [`docs/power-physics-model.md`](docs/power-physics-model.md)
§8.3):

$$0.245\,v^3 + 3.53\,v = P(\tilde{p}, \text{cad}) \quad (v\text{ in m/s}, \quad v_{\text{km/h}} = 3.6\,v)$$

encoded in `0.01 km/h` units (`uint16_t(round(v_kph * 100.0f))`; `0` when
$P \le 0\text{ W}$). Both FTMS `0x2AD2`, `/ws/echenon/live`, and the TCX
exporter (§9.4) use this identical `PowerEngine::computeVirtualSpeedKph(P)`
derivation so speed and integrated distance reflect real effort across all
resistance levels.

`FTMSServer` tracks subscriptions through `onSubscriptionChanged()` and, in
`tick()`, notifies only when at least one client is subscribed and the stack
is not paused.

### 7.6 Notification Rate & Safety

FTMS notifications are sent at **2 Hz** (every 500 ms) and never faster than
**4 Hz**; higher rates increase connection-event density and compete with WiFi
for the shared radio (§8.5).

HR re-broadcast notifications follow the strap's native rate (typically 1 Hz).

### 7.7 App Compatibility

These apps form the FTMS test matrix. Support is verified per app in PR-05
(v1 telemetry, including how each app behaves with read-only FTMS) and PR-08
(**v2** control).

| App | Platform | Notes |
|---|---|---|
| **Zwift** | Windows, Mac, iOS, Android | Virtual cycling world |
| **TrainerRoad** | iOS, Android, Windows, Mac | Structured power workouts |
| **Xert EBC** | iOS, Android, Karoo | SMART workouts, live MPA |
| **Peloton Digital** | iOS, Android | Instructor classes; FTMS support unverified |
| **Wahoo SYSTM** | iOS, Android, Mac | Video workouts with power targets |
| **Rouvy** | iOS, Android, Windows, Mac | Augmented-reality routes |
| **MyWhoosh** | iOS, Android, Windows, Mac | Free virtual cycling |
| **Kinomap** | iOS, Android | Real-route video rides |
| **Garmin devices** | Edge / Forerunner / Fenix | Records as an Indoor Cycling activity |
| **Apple Watch** | watchOS | Live workout tracking |

### 7.8 Multi-Client BLE Connections

Echenon supports up to **3 simultaneous Peripheral connections plus 1
Central** (the HR strap). Example scenarios:

- Xert EBC (coaching) and Zwift (visuals) at the same time
- Echelon tablet (HR service) and Zwift (FTMS) at the same time
- Garmin watch and a phone app at the same time

The framework's `sdkconfig.ble.defaults` allocates 2 connections and 4
controller activity slots. NimBLE counts connections of both roles against
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`, so the Echenon target's
`hardware/ex3-waveshare-nano/sdkconfig.defaults`, applied after
`sdkconfig.ble.defaults`, raises both: 4 connections (3 peripheral + 1
central) and 6 activity slots (4 connections + advertising + scanning).

```ini
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4
CONFIG_BT_CTRL_BLE_MAX_ACT=6
```

`BLEConfig::maxPeripheralConnections` keeps its framework default of 3. PR-05
confirms the slot arithmetic against the framework and measures the resulting
internal-DRAM cost; the framework measured about 69 KB for its sample
configuration.

**Device name.** `main.cpp` calls
`bleManager.setDefaultAdvertisedName("Echenon")`. With `nameSuffix = true`
(the framework default), BLEManager appends a `-XXXX` suffix from the
Bluetooth MAC, so each unit advertises as **`Echenon-XXXX`** (for example
`Echenon-1A2B`). The suffix distinguishes several Echenon devices in one
household or gym.

---

## 8. Heart Rate Monitor Relay

### 8.1 Overview

The ESP32 connects to an HR device as a BLE Central and delivers the heart
rate to all three output paths.

**Compatible devices:** any standard BLE device advertising the Heart Rate
Service (`0x180D`): chest straps (Polar H10, Garmin HRM-Pro), optical armbands
(Wahoo TICKR Fit, Scosche Rhythm+, CooSpo HW808) and HR watches in broadcast
mode. The parser supports the full `0x2A37` specification: uint8 and uint16
HR formats, optional sensor-contact status, optional RR-intervals and optional
energy expended.

**Primary tested device:** Echelon Beat Advanced Armband (model **HW901**,
OEM Shenzhen GWSTAI, BLE company `0x0F05`, FCC ID `2AWD4HW901`). It advertises
as `EBEAT-NNNNNNN`. See
[`docs/echelon-beat-ble-validation.md`](docs/echelon-beat-ble-validation.md)
for the full GATT dump and comparative test results.

| Beat characteristic | Value | Firmware impact |
|---|---|---|
| HR format | uint8 only | Parser handles both formats |
| Sensor contact flags | Not supported (flags always `0x00`) | Firmware does not assume other devices lack them (§8.4) |
| RR-intervals | Not present | Parser extracts them when present |
| Energy expended | Not present | Field skipped when present |
| Body Sensor Location | `0x04` (Hand) | `HRMRepeater` reports `0x00` ("Other") until EventBus routing is added (known limitation, S4) |
| Battery Service | `0x180F` with Read + Notify | Shown in the BLE portal tab when available |
| Pairing | Not required (open connection) | Opportunistic bonding: bonds if the peer supports SMP |
| Max connections | 1 Central only | The ESP32 holds the connection exclusively |
| Notification rate | ~1 Hz | Typical for optical sensors; chest straps may be faster |
| Auto-sleep | Stops advertising ~30–60 s after losing skin contact | Causes a full disconnect (§8.4) |
| No-contact signal | Sends BPM = 0 (not contact flags) | Both methods are handled (§8.4) |

```
[HR strap / Echelon Beat armband]
        │
        │  BLE Central (HRMClient, IBLEClientProvider)
        ▼
   HRMClient ──publish──► EventBus topic "echenon.hr"
                               │
        ┌──────────────────────┼──────────────────────┐
        ▼                      ▼                      ▼
  HRMRepeater             FTMSServer             SessionManager
  BLE HR Service 0x180D   HR in Indoor Bike      HR per data point
  → Echelon app (tablet)  Data 0x2AD2            → TCX → Strava
                          → Zwift / TrainerRoad
```

### 8.2 Foundation BLE Integration for HR

**`HRMClient` implements `IBLEClientProvider`.** It declares what it wants;
BLEManager handles scanning, connecting, subscribing and reconnecting.

```cpp
class HRMClient : public IBLEClientProvider, public ITickable {
public:
    const char* getProviderId() const override { return "hr_client"; }
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }

    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override {
        if (max == 0) return 0;
        out[0] = 0x180D;
        return 1;
    }

    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override {
        size_t n = 0;
        if (n < max) out[n++] = { 0x180D, 0x2A37, /*indicate*/false, /*required*/true  };
        if (n < max) out[n++] = { 0x180F, 0x2A19, /*indicate*/false, /*required*/false }; // battery
        return n;
    }

    void onConnected(const BLEStoredDevice& device) override;
    void onNotification(const BLENotification& notification) override;
    void onDisconnected(BLEDisconnectReason reason) override;
    void tick(uint32_t currentMillis) override;   // 10 s stale-data timeout

private:
    void publishHr(uint8_t bpm, HRContactState state, uint32_t receivedAtMs);
};
```

**HR event path.** `HRMClient::publishHr()` publishes an `HrSample
{ bpm, contactState, receivedAtMs }` on the EventBus topic `echenon.hr`.
`HRMRepeater`, `FTMSServer` and `SessionManager` subscribe to it. The EventBus
contract requires a payload to stay valid until every subscriber has run, so
`HRMClient` publishes from a small ring buffer of `HrSample` slots, the same
pattern BLEManager uses for its own events. `HRMClient` holds no reference to
`HRMRepeater`.

**HR Measurement parsing** handles the full `0x2A37` specification, not just
the subset the Beat uses:

```cpp
void HRMClient::onNotification(const BLENotification& n) {
    if (n.charUuid16 == 0x2A19) {          // Battery Level
        _batteryPct = n.data[0];           // 0–100 %
        return;
    }
    if (n.charUuid16 != 0x2A37 || n.length < 2) return;

    // --- Flags byte (Bluetooth SIG Heart Rate Measurement) ---
    uint8_t flags            = n.data[0];
    bool    hrIs16Bit        = flags & 0x01;
    bool    contactSupported = flags & 0x04;  // bit 2
    bool    contactDetected  = flags & 0x02;  // bit 1 (valid only if bit 2 set)
    bool    energyPresent    = flags & 0x08;
    bool    rrPresent        = flags & 0x10;

    // --- HR value ---
    uint16_t bpm;
    size_t   idx;
    if (hrIs16Bit) {
        if (n.length < 3) return;
        bpm = n.data[1] | (n.data[2] << 8);
        idx = 3;
    } else {
        bpm = n.data[1];
        idx = 2;
    }

    // --- No-contact detection (two device-dependent methods) ---
    // 1. Contact flags (Polar H10, Garmin HRM-Pro, …): supported && !detected
    // 2. BPM = 0 (Echelon Beat HW901 and many ODM armbands)
    if (bpm == 0 || (contactSupported && !contactDetected)) {
        _contactState = HRContactState::NO_CONTACT;
        publishHr(0, _contactState, n.receivedAtMs);   // relay BPM = 0 to downstream
        return;
    }

    // --- Valid HR reading ---
    _contactState  = HRContactState::ACTIVE;
    _lastValidHrMs = n.receivedAtMs;
    publishHr(bpm > 255 ? 255 : bpm, _contactState, n.receivedAtMs);

    // --- Optional energy expended field ---
    if (energyPresent && idx + 2 <= n.length) idx += 2;

    // --- Optional RR-intervals (kept for future TCX enrichment) ---
    if (rrPresent) {
        while (idx + 2 <= n.length) {
            uint16_t rrRaw = n.data[idx] | (n.data[idx + 1] << 8);
            _lastRrMs = rrRaw / 1024.0f * 1000.0f;
            idx += 2;
        }
    }
}
```

**HR contact state**, derived from hardware validation:

```
                    ┌─────────────┐
     Contact made   │   ACTIVE    │  BPM > 0, contact detected (or unsupported)
    ┌──────────────►│             │  → relay to FTMS / TCX / re-broadcast
    │               └──────┬──────┘
    │                      │ BPM = 0 OR contact flags say off-skin
    │               ┌──────▼──────┐
    │               │ NO_CONTACT  │  Relay BPM = 0 (transparent pass-through)
    │  Contact back │             │  → armband may regain contact any moment
    ├──────────────►└──────┬──────┘
    │                      │ Connection lost (auto-sleep or out of range)
    │               ┌──────▼──────┐
    │               │DISCONNECTED │  onDisconnected() fires
    │               │             │  → BLEManager reconnect schedule (§8.4)
    └──────────────►└─────────────┘
```

**`HRMRepeater` implements `IBLEServiceProvider`.** The HR service always
exists (static GATT). It notifies 0 BPM, or stops notifying, when no valid HR
is available.

```cpp
class HRMRepeater : public IBLEServiceProvider {
public:
    const char* getProviderId() const override { return "hr_repeater"; }
    bool registerServices(NimBLEServer& server) override;   // 0x180D: 0x2A37, 0x2A38
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override {
        adv.addServiceUuid16(0x180D, BLEAdvertisingBuilder::Priority::Normal);
    }
    void subscribe(EventBus& bus);   // topic "echenon.hr"; notifies if subscribed
};
```

The HR UUID uses `Normal` priority, so it spills into the scan response when
the FTMS elements fill the primary advertising packet. The Body Sensor
Location (`0x2A38`) value is read from the connected strap and defaults to
`0x00` (Other) if the strap does not expose it; it is not hard-coded.

**Stale data.** If no valid notification arrives for 10 s, `HRMClient`
publishes HR = 0. On disconnect it publishes HR = 0 immediately.

**Discovery and pairing.** `bleManager.startDiscovery("hr_client", 30000)`
runs a user-initiated scan; `bleManager.pair("hr_client", addr)` pairs;
`bleManager.forget("hr_client")` unpairs. BLEManager's device registry
persists the paired identity.

**Portal.** BLEManager provides the BLE portal tab with device cards, scan,
pair and forget, reconnect phase and battery level. Echenon adds no HR portal
page.

### 8.3 Tablet BLE Connections

The tablet keeps two independent BLE connections:

| Connection | Target | Purpose |
|---|---|---|
| 1 | Factory Echelon PCB | Bike metrics for the class UI |
| 2 | Echenon ESP32 (`Echenon-XXXX`) | Heart rate re-broadcast from the strap |

> **Single-connection constraint:** many consumer HR monitors, including the
> Echelon Beat, accept only **one BLE Central** at a time. When the ESP32
> connects to the armband, the Echelon app on the tablet cannot connect to it
> directly. The ESP32 holds the HR connection and re-broadcasts it through
> `HRMRepeater`, which the tablet connects to instead. Before pairing through
> the Echenon portal, no other app (Echelon, Zwift, …) may hold the strap's
> connection. Monitors that accept several Centrals (e.g. Polar H10) have no
> such constraint.

**PR-03 entry gates:**

1. **Echelon-app HR spike** (register row #12): nRF Connect advertises an
   emulated `0x180D` peripheral named `Echenon-TEST`, and the Echelon app must
   list it and display its HR.
2. **Framework central-role acceptance evidence**: RFC §17.2 items 4–9 run
   against a real `0x180D` peripheral on a framework tag that includes the
   WO-8 use-after-free fix. At `v0.0.12-dev.82` the framework's acceptance
   record marks the central role "N/A", so this gate is **open**.

### 8.4 Reconnect Policy

**Two-tier no-contact detection:**

| Tier | Trigger | Latency | Applies to |
|---|---|---|---|
| **Instant** | `BPM = 0` from the device | **< 1 s** | Devices that send zero when the optical sensor loses pulse (Echelon Beat, many ODM armbands) |
| **Instant** | Contact flags: `contactSupported && !contactDetected` | **< 1 s** | Devices that implement BLE HR contact status (Polar, Garmin, Wahoo) |
| **Timeout** | No valid notification for **10 s** | **10 s** | All devices: connection drops, interference, device crashes |

When no-contact is detected by either tier:

- `HRMRepeater` notifies 0 BPM (transparent pass-through of the no-contact signal)
- the FTMS packet clears flag bit 9 (HR not present)
- the session records HR = 0

Notifications resume immediately when a valid HR arrives.

**Auto-sleep.** Some monitors, including the Beat, stop advertising entirely
about 30–60 s after losing skin contact. This causes a full disconnect
(`onDisconnected()`), not just a stale timeout, and the reconnect schedule
re-acquires the device when the rider puts it back on. Because the Beat
sleeps this way, the slow retry phase is the **common** recovery path, not an
edge case. Riders put the armband on before starting a session.

**Reconnect schedule** (BLEManager's `BLEReconnectPolicy`):

- **Direct connect** to the stored address, not a general scan.
- **Fast phase**: attempts after `{2, 4, 8, 16, 32}` s (the framework default
  `reconnectFastScheduleS`).
- **Slow phase**: one attempt every **15 s**, indefinitely while
  `AppState::Running`. The schedule never reaches `Exhausted` while Running.
  No path gives up after a fixed number of attempts.
- **Suspension**: `ScanInhibit` tokens (e.g. during an HTTPS upload) and OTA
  suspend the schedule; it resumes when they end.

**Configuring the slow phase.** `slowRetryMs` is a persisted field of
BLEManager's `BLEConfig` (framework default 60 000 ms; `0` would mean give up).
`getReconnectPolicy()` returns a **copy** of the policy, so changing that copy
has no effect. Echenon sets `slowRetryMs = 15000` through BLEManager's
configuration (the `ble` `IConfigurable` namespace), including after a factory
reset restores the framework default. PR-03 verifies the exact mechanism.

**HR gap criteria:**

| Criterion | Limit | What it measures |
|---|---|---|
| **(a) Stale detection** | HR marked absent within **12 s** of the last valid notification | App-level policy: 10 s threshold + 2 s grace |
| **(b) Steady state** | No gap **> 2.5 s** while connected and on-skin | Radio coexistence jitter at a ~1 Hz strap; checked in PR-03 and PR-05, finally verified in PR-09 |
| **(c) Recovery** | Reconnected **≤ 20 s** after the strap returns (slow interval 15 s + 5 s connection time) | The slow phase of the reconnect schedule |

### 8.5 Radio Coexistence Policy (WiFi + BLE)

> [!WARNING]
> The ESP32-S3 has a **single 2.4 GHz radio** shared between WiFi and BLE by
> time-division coexistence. Poor coexistence management causes FTMS
> notification jitter, HR drops, portal timeouts and upload failures. This
> section is binding policy for all BLE and WiFi code.

**ESP-IDF coexistence configuration.** The framework's
`sdkconfig.ble.defaults` sets `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`. Echenon
adds nothing that changes the coexistence scheme.

**DRAM budget.** BLE costs about 69 KB of internal DRAM (35–40 KB controller
blob plus 25–30 KB NimBLE host), as measured by the framework. All BLE memory
stays in internal DRAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=y`, set by
the framework): the BLE controller cannot use PSRAM on the ESP32-S3. PR-05
measures Echenon's figure with the 4-connection override.

**Coexistence preference.** Application code never calls
`esp_coex_preference_set()`; the framework's setting stands.

**Scanning discipline.**

*Discovery scan* (finding new devices):

| Rule | Rationale |
|---|---|
| **User-initiated only** | Scans run only when the rider opens the BLE pairing page or triggers one via MCP. Echenon never scans autonomously for new devices. |
| **Duty cycle clamped by the framework** | Scan window ≤ 50 % of the interval normally, ≤ 25 % while any peripheral connection exists. Application code does not bypass this. |
| **Duration bounded to 30 s** | Auto-stop after 30 s; the rider can re-trigger. |
| **Suppressed during WiFi-heavy work** | Upload and other bulk transfers hold a `ScanInhibit` token; nothing calls NimBLE scan APIs directly. |
| **Not in CONFIGURING / AP mode** | BLEManager blocks scanning until `AppState::Running`, keeping WiFi responsive for setup. |

*Reconnect* (known, previously paired device):

| Rule | Rationale |
|---|---|
| **Direct connect to the stored address** | Uses the persisted identity; far cheaper than a general scan. |
| **Fast then slow schedule** | `{2, 4, 8, 16, 32}` s, then every 15 s indefinitely while Running (§8.4). |
| **Conservative duty cycle when connections are active** | The framework's reconnect scan parameters (interval 160 ms, window 40 ms) apply. |
| **Stale-data protection throughout** | While disconnected, HR = 0 (§8.4); the schedule keeps running. |

**Phase-aware behaviour:**

| Foundation phase | WiFi state | BLE behaviour |
|---|---|---|
| **BOOT** | Off | BLE not started |
| **CONFIGURING** (AP mode) | AP active, portal serving | No scanning or reconnecting; no advertising (`advertiseInConfiguring = false`, the framework default). The rider configures WiFi first. |
| **RUNNING** (STA mode) | Connected | Normal operation. Discovery only when the rider opens the pairing page; reconnect to the known strap is automatic. |

**Upload and OTA radio budget:**

- **Strava upload.** Each network stage of the upload pipeline holds
  `auto token = bleManager.inhibitScanning("strava_upload");` for its
  duration. The call is compiled under `#ifdef WITH_BLE`, so PR-06 does not
  depend on PR-03. Connections and advertising are unaffected; the
  DataManager's backoff absorbs deferred fetches.
- **OTA download.** BLEManager pauses the stack on OTA progress and, with
  `otaDisconnectPeers = true` (the framework default), disconnects peers. The
  rider is not riding during an OTA.
- **OTA is manual-only.** The framework's `OTAUpdateManager` with
  `autoCheck = true` runs a full update, including a reboot, as soon as WiFi
  connects after boot, which is exactly when a rider powers on the bike.
  Until framework [issue #226](https://github.com/matthewmcneill/esp32-app-foundation/issues/226)
  adds `checkOnBoot` and a runtime "busy" guard, Echenon forces
  **`autoCheck = false`** after config load and again after every factory
  reset (the framework's OTA `resetConfig()` restores `true`). The framework
  exposes no public setter at `v0.0.12-dev.82`, so Echenon applies the value
  through the OTA module's configuration; PR-04a verifies the mechanism. OTA
  runs only from the System portal tab.

**NimBLE connection parameters.** BLEManager applies them from `BLEConfig`:

| Role | Parameters | Notes |
|---|---|---|
| Peripheral (FTMS, HR re-broadcast) | Preferred 12–24 units (15–30 ms), latency 0, supervision 400 (4 s) | Requested only; the Central (Zwift etc.) sets the actual interval |
| Central (HR strap) | 24–60 units (30–75 ms), latency 0, supervision 600 (6 s) | Relaxed; HR updates are ~1 Hz |

> [!NOTE]
> Connection-parameter updates are not requested immediately on connect;
> Android centrals can drop the link with GATT CONN TIMEOUT (error 147/133)
> if they are.

**Concurrent FTMS and ride page traffic.** During a ride the ride page and FTMS
are both active; this is the primary case, not an edge case.

| Stream | Protocol | Rate | Payload |
|---|---|---|---|
| FTMS Indoor Bike Data | BLE notify | 2 Hz | 11 bytes |
| HR from the strap | BLE notify (Central) | ~1 Hz | 2–4 bytes |
| HR re-broadcast | BLE notify | ~1 Hz | 2–4 bytes |
| Ride page WebSocket push | WiFi TCP | 2 Hz | ~100 bytes JSON |

Payload airtime is small (on the order of 1 % of the radio), but every BLE
connection also fires a connection event each interval, and WiFi beacons, ACKs
and AP traffic add more. **The measured coexistence tests are the arbiter, not
this arithmetic.** The FTMS and WebSocket timers are offset by 250 ms to
spread CPU work in the tick loop; this is a scheduling preference, not a
radio-level guarantee, because the BLE controller clocks connection events and
the coexistence arbiter works per event type and priority.

| Burst scenario | WiFi burst | BLE impact | Mitigation |
|---|---|---|---|
| Initial `/ride` load | ~15 KB gzipped | FTMS may miss one notification | Acceptable; happens before riding |
| Portal tab navigation | ~5 KB per tab script | Negligible | gzip + PROGMEM assets |
| Strava upload | ~30–50 KB `tcx.gz` over HTTPS | 2–3 FTMS notifications may jitter | Runs after the session ends |
| OTA update | ~1.5 MB sustained | BLE paused | Rider is not riding |

**Coexistence verification.** PR-03 (HR relay) and PR-05 (FTMS) each run a
progressive check of the scenario below at their own configuration. The final
verification at **maximum configuration** belongs to **PR-09** (§15.10):

1. **3 Peripheral connections** (FTMS notifying at 2 Hz on at least one, plus
   the Echelon tablet and one more app, e.g. nRF Connect × 2 + Zwift)
2. **1 Central connection** (HR strap relaying at ~1 Hz)
3. `/ride` page open via WebSocket (2 Hz push)
4. A Strava upload triggered (or simulated with a large HTTPS POST)
5. **5 minutes continuous**
6. **Pass criteria:**
   - FTMS notifications: no gap > 600 ms (one missed 500 ms cycle allowed)
   - HR relay, steady state: no gap > 2.5 s at a ~1 Hz strap (criterion b)
   - HR relay, dropout and reconnect: reconnected ≤ 20 s after the strap
     returns (criterion c)
   - WebSocket: no disconnect during the upload
   - Upload: every pipeline stage completes within its 15 s
     `FetchContext` budget
   - Minimum free internal heap over the run recorded, with the threshold set
     from the PR-05 measurement
7. **Measurement points:**
   - FTMS gap: time between consecutive Indoor Bike Data notifications at the
     Central (nRF Connect log)
   - HR relay latency (< 50 ms p99): from `BLENotification.receivedAtMs` to
     the re-broadcast notification queued on `HRMRepeater`; over-the-air
     latency at the external Central is excluded
   - HR gap: time between consecutive HR notifications at the Echelon tablet
     (or nRF Connect as proxy)

---

## 9. Session Recording

### 9.1 Overview

The session manager records ride data independently of the ride page and FTMS
paths. It buffers data points in PSRAM during the ride and hands the completed
session to `UploadQueue` when the ride ends.

### 9.2 Session Lifecycle

| State | Trigger | Action |
|---|---|---|
| **Idle** | Boot / after hand-off | Waiting for activity |
| **Recording** | Sustained cadence for **5 s** (`session_auto_start_s` default = 5 s, configurable `1–60 s` when `session_auto_start = true`) or "Start" (manual) | Buffering data points |
| **Paused** | Cadence = 0 during auto-recording (coasting), or manual UI "Pause" | Coasting keeps recording 1 Hz zero-power points (`<Watts>0</Watts>`) to preserve Xert MPA continuity; manual UI "Pause" suspends point emission until "Resume" |
| **Ending** | No cadence for 180 s (`session_inactivity_s` default = 180 s) or "Stop" / "End" (manual) | Finalise the session |
| **Handed off** | Session finalised | Queued in `UploadQueue` |

- Auto-start (`session_auto_start = true`, `session_auto_start_s = 5`) is the
  default, and can be toggled off in configuration so the rider starts and ends
  sessions explicitly from the UI. Rest intervals during an active ride
  (coasting with `cadence = 0`) do not end the session; zero-power points are
  recorded continuously at 1 Hz. The 3-minute auto-end timeout (`180 s`) means
  only a genuine dismount ends a session automatically, and trailing zero-cadence
  points after the last pedal stroke are trimmed on finalisation.
- **Ghost-session prevention.** A single stray pulse does not start a
  session; auto-start needs sustained cadence (`> 0 RPM`) for `session_auto_start_s`
  consecutive seconds.
- **Wall-clock anchor.** Timestamps are tracked via wraparound-safe signed
  `int32_t` `millis()` deltas since session start. When the first NTP-synced
  time (`time(nullptr) > 1700000000`) is available — even if NTP syncs mid-ride
  — the session's `start_epoch_s` is back-computed (`now_epoch - elapsed_s`), so
  sessions started before NTP sync still carry accurate real-world UTC timestamps.
- **Crash recovery.** An append-only binary checkpoint (`/ride_active.bin`) is
  flushed to LittleFS every 30 s (`30` records = `240 B`). After a reboot, an
  unfinished checkpoint with $\ge 60$ active records is recovered and finalised
  as a completed session.
- **Trailing zeros.** Trailing zero-cadence points after the last active pedal
  stroke are trimmed before TCX generation (`trimTrailingZeros()`).

### 9.3 Ride Data Buffer (`2 Hz` Sampling $\to$ `1 Hz` Anti-Aliased Record)

`SessionManager::tick()` samples the live telemetry at **2 Hz (every 500 ms)**
in lockstep with `PowerEngine` and `FTMSServer`, and averages each consecutive
pair of `500 ms` samples into a compact 8-byte **1 Hz `RideRecord`** in PSRAM:

```cpp
struct __attribute__((packed)) RideRecord {
    uint16_t elapsed_sec;         // Elapsed active/coasting seconds since session start (0..65535)
    uint16_t power_watts;         // 2-sample (2 Hz) averaged mechanical power (0..2000 W)
    uint8_t  cadence_rpm;         // 2-sample (2 Hz) averaged crank cadence (0..255 RPM)
    uint8_t  heart_rate_bpm;      // Latest valid HR BPM (0 if unavailable / stale)
    uint8_t  internal_resistance; // Canonical internal resistance i ∈ 0..254 (round(254 * pTilde), 255 = UNCALIBRATED; §5.3)
    uint8_t  flags;               // Bit 0: hr_valid, Bit 1: resistance_calibrated, Bit 2: paused_marker
};  // 8 bytes per 1 Hz record (static_assert(sizeof(RideRecord) == 8))
```

Storing `internal_resistance` (`0..254`, `255 = UNCALIBRATED`) rather than the
quantized `1..32` console level preserves the fundamental normalized cam
coordinate $\tilde{p} = i / 254.0$ at $8\times$ finer resolution, allowing
lossless reconstruction of Echelon (`1..32`), Peloton (`0..100`), or continuous
$\tilde{p}$ downstream. At 1 Hz (`8 bytes/s`) for 60 minutes:
`3,600 records × 8 bytes = 28.8 KB` (`115.2 KB` for a 4-hour `14,400`-record
ring buffer in the 8 MB OPI PSRAM). For optional persistent storage of
completed rides to LittleFS, see §14 (Run-Data Persistence).

### 9.4 TCX Generation

**Execution model.** TCX generation and upload run in the **DataManager
pipeline**, never in `tick()`. When a session ends, `SessionManager`
publishes `session/ended` on the EventBus and `UploadQueue` calls
`DataManager::requestPriorityFetch(this)`.

The pipeline is **split**: each stage is a separate `fetchUpdate()` call, and
each stage fits the 15 s `FetchContext` budget:

| Stage | Work | Completes when |
|---|---|---|
| 1. Generate | Stream 1 Hz TCX XML through gzip (`uzlib` / `miniz` deflate in a 4 KB heap scratch buffer) to `tcx.gz` on LittleFS | File closed; checks `ctx.deadline` periodically so long rides complete safely within the 15 s budget |
| 2. Refresh | Refresh the Strava access token if it has expired (`now >= expires_at - 60`) | New tokens persisted (§10.4) |
| 3. Upload | `POST /api/v3/uploads` with the `tcx.gz` file | Upload id stored |
| 4. Poll | `GET /api/v3/uploads/{id}` until processed | Activity id received, or duplicate reported |

A stage that fails returns the matching `UpdateStatus`, and the job resumes
at that stage on the next fetch. DataManager supplies TWDT unsubscription,
the deadline-bounded transport, health state with exponential backoff, and
diagnostic visibility in the portal, over MCP and on the EventBus.

TCX content: `Activity Sport="Biking"`, lap summary (`TotalTimeSeconds`,
`DistanceMeters`, `Calories`, `AverageHeartRateBpm`, `MaximumHeartRateBpm`,
`Cadence`), and per-trackpoint `Time`, cumulative `DistanceMeters` (integrated
at 1 Hz from virtual road speed $v(P)$ via `PowerEngine::computeVirtualSpeedKph`),
`HeartRateBpm` (when `hr_valid`), `Cadence`, and `<ns3:TPX>` containing both
`<ns3:Speed>` (m/s) and `<ns3:Watts>`. The file is streamed (no DOM in
memory). Size: about 550–720 KB uncompressed for an hour, about 30–50 KB as
`tcx.gz`.

**1 Hz trackpoints.** The TCX file contains exactly one `<Trackpoint>` per
elapsed second, averaging the two 2 Hz samples of its second (§9.3).

Xert computes second-by-second Maximal Power Available (MPA) with continuous
differential equations over the power stream. Gaps, smoothing or sub-1 Hz data
corrupt MPA, produce false breakthrough detections and distort training-load
distribution.

**TCX output rules:**

- One `<Trackpoint>` per elapsed riding/coasting second; no gaps, no skipped seconds during active riding
- When coasting (`cadence == 0`), `<ns3:Watts>0</ns3:Watts>` and `<ns3:Speed>0.00</ns3:Speed>` are written; the trackpoint is never dropped and stale values are never repeated
- Timestamps in ISO 8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`)

```xml
<Trackpoint>
  <Time>2026-09-17T21:00:00Z</Time>
  <DistanceMeters>142.50</DistanceMeters>
  <HeartRateBpm><Value>145</Value></HeartRateBpm>
  <Cadence>90</Cadence>
  <Extensions>
    <ns3:TPX>
      <ns3:Speed>8.67</ns3:Speed>
      <ns3:Watts>265</ns3:Watts>
    </ns3:TPX>
  </Extensions>
</Trackpoint>
```

### 9.5 Offline Resilience

If WiFi is unavailable when a session ends:

1. The `tcx.gz` file is kept on LittleFS.
2. The upload queue (job list and stage per job) is persisted.
3. Queued jobs resume on the next WiFi connection.
4. Up to 5 sessions are queued; beyond that the oldest un-uploaded session is
   kept and new ones are refused with a portal warning.

Queue sizing is checked against the 2.75 MiB LittleFS partition: five gzipped
sessions fit comfortably; uncompressed sessions would not, which is why only
`tcx.gz` is stored.

### 9.6 Manual Download

The web portal exposes a download endpoint:

```
GET /api/session/download?id=<session_id>&format=tcx
```

Riders can download the file and upload it manually to any platform (Xert,
TrainingPeaks, Golden Cheetah, …) when cloud sync is unavailable or not
configured. The file contains heart rate (health data), so the endpoint
requires authentication.

---

## 10. Upload Services

### 10.1 Overview — Multi-Sink UploadQueue Architecture

Upload is the third independent output path. When a session ends, the
recorded data goes to **each configured and enabled cloud service**.
The upload path knows nothing about the ride page or FTMS; it receives
a completed session file and fans it out across all registered sinks.

**Multi-sink registry.** `UploadQueue` holds up to `MAX_SINKS = 4`
registered `IUploader` implementations, managed via
`registerSink(IUploader*)`. Upon session completion,
`enqueueSession(session_id)` creates one `UploadJob` per configured and
enabled sink, each keyed by `(session_id, service_id)`.

**Sequential execution.** At most one sink performs TLS I/O at any time.
`UploadQueue::fetchUpdate()` processes the front job in the queue, gated
by `BLEManager::ScanInhibit`, so BLE FTMS / HRM streams never experience
radio starvation during mid-ride uploads or post-ride queue draining.
Jobs drain in FIFO order; within a session, sinks execute in registration
order.

**Per-sink auto-sync toggles.** Each sink has an independent auto-sync
preference (`auto_sync_strava`, `auto_sync_intervals`, `auto_sync_wahoo`) stored via
`ConfigManager`. A disabled sink is skipped during `enqueueSession()`
fan-out but can still receive manually triggered uploads from the Stored
Rides pill.

### 10.2 Extensible Architecture

Each cloud service is described by an `IUploader`. `IUploader` **describes
capabilities only**: identity, configuration state, and how to execute
each stage (token refresh, upload, poll). It performs no scheduling.
`UploadQueue`, an `IDataSource`, is the **only execution path**: it owns the
transport, holds the `ScanInhibit` token and runs one stage per
`fetchUpdate()`.

```cpp
namespace echenon {

class IUploader {
public:
    virtual ~IUploader() = default;
    virtual const char* serviceId() const = 0;         // "strava", "intervals", "wahoo"
    virtual const char* name() const { return serviceId(); }
    virtual bool isConfigured() const = 0;             // credentials present
    virtual bool isAuthenticated() const = 0;          // tokens / keys valid
    virtual bool needsTokenRefresh(uint32_t now_epoch_s) const = 0;
    virtual bool isEnabled() const { return true; }    // auto-sync toggle
    virtual UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) = 0;
    virtual UploadStageResult postUpload(
        uint32_t session_id, const std::string& tcx_gz_path,
        uint32_t timeout_ms, std::string& out_upload_id,
        std::string& err_msg) = 0;
    virtual UploadStageResult pollUploadStatus(
        const std::string& upload_id, uint32_t timeout_ms,
        std::string& out_activity_id, std::string& err_msg) = 0;
};

} // namespace echenon
```

**`UploadJob` composite key.** Each job is uniquely identified by the
pair `(session_id, service_id)`. This allows multiple sinks to upload the
same session independently, each with its own stage progression, retry
count, and remote activity ID:

```cpp
struct UploadJob {
    uint32_t    session_id;                // session epoch timestamp
    std::string service_id;               // "strava", "intervals", "wahoo"
    UploadStage stage;                     // Generate → Refresh → Upload → Poll → Done
    uint8_t     attempt;                   // retry count for this stage
    std::string upload_id;                 // remote upload ID (set after POST)
    std::string activity_id;              // remote activity ID (set after POLL)
    std::string activity_url;             // remote activity URL
    std::string error;                     // last error message
};
```

**Shared `.tcx.gz` lifecycle.** TCX generation is expensive (PSRAM
streaming gzip via `StreamingGzipEncoder`). The `UploadQueue` generates
the `.tcx.gz` file **once** per session in `STAGE_GENERATE_TCX_GZ`. All
registered sinks reference the same file path. Artifact cleanup
(`removeFileEverywhere()`) occurs only when **every** sink job for that
session has reached `STAGE_COMPLETE` or `STAGE_FAILED`.

**Multi-sink `UploadQueue`:**

```cpp
class UploadQueue : public IDataSource {
public:
    static constexpr size_t MAX_SINKS = 4;

    bool registerSink(IUploader* sink);                // adds to _sinks[]
    IUploader* findSinkById(const char* service_id) const;

    UpdateStatus fetchUpdate(const FetchContext& ctx) override;
    uint32_t     getUpdateIntervalMs() const override;
    const char*  getSourceName() const override { return "upload_queue"; }
    PriorityTier getPriorityTier() const override { return PriorityTier::Low; }
};
```

### 10.3 Settings Pages

Each uploader contributes its own pill to the **Apps** tab via
`ITabRegistry::addTab()`. Per-sink pills are ordered sequentially:

```
Apps (order 60)
├── Strava         (order 10, /upload_settings.js)   — OAuth 2.0 config pill
├── Intervals.icu  (order 20, /intervals_settings.js) — API key config pill
├── Wahoo Cloud    (order 30, /wahoo_settings.js)     — OAuth 2.0 config pill
└── [Future sinks at order 35+]
```

Each pill displays:

- Service name, icon, and connection status (`Connected: <Athlete>`,
  `Not Configured`, `Token Expired`)
- Auto-sync toggle per service (`auto_sync_strava`, `auto_sync_intervals`, `auto_sync_wahoo`)
- Authentication controls appropriate to the service type:
  - **OAuth sinks (Strava, Wahoo Cloud):** `[Connect]` redirect → consent → callback,
    `[Disconnect]` to revoke and clear tokens
  - **API-key sinks (Intervals.icu):** Inline form for athlete ID and
    API key, `[Test Connection]` button, `[Remove]` to clear
- Upload queue status: pending count, last upload result, last error

### 10.3a Intervals.icu Sink (API-Key Reference)

`IntervalsIcuUploader` conforms to `IUploader` and `IConfigurable`
(`"echenon_intervals"`) and serves as the reference API-key sink
implementation.

**Authentication.** Intervals.icu uses a personal API key (no OAuth
redirect dance). The rider pastes their athlete ID and API key into the
Intervals.icu pill in the Apps tab. Credentials are stored encrypted via
`KeyManager` NVS with namespaced keys (`intervals.api_key`,
`intervals.athlete_id`) and registered with
`Redactor::instance().registerSecret()`.

**Upload endpoint:**

```
POST https://intervals.icu/api/v1/athlete/{athlete_id}/activities
Content-Type: application/octet-stream
Authorization: Basic base64(API_KEY:{api_key})
Body: <.tcx.gz binary>
```

**Stage behaviour:**

| Stage | Intervals.icu behaviour |
|---|---|
| `refreshToken()` | No-op → `SUCCESS` (API keys do not expire) |
| `postUpload()` | `POST` the `.tcx.gz` file; returns `SUCCESS` with the activity ID on 200/201 |
| `pollUploadStatus()` | Returns `SUCCESS` immediately (Intervals.icu processes synchronously; no async poll needed) |

**Error handling:**

| HTTP Status | `UploadStageResult` | Meaning |
|---|---|---|
| 200 / 201 | `SUCCESS` | Activity created |
| 401 | `PERMANENT_ERROR` | Invalid API key |
| 429 | `RETRYABLE_ERROR` | Rate limited; backoff and retry |
| 5xx | `RETRYABLE_ERROR` | Server error; backoff and retry |

**REST API routes (Intervals.icu configuration):**

| Route | Method | Auth | Description |
|---|---|---|---|
| `/api/intervals/status` | `GET` | ✓ | Connection status and athlete info |
| `/api/intervals/credentials` | `POST` | ✓ | Save athlete ID and API key |
| `/api/intervals/test` | `POST` | ✓ | Test connection with saved credentials |
| `/api/intervals/disconnect` | `POST` | ✓ | Clear saved credentials |

### 10.3b Wahoo Cloud Sink (OAuth 2.0 Sink #3)

`WahooUploader` conforms to `IUploader` and `IConfigurable`
(`"echenon_wahoo"`, schema v1) and serves as the second OAuth 2.0 sink
(Sink #3 in the 4-sink registry).

**Authentication & Bring-Your-Own-App (BYOA).** Like Strava, Wahoo Cloud
uses a Bring-Your-Own-App model to avoid embedding shared client credentials
in firmware. Riders register an application on the Wahoo Developer Portal
(`https://developers.wahooligan.com`), configuring:
- **Redirect URI:** `http://echenon.local/api/wahoo/callback` (or the device IP)
- **Requested Scopes:** `user_read workouts_write`

Credentials and tokens are stored encrypted via `KeyManager` / `ISecretStore`
under NVS namespace `echenon_wahoo`:
- `wahoo.client_secret`: BYOA Client Secret (write-only)
- `wahoo.refresh_token`: OAuth 2.0 refresh token
- `wahoo.access_token`: Active bearer access token

Non-secret configuration (`client_id`, `enabled`, `user_name` / `athlete_name`,
`expires_at`) is persisted via `ConfigManager`. All secrets are registered with
`Redactor::instance().registerSecret()` so tokens never appear in serial logs
or crash dumps.

**OAuth 2.0 Authorization & Token Exchange:**
1. **Authorize URL:** When the rider clicks "Connect with Wahoo", the portal
   fetches `GET /api/wahoo/auth-url`. `WahooUploader::buildAuthorizeUrl()`
   generates a 128-bit (32-hex) single-use CSRF state nonce with a 10-minute TTL
   (`STATE_TTL_MS = 600000u`) and returns:
   `https://api.wahooligan.com/oauth/authorize?client_id={client_id}&response_type=code&redirect_uri={redirect_uri}&scope=user_read+workouts_write&state={nonce}`
2. **Consent & Callback:** Wahoo redirects the browser back to
   `GET /api/wahoo/callback?code={code}&state={state}`.
3. **State Validation:** `WahooUploader::validateAndConsumeState()` verifies the
   state nonce and immediately consumes it to eliminate replay attacks.
4. **Token Exchange:** Echenon POSTs to `https://api.wahooligan.com/oauth/token`
   with `grant_type=authorization_code`, `code`, `client_id`, `client_secret`,
   and `redirect_uri`.
5. **Token Refresh & Rotated Refresh-Token-First Persistence:** When `now >= expires_at - 60`,
   `refreshToken()` exchanges the refresh token at `https://api.wahooligan.com/oauth/token`
   with `grant_type=refresh_token`. Wahoo rotates refresh tokens upon exchange.
   To guarantee crash consistency mid-refresh, the newly issued `refresh_token`
   is persisted to the secret store *before* the new `access_token` and metadata.
   Both new tokens are re-registered with `Redactor`.

**Multipart Activity Upload Endpoint:**
```
POST https://api.wahooligan.com/v1/workouts
Content-Type: multipart/form-data; boundary=----EchenonWahooBoundary{session_id}
Authorization: Bearer {access_token}
```

Multipart payload fields:
- `workout[workout_type_id]=1` (Cycling workout)
- `workout[file]`: filename `echenon_{session_id}.tcx.gz`, `Content-Type: application/gzip`
- Body: compressed binary payload of the session's `.tcx.gz` file

**Synchronous Status Handling:**
Unlike Strava (which returns an asynchronous upload ticket requiring polling),
Wahoo Cloud creates workouts synchronously. A successful `POST /v1/workouts`
returns HTTP 200 or 201 with the created workout object JSON containing `id` or `id_str`.
Therefore, `WahooUploader::pollUploadStatus()` completes immediately with `SUCCESS`
and maps the workout ID directly to `out_activity_id`.

**Stage Behaviour:**

| Stage | Wahoo Cloud Behaviour |
|---|---|
| `refreshToken()` | Checks token expiration (`now >= expires_at - 60`); POSTs to `https://api.wahooligan.com/oauth/token` if needed; persists rotated refresh token first |
| `postUpload()` | Reads session `.tcx.gz`; builds multipart request with `workout[workout_type_id]=1`; POSTs to `https://api.wahooligan.com/v1/workouts`; parses workout ID |
| `pollUploadStatus()` | Returns `SUCCESS` immediately with `out_activity_id = upload_id` (synchronous processing; no async polling needed) |

**Error Handling & Status Taxonomy:**

| HTTP Status | `UploadStageResult` | Description |
|---|---|---|
| 200 / 201 | `SUCCESS` | Workout created synchronously; workout ID captured |
| 401 | `PERMANENT_ERROR` | Unauthorized; invalid or revoked credentials/tokens |
| 429 | `RETRYABLE_ERROR` | Rate limited; backoff and retry according to queue policy |
| 5xx / <= 0 | `RETRYABLE_ERROR` | Server or network transport failure; exponential retry |
| 400 / 403 / 404 / 422 | `PERMANENT_ERROR` | Rejected payload or invalid external reference |

**REST API Routes (Wahoo Cloud Configuration):**

| Route | Method | Auth | Description |
|---|---|---|---|
| `/api/wahoo/status` | `GET` | ✓ | Connection status, athlete name, token expiry, enabled state |
| `/api/wahoo/credentials` | `POST` | ✓ | Save BYOA client ID and client secret (stored encrypted) |
| `/api/wahoo/auth-url` | `GET` | ✓ | Generate single-use CSRF state and return OAuth authorize URL |
| `/api/wahoo/callback` | `GET` | — | OAuth redirect callback receiving code and state |
| `/api/wahoo/disconnect` | `POST` | ✓ | Clear Wahoo Cloud tokens and disconnect |

### 10.4 Strava Integration (v1)

Strava uses a **bring-your-own-application (BYOA)** model: each rider creates
their own Strava API application, so no shared client secret ships in the
firmware.

**Setup and OAuth 2.0 flow:**

1. The rider creates a Strava API application and sets its **Authorization
   Callback Domain** to the host the browser uses to reach Echenon:
   `echenon.local`, or the device's IP address where mDNS does not resolve.
   Strava rejects a redirect whose host does not match this domain.
2. The rider enters the application's Client ID and Client Secret on the
   Upload Services tab. They are stored encrypted via `KeyManager`.
3. "Connect Strava" redirects the browser to Strava's consent page with
   `scope=activity:write`, `redirect_uri=http://<host>/api/strava/callback`
   and a **`state`** value: a random single-use nonce, stored with a short
   expiry.
4. Strava redirects back with an authorization code and `state`. The callback
   **rejects** any request whose `state` is missing, unknown, expired or
   already used.
5. Echenon exchanges the code for access and refresh tokens over HTTPS.
6. Tokens are stored encrypted via `KeyManager` and registered with
   `Redactor::instance().registerSecret()` so they never appear in logs.

**Token refresh.** Access tokens expire after 6 hours. The Refresh stage runs
before an upload whenever the access token has expired. Strava may **rotate**
the refresh token on each refresh; the new refresh token is persisted before
the new access token is used, so a reboot mid-refresh never leaves a stale
token. Every refresh **re-registers** the new tokens with the Redactor and
checks the returned `RegisterResult`. Redactor storage is append-only, so a
rejected registration (for example, capacity exhausted) is logged via
`Redactor::describe()` and surfaced as a portal warning.

**Upload execution (ADR-17).** The HTTPS upload takes 2–10 s, far beyond the
10 ms tick budget. `UploadQueue` runs it on the DataManager task:

```cpp
UpdateStatus UploadQueue::fetchUpdate(const FetchContext& ctx) {
    UploadJob* job = _jobs.front();
    if (!job || !_uploader->isConfigured()) return UpdateStatus::Skipped;

#ifdef WITH_BLE
    auto inhibit = _ble ? _ble->inhibitScanning("strava_upload")
                        : BLEManager::ScanInhibit{};
#endif

    switch (job->stage) {
        case UploadStage::Generate:
            return _tcx.generateChunk(*job, ctx.deadline);   // resumable
        case UploadStage::Refresh:
        case UploadStage::Upload:
        case UploadStage::Poll: {
            HttpRequestSpec req;
            if (!_uploader->buildRequest(job->stage, *job, req)) return UpdateStatus::ParseError;
            HttpResult res = _http.execute(*ctx.client, req, ctx.deadline);
            if (!res.ok) return UpdateStatus::FetchError;
            return toUpdateStatus(_uploader->handleResponse(job->stage, *job,
                                                            res.status, res.body, res.len));
        }
        default:
            return UpdateStatus::Skipped;
    }
}
```

This gives the upload:

- **Bounded transport**: the framework's timeouts (5 s connect, 15 s total)
  stop a hung connection from tripping the TWDT
- **Health state machine**: failed uploads back off exponentially rather than
  retrying in a tight loop
- **TWDT safety**: DataManager unsubscribes the fetch task from the watchdog
- **Diagnostic visibility**: upload health appears in the portal Status tab,
  the MCP `get_data_sources` tool and the EventBus (`data.source.health`)

### 10.5 Downstream Platform Sync — Provider Tier Taxonomy

Not every external platform requires a direct cloud uploader on the ESP32.
The design distinguishes four integration tiers:

#### Tier A — Downstream Platform Sync (No ESP32 Code Required)

Some platforms ingest data from Strava automatically and need no direct
Echenon integration:

| Platform | Sync Method | Setup |
|---|---|---|
| **Xert** | Automatic Strava webhook | Connect Xert to Strava once in Xert Online → Sync settings |
| **Relive** | Automatic Strava sync | Connect in Relive settings |
| **VeloViewer** | Automatic Strava sync | Connect in VeloViewer settings |

When Echenon uploads a ride to Strava, Xert pulls the full 1 Hz power
stream, runs its MPA model, checks for breakthroughs and updates the
athlete's fitness signature — with zero Xert-specific code on the ESP32.

Additionally, Xert captures rides live when the rider uses the Xert EBC
app on the tablet: the app connects to Echenon's BLE FTMS service
(`0x1826`, `0x2ACC`, `0x2AD2` at 2 Hz) and records the session directly,
syncing to Xert Online on completion. This means Xert has **two
independent ingest paths** — live BLE FTMS capture and Strava webhook
sync — neither requiring a dedicated `IUploader` implementation.

#### Tier B — Direct ESP32 Cloud Uploaders (On-Device `IUploader`)

These services benefit from direct upload because the rider may not
always have the tablet open (console-off standalone rides, quick free
rides), the session recording in `RideStore` captures the full 1 Hz data
stream with internal resistance, HR, and calibrated power, and auto-upload
on session completion provides a "ride and forget" experience.

| Service | Auth Type | Status | Notes |
|---|---|---|---|
| **Strava** | OAuth 2.0 (BYOA) | **Core (live)** | Reference OAuth sink (§10.4) |
| **Intervals.icu** | REST API key | **Core (live)** | Reference API-key sink (§10.3a) |
| **Wahoo Cloud** | OAuth 2.0 (BYOA) | **Core (live)** | Wahoo Cloud OAuth sink (§10.3b) |

#### Tier C — Future / Manual Export

| Service | Status | Reason |
|---|---|---|
| **Garmin Connect** | Manual TCX export | Garmin Health API requires corporate enterprise partner approval; manual upload via Stored Rides pill works seamlessly |
| **Generic Webhook** | Future PR | HTTP POST multipart/TCX for Home Assistant / NAS / InfluxDB integrations |

#### Tier D — BLE FTMS Clients (Not Upload Targets)

Zwift, Xert EBC, TrainerRoad, and SYSTM connect to Echenon's BLE FTMS
service as real-time clients. They originate their own activity records.
Echenon does not upload to them.

---

## 11. Session UI — Ride Page & Portal Tabs

Echenon's web UI is a **two-surface model**:

| Surface | URL | Purpose | When |
|---|---|---|---|
| **Ride page** | `echenon.local/ride` | Daily-use rider control surface: live metrics, session controls, ride history | Every ride — PWA or browser |
| **Portal** | `echenon.local` | Configuration, setup, diagnostics | Initial setup and occasional changes |

The ride page is where the rider spends time; the portal is where
configuration happens. A ⚙️ gear icon on the ride page links to the portal;
the portal's Echenon tab links back with an "Open Ride Page →" button.

### 11.1 Ride Page

The ride page is a standalone, lightweight training-computer UI served at
`http://echenon.local/ride` (or the device IP). It is independent of FTMS and
upload: it does not know what consumes the BLE data or where sessions go. It is
a live monitor and session controller.

It is designed to run in two primary contexts:

- **Split-screen** (280–430 px): Android's freeform or split-screen mode;
  Chrome alongside the Echelon app, Zwift, YouTube or nothing
- **Full phone screen**: standalone PWA via "Add to Home Screen"

For networks where mDNS does not resolve, the IP address is printed in the
serial log, shown as a QR code and shown on the AP captive page. PR-01 verifies
that `echenon.local` resolves in Chrome on the P12.

**Features:**

- Live metrics via WebSocket at 2 Hz (`/ws/echenon/live`)
- Session controls (start / pause / resume / stop) via REST API
- Ride history with TCX download
- Upload queue mini-status from `/api/strava/status`
- ⚙️ gear icon → portal
- PWA manifest (`display: standalone`, dark theme)
- Dark theme, responsive 280–430 px layout
- Flash budget: ≤ 3 KB gzipped

**Live Ride View.** Updated in real time via WebSocket:

```
┌──────────────────────────────────────────────┐
│  ● REC  00:32:15             [■ Stop]       │
│                                              │
│  ⚡ 187W        🔄 82 rpm        ❤️ 145 bpm │
│  ◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼◼░░░░░░░░░░░░ 18/32      │
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
- Start / Stop button (manual override of auto start/stop)
- Live power, cadence and heart rate, large and glanceable
- Resistance bar on the active display scale (32 levels in Intercept and on
  the passive board; `[DR-2]` levels in Override (v2))
- ERG target and state indicator (`ACTIVE` / `SUSPENDED_BY_RIDER`) when the
  Control Point is active
- Rolling 5-minute power chart
- HR zone indicator (Z1–Z5, colour-coded)
- Running averages and peaks

**Post-Ride Summary.** Shown after a session ends:

- Duration and estimated distance
- Average and maximum power, cadence and heart rate
- Time-in-zone chart (HR and/or power zones)
- Power curve (best 5 s, 30 s, 1 min, 5 min and 20 min averages)
- Upload status (uploaded ✓ / pending / failed)
- "Discard" (delete the session without uploading; authenticated)

### 11.2 Portal Tabs

The portal at `echenon.local` uses a **6-tab bottom navigation** layout. The
default App tab is disabled (`app.disableDefaultAppTab()`).

| Tab | Order | Content |
|---|---|---|
| **WiFi** | 10 | Framework WiFi management |
| **Bluetooth** | 15 | Framework BLE pairing and status |
| **Echenon** | 50 | Device status summary, Stored Rides, "Open Ride Page →" button |
| **Apps** | 60 | Per-sink configuration pills (see below) |
| **Security** | — | Framework authentication settings |
| **System** | — | Framework system info, OTA, diagnostics; event log viewer (PR-14) |

**Echenon tab (order 50):** Shows device connection status, sensor readings,
a prominent "Open Ride Page →" button that links to `/ride`, and the
**Stored Rides** card:

```
┌─────────────────────────────────────────────────────────┐
│  Echenon                                                │
│                                                         │
│  ┌─ Live Metrics ─────────────────────────────────────┐ │
│  │  Power: 187W  Cadence: 82 rpm  HR: 145 bpm        │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
│  ┌─ Stored Rides ─────────────────────────────────────┐ │
│  │  Oct 28, 2026 18:32  │  42 min  │  178W avg       │ │
│  │    Strava: ✅ Synced  Intervals: ✅ Synced         │ │
│  │    [Download TCX]  [Delete]  [Force Push ▾]        │ │
│  │──────────────────────────────────────────────────── │ │
│  │  Oct 27, 2026 07:15  │  35 min  │  162W avg       │ │
│  │    Strava: ✅ Synced  Intervals: ⏳ Pending        │ │
│  │    [Download TCX]  [Delete]  [Force Push ▾]        │ │
│  └────────────────────────────────────────────────────┘ │
│                                                         │
│  [ Open Ride Page → ]                                   │
└─────────────────────────────────────────────────────────┘
```

The Stored Rides card fetches data from `GET /api/session/history`
(extended with per-sink sync status) and `GET /api/echenon/runs`. Each
ride shows date/time, duration, average power, per-sink sync badges
with clickable links, and one-click `[Download TCX]`, `[Delete]`, and
per-sink `[Force Push / Retry]` actions.

**Apps tab (order 60):** Uses a PillBar for sub-navigation with per-sink
configuration pills and system pills:

| Pill | Order | Content |
|---|---|---|
| **Strava** | 10 | OAuth 2.0 configuration: connect/disconnect, auto-sync toggle, upload status |
| **Intervals.icu** | 20 | API key configuration: athlete ID, API key form, test connection, auto-sync toggle |
| **Wahoo Cloud** | 30 | OAuth 2.0 configuration: connect/disconnect, auto-sync toggle, upload status (`/wahoo_settings.js`) |
| **Level Mapping** | 25 | Calibration wizard (moved from Calibration tab) |
| **Power Tuning** | 30 | Recalibration (moved from Recalibration tab) |
| **Diagnostics** | 35 | Event log viewer (PR-14) |

Future sinks are registered at order 35+ (before Level Mapping) to keep
connected-apps pills grouped at the top.

**REST API extensions (PR-19 / PR-21):**

| Route | Method | Auth | Description |
|---|---|---|---|
| `/api/upload/sinks` | `GET` | ✓ | List all registered sinks with connection status, enabled state, and pending job counts |
| `/api/upload/status` | `GET` | ✓ | Aggregate upload health: total pending, last error, per-sink summary |
| `/api/session/upload` | `POST` | ✓ | Force-push / retry: `{session_id, service_id}` for single-sink or `{session_id}` for all sinks |
| `/api/session/history` | `GET` | ✓ | Session list with per-sink sync status array: `[{service_id, status, remote_id, remote_url, error}]` |
| `/api/intervals/status` | `GET` | ✓ | Intervals.icu connection status |
| `/api/intervals/credentials` | `POST` | ✓ | Save Intervals.icu athlete ID and API key |
| `/api/intervals/test` | `POST` | ✓ | Test Intervals.icu connection |
| `/api/intervals/disconnect` | `POST` | ✓ | Clear Intervals.icu credentials |
| `/api/wahoo/status` | `GET` | ✓ | Wahoo Cloud connection status, athlete name, expiry, auto-sync |
| `/api/wahoo/credentials` | `POST` | ✓ | Save Wahoo Cloud Client ID and write-only Client Secret |
| `/api/wahoo/auth-url` | `GET` | ✓ | Generate single-use CSRF state and return OAuth authorize URL |
| `/api/wahoo/callback` | `GET` | — | OAuth redirect callback receiving code and state |
| `/api/wahoo/disconnect` | `POST` | ✓ | Clear Wahoo Cloud tokens and disconnect |

**Removed from top-level tabs:** The default App tab, Calibration,
Recalibration, Session (history), and Upload Services are no longer
top-level tabs. Session history is accessible from the Stored Rides card
in the Echenon tab. Calibration and upload configuration are consolidated
under the Apps tab.

Signal-logger content is compiled behind `WITH_SIGNAL_LOGGER` and is not
visible in production builds.

### 11.3 Technical Implementation

**Asset delivery: gzip + PROGMEM (canonical).** All consumer web assets use
the framework's gzip + PROGMEM pipeline. Sources live in `web/`;
`portalBuilder.py` compiles them at build time into gzip-compressed `PROGMEM`
byte arrays in `include/portal_assets/`, served by `serveGzipAsset()` with no
DRAM cost and ETag caching.

> `data/www/` is reserved for the framework's SPA skeleton (`index.html`,
> `app.js`). Echenon places no files there.

```cpp
// SessionUI.cpp — registerRoutes()
#include "utils/serveGzipAsset.hpp"
#include "portal_assets/ride_js.hpp"      // from web/ride.js
#include "portal_assets/ride_html.hpp"    // from web/ride.html

void SessionUI::registerRoutes(PsychicHttpServer& server) {
    // Standalone ride page (not a portal tab)
    server.on("/ride", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
        return serveGzipAsset(req, res,
            ride_html_gz, ride_html_gz_len, ride_html_gz_etag,
            "text/html");
    });
    server.on("/ride.js", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
        return serveGzipAsset(req, res,
            ride_js_gz, ride_js_gz_len, ride_js_gz_etag,
            "application/javascript");
    });

    // REST API (mutations require authentication, §11.4)
    server.on("/api/session/start",   HTTP_POST, ...);
    server.on("/api/session/stop",    HTTP_POST, ...);
    server.on("/api/session/status",  HTTP_GET,  ...);
    server.on("/api/session/history", HTTP_GET,  ...);

    // WebSocket for the live 2 Hz push
    _ws = server.websocket("/ws/session");
    _ws->onOpen([this](PsychicWebSocketClient* client) { ... });
}

void SessionUI::registerTabs(ITabRegistry& registry) {
    // Session History tab in the main portal
    registry.addTab("Session", nullptr, 60, kSessionIcon, "/session.js");
}
```

**Real-time data.** WebSocket push at 2 Hz on the PsychicHttp endpoint
`/ws/session`; the ride page script opens the socket on page load.

**Charts.** Lightweight `<canvas>` drawing, with no Chart.js or similar
library, so the ride page loads quickly in a small window.

**DRAM.** WebSocket frame buffers consume DRAM. The ride page's larger data
structures (rolling chart history, session summary) are allocated in PSRAM to
keep DRAM free for the WiFi and BLE stacks:

```cpp
// Chart history in PSRAM
_chartBuffer = (PowerPoint*)ps_malloc(CHART_POINTS * sizeof(PowerPoint));
```

The WebSocket JSON payloads (~100 bytes per update) stay in DRAM. The
framework's `DeepStackExecutor` is not needed; ride page requests are
lightweight.

### 11.4 Trust Boundaries

- **Read-only live data** (ride page, `/ws/session`, `/api/session/status`):
  no authentication.
- **Mutations, downloads and motor commands** (session start/stop/discard,
  calibration, uploader credentials, `GET /api/session/download`,
  `POST /api/resistance`): require `AuthManager` authentication.
- **Diagnostics read-only** (`GET /api/echenon/events`,
  `GET /api/echenon/runs`): require authentication for data privacy (event
  timestamps and ride data reveal usage patterns).
- **Diagnostics downloads** (`GET /api/echenon/runs/<id>.csv.gz`): require
  authentication (ride data contains health data such as heart rate).
- **Diagnostics mutations** (`DELETE /api/echenon/runs/<id>`): require
  authentication.
- **v2: `POST /api/resistance`** is accepted only in Override Mode on the MITM
  board; it returns `409` otherwise.
- **v2: BLE Control Point**: any Central in range can Request Control. This is an
  accepted, documented risk.

---

## 12. Build System

### 12.1 Project Structure

```
Echenon/
├── .gitmodules
├── platformio.ini
├── sdkconfig.defaults                   project-level SDK defaults (§12.2)
├── framework/                           ESP32 App Foundation (git submodule)
├── docs/
│   ├── ex3_specification.md             EX-3 stock hardware reference
│   ├── hardware_specification.md        Phase-by-phase circuit build guide
│   ├── implementation-strategy.md       Development rationale
│   ├── use_case_analysis.md             Use cases UC1–UC6
│   ├── diagnostics.md                   DiagnosticsModule developer guide (PR-14)
│   ├── run-data-persistence.md          Run-data persistence developer guide (PR-14)
│   ├── measurements/phase0/README.md    Phase 0 register and evidence
│   └── echelon-teardown/                Teardown photos and notes
├── hardware/
│   └── ex3-waveshare-nano/              Target: EX-3 on Waveshare S3-Nano
│       ├── board.h                      Pin map, HAS_* bike signals, identity
│       ├── partitions.csv               Flash partition table
│       └── sdkconfig.defaults           Target SDK overrides
├── src/
│   ├── main.cpp
│   └── echenon/
│       ├── EchenonModule.h / .cpp
│       ├── SignalLogger.h / .cpp        PR-01
│       ├── CadenceSensor.h / .cpp       PR-02
│       ├── ResistanceSensor.h / .cpp    PR-02
│       ├── PowerEngine.h / .cpp         PR-02
│       ├── CalibrationWizard.h / .cpp   PR-02 (motor sweep added in PR-07)
│       ├── HRMClient.h / .cpp           PR-03
│       ├── HRMRepeater.h / .cpp         PR-03
│       ├── FTMSServer.h / .cpp          PR-05 (targets in PR-08)
│       ├── SessionManager.h / .cpp      PR-06
│       ├── RideStore.h / .cpp           PR-06
│       ├── TCXGenerator.h / .cpp        PR-06
│       ├── IUploader.h                  PR-06
│       ├── UploadQueue.h / .cpp         PR-06
│       ├── StravaUploader.h / .cpp      PR-06
│       ├── SessionUI.h / .cpp          PR-06 / PR-09
│       ├── DiagnosticsModule.h / .cpp  PR-14
│       ├── RidePersistence.h / .cpp    PR-14
│       ├── DiagnosticsRoutes.h / .cpp  PR-14
│       ├── InterceptController.h / .cpp PR-07
│       ├── ResistanceMotor.h / .cpp     PR-07
│       └── ERGController.h / .cpp       PR-08
├── test/
│   └── test_native/                     googletest suites for pure-logic modules (§15.1)
├── web/                                 gzip + PROGMEM pipeline sources
│   ├── ride.html                        → portal_assets/ride_html.hpp
│   ├── ride.js                          → portal_assets/ride_js.hpp
│   ├── ride.css                         → portal_assets/ride_css.hpp
│   ├── session.js                       → portal_assets/session_js.hpp
│   ├── calibration.js                   → portal_assets/calibration_js.hpp
│   ├── upload_settings.js               → portal_assets/upload_settings_js.hpp
│   └── manifest.json                    → portal_assets/manifest_json.hpp (PWA)
├── include/
│   └── portal_assets/                   generated by portalBuilder.py
├── SPEC.md
├── README.md
└── LICENSE
```

**Build pipeline.** `portalBuilder.py` scans `web/`, gzip-compresses each
file and emits `include/portal_assets/<name>_<ext>.hpp` headers with `PROGMEM`
byte arrays and an ETag. `registerRoutes()` serves them with
`serveGzipAsset()`. `data/www/` is reserved for the framework's SPA skeleton.

### 12.2 platformio.ini

Echenon follows the consumer pattern of `framework/template-app/platformio.ini`:
it declares its own base environment, with every `../` path of the template
rewritten to `framework/`. It does **not** `extends` a framework environment:
the framework's `env:esp32s3nano` lives in `framework/platformio.ini`, is not
visible to Echenon's project file, and carries `-D WITH_DISPLAY_U8G2`.

```ini
[platformio]
default_envs = ex3-waveshare-nano

; ── Echenon base env (pattern: framework/template-app/platformio.ini) ──
[env:echenon_base]
platform = espressif32
framework = arduino, espidf
monitor_speed = 115200
upload_speed = 921600
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_ASYNC_TCP_RUNNING_CORE=1
    -DCONFIG_ASYNC_TCP_STACK_SIZE=4096
    -DSECURITY_VERSION=1
    -std=gnu++17
    -I framework/include
    -I framework/hardware
    -I framework/hardware/_common
    -I framework/modules
    -I framework
    -I .
build_unflags = -std=gnu++11
lib_ldf_mode = deep+
lib_compat_mode = off
extra_scripts =
    pre:framework/scripts/sync_sdkconfig.py
    pre:framework/scripts/inject_version.py
    pre:framework/scripts/merge_framework_data.py
    pre:framework/scripts/portalBuilder.py
lib_extra_dirs =
    framework/modules
    framework/lib
lib_deps =
    Application
    PreconditionEngine
    bblanchon/ArduinoJson@^7.4.3
    hoeken/PsychicHttp@^3.0.0

; ── BLE mix-in (declared locally; matches framework/platformio.ini [ble]) ──
[ble]
build_flags = -D WITH_BLE
lib_deps =
    h2zero/esp-nimble-cpp@^2.1.0
    BLEManager

; ── Target: EX-3 on Waveshare ESP32-S3-Nano ──
[env:ex3-waveshare-nano]
extends = env:echenon_base
board = um_nanos3
board_build.partitions = hardware/ex3-waveshare-nano/partitions.csv
board_build.filesystem = littlefs
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.cmake_extra_args =
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"
build_flags =
    ${env:echenon_base.build_flags}
    ${ble.build_flags}
    -DBOARD_ID=\"ex3-waveshare-nano\"
    -DBOARD_HEADER=\"hardware/ex3-waveshare-nano/board.h\"
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -D WITH_OTA            ; from PR-04a
    -D WITH_HR_RELAY       ; from PR-03
    -D WITH_FTMS           ; from PR-05
    -D WITH_SESSION        ; from PR-06
    -D WITH_STRAVA         ; from PR-06
lib_deps =
    ${env:echenon_base.lib_deps}
    ${ble.lib_deps}

; ── Host-side unit tests (pattern: framework/template-app env:native) ──
[env:native]
platform = native
lib_compat_mode = off
lib_ldf_mode = chain
test_framework = googletest
build_flags =
    -std=c++17
    -DUNIT_TEST
    -I framework/include
    -I framework/hardware
    -I framework/hardware/_common
    -I framework
    -I framework/test/stubs/platform
    -I src
    -DBUILD_VERSION=\"0.0.0-native\"
    -DVERSION_MAJOR=0
    -DVERSION_MINOR=0
    -DVERSION_PATCH=0
    -DVERSION_CHANNEL=3
    -DVERSION_PRE_NUM=0
    -DBOARD_ID=\"native\"
    -DBOARD_HEADER=\"framework/hardware/native/board.h\"
    -DSECURITY_VERSION=1
lib_deps =
    bblanchon/ArduinoJson@^7.4.3
    Application
    DeviceCrypto
lib_extra_dirs =
    framework/modules
    framework/lib
lib_ignore = U8G2Display, LVGLDisplay, OTAUpdateManager
```

- **Phase-dependent flags.** PR-01 builds this env without `${ble.*}`,
  without `sdkconfig.ble.defaults` in the chain and without the
  application `WITH_*` flags. Each flag is added by the PR shown in its
  comment. `WITH_ENCODER`, `WITH_MOTOR_CONTROL` and `WITH_ERG` are added by
  PR-07 and PR-08 in a MITM build.
- **SDK config chain.** The project-root `sdkconfig.defaults` holds the
  project-level defaults. The framework's `sync_sdkconfig.py` pre-script keeps
  it in step with the framework root file, as it does for the template app.
  The framework BLE defaults follow, and the target's own
  `sdkconfig.defaults` comes last, so the target's octal-PSRAM, 16 MB flash,
  USB-console and NimBLE slot settings win.
- **Verification.** PR-01 WO1 proves the env builds against the bumped
  submodule, including that `-I .` resolves `BOARD_HEADER` and that the
  pre-scripts run correctly from the project root.

### 12.3 board.h and Build Flags

**Flag families:**

| Family | Meaning | Where defined | Examples |
|---|---|---|---|
| `HAS_*` | This bike signal exists and is wired to a pin | `board.h` | `HAS_CADENCE`, `HAS_RESISTANCE_POT`, `HAS_MOTOR_SENSE`, `HAS_KNOB_ENCODER`, `HAS_MOTOR_CONTROL` |
| `WITH_*` (framework) | Compile this framework module | `platformio.ini` | `WITH_BLE`, `WITH_OTA`, `WITH_ENCODER` |
| `WITH_*` (application) | Compile this Echenon module | `platformio.ini` | `WITH_HR_RELAY`, `WITH_FTMS`, `WITH_SESSION`, `WITH_STRAVA`, `WITH_MOTOR_CONTROL`, `WITH_ERG` |

**Forbidden flags.** The Echenon env **never** defines `WITH_DISPLAY_U8G2` or
`WITH_BUTTONS`:

- Echenon has no display, and under `WITH_DISPLAY_U8G2` the framework's
  display driver drives its CS and DC pins as SPI outputs. The reference
  board puts display DC on GPIO 9 and CS on GPIO 10, which are the encoder
  Ch B and encoder-VCC taps; driving them would push signals into the
  console's encoder loom and break the passive premise.
- Under `WITH_BUTTONS` the reference board configures a mode button on GPIO 8
  with `INPUT_PULLUP`, which would bias the encoder Ch A line.
- `board.h` defines no display or button pin blocks, so if either flag were
  ever set, the framework's board contract would fail the build rather than
  drive a tap pin.

**`board.h` (Phase 0/1 map, §4.4.1)**, authored from
`framework/hardware/_TEMPLATE/board.h` and checked by
`framework/hardware/_common/board_contract.h`:

```cpp
// hardware/ex3-waveshare-nano/board.h
#pragma once
#include "board_pins.h"

// ── Identity & capabilities (framework template fields) ──
#define BOARD_NAME        "EX-3 on Waveshare ESP32-S3-Nano"
#define BOARD_ID_STRING   "ex3-waveshare-nano"
constexpr bool HAS_PSRAM = true;

// ── Bike signals (HAS_*) and pins: Phase 0/1 map, SPEC §4.4.1 ──
// Every tap: series resistor, INPUT with no internal pull-up or pull-down.
#define HAS_RESISTANCE_POT   1
constexpr int8_t PIN_POT_WIPER        = 1;   // A0  Blue,   ADC1
constexpr int8_t PIN_POT_HI           = 2;   // A1  Yellow, ADC1
constexpr int8_t PIN_POT_LO           = 3;   // A2  Green,  ADC1 (strapping pin, §4.4.1)

#define HAS_CADENCE          1
constexpr int8_t PIN_CADENCE_RETURN   = 4;   // A3  Grey,   ADC1
constexpr int8_t PIN_CADENCE          = 5;   // D2  Orange, ADC1 / interrupt

#define HAS_MOTOR_SENSE      1
constexpr int8_t PIN_MOTOR_SENSE_P    = 6;   // D3  Red,   ADC1 via ÷4 divider
constexpr int8_t PIN_MOTOR_SENSE_N    = 7;   // D4  Black, ADC1 via ÷4 divider

#define HAS_KNOB_ENCODER     1
constexpr int8_t PIN_KNOB_ENC_A       = 8;   // D5  Black,  ADC1 (J3 left pin)
constexpr int8_t PIN_KNOB_ENC_B       = 9;   // D6  Red,    ADC1 (J3 right pin)
constexpr int8_t PIN_KNOB_ENC_VCC     = 10;  // D7  Orange, ADC1 (J3 centre pin, monitor; kept on v1)

#define HAS_MOTOR_CONTROL    0               // v2 active board only (Phase 5+)
// GPIO 43/44 (UART0) reserved for a future ANT+ add-on: never assigned (§4.4.2)

// No display pin block and no button block: WITH_DISPLAY_U8G2 and
// WITH_BUTTONS are forbidden for this target (§12.3).
```

**v1 passive board (DR-1).** DR-1 keeps all ten taps on the v1 board, so the
v1 configuration uses this `board.h` unchanged: `HAS_RESISTANCE_POT`,
`HAS_CADENCE`, `HAS_MOTOR_SENSE` and `HAS_KNOB_ENCODER` are `1`,
`HAS_MOTOR_CONTROL` is `0`, and no output pin is defined. The production env
adds `WITH_ENCODER` and the `EncoderModule` descriptor (in `board.h` since
PR-01 WO4a):

```cpp
#ifdef WITH_ENCODER
#include "EncoderModule/EncoderConfig.h"
constexpr BoardEncoder BOARD_ENCODERS[] = {
    { .id = "knob", .pinA = PIN_KNOB_ENC_A, .pinB = PIN_KNOB_ENC_B,
      .stepsPerDetent = 4,                       // Confirmed in B04
      .reversed = false,
      .accel = { .enabled = false } },
};
constexpr size_t BOARD_ENCODER_COUNT = sizeof(BOARD_ENCODERS) / sizeof(BOARD_ENCODERS[0]);
#define BOARD_ENCODER_COUNT BOARD_ENCODER_COUNT
#endif
```

**v2 revision `[DR-2]`.** A MITM build sets `HAS_MOTOR_CONTROL 1` with the
driver and encoder-emulation pins decided at DR-2 (draft: driver IN1/IN2 on
GPIO 10/11, which collides with the v1 encoder-VCC monitor on GPIO 10). DR-2
also decides which v1 monitor taps (`PIN_POT_HI`, `PIN_POT_LO`,
`PIN_CADENCE_RETURN`, `PIN_KNOB_ENC_VCC`) the active board keeps.

The pin names `PIN_KNOB_ENC_A` / `PIN_KNOB_ENC_B` deliberately avoid the
framework contract's `PIN_ENCODER_A` / `PIN_ENCODER_B`, which belong to the
LVGL display contract.

---

## 13. Diagnostics

Diagnostics is a production subsystem that records structured device events in a
bounded PSRAM log and exposes them through a paged REST API. It is **separate
from `SignalLogger`** (which remains behind `WITH_SIGNAL_LOGGER` and is not
compiled in production builds). Diagnostics is always available; no build flag is
required.

### 13.1 DiagnosticsModule Overview

`DiagnosticsModule` (`src/echenon/DiagnosticsModule.h/.cpp`) implements
`ITickable` and records structured diagnostic events in a fixed-capacity PSRAM
ring buffer. Its purpose is to provide a lightweight, always-on audit trail for
production devices: boot events, session lifecycle, safety alerts,
calibration milestones and firmware changes.

- **Separation from SignalLogger.** `SignalLogger` is a Phase 0 development
  tool that captures raw ADC data at high frequency across all channels. It is
  guarded by `WITH_SIGNAL_LOGGER` and is not compiled in production builds.
  `DiagnosticsModule` records high-level structured events at low volume and
  is always compiled.
- **Separation from the Audit module.** The framework's optional `AuditModule`
  (`WITH_AUDIT`) provides a general-purpose flash-backed audit trail with rate
  limiting and token-bucket flushing. `DiagnosticsModule` is Echenon-specific,
  PSRAM-backed, and focused on the bike's operational lifecycle.

### 13.2 Diagnostic Event Types

Six event types are defined by `DiagEventType`:

| Type | Value | Payload encoding (3 bytes) | Source |
|---|---|---|---|
| `BOOT` | `0` | 24-bit little-endian `uptime_s` (max 16,777,215 s) | Boot sequence |
| `SESSION_START` | `1` | Low 16-bit LE `session_id`; `payload[2]` unused | `SessionManager` |
| `SESSION_END` | `2` | 16-bit LE `duration_s` in `[0..1]`; `payload[2]` = `record_count / 64` (coarse granularity) | `SessionManager` |
| `RUNAWAY_ALERT` | `3` | 16-bit LE `duration_ms / 100` in `[0..1]` (100 ms resolution); `payload[2]` unused | Runaway detection (§5.2) |
| `CALIBRATION_DONE` | `4` | `payload[0]` = calibration type (`0` = initial, `1` = recalibration); `[1..2]` unused | `CalibrationWizard` |
| `FIRMWARE_CHANGE` | `5` | `payload[0..2]` = major, minor, patch | Boot after OTA |

Each event is 8 bytes: 4-byte UTC epoch + 1-byte type discriminator + 3-byte
payload, packed with `__attribute__((packed))` and verified at compile time
with `static_assert(sizeof(DiagnosticEvent) == 8)`.

### 13.3 Event Log Ring Buffer

- **Allocation:** PSRAM via `heap_caps_calloc(EVENT_LOG_CAPACITY,
  sizeof(DiagnosticEvent), MALLOC_CAP_SPIRAM)` with fallback to internal heap
  if PSRAM is unavailable.
- **Capacity:** 100 entries (`EVENT_LOG_CAPACITY = 100`), consuming
  `100 × 8 = 800 bytes`.
- **Wrap semantics:** when the buffer is full, the oldest event is overwritten
  (classic ring buffer). `_head` points to the next write position; `_count`
  tracks the number of stored events (clamped to capacity).
- **Index convention:** `getEvent(0)` returns the oldest event. When the buffer
  is not full, oldest is at position 0. When full, oldest is at `_head` (the
  slot that was just overwritten).
- **Clearing:** `clearEvents()` resets `_head` and `_count` to zero and
  zero-fills the buffer.
- **Memory budget:** 800 bytes of PSRAM (well within the 4 KB budget for
  diagnostics data).

### 13.4 Query API

#### EventQuery Struct

```cpp
struct EventQuery {
    uint32_t from_epoch = 0;            // inclusive lower bound (0 = no filter)
    uint32_t to_epoch   = UINT32_MAX;   // inclusive upper bound
    size_t   limit      = 100;          // max results
};
```

`DiagnosticsModule::queryEvents(const EventQuery& query, DiagnosticEvent* out,
size_t out_capacity)` iterates over stored events in oldest-first order and
copies those whose `epoch_s` falls within `[from_epoch, to_epoch]` into the
output buffer, stopping at `min(limit, out_capacity)` results.

#### REST Endpoints

All diagnostics endpoints are registered by `DiagnosticsRoutes::registerRoutes()`
and require `AuthManager` authentication (§11.4).

**`GET /api/echenon/events`** — Diagnostic event log.

| Parameter | Type | Default | Description |
|---|---|---|---|
| `from` | `uint32_t` | `0` | Inclusive lower epoch bound |
| `to` | `uint32_t` | `UINT32_MAX` | Inclusive upper epoch bound |
| `limit` | `size_t` | `100` | Maximum events to return (capped at 100) |

Response (chunked JSON streaming):

```json
{
  "events": [
    {
      "epoch": 1698400000,
      "type": "BOOT",
      "type_id": 0,
      "payload": {"uptime_s": 12345}
    }
  ],
  "count": 1,
  "total": 42
}
```

**`GET /api/echenon/runs`** — Stored ride listing.

Response:

```json
{
  "runs": [
    {
      "session_id": 12345,
      "start_epoch": 1698400000,
      "file_size_bytes": 4096,
      "record_count": 512
    }
  ],
  "count": 1,
  "total_size_kb": 4
}
```

**`GET /api/echenon/runs/<id>.csv.gz`** — Streamed CSV download.

Downloads the stored ride as CSV (chunked transfer encoding, ≤ 4 KB chunks).
CSV columns: `elapsed_s,power_w,cadence_rpm,hr_bpm,resistance,flags`.
Content-Disposition header triggers a file download as `ride_<id>.csv`.

**`DELETE /api/echenon/runs/<id>`** — Delete a stored ride.

Response: `{"ok": true}` on success; `404` if the ride is not found.

---

## 14. Run-Data Persistence

`RidePersistence` (`src/echenon/RidePersistence.h/.cpp`) provides optional
persistent storage of completed rides to LittleFS. When enabled, it saves
each completed ride as a compressed binary file and manages storage within a
configurable capacity budget with oldest-first eviction.

### 14.1 Storage Format

Each ride is stored as a `.ride.gz` file containing gzip-compressed binary
data (gzip on ESP32; uncompressed under `UNIT_TEST`):

| Segment | Size | Content |
|---|---|---|
| `RideFileHeader` | 16 bytes | Magic (`0x45434852` = "ECHR"), version (`1`), flags (NTP valid, finalized), `session_id`, `start_epoch_s` |
| `RideRecord[]` | 8 bytes each | Packed ride data records (§9.3): `elapsed_sec`, `power_watts`, `cadence_rpm`, `heart_rate_bpm`, `internal_resistance`, `flags` |

`RideFileHeader` is packed and compile-time verified:
`static_assert(sizeof(RideFileHeader) == 16)`.

### 14.2 Configuration

| Setting | Type | Default | Range | Description |
|---|---|---|---|---|
| `save_rides_to_flash` | `bool` | `false` | — | Enable or disable ride persistence to LittleFS |
| `diag_ride_cap_kb` | `uint32_t` | `512` | `64–2048` | Maximum total storage budget for persisted rides (KB) |

Both settings are fields of the `echenon` configuration namespace and are
persisted by `ConfigManager`.

### 14.3 Eviction Policy

- **Oldest-first eviction.** When `saveRide()` would exceed the configured
  `diag_ride_cap_kb` cap, the oldest stored ride is deleted before writing the
  new one. Eviction repeats until sufficient space is available.
- **Flash wear mitigation.**
  - Block-append writes: ride data is written in a single sequential pass,
    not updated in place.
  - Minimum checkpoint interval: ≥ 30 s between LittleFS writes (consistent
    with the `RideStore` crash-recovery checkpoint interval, §9.2).
  - Files are written once and only deleted during eviction or explicit user
    action.

### 14.4 File Layout

```
/rides/persist/
├── ride_12345.ride.gz
├── ride_12346.ride.gz
└── ride_12347.ride.gz
```

The directory `/rides/persist/` is separate from the session working directory
(`/rides/`) used by `RideStore` for active session checkpoints and the
completed-ride FIFO. `RidePersistence::begin()` scans this directory on boot
to rebuild its stored-ride inventory.

---

## 15. Testing & Verification

Tests are organised by delivery phase (§1.5), one subsection per PR plus DR-1,
and grouped by release:

| Release | Subsections |
|---|---|
| **v1 "Passive"** | §15.1 (v1 rows), §15.2 PR-01, §15.3 DR-1, §15.4 PR-02, §15.5 PR-03, §15.6 PR-04a / PR-05, §15.7 PR-06, §15.10 PR-09, §15.11 v1 limitations |
| **v2 "Active"** (provisional) | §15.8 PR-07, §15.9 PR-08, §15.12 PR-13. Tests for DR-2, PR-10, PR-11 and PR-12 are written after DR-2 |

Test IDs use the phase number. Use-case tests derive from
[`docs/use_case_analysis.md`](docs/use_case_analysis.md), with pin numbers,
level scales and mode names updated to this specification. Every PR also
passes the native unit tests for the modules it touches (§15.1).

### 15.1 Native Unit Tests (All PRs)

Every pure-logic module has googletest suites that run on the host with
`pio test -e native`, following the framework's `env:native` pattern. Hardware
access sits behind thin interfaces, so the logic is testable without a board.

| Module / logic | Minimum coverage | PR |
|---|---|---|
| `SignalLogger` & `SignalLogic` (24 tests) | `SignalLoggerDecimationTest` (3 tests: min/mean/max, window boundaries, sentinels), `SignalLoggerEventOrderingTest` (2 tests: chronological order, ring buffer wrap), `SignalLoggerCsvFormatTest` (5 tests: ride log header/row, burst header/row, events), and `SignalLogicTest` (14 tests: pack/unpack, channel/slot mapping, deinterleaving, motor state machine, timestamps) | PR-01 |
| Cadence math | RPM from intervals, 25 ms lockout, 3 s timeout | PR-02 |
| Level mapping (`ResistanceSensor`) | Wiper → ℓ → `i` (B03 default and calibrated table), Echelon `L` ↔ `i` round trip with exact endpoints, Peloton power-equivalence mapping, runaway timer | PR-02 |
| `PowerEngine` | Table lookup, clamping, bilinear interpolation, CSV matrix parsing, recalibration update rule | PR-02 |
| **v2:** cam-angle tracker | Click-off detection, half-revolution counting in both directions | PR-07 |
| HR Measurement parser | uint8/uint16, contact flags, BPM = 0, energy and RR fields, truncated input | PR-03 |
| HR stale-data timer | 10 s timeout, disconnect → HR = 0 | PR-03 |
| FTMS encoders | Indoor Bike Data flags and layout; Target Setting Features = 0 (v1) | PR-05 |
| **v2:** Control Point parser | Feature bits by mode, opcode results (`0x05` / `0x02` / success) | PR-08 |
| Session state machine | Start threshold, pause, 180 s auto-end, trailing-zero trim, checkpoint recovery | PR-06 |
| TCX writer | Exactly one trackpoint per second, 2 Hz → 1 Hz averaging, `<Watts>0</Watts>` when coasting | PR-06 |
| Upload stage machine | Stage sequencing, `UpdateStatus` mapping, duplicate = success, resume after failure | PR-06 |
| OAuth helpers | `state` generation and single-use validation, rotated-token persistence order | PR-06 |
| DiagnosticsModule | Ring buffer wrap, event recording, time-windowed query, capacity clamping, payload encoding/decoding | PR-14 |
| RidePersistence | Save/load/delete rides, cap enforcement with oldest-first eviction, begin() directory scan, gzip round-trip | PR-14 |
| DiagnosticsRoutes helpers | `diagEventTypeToString()`, `formatPayloadJson()` for all 6 event types, payload decode round-trips | PR-14 |
| **v2:** Position controller logic | Direction choice, never crossing an extreme, settle/retry limit, run-time bound, plausibility faults | PR-07 |
| **v2:** ERG state machine | `ACTIVE` / `SUSPENDED_BY_RIDER` / `RELEASED` transitions, stored targets, hold time, Status opcodes, deadband, rate limit | PR-08 |

### 15.2 PR-01 — Phase 0: Observe

> Fly-wire logger on USB power, Phase 0/1 pin map (§4.4.1). No bike wires are
> altered and no BLE is compiled.

**Entry condition (bench-ready):** scope with ≥ 2 channels and single-shot
triggering; motor-current measurement (shunt + scope, or DC clamp); JST-SM
9-pin and JST 3-pin Y / back-probe harness; series resistors and ÷4 dividers
fitted.

| # | Test | Expected result |
|---|---|---|
| 0.1 | Flash firmware via USB-C. | Device boots to the framework's setup AP. |
| 0.2 | Connect to the AP and configure home WiFi. | Portal reachable at `echenon.local`; IP fallback shown in serial log and QR code. |
| 0.3 | Open `echenon.local` in Chrome on the Lenovo P12. | Portal loads; `echenon.local` resolves on the tablet. |
| 0.4 | Build check. | `ex3-waveshare-nano` env builds against framework `v0.0.12-dev.82` with BLE off; no `WITH_DISPLAY_U8G2` or `WITH_BUTTONS`. |
| 0.5 | Pin idle check with the logger connected and unpowered, then powered. | No tap pin drives or biases a console line; every tap pin reads as input with no internal pull. |
| 0.6 | WO0 spike: ADC-continuous throughput on 8 channels with WiFi on. | Sustained rate recorded; burst capture at 1–5 kHz on 8 channels is achievable (GPIO 8/9 released for PCNT). |
| 0.7 | WO0 spike: GPIO 8/9 dedicated to PCNT peripheral (`EncoderModule`). | Hardware PCNT decoding verified with 0% bounce, 100% detection. Cadence sampled on GPIO 5 via ADC software threshold. |
| 0.8 | WO0 spike: encoder signal verification. | Correct wire colours (Black=A, Red=B, Orange=VCC) confirmed; PCNT decodes both channels cleanly (supersedes A08 analog conditioning spike). |
| 0.9 | Ride log: a full 60-minute ride. | Decimated min/mean/max at 10–20 Hz per channel (8 channels) plus the µs event log fit in PSRAM and export as CSV. |
| 0.10 | Burst capture: manual trigger, motor-drive-onset trigger and power-on trigger. | 5–30 s windows at 1–5 kHz on all 8 channels download as CSV. |
| 0.11 | Scope gate: both motor wires, both directions and at rest, at 10 ms/div and at µs/div. | Waveform **characterised** (DC level or PWM frequency and duty) and filed as evidence. Either result allows logging to proceed. |
| 0.12 | Power-on capture (register #13): logger on USB, then 9 V applied to the bike. | Console power-on behaviour captured in single-shot mode from several starting positions. |
| 0.13 | Phase 0 register. | Every row of [`docs/measurements/phase0/README.md`](docs/measurements/phase0/README.md) has an Evidence file and a Result. |

**Exit:** test 0.13 passes. The register is the input to DR-1.

### 15.3 DR-1 — Design Freeze (v1 Passive)

| # | Check | Expected result | Status (2026-10-12) |
|---|---|---|---|
| D.1 | Register completeness. | Every register row has evidence and a result; every v1 `[DR-1: …]` value in this specification has a decision. | **Done.** Register complete (row #10 waived for v1, row #12 moved to PR-03 WO3); v1 values resolved. Remaining placeholders are `[PR-02: …]` or `[DR-2: …]` |
| D.2 | Pin map. | §4.4.2 v1 rows confirmed or replaced; the PROVISIONAL marking is removed for v1. | **Done.** v1 rows frozen (all ten PR-01 taps, no outputs, UART0 reserved); v2 rows stay provisional under DR-2 |
| D.3 | Resistance model. | v1: wiper profile, working range and level model decided. Click-off threshold, fast-path rate and Override scale are v2 (DR-2). | **Done.** Linear B03 fit over 165–1327 counts, 4 detents per level, internal scale and mappings (§5.2–§5.3) |
| D.4 | Power model. | Model form decided from data. | **Updated 2026-10-24 (superseding DR-1).** 4-parameter first-principles electromechanical formula (`Physics P3r`: $R^2 = 0.999522$, $\text{RMSE} = 3.77\text{ W}$, [`docs/power-physics-model.md`](docs/power-physics-model.md)) as Tier 1 default in `PowerEngine`, with Tier 2 custom matrix and Tier 3 recalibration table overrides (§6) |
| D.5 | v1 power path. | VIN supply, VIN/USB selection and the power-path check decided (MITM safety moved to DR-2). | **Approved with condition.** VIN or USB, never both; PR-04a WO1 checks the Waveshare power path from the schematic before soldering and designs the selector (§4.6) |
| D.6 | Standalone supports. | None on the v1 board (R3/R4/R5 and the Green–GND link are DR-2 items). | **Done.** None in v1 |
| D.7 | Decision record. | [`DR-1-design-freeze.md`](.agents/plugins/principal-architect-workspace/reviews/DR-1-design-freeze.md) approved by the owner. | **Approved with conditions**, 2026-10-12 |

### 15.4 PR-02 — Phase 1: Calibrate

> Same fly-wire rig. Gated on DR-1.

| # | Test | Expected result |
|---|---|---|
| 1.1 | Pedal at steady cadence; compare with the Echelon app. | Cadence within ±1 RPM. |
| 1.2 | Run passive calibration: step the console through levels 1–32 and back. | 32-entry wiper table saved (display-path counts per level). |
| 1.3 | Turn the knob to level 19. | Live feed shows level 19 of 32, matching the Echelon app. |
| 1.4 | Sweep levels 1–32 while logging. | Internal `i` and Echelon `L` track the console at every level in both directions; `L` ↔ `i` round-trips with exact endpoints. |
| 1.5 | Power-data acquisition WO: 320-point automated sweep across levels 1–32 × 40–130 RPM and regression derivation of the first-principles physical model (`Physics P3r`). | Grid (`sweep_20261024.csv`), regression analysis (`C07`), and physical model specification (`docs/power-physics-model.md`) recorded as PR-02 evidence; `PowerEngine` evaluates `Physics P3r` in Default tier and seeds `DEFAULT_POWER_TABLE`. |
| 1.6 | Compare computed power with the Echelon app's displayed power. | Within ±15 % (achieved $R^2 = 0.999522$, $\text{RMSE} = 3.77\text{ W}$, $\text{MAE} = 2.71\text{ W}$). |
| 1.7 | Reboot. | Calibration and recalibrated table persist. |
| 1.8 | Hold the wiper outside 165–1327 counts for longer than `10` s (`runaway_timeout_s` default; bench simulation; do not provoke a real runaway). | Runaway event on the EventBus and a portal alert. |
| 1.9 | Peloton mapping (deferred to PR-05 per owner decision 2026-10-20). | 0…100 value matches the published Peloton curve at equal power and cadence (validated in PR-05). |

### 15.5 PR-03 — Phase 2: HR Relay (UC1 HR path)

> BLE on. Entry gates: the Echelon-app HR spike and framework central-role
> acceptance evidence (§8.3).

| # | Test | Expected result |
|---|---|---|
| 2.1 | nRF Connect: scan. | `Echenon-XXXX` advertises the HR service (`0x180D`). |
| 2.2 | BLE portal tab: scan and pair the HR armband. | Strap paired; portal shows HR and battery level. |
| 2.3 | In the Echelon app, pair `Echenon-XXXX` as the heart-rate device. | Echelon app shows HR from the re-broadcast, not directly from the armband. |
| 2.4 | Remove the armband from skin. | HR marked absent within 12 s (criterion a); BPM = 0 relayed. |
| 2.5 | Let the armband auto-sleep, then put it back on. | Reconnected ≤ 20 s after it starts advertising (criterion c); no give-up after any number of attempts. |
| 2.6 | Factory reset, then repeat 2.5. | Slow retry is still 15 s. |
| 2.7 | Progressive coexistence check: HR relay plus portal open plus a large HTTPS POST, 5 minutes. | No HR gap > 2.5 s while on-skin (criterion b). |

### 15.6 PR-04a and PR-05 — Phase 3: Passive Tap Board and FTMS Telemetry (UC1, UC3) — v1

**PR-04a — Passive tap board (permanent install):**

| # | Test | Expected result |
|---|---|---|
| 3.1 | Install the passive tap board between frame and console. | Console powers on normally from its own supply path; the ESP32 runs from VIN with the selector in the VIN position. |
| 3.1a | Before soldering: PR-04a WO1 power-path check. Then on the built board: selector in the USB position with USB-C connected. | WO1 schematic finding and selector design committed. In the USB position the 9 V line is disconnected from VIN and the board runs from USB alone; VIN and USB are never connected together. |
| 3.1b | Inspect GPIO 43/44. | Unconnected and marked reserved (UART0). |
| 3.2 | Echelon class with knob changes (UC1). | Class, resistance changes and motor behaviour identical to stock; ride page tracks cadence and level. |
| 3.3 | Compare console behaviour with and without the board. | No difference at power-on, during rides or in sleep (taps are invisible). |
| 3.4 | Manual OTA from the System tab. | Update applies; device reboots into the new firmware. |
| 3.5 | Reboot, then factory reset. | `autoCheck` is `false` after each; no automatic update at boot. |

**PR-05 — FTMS telemetry (fly-wire rig is sufficient):**

| # | Test | Expected result |
|---|---|---|
| 3.6 | nRF Connect: scan and connect. | `Echenon-XXXX` advertises FTMS (`0x1826`) with service data. |
| 3.7 | Subscribe to Indoor Bike Data. | 2 Hz notifications with cadence, level, power and HR (when valid). |
| 3.8 | Read Fitness Machine Feature. | Target Setting Features field = 0. |
| 3.9 | Inspect the FTMS service for `0x2AD9`. | Absent if FTMS v1.0 allows omission; otherwise present, Request Control only, every target opcode returns `0x02`. Resistance never changes. |
| 3.9a | Connect Zwift, TrainerRoad, Xert EBC and two other §7.7 apps. | Each pairs and shows telemetry; behaviour without resistance control (manual/level mode, ERG refused, warning) recorded per app. |
| 3.10 | Pair a fitness app (e.g. Xert EBC) as FTMS device while the Echelon class runs (UC3). | App shows live power, cadence and HR matching the ride page. |
| 3.11 | Echelon tablet (HR) plus a fitness app (FTMS) at the same time. | Both connections coexist; data flows to both. |
| 3.12 | Progressive coexistence check: 3 peripherals + HR central + ride page, 5 minutes. | FTMS gap ≤ 600 ms; HR gap ≤ 2.5 s; free DRAM recorded. |

### 15.7 PR-06 — Phase 4: Session, TCX and Strava (UC2) — v1

| # | Test | Expected result |
|---|---|---|
| 4.1 | Start pedalling. | `/api/session/status` reports Recording after the sustained-cadence threshold; a single stray pulse does not start a session. |
| 4.2 | Ride 5+ minutes, stop, wait 3 minutes. | Session ends; it appears in the Session History tab with its summary. |
| 4.3 | Inspect the generated `tcx.gz`. | One trackpoint per second, no gaps, zero watts written when coasting, UTC timestamps. |
| 4.4 | Connect Strava (BYOA) with the callback domain set to `echenon.local`. | OAuth completes; tokens stored encrypted; tokens redacted in logs. |
| 4.5 | Replay the OAuth callback with a wrong or reused `state`. | Rejected. |
| 4.6 | Upload a session. | Activity appears in Strava with power, cadence, HR and duration; each pipeline stage ran as a separate fetch within 15 s. |
| 4.7 | Upload the same session again. | Strava duplicate treated as success; job removed from the queue. |
| 4.8 | Force a token refresh. | Rotated refresh token persisted before use; new tokens re-registered with the Redactor. |
| 4.9 | Finish a ride with WiFi off, then restore WiFi. | Session queued and uploaded on reconnect. |
| 4.10 | Cut power mid-ride, then power on. | Session recovered from the checkpoint. |
| 4.11 | Download the TCX from the portal without and with login. | Refused without authentication; served with it. |
| 4.12 | Build without `WITH_BLE`. | Upload pipeline compiles and runs; no `ScanInhibit` dependency. |

### 15.8 PR-07 — Phase 5: MITM Board, Motor Control and Safety (UC6 hardware) — v2

> **v2 (provisional).** Active intercept board installed. Pin map from DR-2.
> Tests 5.4–5.7 and 5.12 (standalone supports) move to PR-12 when v2 is
> decomposed; they use the DR-2 pot-reference replica, not R3/R4/R5.

| # | Test | Expected result |
|---|---|---|
| 5.1 | Power on with firmware halted or in reset. | Motor path is console pass-through (fail-safe); the console controls resistance. |
| 5.2 | Intercept Mode: change resistance during a class (UC1 on the MITM board). | Motor follows the console through the relay (GPIO 6/7 sense → GPIO 10/17 drive) with no perceptible delay. |
| 5.3 | Motor-driven calibration sweep. | Click-offs and cam extremes found; θ<sub>min</sub> and θ<sub>max</sub> saved. |
| 5.4 | Disconnect the console (J2, J4); set Override Mode; close the standalone supports. | ESP32 boots normally; sense inputs read low. |
| 5.5 | Standalone cadence: pedal slowly. | Pulses on GPIO 5 (pull-up from R3). |
| 5.6 | Standalone pot: read the wiper. | Wiper within the standalone calibration (reference from R4). |
| 5.7 | Standalone encoder: turn the knob. | `encoder.rotate` events (encoder powered via R5); counts per detent match DR-1. |
| 5.8 | Knob control in Override. | Motor moves one display level per detent; the cam never crosses an extreme. |
| 5.9 | Pot fault injection (open wiper, stuck value). | Motor stops and a fault is raised. |
| 5.10 | Stall injection. | Motor stops at the run-time bound; angle marked invalid; re-homing requested. |
| 5.11 | Reboot in Override with cadence present. | Stored half verified by a small move; no full homing sweep while pedalling. |
| 5.12 | Reconnect the console, open the standalone supports, switch to Intercept Mode. | Pass-through restored; no contention; console position still valid (no desync). |

### 15.9 PR-08 — Phase 6: ERG and Control Point (UC4, UC5) — v2

> **v2 (provisional).** Test 6.8's external HR-zone app is joined by PR-11's
> portal-native HR-zone control when v2 is decomposed.

| # | Test | Expected result |
|---|---|---|
| 6.1 | Override Mode, console connected but idle (UC4). Pair an FTMS app. | Feature target bits set; Request Control succeeds. |
| 6.2 | App sends Set Target Power 200 W. | ERG `ACTIVE`; motor moves; ride page shows ~200 W; Status `0x08` Target Power Changed sent. |
| 6.3 | Vary cadence 60–100 RPM. | Resistance adjusts to hold 200 W without hunting (deadband and rate limit). |
| 6.4 | Turn the knob mid-ERG. | ERG → `SUSPENDED_BY_RIDER`; the knob sets resistance; new target writes are stored, not applied. |
| 6.5 | App sends Request Control again, or the hold time expires. | ERG → `ACTIVE` with the latest stored target. |
| 6.6 | **Motor boundary:** ERG demands high resistance at low cadence. | The target near cam-max settles without crossing the extreme. |
| 6.7 | **ERG acceptance:** step targets across the working range. | Setpoint reached within `[DR-2]` s without oscillation; position error ≤ 1 level. |
| 6.8 | Echelon class running; HR-zone app sends Set Target Resistance (UC5). | Motor follows the app; class content unchanged; Echelon resistance bar shows the actual position. |
| 6.9 | Reset (`0x01`), then a control-loss event. | ERG → `RELEASED`; Status `0xFF` Control Permission Lost sent on control loss. |
| 6.10 | `POST /api/resistance` in Override Mode, then in Intercept Mode, then unauthenticated. | Accepted; `409`; refused. |

### 15.10 PR-09 — Phase 7: v1 Integration and Release Candidate — v1

| # | Test | Expected result |
|---|---|---|
| 7.1 | **v1 coexistence verification** (§8.5) at the maximum v1 configuration. | All pass criteria met; results documented. |
| 7.3 | Split-screen: Chrome showing `/ride` on the P12 alongside the Echelon app. | Ride page shows live data without interfering with the class. |
| 7.4 | Dual-app: Xert EBC + Zwift (visuals) over read-only FTMS. | Both receive telemetry; neither can change resistance. |
| 7.5 | Ride in Zwift, which also uploads to Strava. | Echenon auto-upload disabled for the session, or Strava reports a duplicate that Echenon treats as success. |
| 7.6 | Power-cycle mid-ride. | Reboot; session recovered; no corruption. |
| 7.7 | Manual OTA. | Update applies; new version runs; no automatic check at boot. |
| 7.8 | UC1–UC3 end-to-end runs from [`docs/use_case_analysis.md`](docs/use_case_analysis.md). | Each use case passes on the installed passive tap board. |
| 7.9 | **P12 Split-Screen Acceptance**: Open `/ride` on the Lenovo Tab P12 in Android split-screen with the Echelon app. | Metrics update at 2 Hz; session start/stop works from the ride page; PWA "Add to Home Screen" installs and launches in standalone mode. |

Test 7.2 (mode switching) moved to §15.12 (v2).

### 15.11 Known Limitations

- Power is estimated, not measured. Accuracy improves with Tier 2 or Tier 3
  calibration.
- Speed is synthetic (from cadence); Strava distance is approximate.
- **v1 has no resistance control.** FTMS is read-only; ERG, resistance
  targets and HR-zone auto-resistance need the v2 active board.
- **v1 cannot prevent the stock console runaway** (a fast knob spin past level
  32 leaves the motor turning; A09-20261012 #3). The passive board can detect
  and report it; prevention is a v2 item (DR-2 §2.4).
- ANT+ is not supported (no ANT radio); UART0 is reserved for a future add-on.
- Motor positioning precision depends on the quality of the cam-angle
  calibration and on the DR-2 control parameters.
- The Echelon app may show slightly different cadence or resistance values,
  because it reads the same sensors independently.
- **v2:** pot calibration differs between console-referenced and standalone
  (replica-referenced) operation. Echenon keeps both, but a change of console
  connection state needs the matching calibration to exist.
- OTA is manual-only until framework issue #226 lands.
- **v2:** any BLE Central in range can take FTMS control.

### 15.12 PR-13 — Phase 7: v2 Integration and Release — v2

> **v2 (provisional).** Detailed tests are written after DR-2.

| # | Test | Expected result |
|---|---|---|
| 8.1 | **v2 coexistence verification** (§8.5) at maximum configuration. | All pass criteria met; results documented. |
| 8.2 | Mode switching: mode 1 (relay) → mode 2 (console off) → mode 1 via the portal. | Clean transitions; motor control passes correctly; no console desync. |
| 8.3 | Dual-app: Xert EBC (ERG) + Zwift (visuals) over FTMS. | Both receive data; ERG commands from EBC drive resistance. |
| 8.4 | Portal-native HR-zone auto-resistance during an Echelon class. | Resistance tracks the HR zone; knob overrides. |
| 8.5 | UC4–UC6 end-to-end runs from [`docs/use_case_analysis.md`](docs/use_case_analysis.md). | Each use case passes on the active board. |

---

## Appendix J: User Journeys

Journeys 1, 2, 3, 5, 6 and 7 are **v1**: they work with the **passive tap
board** (in Journey 3, FTMS is read-only). Journeys 4, 4b, 4c and 4d are **v2**:
they need the **active intercept board**.

### Journey 1 — Daily Echelon Class (Hands-Free)

> Ride an instructor class. The ride appears in Strava automatically.

1. Put on the HR armband.
2. Switch on the bike. Echenon boots, connects to WiFi and reconnects to the
   armband.
3. **Tablet:** open the Echelon app → select a class → pair the bike
   (factory BLE).
4. In the Echelon app's HR settings, pair `Echenon-XXXX` as the heart-rate
   device (one time only).
5. Optionally open `echenon.local/ride` as a floating window.
6. **Start pedalling**; Echenon starts recording automatically.
7. Ride the class: HR in the Echelon app, live power and charts on the
   ride page.
8. Class ends → stop pedalling → dismount.
9. After 3 minutes the session ends and uploads to the rider's Strava.
10. Later, Strava shows the full ride with power, cadence and HR.

**No buttons. No apps to start. Just ride.**

---

### Journey 2 — Manual Session Control

> Exclude the warm-up; start and stop explicitly.

1–5. As Journey 1.
6. Warm up. The ride page shows live data with status "Idle".
7. Tap **Start** on the ride page (logged in).
8. Ride the class.
9. Tap **Stop** and review the post-ride summary.
10. The session uploads automatically, or tap **Discard**.

---

### Journey 3 — Zwift Virtual Ride (Telemetry)

> No instructor class; a virtual world with the rider setting resistance by
> knob.

1. Switch on the bike; Echenon boots.
2. **Tablet:** open Zwift → pair `Echenon-XXXX` as a power and cadence
   source (FTMS).
3. HR comes in the FTMS data (or pair the strap directly in Zwift if
   Echenon is not holding it).
4. Ride; the avatar's speed follows live power. The rider changes resistance
   with the knob; Zwift cannot control resistance on the passive board.
5. End the ride in Zwift; Zwift uploads to Strava.
6. Echenon also records. To avoid a Strava duplicate, disable Echenon's
   auto-upload, or rely on Strava's duplicate detection (which Echenon treats
   as success).

---

### Journey 4 — Structured Training with ERG (TrainerRoad / SYSTM) — MITM board (**v2**)

> Power-based workout with automatic resistance control.

1. Set Echenon to **Override Mode** in the portal.
2. Switch on the bike; Echenon boots.
3. **Tablet:** open the training app → pair `Echenon-XXXX` (FTMS).
4. The app sends target power through the FTMS Control Point; Echenon moves
   the resistance motor to hold the target at the current cadence.
5. "Target: 180 W — Current: 178 W": the bike adjusts itself.
6. Complete the workout; the app uploads to Strava.

**No knob turning required.** Turning the knob suspends ERG until the app
re-requests control or the hold time expires.

---

### Journey 4b — Xert SMART Workout with ERG — MITM board (**v2**)

> Dynamic workout with real-time fatigue tracking.

1. Set Echenon to **Override Mode**.
2. Switch on the bike; Echenon boots.
3. **Tablet:** open **Xert EBC** → pair `Echenon-XXXX` (FTMS).
4. Select a SMART workout.
5. EBC shows a live **MPA gauge** and dynamic targets, for example "ride at
   250 W until MPA drops to 320 W, then recover until MPA reaches 95 %".
6. Xert sends target power through the Control Point; resistance changes
   automatically as targets shift.
7. Exceeding the predicted MPA triggers a **breakthrough**, and Xert raises
   the rider's fitness signature without an FTP test.
8. End the ride; the activity reaches Xert through Strava.

**Manual fallback.** Turning the red knob moves ERG to `SUSPENDED_BY_RIDER`
and gives the rider direct control. ERG resumes with the latest target when
EBC requests control again or the hold time expires.

**Dual-app setup** (optional): Xert EBC for coaching plus Zwift for visuals,
both connected to `Echenon-XXXX` (up to 3 peripheral connections).

---

### Journey 4c — Standalone Ride (No Echelon Console) — MITM board (**v2**)

> The console is disconnected; the bike is a pure FTMS smart trainer.
> *(UC6 in [`docs/use_case_analysis.md`](docs/use_case_analysis.md))*

1. Disconnect the console, close the standalone supports (R3, R4, R5 and the
   Green–GND link) and set **Override Mode**.
2. Put on the HR armband; switch on the bike; Echenon boots and verifies the
   stored cam half with a small move.
3. The sensors work without the console, powered by the board's 3.3 V rail
   through the standalone supports.
4. The red knob controls resistance directly on the Override scale.
5. Optionally connect Zwift, Xert or any FTMS app; ERG is available.
6. Open the ride page for live power, HR and cadence.
7. Pedal; recording starts automatically.
8. Stop; the session uploads to Strava.

**The EX-3 becomes an open smart trainer** with no Echelon subscription, app
or ecosystem dependency.

---

### Journey 4d — Echelon Class with External Auto-Resistance (HR Zones) — MITM board (**v2**)

> Follow an Echelon class while an external app sets resistance by heart-rate
> zone. *(UC5 in [`docs/use_case_analysis.md`](docs/use_case_analysis.md))*

1. Set Echenon to **Override Mode**.
2. Put on the HR armband; switch on the bike; Echenon boots.
3. **Tablet:** open the Echelon app → select a free-ride or endurance class.
4. Open an HR-zone training app (e.g. Wahoo SYSTM, Xert EBC) → pair
   `Echenon-XXXX` as FTMS.
5. The app receives live HR from the FTMS Indoor Bike Data.
6. The app sends Set Target Resistance by HR zone; the motor adjusts.
7. The instructor provides coaching and music; resistance follows the rider's
   heart rate, not the instructor's callouts.
8. The Echelon app's resistance bar shows the actual position (it reads the
   pot), not the called-out level. This is expected.
9. Turning the knob suspends auto-resistance.
10. End the class; the session uploads to Strava.

**The Echelon app never commands resistance**; it only reads the pot, so it
never conflicts with Override Mode.

---

### Journey 5 — First-Time Setup

> One-time configuration after installing the board.

1. Flash the firmware via USB-C.
2. Power on; the framework's setup access point, named after the `echenon`
   hostname, appears.
3. Connect a phone or tablet to it; the captive portal opens.
4. Enter home WiFi credentials; Echenon connects.
5. Open `http://echenon.local` (or the IP shown on the captive page).
6. **Setup tab → Level Mapping pill:** run the passive calibration (step the
   console through its 32 levels as prompted) and choose the power source.
7. **Bluetooth tab:** Scan → select the HR strap → paired.
8. **Setup tab → Apps pill:** create a Strava API application, set its
   Authorization Callback Domain to `echenon.local`, enter the Client ID and
   Client Secret, then **Connect Strava** and authorize.
9. Done. For daily use, open `echenon.local/ride` (or install as a PWA).
   Everything is automatic from now on.

---

### Journey 6 — Free Ride (No App)

> Just pedal: no class, no virtual world; perhaps YouTube or a film.

1. Put on the HR armband; switch on the bike; Echenon boots.
2. **Tablet:** open anything, or nothing.
3. Optionally open the ride page alongside the Echelon app.
4. Pedal; Echenon starts recording.
5. Ride as long as you like; the ride page shows live data.
6. Stop; after 3 minutes the session ends and uploads to Strava.

**The simplest case.** No fitness apps are involved; Echenon records the ride
on its own.

---

### Journey 7 — OTA Firmware Update (Manual)

> Update the firmware without opening the console.

> [!NOTE]
> Automatic OTA is **disabled** (`autoCheck = false`, forced after config load
> and after factory reset) until framework
> [issue #226](https://github.com/matthewmcneill/esp32-app-foundation/issues/226)
> adds a boot-time guard. Without it, an update could start mid-warm-up.

1. Open `http://echenon.local` → **System** tab (logged in).
2. Click **Check for Updates**.
3. If an update is available, review the changelog and click **Update**.
4. BLE pauses; a progress bar shows download and verification; Echenon
   reboots.
5. The dashboard reconnects; the new firmware is running.

No USB cable, no laptop, no console disassembly: the ESP32 stays inside the
bike permanently.
