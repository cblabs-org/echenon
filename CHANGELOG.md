# Changelog

All notable changes to the **Echenon** firmware and hardware documentation will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned (Post-Beta / v2 Roadmap)
- Re-enable wireless OTA updates (`WITH_OTA`) once upstream framework boot-guard hardening is complete.
- v2 Active Intercept (MITM) Board (`DR-2`): closed-loop H-bridge resistance motor control, FTMS Control Point (`0x2AD9`) ERG/SIM target support, portal-native HR-zone auto-resistance, and console-off standalone mode (`UC6`).

---

## [0.3.0-beta.1] - 2026-11-14

### Added — First Public Beta (`v1` Passive Tap Board on Echelon EX-3)
- **Hardware & Builder Documentation (`ex3-waveshare-nano`):**
  - Frozen v1 Passive Tap Board pin map (`hardware/ex3-waveshare-nano/board.h`) for the **Waveshare ESP32-S3-Nano** (`ESP32-S3R8`, 16 MB Flash / 8 MB OPI PSRAM) on the **Echelon EX-3**.
  - Standalone DIY beta construction, pre-power multimeter verification, USB-C flashing, and commissioning guide in [`docs/beta-build-guide.md`](docs/beta-build-guide.md), specifying the mandatory **`SW0` mechanical `VIN` / `USB-C` power interlock** (`+9 V VIN` OR `USB-C`, never both).
- **Sensor Acquisition & Safety Monitoring:**
  - `CadenceSensor`: Interrupt-driven crank reed-switch acquisition (`GPIO 5`) with ESP32-S3 hardware IO-MUX glitch filtering, `250 ms` minimum period (240 RPM ceiling), `50 ms` continuous-HIGH open-release hysteresis, and wraparound-safe `3 s` zero-RPM timeout.
  - `ResistanceSensor`: Continuous ADC1 DMA sampling (`GPIO 1` wiper + monitor taps) with 20 Hz floating-point EMA filtering (`getFilteredWiperFloat()`) and canonical internal resistance mapping (`0–254`, `255 = UNCALIBRATED`).
  - `EncoderModule`: Hardware `PCNT` quadrature decoding of the handlebar resistance knob (`GPIO 8, 9`).
  - `RunawayDetector`: Out-of-range potentiometer travel detector (`< 151` or `> 1353` ADC counts for `> 10 s`) publishing alerts to the portal and `DiagnosticsModule`.
- **Physics Power Engine & Per-Bike Calibration:**
  - `PowerEngine`: First-principles 4-parameter electromechanical model (`Physics P3r`: eccentric cam kinematics, exponential air-gap magnetic flux, Kloss eddy-current inductive rollover, and Coulomb drivetrain friction; $R^2 = 0.9995$, $\text{RMSE} = 3.77\text{ W}$), Peloton (`0–100`) braking-curve equivalence, and flat-road virtual speed derivation.
  - `CalibrationWizard`: Multi-point passive resistance calibration with Ordinary Least Squares (OLS) linear endpoint fitting (`fitLinearEndpoints`) persisted via `ConfigManager`.
  - `RecalibrationUI`: Live rider reference-point capture (`5 s` averaging window, up to 100 points) fitting both the global eddy-current gain (`a_scale`) and the $32 \times 31$ lookup matrix, plus custom CSV power-table upload (`/api/echenon/power-table/upload`).
- **BLE FTMS (`0x1826`) & Heart Rate Relay (`0x180D`):**
  - `FTMSServer`: Read-only Bluetooth SIG Fitness Machine Service (`0x1826`) broadcasting Indoor Bike Data (`0x2AD2`) at 2 Hz (power with configurable 0–10 s EMA smoothing, cadence, resistance level `1–32`, virtual speed, and heart rate) to up to 3 simultaneous BLE Central clients (Zwift, MyWhoosh, TrainerRoad, Xert EBC, Wahoo SYSTM, Garmin).
  - `HRMClient` & `HRMRepeater`: BLE Central connection to any `0x180D` heart-rate strap/armband (including single-connection optical sensors like the Echelon Beat `HW901`) with automatic `15 s` slow-phase reconnect and transparent `0x180D` peripheral re-broadcast to the Echelon tablet app.
- **Session Recording, `.tcx.gz` Export & Multi-Sink Cloud Upload:**
  - `SessionManager` & `RideStore`: Hands-free auto-start/stop session state machine buffering 1 Hz anti-aliased `RideRecord` samples in PSRAM with 30 s `/ride_active.bin` crash-recovery checkpoints on LittleFS.
  - `TCXGenerator`: Streaming 1 Hz Garmin TCX XML + gzip compressor (`uzlib`/`miniz`) writing `<ns3:Watts>` and `<ns3:Speed>` trackpoint extensions without DOM heap spikes.
  - `UploadQueue`: Sequential background `IDataSource` upload pipeline with `BLEManager::ScanInhibit` radio coexistence and per-sink auto-sync toggles (`auto_sync_strava`, `auto_sync_intervals`, `auto_sync_wahoo`).
  - Cloud uploaders: `StravaUploader` (BYOA OAuth 2.0 with token rotation and `Redactor` secret registration), `IntervalsIcuUploader` (REST API key), and `WahooUploader` (BYOA OAuth 2.0).
- **Web Cockpit (`/ride`), Portal & Diagnostics:**
  - `SessionOverlay` & `/ride`: Standalone mobile/tablet split-screen PWA ride cockpit with 2 Hz WebSocket push (`/ws/echenon/live`), rolling 5-minute power chart, HR zone indicator, and session lifecycle REST routes (`/api/session/*`, `/api/upload/*`).
  - `DiagnosticsModule`, `RidePersistence` & `DiagnosticsRoutes`: PSRAM ring-buffer operational event log (`/api/echenon/events`) and optional LittleFS `.ride.gz` binary ride storage (`/api/echenon/runs`, streamed CSV export, oldest-first cap eviction).
  - Portal UI compliance (`web/`): Accessible WCAG AA contrast, semantic design tokens (`var(--token, #fallback)`), and Stored Rides management pill.
- **Portal Security & Model Context Protocol (`IMCPContributor`):**
  - `AuthManager::checkPortalAuth()` enforced across all `/api/echenon/*`, `/api/session/*`, `/api/upload/*`, `/api/strava/*`, `/api/intervals/*`, and `/api/wahoo/*` endpoints (`WO-09.1`).
  - `EchenonModule::registerMCPTools()` registers JSON-RPC MCP tools at `POST /api/mcp`: `echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, and `echenon_set_ftms_smoothing`.
  - `CalibrationRig` (`-DWITH_CALIBRATION_RIG`) migrated from legacy HTTP routes to `IMCPContributor` tools (`echenon_rig_status` and `echenon_rig_command`).

### Notes & Beta Constraints (`v0.3.0-beta.1`)
- **`WITH_OTA` Disabled for Beta Safety:** Wireless OTA updates are intentionally disabled in `v0.3.0-beta.1` (`platformio.ini`). All firmware and LittleFS updates during the beta period must be performed over **USB-C** with the mechanical `SW0` switch isolating bike `+9 V VIN`.
- **Passive Read-Only Scope:** `v0.3.0-beta.1` supports the v1 Passive Tap Board on the **Echelon EX-3** (`HAS_MOTOR_CONTROL = 0`). Resistance is adjusted manually via the bike's handlebar knob.
