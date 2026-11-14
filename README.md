# Echenon (`v0.3.0-beta.1`)

**Open-Source Smart Bike Telemetry & FTMS Bridge for the Echelon EX-3 (Passive Tap v1)**

[![Release](https://img.shields.io/badge/release-v0.3.0--beta.1-orange.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Target](https://img.shields.io/badge/hardware-Waveshare%20ESP32--S3--Nano-teal.svg)](docs/beta-build-guide.md)
[![Bike](https://img.shields.io/badge/bike-Echelon%20EX--3-success.svg)](docs/ex3_specification.md)

Echenon turns your **Echelon EX-3** indoor bike into an open, self-contained training computer and Bluetooth **FTMS (`0x1826`)** power/cadence bridge that works natively with **Zwift, MyWhoosh, TrainerRoad, Xert EBC, Wahoo SYSTM, and Garmin head units** — without cutting a single factory wire. Ride any app or instructor class while Echenon computes physics-calibrated power, relays your BLE heart-rate monitor, serves a live local web cockpit (`/ride`), and automatically syncs completed `.tcx.gz` activities to **Strava**, **Intervals.icu**, and **Wahoo Cloud**.

---

> [!IMPORTANT]
> ### 🛠️ Looking for Early Beta Builders (`v0.3.0-beta.1` — First Public Beta)
>
> We are recruiting a small cohort of capable DIY makers to build, install, and ride-test the **v1 Passive Tap Board** on their own **Echelon EX-3** bikes.
>
> **Who should join the beta:**
> - You own an **Echelon EX-3** (`ECHEX-3` / `S1001O-A03` console PCB) and want open FTMS telemetry, calibrated virtual power, and automatic cloud ride uploads.
> - You are comfortable soldering through-hole resistors/capacitors onto a `70 × 50 mm` perfboard, crimping 9-pin JST-SM and 3-pin JST connectors, verifying continuity/voltages with a multimeter, and flashing an ESP32-S3 over USB-C using PlatformIO.
> - You are willing to share structured feedback, calibration curves, and diagnostic logs via [GitHub Issues](CONTRIBUTING.md).
>
> 👉 **Start here:** Read the standalone [**v1 Beta Hardware Build & Commissioning Guide (`docs/beta-build-guide.md`)**](docs/beta-build-guide.md).

---

> [!CAUTION]
> ### ⚡ CRITICAL POWER SAFETY: `SW0` Mechanical `VIN` / `USB-C` Interlock
>
> The Echenon v1 Passive Tap Board powers the **Waveshare ESP32-S3-Nano** (`U1`) directly from the bike's `+9 V DC` supply rail (`J1` Pin 8, White) via the board's onboard **MP2322 `VIN` (6–21 V)** regulator (no external buck converter is used).
>
> **Power the ESP32-S3-Nano via bike `+9 V VIN` OR computer `USB-C` — NEVER BOTH SIMULTANEOUSLY.**
> - Always install the mandatory **`SW0` mechanical interlock switch** in series with `J1-8` (`+9 V`) to `U1 VIN`.
> - **Before plugging in USB-C:** Flip `SW0` to **USB / VIN-OFF** (or unplug the bike's 9 V wall adapter).
> - **Before powering the bike on `+9 V`:** Unplug the USB-C cable completely from `U1`, then flip `SW0` to **BIKE / VIN-ON**.
> - See [**`docs/beta-build-guide.md` (Power Interlock Checklist & §5 Pre-Power Sanity Checks)**](docs/beta-build-guide.md) before applying power.

---

## Key Capabilities (Implemented in `v0.3.0-beta.1`)

- **Non-Destructive Inline Passive Tap Harness:** Plugs inline between the EX-3's factory 9-pin JST-SM main loom (`J1 ↔ J2`) and 3-pin JST handlebar encoder loom (`J3 ↔ J4`). Every tap is high-impedance with zero internal GPIO pull-ups/pull-downs — the stock Echelon console and resistance knob continue to work normally, and unplugging the harness restores the bike to 100% stock.
- **First-Principles Calibrated Power Engine (`Physics P3r`):** Computes mechanical watts at 2 Hz from continuous ADC DMA wiper position ($\tilde{p} \in [0, 1]$) and glitch-filtered crank cadence using an electromechanical eddy-current & Kloss induction-rollover model ($R^2 = 0.9995$, $\text{RMSE} = 3.77\text{ W}$ across a 320-point empirical sweep), plus Peloton (`0–100`) braking-curve equivalence and virtual road speed.
- **Per-Bike Calibration Wizard & Live Recalibration UI:** Includes an OLS linear-endpoint **Calibration Wizard** for potentiometer end-stop tolerances, a **Recalibration UI** (`RecalibrationUI`) for fitting reference power readings (`a_scale` + $32 \times 31$ lookup matrix), and custom CSV power-table uploads.
- **Standard BLE FTMS (`0x1826`) + Heart Rate Relay (`0x180D`):**
  - Broadcasts **FTMS Indoor Bike Data (`0x2AD2`)** at 2 Hz (instantaneous power with configurable 0–10 s EMA smoothing, cadence, resistance level `1–32`, virtual speed, and heart rate) for up to 3 simultaneous BLE Central apps (Zwift, MyWhoosh, TrainerRoad, Xert EBC, Wahoo SYSTM, Garmin watches).
  - Connects as a BLE Central (`HRMClient`) to any standard BLE heart-rate strap/armband (including single-connection optical straps like the Echelon Beat `HW901`) and re-broadcasts (`HRMRepeater`) `0x180D` to the Echelon tablet app while embedding HR into FTMS and recorded sessions.
- **Autonomous Ride Recording & Multi-Sink Cloud Sync:**
  - Automatically starts recording on sustained pedalling, buffers 1 Hz anti-aliased `RideRecord` telemetry in 8 MB OPI PSRAM with 30 s LittleFS crash-recovery checkpoints (`/ride_active.bin`), and streams gzipped `.tcx.gz` files (`TCXGenerator`) with 1 Hz `<Watts>` and `<Speed>` extensions.
  - Multi-sink background `UploadQueue` (`IDataSource`) with `BLEManager::ScanInhibit` radio coexistence pushes completed sessions to **Strava** (`StravaUploader`, BYOA OAuth 2.0), **Intervals.icu** (`IntervalsIcuUploader`, API key), and **Wahoo Cloud** (`WahooUploader`, BYOA OAuth 2.0), plus optional LittleFS `.ride.gz` persistence (`RidePersistence`).
- **Live Web Cockpit (`/ride`), Portal & JSON-RPC MCP Tools (`/api/mcp`):**
  - **Standalone `/ride` page** (installable as a mobile/tablet split-screen PWA) with 2 Hz WebSocket telemetry (`/ws/echenon/live`), rolling 5-minute power chart, HR zones, and session start/pause/resume/stop controls, backed by `SessionOverlay` REST routes (`/api/session/*`, `/api/upload/*`, `/api/strava/*`, `/api/intervals/*`, `/api/wahoo/*`).
  - **Main Web Portal (`http://echenon.local`)** with live dashboard, Calibration Wizard, Power Tuning, Stored Rides history (`/stored_rides.js`), and Diagnostics event/run viewer (`DiagnosticsRoutes`), protected by **`AuthManager` portal authentication** (`WO-09.1`).
  - **Model Context Protocol (`POST /api/mcp` via `IMCPContributor`):** Exposes structured JSON-RPC tools (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`, plus `echenon_rig_status` and `echenon_rig_command` when built with `-DWITH_CALIBRATION_RIG` — migrated 100% off legacy HTTP endpoints).

---

## Shopping List & 5-Step Quick Start (`~USD $25–$35`)

### Bill of Materials Summary (v1 Passive Tap Board)

Full circuit schematics, pin-by-pin wire colour tables, and multimeter checklists are in [**`docs/beta-build-guide.md`**](docs/beta-build-guide.md):

| Ref | Part | Qty | Value / Spec | Role |
|---|---|---|---|---|
| `U1` | **Waveshare ESP32-S3-Nano** | 1 | `ESP32-S3R8` (16 MB Flash / 8 MB OPI PSRAM, onboard MP2322 `VIN` 6–21 V regulator) | Main MCU (**no external `U3` buck converter needed**) |
| `SW0` | **Mechanical Interlock Switch** | 1 | SPDT slide/toggle switch rated ≥ 1 A @ 12 V DC (+ optional `D1` `1N5819`/`SS14` Schottky diode) | Series disconnect on `J1-8` (`+9 V`) to `U1 VIN` (**`VIN` OR `USB-C`, never both**) |
| `RS1–RS3` | **22 kΩ Resistors** | 3 | `22 kΩ`, 1% metal film, 1/4 W | High-Z series taps on potentiometer lines (`GPIO 1, 2, 3`) |
| `RS4–RS8` | **4.7 kΩ Resistors** | 5 | `4.7 kΩ`, 1% metal film, 1/4 W | Series protection & RC filter input for cadence (`GPIO 4, 5`) and encoder (`GPIO 8, 9, 10`) |
| `R1a, R2a` | **30 kΩ Resistors** | 2 | `30 kΩ`, 1% metal film, 1/4 W | Upper leg of ÷4 motor-sense dividers (`GPIO 6, 7`) |
| `R1b, R2b` | **10 kΩ Resistors** | 2 | `10 kΩ`, 1% metal film, 1/4 W | Lower leg to `GND` of ÷4 motor-sense dividers (`GPIO 6, 7`) |
| `C1 ×5` | **100 nF Ceramic Capacitors** | 5 | `100 nF` (`0.1 µF`), X7R, ≥ 16 V | Low-pass filter (~338 Hz) & ADC charge reservoir from **ESP32 GPIO side** of `RS4–RS8` to `GND` (`GPIO 4, 5, 8, 9, 10`) |
| `J1 / J2` | **JST-SM 9-Pin Socket + Plug** | 1 pair | 9-pin JST-SM female receptacle (`J1`) + male plug (`J2`) | Main frame-to-console inline pass-through |
| `J3 / J4` | **JST 3-Pin Socket + Plug** | 1 pair | 3-pin JST female receptacle (`J3`) + male plug (`J4`) | Handlebar resistance knob encoder inline pass-through |
| `J5` + Board | **Headers, Perfboard & Wire** | 1 set | `2× 15-pin` `2.54 mm` female socket strips, `70 × 50 mm` FR-4 perfboard, `22 AWG` wire | Carrier board fitting inside the EX-3 console housing |

### Quick Start in 5 Steps

1. **Assemble & Bench-Check the Harness (`U1` Unseated):**
   Follow [**`docs/beta-build-guide.md` (§2–§5)**](docs/beta-build-guide.md) to solder the pass-through looms, resistor/capacitor taps, and `SW0` interlock onto the `70 × 50 mm` perfboard. Complete every unpowered continuity check and live voltage check **before** seating the ESP32-S3-Nano (`U1`) into `J5`.
2. **Clone Repository & Initialize Submodule:**
   ```bash
   git clone --recurse-submodules https://github.com/cblabs-org/echenon.git
   cd echenon
   ```
   *(Note: Echenon builds on the `framework/` submodule (`esp32-app-foundation`), whose public repository access accompanies the `v0.3.0-beta.1` beta launch.)*
3. **Isolate `VIN` (`SW0` → USB) & Flash Over USB-C:**
   Verify `SW0` is in the **USB / VIN-OFF** position (or the board is unplugged from the bike), seat `U1` into `J5`, connect a USB-C data cable, and flash both the firmware and LittleFS portal image:
   ```bash
   pio run -e ex3-waveshare-nano -t upload
   pio run -e ex3-waveshare-nano -t uploadfs
   ```
4. **WiFi Onboarding & Portal Authentication:**
   Connect to the **`Echenon`** setup WiFi AP (`http://192.168.4.1`), join your 2.4 GHz home WiFi, then open **`http://echenon.local`** and set your administrator password (`AuthManager` protects all configuration, calibration, and ride management routes).
5. **Install in Console, Calibrate & Ride:**
   Unplug the USB-C cable completely, connect `J1 ↔ J2` and `J3 ↔ J4` inline inside the EX-3 console shell, flip `SW0` to **BIKE / VIN-ON**, and power on the bike. Run the **Level Mapping Calibration Wizard** in the portal (`Apps → Level Mapping`), pair your BLE heart-rate monitor (`Bluetooth` tab), and open **`http://echenon.local/ride`** or pair `Echenon-XXXX` in Zwift / TrainerRoad / MyWhoosh!

---

## Known Beta Limitations & Roadmap (`v1` → `v2`)

1. **Passive Read-Only Tap in `v1` (No ERG / Active Resistance Control Yet):**
   - The `v1` board is strictly an inline passive observer. You adjust resistance manually with the red handlebar knob while Echenon broadcasts live watts, cadence, resistance level, and heart rate over BLE FTMS (`0x1826`). Closed-loop motor control (`ERG` mode, FTMS Control Point `0x2AD9`, HR-zone auto-resistance, and console-off standalone mode) is planned for the **v2 Active Intercept (MITM) Board**.
2. **`WITH_OTA` Disabled in `v0.3.0-beta.1` (USB-C Updates Only):**
   - Over-the-air firmware updating (`WITH_OTA`) is intentionally disabled in `v0.3.0-beta.1` while upstream framework OTA boot-guard hardening is finalised. All firmware and LittleFS updates during beta are performed over **USB-C with `SW0` flipped to `USB / VIN-OFF`**.
3. **Stock Console Motor Control & Power-Off Best Practice:**
   - Because the stock EX-3 console drives the motor directly in `v1`, stock console firmware quirks remain unchanged (including the stock EX-3 behaviour where unplugging power while parked at a high resistance level can misindex the console's step counter). Echenon detects out-of-range potentiometer travel (`RunawayDetector`) and logs a portal alert, but passive hardware cannot cut motor power. **Best practice:** return the knob to **Level 1** before unplugging the bike.
4. **Verified Bike Model Scope:**
   - Verified on the **Echelon EX-3** (`ECHEX-3` / `S1001O-A03` console PCB). Other Echelon Connect models (EX-1, EX-4s, EX-5, Sport) may share similar harness pinouts but are currently untested — hardware reports and pull requests are welcome!

---

## Documentation Index

| Document | Description |
|---|---|
| [**`docs/beta-build-guide.md`**](docs/beta-build-guide.md) | **Start Here:** Standalone v1 Passive Tap Board BOM, wiring tables, schematics, multimeter checks, USB-C flashing & calibration guide |
| [**`SPEC.md`**](SPEC.md) | Canonical Echenon system & firmware specification (v1 Passive & provisional v2 Active architecture) |
| [**`docs/hardware_specification.md`**](docs/hardware_specification.md) | Detailed circuit engineering reference, oscilloscope evidence, impedance analysis & v2 MITM draft notes |
| [**`docs/ex3_specification.md`**](docs/ex3_specification.md) | Echelon EX-3 stock hardware teardown, connector pinouts, actuator cam & sensor electrical profiles |
| [**`docs/power-physics-model.md`**](docs/power-physics-model.md) | First-principles derivation & 320-point empirical verification of the `Physics P3r` eddy-current power model |
| [**`docs/data-sinks.md`**](docs/data-sinks.md) | Multi-sink `UploadQueue` architecture and setup guide for Strava, Intervals.icu, and Wahoo Cloud |
| [**`docs/diagnostics.md`**](docs/diagnostics.md) | PSRAM ring-buffer `DiagnosticsModule` event types and `/api/echenon/events` REST API |
| [**`docs/run-data-persistence.md`**](docs/run-data-persistence.md) | Optional LittleFS `.ride.gz` binary ride storage (`RidePersistence`) and CSV export routes |
| [**`docs/use_case_analysis.md`**](docs/use_case_analysis.md) | Operating scenarios (`UC1`–`UC6`) across Echelon classes, Zwift/FTMS apps, and free rides |
| [**`CONTRIBUTING.md`**](CONTRIBUTING.md) | Beta bug reporting guidelines, secret-scrubbing checklist, build environments & developer workflow |
| [**`CHANGELOG.md`**](CHANGELOG.md) | Release history and `v0.3.0-beta.1` capability summary |
| [**`LICENSE`**](LICENSE) | Apache License 2.0 and third-party MIT attribution notices |
