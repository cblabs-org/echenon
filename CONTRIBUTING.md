# Contributing to Echenon (`v0.3.0-beta.1`)

Thank you for helping build and test **Echenon**! Whether you are an early DIY beta builder testing the **v1 Passive Tap Board** on your **Echelon EX-3**, reporting telemetry/BLE edge cases, or submitting firmware and portal improvements, your contributions are very welcome.

---

> [!CAUTION]
> ### ⚡ HARDWARE SAFETY FIRST: `SW0` Mechanical `VIN` / `USB-C` Interlock
>
> Before connecting a computer USB-C cable to your **Waveshare ESP32-S3-Nano** (`U1`) for flashing, serial logging, or bench debugging:
> - **Power the board from bike `+9 V VIN` OR computer `USB-C` — NEVER BOTH SIMULTANEOUSLY.**
> - Always flip your mechanical **`SW0` interlock switch** to **USB / VIN-OFF** (or unplug the bike's 9 V wall adapter completely) **before** plugging in USB-C.
> - Always unplug USB-C completely **before** flipping `SW0` back to **BIKE / VIN-ON**.
> - Note that wireless OTA (`WITH_OTA`) is intentionally disabled in **`v0.3.0-beta.1`**; all beta firmware and LittleFS updates are performed over USB-C with `SW0` isolating `VIN`.
> - Review [**`docs/beta-build-guide.md`**](docs/beta-build-guide.md) in full before powering any hardware.

---

## 1. Reporting Beta Issues & Hardware Feedback

When opening a GitHub Issue for a **`v0.3.0-beta.1`** build or ride test, please include:

1. **Firmware & Environment Details:**
   - Firmware version tag (`v0.3.0-beta.1` or `git describe --tags --always` commit SHA)
   - PlatformIO build environment (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, or `ex3-waveshare-nano-rig`)
   - Bike model & console PCB revision (e.g., **Echelon EX-3**, `S1001O-A03`)
   - Connected BLE apps / devices (e.g., Zwift on macOS/iOS, TrainerRoad, MyWhoosh, Garmin Edge, Echelon Beat `HW901` or Polar H10 strap)
2. **Diagnostic Snapshot:**
   - Output from the portal **Diagnostics** pill (`GET /api/echenon/events`, `GET /api/echenon/live`, or `POST /api/mcp` tool `echenon_get_telemetry`)
   - Your calibrated potentiometer endpoints (`pot_min` / `pot_max` or `GET /api/echenon/calibration/status`) if reporting a resistance or power accuracy issue
   - USB-CDC serial monitor log (`115200` baud, captured with `SW0` in **USB / VIN-OFF** if bench-testing)
3. **Privacy & Secret Scrubbing Checklist (Mandatory Before Posting):**
   - [ ] **Scrub all WiFi identifiers:** Remove personal home WiFi SSIDs, BSSIDs (MAC addresses), and local network hostnames/IPs if sensitive.
   - [ ] **Scrub all credentials & tokens:** Never paste portal passwords, `AuthManager` session cookies/tokens, Strava/Wahoo OAuth client secrets or tokens, or Intervals.icu API keys (firmware logs redact registered secrets automatically via `Redactor`, but always double-check raw HTTP/HAR dumps).

---

## 2. Developer Setup & Build Environments

### Cloning the Repository

Echenon depends on the [ESP32 App Foundation](https://github.com/cblabs-org/esp32-app-foundation) submodule under `framework/`. Clone with submodules enabled:

```bash
git clone --recurse-submodules https://github.com/cblabs-org/echenon.git
cd echenon
```

### Build & Test Commands ([`platformio.ini`](platformio.ini))

| Task | Command | Description |
|---|---|---|
| **Host Native Unit Tests** | `pio test -e native --verbose` | Runs the GoogleTest suite (`test/test_native/`) on your host machine without hardware |
| **Production v1 Beta Build** | `pio run -e ex3-waveshare-nano` | Standard `v0.3.0-beta.1` firmware (BLE FTMS + HR Relay, Session Recording, Strava/Intervals/Wahoo Upload, MCP tools; `WITH_OTA` disabled) |
| **Diagnostic Logger Build** | `pio run -e ex3-waveshare-nano-diag` | Adds `WITH_SIGNAL_LOGGER` (8-channel ADC1 DMA capture) and `WITH_HR_SYNTHETIC` |
| **Calibration Rig Build** | `pio run -e ex3-waveshare-nano-rig` | Adds `WITH_CALIBRATION_RIG` (`echenon_rig_status` & `echenon_rig_command` MCP tools) |
| **Flash Firmware + LittleFS** | `pio run -e ex3-waveshare-nano -t upload && pio run -e ex3-waveshare-nano -t uploadfs` | Uploads application binary and LittleFS web portal over USB-C (**`SW0` must be in `USB / VIN-OFF`!**) |

---

## 3. Repository Layout & Architecture Rules

```text
Echenon/
├── docs/                            Hardware guides, physics derivation & specifications
│   ├── beta-build-guide.md          Standalone v1 Passive Tap Board builder guide
│   ├── hardware_specification.md    Detailed circuit engineering & oscilloscope evidence
│   ├── ex3_specification.md         Echelon EX-3 stock hardware reference
│   ├── power-physics-model.md       First-principles Physics P3r power model derivation
│   ├── data-sinks.md                Multi-sink UploadQueue guide (Strava, Intervals, Wahoo)
│   ├── diagnostics.md               DiagnosticsModule event log guide
│   └── run-data-persistence.md      LittleFS .ride.gz persistence guide
├── framework/                       ESP32 App Foundation submodule
├── hardware/
│   └── ex3-waveshare-nano/          Target pin map (board.h), partitions.csv & sdkconfig.defaults
├── src/
│   ├── main.cpp                     Static composition root (ADR-16 / ADR-39)
│   └── echenon/                     Application modules (sensors, power, BLE, session, uploaders, MCP)
├── web/                             Portal & /ride UI assets (compiled to gzip PROGMEM by portalBuilder.py)
├── test/
│   └── test_native/                 Host-side GoogleTest unit test suites
├── SPEC.md                          Canonical system & firmware specification
└── platformio.ini                   PlatformIO build matrix
```

### Coding & Pull Request Guidelines
- **Service Module Pattern:** Keep module instances `static` in `src/main.cpp` and inject narrow dependencies (`ConfigManager*`, `AuthManager*`, `EventBus*`, `KeyManager*`) via explicit setters — never use `extern Application app;` inside `src/echenon/`.
- **Portal Security & MCP Parity:** Protect all `/api/echenon/*` and `/api/session/*` routes with `AuthManager::checkPortalAuth()`, and expose programmatic device capabilities via `IMCPContributor` (`POST /api/mcp`).
- **Portal CSS Tokens:** Use semantic CSS custom properties with fallbacks (`var(--token, #fallback)`) across all `web/` stylesheets and JS modules.
- **Native Test Floor:** Every change to pure-logic modules (`src/echenon/`) must pass `pio test -e native --verbose` with zero warnings or regressions, and all three firmware environments (`ex3-waveshare-nano`, `-diag`, `-rig`) must compile cleanly.
- **File Headers & License:** All source files carry the standard Apache-2.0 SPDX header (`SPDX-License-Identifier: Apache-2.0`). By submitting a pull request or contribution, you agree that your contribution is licensed under the [Apache License, Version 2.0](LICENSE).
