# Release Audit Report — Echenon `v0.3.0-beta.1`

## 1. Release Metadata & Candidate Summary

| Field | Value |
|---|---|
| **Application** | Echenon (`cblabs-org/echenon`) |
| **Target Release Tag** | `v0.3.0-beta.1` *(Owner-gated per `.agents/rules/03-versioning.md`; current latest tag: `v0.3.0-dev.47`)* |
| **Evaluated Commit** | `4ba5c82eb287daff0f9ae6507f2a2a6b7bf54d5e` (`PR-09` integration branch `pr-09-integration/wo8-release-audit-verify`) |
| **Date** | 2026-11-14 |
| **Audit Mode** | Consumer Application Compliance & Beta Release Quality Gate (`Beta` channel) |
| **Target Hardware** | v1 Passive Tap Board / Waveshare ESP32-S3-Nano (`um_nanos3`, ESP32-S3R8 — 16 MB Flash, 8 MB Octal PSRAM) |
| **Build Environments** | `ex3-waveshare-nano` (production beta), `ex3-waveshare-nano-diag` (signal logger + synthetic HR), `ex3-waveshare-nano-rig` (calibration rig), `native` (host unit test suite) |
| **`WITH_OTA` Status** | **Disabled for Beta** (`; -D WITH_OTA` commented out in [`platformio.ini`](../platformio.ini#L67)) — Beta firmware updates are performed exclusively via USB-C with the mechanical `SW0` (`VIN`) power interlock switched **OFF** (see [`docs/beta-build-guide.md`](beta-build-guide.md)) |
| **🟢 MEASURED Evidence** | **Committed HIL bench evidence** from Phase 2 & Phase 3 (`v0.3.0-dev.44` / `v0.3.0-dev.45` on `/dev/cu.usbmodem101`); **no USB board attached in this automated audit session** (`/dev/cu.debug-console` and `/dev/cu.Bluetooth-Incoming-Port` only) — final 60 s live bench soak on `v0.3.0-beta.1` candidate is gated in **Owner Launch Gate Step 1** prior to cutting the tag |
| **Overall Audit Verdict** | **PASS (Beta Channel Ready — Subject to Owner Pre-Tag 60 s Bench Soak & Tag Gate)** |

---

## 2. 10-Dimension Release Audit Matrix (`Beta` Channel)

Every dimension from the [ESP32 App Foundation Release Audit Protocol](../.agents/skills/framework-expert/guides/release-audit.md) has been evaluated against the integrated `PR-09` tree (`WO-09.1` through `WO-09.7`):

| Dim | Dimension Name | Status | Evidence Quality | Primary Evidence & Verification Basis |
|---|---|---|---|---|
| **D1** | **Documentation & House Style** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` + 🔴 `JUDGED` | All 63 authored C++ files in `src/` carry Apache-2.0 SPDX headers (`0` missing); `0` missing Doxygen `@brief`, `0` undocumented constants, `0` `TAG` mismatches, `0` directory/file mismatches; public OSS docs ([`LICENSE`](../LICENSE), [`README.md`](../README.md), [`CHANGELOG.md`](../CHANGELOG.md), [`CONTRIBUTING.md`](../CONTRIBUTING.md), [`docs/beta-build-guide.md`](beta-build-guide.md)) complete. |
| **D2** | **Coding Discipline & Encapsulation** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` + 🔴 `JUDGED` | `0` `extern Application` in `src/`; `0` stored `Application*`; `0` `Application.hpp` in headers; `0` sibling module imports; `0` public member variables; `0` Law-of-Demeter violations; all 13 `std::function` uses in `src/echenon/` carry `// GRAVE-05:` cold-path/boot-bound justification comments; single `extern uint32_t millis();` in [`src/echenon/FTMSServer.cpp:37`](../src/echenon/FTMSServer.cpp#L35-L38) guarded by `#ifdef UNIT_TEST`. |
| **D3** | **Framework Best Practices** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | Single `#include "AppFoundation.h"` facade in [`src/main.cpp:8`](../src/main.cpp#L8) and [`src/echenon/EchenonModule.h:10`](../src/echenon/EchenonModule.h#L10); all modules registered in `setup()` before `app.begin(config)` compact-and-lock ([`src/main.cpp:105-192`](../src/main.cpp#L105-L192)); `EchenonModule` implements `ITickable`, `IConfigurable`, `IWebContributor`, and `IMCPContributor` (7 MCP tools). |
| **D4** | **DRAM & Memory Budget** | **PASS** | 🟡 `MECHANICAL` + 🟢 `MEASURED` | Static internal DRAM is `58,884 B / 327,680 B` (`18.0%`) on `ex3-waveshare-nano`; largest `.dram0` symbol is `g_cnxMgr` (`3,880 B`, `1.18%` of DRAM, well below the 10% guidance ceiling); `CONFIG_SPIRAM=y` (`8 MB` Octal PSRAM) active; Phase 3 bench soak ([`pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md#L79-L83)) measured `102,895 B` free internal DRAM (`min_free = 60,844 B`, >4× the 15 KB floor) and `8,267,732 B` free PSRAM. |
| **D5** | **Security** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | `0` committed secrets; `0` `KeyManager` bypasses; `0` `setInsecure()` in `src/`; 23 `AuthManager::checkPortalAuth()` guards across all sensitive REST routes in `EchenonModule`, `SignalLogger`, `DiagnosticsRoutes`, and `SessionOverlay`; all `/api/echenon/rig/*` HTTP endpoints removed (`0` matches) and migrated to authenticated `IMCPContributor` tools; `0` personal SSIDs/BSSIDs/URLs outside `.agents/`. |
| **D6** | **Web Portal & Design System** | **PASS** | 🟡 `MECHANICAL` + 🔴 `JUDGED` + 🟢 `MEASURED` | `0` bare hex colors in `web/*.js` (all CSS colors use `var(--token, #fallback)` design tokens; only standalone PWA `web/ride.html` `:root` token definitions and `web/manifest.json` metadata contain literal hex); `0` missing ARIA tabpanel roles; Feather SVG iconography; `window.apiFetch` auth wrapper used across portal scripts. |
| **D7** | **Build Integrity & Test Coverage** | **PASS** | 🟡 `MECHANICAL` | `269 / 269` native unit tests pass (`0` failed, `0` errored, `0` skipped in `0.54 s`) across all 20 non-dormant test suites in `test/test_native/`; all 3 ESP32-S3 environments (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`) compile and link cleanly (`success: true`). |
| **D8** | **Parallel Mechanism Detection** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | `0` stale `lib_deps`; no duplicate drivers or parallel infrastructure — reuses framework `EncoderModule` (PCNT), `BLEManager` (`IBLEServiceProvider` / `IBLEClientProvider` / `ScanInhibit`), `ConfigManager`, `KeyManager`, `AuthManager`, `EventBus`, `TimeManager`, `DataManager`, and `MCPToolRegistry`. |
| **D9** | **Config Shadowing** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | All `SDKCONFIG_DEFAULTS` (`sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults`) and `build_flags` verified intentional: `CONFIG_SPIRAM=y`, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4`, `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (SPEC §7.8), headless operation (no `LV_CONF_PATH` / display / button flags per SPEC §12.3). |
| **D10** | **Breaking Change & Migration** | **PASS WITH BETA WAIVER** | 🟡 `MECHANICAL` + 🟠 `TRACED` | First external beta release (`v0.3.0-beta.1`); partition table ([`hardware/ex3-waveshare-nano/partitions.csv`](../hardware/ex3-waveshare-nano/partitions.csv)) unchanged (`app0`/`app1` 6 MB each, `config` 1 MB, `storage` 2.75 MB); `CalibrationRig` HTTP routes migrated to MCP (`echenon_rig_status`, `echenon_rig_command`); OTA upgrade boot-log check waived for beta because `WITH_OTA` is intentionally disabled in favor of USB-C updates with `SW0` `VIN` isolated. |

> [!NOTE]
> **Evidence Grading Disclosure:** No dimension is graded solely from `🔴 JUDGED` evidence. Every dimension is anchored by `🟡 MECHANICAL` automated checks and/or `🟠 TRACED` code-path inspection, with `🟢 MEASURED` hardware telemetry from committed Phase 2/3 HIL runs and a mandatory pre-tag 60 s bench soak gate on the final candidate binary.

---

## 3. Detailed Per-Dimension Findings & Analysis

### Dimension 1 — Documentation & House Style (`PASS`)

- **File Headers (`🟡 MECHANICAL`):** All 63 C++ source and header files under `src/` (`src/main.cpp` + 62 files in `src/echenon/`) include the canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` header block (`find src -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) | wc -l` = `63`; `grep -rnL "SPDX-License-Identifier: Apache-2.0" src/` returns only `src/CMakeLists.txt`).
- **Doxygen & Constants (`🟡 MECHANICAL`):** `release-audit-checks.sh` reports `missing_doxygen: PASS (0 hits)`, `undocumented_constants: PASS (0 hits)`, `tag_mismatch: PASS (0 hits)`, and `directory_file_mismatch: PASS (0 hits)`.
- **Public Repository & Builder Documentation (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - [`LICENSE`](../LICENSE) — Apache-2.0 license text (`12,554 B`).
  - [`README.md`](../README.md) — Beta tester recruitment guide, hardware overview, v1 stock runaway limitation disclosure, and mechanical `VIN`/`USB-C` power switch procedure (`14,112 B`).
  - [`CHANGELOG.md`](../CHANGELOG.md) — `v0.3.0-beta.1` release notes and milestone history (`6,416 B`).
  - [`CONTRIBUTING.md`](../CONTRIBUTING.md) — Developer setup, native test floor, and PR workflow (`6,860 B`).
  - [`docs/beta-build-guide.md`](beta-build-guide.md) — Standalone v1 Passive Tap Board builder guide with BOM (`C1 ×5` 100 nF ceramic decoupling errata synchronized with [`docs/hardware_specification.md`](hardware_specification.md)), netlist, stripboard assembly, and `SW0` (`VIN`) safety interlock instructions (`29,695 B`).
- **Advisory Notes (Non-Blocking):**
  - `release-audit-checks.sh` flags `missing_file_header` on the 20 unit test files in `test/test_native/*.cpp` and 1 standalone BLE peripheral test sketch in `tools/ble-test/control-peripheral/src/main.cpp`. All 63 production/diagnostic/rig firmware files in `src/` pass 100%.
  - `member_naming` hits reported by the script (`115` hits) reside entirely inside the upstream `framework/` submodule (`0` hits in `src/`).

### Dimension 2 — Coding Discipline & Encapsulation (`PASS`)

- **Zero Global `Application` Coupling (`🟡 MECHANICAL`):**
  - `grep -rn "extern Application" src/` → **0 matches**.
  - `stored_application_ptr: PASS (0 hits)`, `application_hpp_in_header: PASS (0 hits)`, `sibling_module_import: PASS (0 hits)`, `public_member_variables: PASS (0 hits)`, `law_of_demeter: PASS (0 hits)`.
  - All top-level module instances in [`src/main.cpp:15-56`](../src/main.cpp#L15-L56) (`bleManager`, `encoderModule`, `timeManager`, `timePrecondition`, `app`, `echenonModule`, `rideStore`, `sessionManager`, `uploadQueue`, `sessionOverlay`, `ridePersistence`, `stravaUploader`) are declared `static` at file scope.
- **Guarded Test Extern (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - The only `extern` symbol in `src/` is `extern uint32_t millis();` in [`src/echenon/FTMSServer.cpp:37`](../src/echenon/FTMSServer.cpp#L35-L38), which is strictly compiled only under `#if !defined(ESP_PLATFORM)` and `#ifdef UNIT_TEST` for host-side unit testing.
- **`std::function` (`GRAVE-05`) Audit (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - All 13 `std::function` type aliases/callbacks in `src/echenon/` carry explicit `// GRAVE-05:` justification comments confirming they are either bound once at boot (stateless/SBO-small lambdas `<= 16 B` invoked only on cold background paths such as `DataManager::fetchUpdate()`, OAuth token refresh, or session end) or passed as transient stack-scoped parameters in `TCXGenerator`:
    - [`src/echenon/UploadQueue.h:105-112`](../src/echenon/UploadQueue.h#L105-L112) (`NowEpochFn`, `NtpSyncedFn`, `TcxGenerateFn`)
    - [`src/echenon/StravaUploader.h:164-173`](../src/echenon/StravaUploader.h#L164-L173) (`SecretRegistrarFn`, `MetadataSaveFn`, `RandomU32Fn`)
    - [`src/echenon/IntervalsIcuUploader.h:43-44`](../src/echenon/IntervalsIcuUploader.h#L43-L44) (`SecretRegistrarFn`)
    - [`src/echenon/WahooUploader.h:50-59`](../src/echenon/WahooUploader.h#L50-L59) (`SecretRegistrarFn`, `MetadataSaveFn`, `RandomU32Fn`)
    - [`src/echenon/TCXGenerator.h:23-31`](../src/echenon/TCXGenerator.h#L23-L31) (`WriteChunkFn`, `DeadlineCheckFn` — stack-scoped only)
    - [`src/echenon/SessionManager.h:120-121`](../src/echenon/SessionManager.h#L120-L121) (`SessionCompletedCallback`)
  - Zero `std::function` instances are used in `tick()` hot paths or `IMCPContributor` tool handlers (which use stateless C-style lambdas with `void* ctx`).

### Dimension 3 — Framework Best Practices (`PASS`)

- **Facade & Lifecycle Ordering (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - Application code includes `"AppFoundation.h"` ([`src/main.cpp:8`](../src/main.cpp#L8), [`src/echenon/EchenonModule.h:10`](../src/echenon/EchenonModule.h#L10)).
  - Every `app.registerModule(...)` and `bleManager.registerServiceProvider(...)` / `registerClientProvider(...)` call in [`src/main.cpp:105-187`](../src/main.cpp#L105-L187) executes before `app.begin(config)` ([`src/main.cpp:192`](../src/main.cpp#L192)), respecting the framework's compact-and-lock registration contract.
- **Calibration Persistence & Physics Synchronization (`🟠 TRACED`):**
  - [`EchenonModule::syncPhysicsCalibration()`](../src/echenon/EchenonModule.cpp#L287-L293) propagates fitted OLS potentiometer endpoints (`_calibrationWizard.getPotMin()`, `_calibrationWizard.getPotMax()`) and recalibration eddy-current scaling (`_recalibrationUI.applyToPhysicsParams(params)`) into `_powerEngine.setPhysicsParams(params)`.
  - Invoked on initial `tick()` startup ([`src/echenon/EchenonModule.cpp:89`](../src/echenon/EchenonModule.cpp#L89)), `applyConfig()` ([`src/echenon/EchenonModule.cpp:321`](../src/echenon/EchenonModule.cpp#L321)), `resetConfig()` ([`src/echenon/EchenonModule.cpp:383`](../src/echenon/EchenonModule.cpp#L383)), REST calibration record/reset and recalibration point/reset routes ([`src/echenon/EchenonModule.cpp:502-608`](../src/echenon/EchenonModule.cpp#L502-L608)), and MCP `echenon_calibration` / `echenon_recalibration` tools ([`src/echenon/EchenonModule.cpp:863-930`](../src/echenon/EchenonModule.cpp#L863-L930)), followed by `_configManager->saveModuleConfig(this)`.
- **MCP Tool Registration (`🟠 TRACED`):**
  - `EchenonModule::registerMCPTools(MCPToolRegistry&)` ([`src/echenon/EchenonModule.cpp:782-1097`](../src/echenon/EchenonModule.cpp#L782-L1097)) registers 5 core tools in production builds (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`) plus 2 rig tools under `WITH_CALIBRATION_RIG` (`echenon_rig_status`, `echenon_rig_command`), with accurate `mutatesState` flags for read-only vs state-mutating operations.

### Dimension 4 — DRAM & Memory Budget (`PASS`)

- **Static DRAM & Flash Footprint (`🟡 MECHANICAL`):**
  - Across all 3 ESP32-S3 build environments, static internal RAM usage is `58,884 B`–`59,188 B` (`18.0%`–`18.1%` of `327,680 B`), leaving `> 268 KB` of static DRAM headroom prior to dynamic stack/heap initialization.
- **ELF Largest-Symbol Check (`🟡 MECHANICAL`):**
  - Inspected via `xtensa-esp32s3-elf-size -A` and `xtensa-esp32s3-elf-objdump -t` on `.pio/build/ex3-waveshare-nano/firmware.elf`: the largest `.dram0` symbol is `g_cnxMgr` at `3,880 B` (`1.18%` of 320 KB internal DRAM), well below the 10% (`~32.7 KB`) advisory threshold.
- **PSRAM & Runtime Heap Headroom (`🟡 MECHANICAL` + `🟢 MEASURED`):**
  - `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, and `CONFIG_SPIRAM_SPEED_80M=y` are active in [`hardware/ex3-waveshare-nano/sdkconfig.defaults:9-12`](../hardware/ex3-waveshare-nano/sdkconfig.defaults#L9-L12).
  - `RideStore` allocates its 14,400-record ring buffer in Octal PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`), keeping internal DRAM free for NimBLE and WiFi/TLS.
  - Committed Phase 3 hardware verification on `/dev/cu.usbmodem101` ([`docs/measurements/phase3/evidence/pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md#L79-L83)) measured:
    - **Internal DRAM free:** `102,895 B`
    - **Internal DRAM minimum free (`dram_min`):** `60,844 B` (exceeds both the 15 KB framework guidance and the 40 KB Echenon SPEC mandate)
    - **PSRAM free:** `8,267,732 B` / `8,388,608 B`

### Dimension 5 — Security (`PASS`)

- **Portal Authentication Coverage (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - Every sensitive application HTTP endpoint enforces `AuthManager::checkPortalAuth(request, response)` (`23` call sites verified across `src/echenon/`):
    - [`src/echenon/EchenonModule.cpp`](../src/echenon/EchenonModule.cpp#L463-L748) — 13 routes: `/api/echenon/calibration/{start,stop,record-level,reset,status}`, `/api/echenon/recalibration/{point,points,reset}`, `/api/echenon/power-table/{upload,active}`, `/api/echenon/live`, `/api/echenon/ftms-smoothing` (`GET` & `POST`).
    - [`src/echenon/SignalLogger.cpp`](../src/echenon/SignalLogger.cpp#L363-L505) — 5 routes (`WITH_SIGNAL_LOGGER` diagnostic builds): `/api/echenon/status` (`GET`), `/api/echenon/burst/start` (`POST`), `/api/echenon/ride-log.csv` (`GET`), `/api/echenon/events.csv` (`GET`), and `/api/echenon/burst.csv` (`GET`).
    - [`src/echenon/DiagnosticsRoutes.cpp`](../src/echenon/DiagnosticsRoutes.cpp#L132-L373) — 4 routes: `/api/echenon/diagnostics/events` (`GET` & `DELETE`), `/api/echenon/diagnostics/rides` (`GET`), `/api/echenon/diagnostics/rides/*` (`GET` & `DELETE`).
    - [`src/echenon/SessionOverlay.cpp`](../src/echenon/SessionOverlay.cpp#L1271) — `checkAuth_()` helper guarding all `/api/session/*`, `/api/strava/*`, `/api/intervals/*`, `/api/wahoo/*`, and `/api/upload/*` endpoints (with OAuth redirect callbacks `/api/strava/callback` and `/api/wahoo/callback` additionally protected by single-use 128-bit cryptographic state nonces and 600 s TTLs).
- **CalibrationRig HTTP Surface Elimination (`🟡 MECHANICAL`):**
  - `grep -rn "/api/echenon/rig/" src/ web/ tools/` returns **0 matches**. All rig control and telemetry operate through the framework's authenticated `/api/mcp` endpoint (`echenon_rig_status`, `echenon_rig_command`) or local USB Serial.
- **Credential Storage & Redaction (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - `committed_secrets: PASS (0 hits)`, `key_manager_bypass: PASS (0 hits)`.
  - Third-party cloud tokens/keys (Strava, Intervals.icu, Wahoo Cloud) are stored in `KeyManager` (`DeviceCrypto` AES-GCM NVS keystore) and registered with `Redactor::registerSecret()` upon load and refresh.
  - Repository-wide privacy check (`git grep -n -E "<REDACTED_SSID>|<REDACTED_OUI>|<REDACTED_SESSION_URL>" -- ":!.agents"`) returns **0 matches**.

### Dimension 6 — Web Portal & Design System (`PASS`)

- **Design Token Compliance (`🟡 MECHANICAL`):**
  - `grep -rn -E "#[0-9a-fA-F]{3,8}" web/ | grep -v "var(--"` confirms **0 bare hex colors** across all 10 JavaScript portal modules (`web/*.js`).
  - Every color reference in `web/calibration-wizard.js`, `web/diagnostics.js`, `web/echenon-dashboard.js`, `web/intervals_settings.js`, `web/recalibration.js`, `web/ride.js`, `web/session.js`, `web/stored_rides.js`, `web/upload_settings.js`, and `web/wahoo_settings.js` uses `var(--token, #fallback)`.
  - The only non-`var(--...)` hex strings in `web/` are the `:root` CSS custom property declarations and `<meta name="theme-color">` in standalone full-screen [`web/ride.html:1-2`](../web/ride.html#L1-L2) and PWA [`web/manifest.json:1`](../web/manifest.json#L1).
- **Accessibility, Iconography & Authenticated Fetch (`🟡 MECHANICAL` + `🔴 JUDGED`):**
  - `missing_aria_role: PASS (0 hits)`.
  - Top-level tabs (`Echenon`, `Apps`) use standard 24×24 Feather SVG `<polyline>` / `<rect>` primitives ([`src/echenon/EchenonModule.cpp:1105-1122`](../src/echenon/EchenonModule.cpp#L1105-L1122)).
  - Portal scripts use `window.apiFetch` for authenticated API requests, including `POST /api/echenon/calibration/reset` in [`web/calibration-wizard.js`](../web/calibration-wizard.js).

### Dimension 7 — Build Integrity & Test Coverage (`PASS`)

- **Native Unit Test Suite (`🟡 MECHANICAL`):**
  - Executed via `platformio-mcp_platformio` `run_tests` (`environment: "native"`): **269 test cases, 269 passed, 0 failed, 0 errored, 0 skipped** (`0.54 s`) across 20 test suites in `test/test_native/`. Every suite has `> 0` executing test cases (zero dormant suites).
- **Multi-Environment Firmware Build Matrix (`🟡 MECHANICAL`):**
  - Executed via `platformio-mcp_platformio` `build_project` across all 3 hardware environments (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`): all 3 environments build and link with `success: true`.

### Dimension 8 — Parallel Mechanism Detection (`PASS`)

- **Framework Reuse Verification (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - `stale_lib_deps: PASS (0 hits)`.
  - Rotary knob decoding uses the framework's `EncoderModule` (`PCNT` hardware quadrature unit configured via `BOARD_ENCODERS` in [`hardware/ex3-waveshare-nano/board.h:34-43`](../hardware/ex3-waveshare-nano/board.h#L34-L43)).
  - BLE GATT server (`HRMRepeater`, `FTMSServer`) and central (`HRMClient`) roles integrate through `BLEManager` (`IBLEServiceProvider`, `IBLEClientProvider`) and use `BLEManager::ScanInhibit` during cloud TLS uploads in `UploadQueue`.
  - Background cloud uploads use the framework's `IDataSource` / `DataManager` (`FetchContext` + `DeadlineToken` + `FrameworkSecureClient`).

### Dimension 9 — Config Shadowing (`PASS`)

- **SDKCONFIG & Build Flag Audit (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - `platformio.ini` chains `SDKCONFIG_DEFAULTS="sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"`.
  - [`hardware/ex3-waveshare-nano/sdkconfig.defaults`](../hardware/ex3-waveshare-nano/sdkconfig.defaults) sets:
    - `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` (16 KB main task stack for S3)
    - `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, `CONFIG_SPIRAM_SPEED_80M=y`, `CONFIG_SPIRAM_MEMTEST=y` (8 MB Octal PSRAM enabled)
    - `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` (16 MB Flash)
    - `CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=8192`
    - `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (Waveshare ESP32-S3-Nano native USB-C CDC/JTAG)
    - `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` and `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (3 peripheral + 1 central connection budget per SPEC §7.8)
  - No `LV_CONF_PATH` override is present because the v1 Passive Tap Board is a headless controller (`WITH_DISPLAY_U8G2` and `WITH_BUTTONS` are intentionally omitted per SPEC §12.3).

### Dimension 10 — Breaking Change & Migration (`PASS WITH BETA WAIVER`)

- **Partition Table Stability (`🟡 MECHANICAL`):**
  - [`hardware/ex3-waveshare-nano/partitions.csv`](../hardware/ex3-waveshare-nano/partitions.csv) matches the 16 MB dual-OTA layout (`nvs` `0x5000`, `otadata` `0x2000`, `app0` `6 MB`, `app1` `6 MB`, `config` `1 MB` LittleFS, `storage` `2.75 MB` LittleFS, `coredump` `64 KB`).
- **API & Tooling Migration (`🟠 TRACED`):**
  - Legacy unauthenticated `/api/echenon/rig/*` HTTP endpoints were retired in `WO-09.1` and replaced by `IMCPContributor` tools `echenon_rig_status` and `echenon_rig_command` (`/api/mcp`), with Python rig scripts in `tools/calibration-rig/` updated in lockstep.
- **Beta OTA Waiver (`🟠 TRACED`):**
  - Per `PROJECT.md` owner design decision and [`SPEC.md`](../SPEC.md), `WITH_OTA` is disabled for the v1 Beta release (`v0.3.0-beta.1`). Beta builders flash updates over USB-C with `SW0` (`VIN`) switched OFF. Consequently, the OTA upgrade boot-log capture check is waived for `v0.3.0-beta.1` and deferred to the post-beta OTA enablement milestone.

---

## 4. Verification Output Summary

### 4.1 Native Unit Test Suite (`platformio-mcp_platformio` → `run_tests`)

| Environment | Test Suites (`test/test_native/`) | Total Cases | Passed | Failed | Errored | Skipped | Duration | Result |
|---|---|---|---|---|---|---|---|---|
| `native` | 20 suites (`test_cadence_sensor`, `test_diagnostics_module`, `test_diagnostics_routes`, `test_ftms_server`, `test_hr_encoding`, `test_hr_parsing`, `test_intervals_uploader`, `test_level_mapping`, `test_power_engine`, `test_quadrature_generator`, `test_recalibration`, `test_ride_persistence`, `test_runaway_detector`, `test_session_manager`, `test_signal_logger`, `test_signal_logic`, `test_strava_uploader`, `test_tcx_generator`, `test_upload_queue`, `test_wahoo_uploader`) | **269** | **269** | **0** | **0** | **0** | `0.54 s` | ✅ **PASS** |

### 4.2 Multi-Environment Firmware Build Matrix (`platformio-mcp_platformio` → `build_project`)

| Environment | Feature Flags Beyond Base | Build Status | Static RAM (`ramUsageBytes`) | RAM % (`327,680 B` max) | Flash (`flashUsageBytes`) | Flash % (`6,291,456 B` `app0`) |
|---|---|---|---|---|---|---|
| `ex3-waveshare-nano` | Production v1 Beta (`WITH_BLE`, `WITH_ENCODER`, `WITH_HR_RELAY`, `WITH_FTMS`, `WITH_SESSION`, `WITH_STRAVA`, `WITH_TIME`) | ✅ `success: true` | **58,884 B** | `18.0%` | **1,747,615 B** | `27.8%` |
| `ex3-waveshare-nano-diag` | Base + `-DWITH_SIGNAL_LOGGER -DWITH_HR_SYNTHETIC` | ✅ `success: true` | **59,188 B** | `18.1%` | **1,764,811 B** | `28.1%` |
| `ex3-waveshare-nano-rig` | Base + `-DWITH_CALIBRATION_RIG` | ✅ `success: true` | **58,972 B** | `18.0%` | **1,752,735 B** | `27.9%` |

### 4.3 ELF Section & Top `.dram0` Symbol Inspection (`ex3-waveshare-nano`)

> **Toolchain Note:** PlatformIO MCP `size_report` returned `ANALYSIS_ELF_MISMATCH`; exact section and symbol sizes below were extracted directly from `.pio/build/ex3-waveshare-nano/firmware.elf` using `xtensa-esp32s3-elf-size -A` and `xtensa-esp32s3-elf-objdump -t`.

| ELF Section | Size (Bytes) | Description |
|---|---|---|
| `.dram0.data` | `20,028 B` | Initialized static internal DRAM |
| `.dram0.bss` | `38,856 B` | Zero-initialized static internal DRAM |
| **Static DRAM Total (`.data` + `.bss`)** | **`58,884 B`** | **`18.0%` of `327,680 B` (`268,796 B` static headroom)** |
| `.iram0.text` | `117,147 B` | Instruction RAM (ISR-safe & critical hot code) |
| `.flash.text` | `1,252,252 B` | XIP Flash code segment |
| `.flash.rodata` | `357,192 B` | Flash read-only data & gzipped portal assets |

| Rank | Symbol Name | Section | Size (Bytes) | % of 320 KB Internal DRAM | Threshold (`< 10%`) |
|---|---|---|---|---|---|
| 1 | `g_cnxMgr` | `.dram0.bss` | `3,880 B` | `1.18%` | ✅ PASS |
| 2 | `app` (`Application`) | `.dram0.bss` | `3,344 B` | `1.02%` | ✅ PASS |
| 3 | `Redactor::_instance` | `.dram0.bss` | `3,172 B` | `0.97%` | ✅ PASS |
| 4 | `port_IntStack` | `.dram0.bss` | `3,072 B` | `0.94%` | ✅ PASS |
| 5 | `echenonModule` (`EchenonModule`) | `.dram0.bss` | `2,872 B` | `0.88%` | ✅ PASS |

### 4.4 Static Hygiene, Encapsulation & Security Audit Checks

| Check Description | Command Executed | Observed Result | Verdict |
|---|---|---|---|
| Authored `src/` file count & Apache-2.0 headers | `find src -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \| wc -l` / `grep -rnL "SPDX-License-Identifier: Apache-2.0" src/` | `63` C++ files; `0` missing headers | ✅ PASS |
| Global `extern Application` ban | `grep -rn "extern Application" src/` | `0` matches | ✅ PASS |
| Unscoped `extern` check in `src/` | `grep -rn "extern " src/` | `1` match (`FTMSServer.cpp:37`, guarded by `#ifdef UNIT_TEST`) | ✅ PASS |
| `std::function` `GRAVE-05` justification comments | `grep -rn -B 2 -A 1 "std::function" src/` | `13` occurrences across 6 headers; all `13` carry `// GRAVE-05:` comments | ✅ PASS |
| Legacy `/api/echenon/rig/` HTTP endpoint removal | `grep -rn "/api/echenon/rig/" src/ web/ tools/` | `0` matches | ✅ PASS |
| `AuthManager::checkPortalAuth()` route coverage | `grep -rn "checkPortalAuth" src/` | `23` guarded endpoints/helpers (`EchenonModule`: 13, `SignalLogger`: 5, `DiagnosticsRoutes`: 4, `SessionOverlay`: 1) | ✅ PASS |
| Bare `#hex` color check in `web/` | `grep -rn -E "#[0-9a-fA-F]{3,8}" web/ \| grep -v "var(--"` | `0` matches in `web/*.js` (only `web/ride.html` `:root` / `<meta>` and `web/manifest.json`) | ✅ PASS |
| Personal SSID / BSSID / URL redaction | `git grep -n -E "<REDACTED_SSID>\|<REDACTED_OUI>\|<REDACTED_SESSION_URL>" -- ":!.agents"` | `0` matches | ✅ PASS |
| `WITH_OTA` disabled for Beta | `grep -rn "WITH_OTA" platformio.ini` | `; -D WITH_OTA` commented out at `platformio.ini:67` | ✅ PASS |

### 4.5 Hardware-in-the-Loop (HIL) Evidence & Current Bench Status

- **Current Session `list_devices` Check (`platformio-mcp_platformio`):**
  - Only `/dev/cu.debug-console` and `/dev/cu.Bluetooth-Incoming-Port` are present on the host; **no Waveshare ESP32-S3-Nano is attached over USB-C during this automated verification turn**.
- **Committed Hardware Verification Baseline:**
  - **Phase 2 BLE Strap & Coexistence Verification:**
    - [`docs/measurements/phase2/wo5-strap-acceptance/evidence.md`](measurements/phase2/wo5-strap-acceptance/evidence.md) — Physical HRM strap discovery, connection, notification relay, and disconnect recovery verified on hardware.
    - [`docs/measurements/phase2/wo6-coexistence/evidence.md`](measurements/phase2/wo6-coexistence/evidence.md) — Concurrent WiFi STA + WebSocket portal + BLE Central (`HRMClient`) + BLE Peripheral (`HRMRepeater` / `FTMSServer`) RF coexistence verified.
  - **Phase 3 Multi-Sink & Portal Soak Verification (`/dev/cu.usbmodem101`):**
    - [`docs/measurements/phase3/evidence/pr19-hardware-verification.md`](measurements/phase3/evidence/pr19-hardware-verification.md) (`v0.3.0-dev.44`) — Dual-sink (`StravaUploader` + `IntervalsIcuUploader`) pipeline, LittleFS `/littlefs` (2.75 MB) & `/config` (1.0 MB) mounts, `KeyManager` + `DeviceCrypto` keystore, and BLE HR/FTMS coexistence verified.
    - [`docs/measurements/phase3/evidence/pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md) (`v0.3.0-dev.45`) — Triple-sink (`Strava`, `Intervals.icu`, `Wahoo Cloud`) registration, OAuth2 128-bit state nonce generation, gzipped portal asset serving (`ETag` + `Content-Encoding: gzip`), and runtime memory stability (`dram_free = 102,895 B`, `dram_min = 60,844 B`, `psram_free = 8,267,732 B`) verified on target hardware.
- **Pre-Tag Live Bench Soak Requirement:**
  - Because `WO-09.1` through `WO-09.4` introduced `IMCPContributor` tool registration, `AuthManager::checkPortalAuth()` guards on `/api/echenon/*`, calibration reset persistence, and portal CSS token updates after `v0.3.0-dev.45`, the Owner **must** run the **60 s Live Bench Soak Gate** (Step 1 below) on attached hardware before cutting `v0.3.0-beta.1`.

---

## 5. Owner Launch Gate Checklist (Explicit Execution Order)

Per [`.agents/rules/03-versioning.md`](../.agents/rules/03-versioning.md) and programme governance, `beta` tags, `main` merges, and public repository flips require explicit human execution. Complete the steps below **in exact sequential order**:

### Step 1 — Mandatory Pre-Tag 60 s Live Bench Soak Gate (`ex3-waveshare-nano`)
> [!CAUTION]
> **Power Interlock Safety Rule:** Before connecting a USB-C cable between your computer and the Waveshare ESP32-S3-Nano on an installed v1 Passive Tap Board, **verify that mechanical switch `SW0` (`VIN`) is switched OFF** (isolating the bike's 9 V / MP1584EN 5 V rail from USB `VBUS`). Never connect `VIN` and `USB-C` simultaneously.

- [ ] **1.1** Verify `SW0` (`VIN` disconnect switch) is **OFF**, then connect the Waveshare ESP32-S3-Nano via USB-C.
- [ ] **1.2** Tag a pre-flash dev build if flashing via MCP (per Rule 03 Flash Gate), build, and flash `ex3-waveshare-nano` to the board.
- [ ] **1.3** Monitor serial boot output for **60 seconds** and confirm:
  - Clean boot transition `BOOT` → `CONFIGURING` → `RUNNING` with zero panics, WDT resets, or brownouts.
  - LittleFS partitions `/config` and `/littlefs` mount cleanly.
  - `GET /api/system` reports `firmware_version` matching the flashed tag, free internal DRAM **`> 40,000 B`** (`dram_min > 15,000 B`), and 8 MB Octal PSRAM active.
- [ ] **1.4** Verify Portal Auth & MCP Smoke Check:
  - Confirm unauthenticated `GET /api/echenon/live` returns `401 Unauthorized` when portal auth is enabled, and succeeds (`200 OK`) with valid session/credentials.
  - Call `POST /api/mcp` (`tools/list` and `tools/call` → `echenon_get_telemetry`) and confirm all Echenon MCP tools respond cleanly.
  - Load the web portal (`http://echenon.local`), verify **Dashboard**, **Stored Rides**, **Apps** (`Strava`, `Intervals.icu`, `Wahoo Cloud`), and **System** (`Level Mapping`, `Power Tuning`, `Diagnostics`) tabs render with proper dark/light theme tokens.
- [ ] **1.5** Confirm BLE advertising is active (`Echenon` advertising FTMS `0x1826`, HRS `0x180D`, and DIS `0x180A`) and a clean hardware reset boots back to `RUNNING`.

### Step 2 — Verify Upstream Framework Submodule Visibility
- [ ] **2.1** Confirm `matthewmcneill/esp32-app-foundation` (referenced in `.gitmodules` at `framework`) is flipped to **Public** on GitHub and accessible to external clones without authentication.

### Step 3 — Review Step 6A Audit & Merge `PR-09` to `main`
- [ ] **3.1** Review the Principal Architect Step 6A Cold-Firewall Audit report and this release audit (`docs/release-audit-v0.3.0-beta.1.md`).
- [ ] **3.2** Merge `feat/pr-09-integration` (`PR-09`) into `main` and pull `main` locally.

### Step 4 — Cut & Push the `v0.3.0-beta.1` Tag (Owner CLI)
- [ ] **4.1** On clean `main` HEAD, verify the latest tag and create the annotated beta tag:
  ```bash
  git checkout main && git pull origin main
  git describe --tags --abbrev=0
  git tag -a v0.3.0-beta.1 -m "Echenon v0.3.0-beta.1 — v1 Passive Tap Board Beta Release"
  git push origin v0.3.0-beta.1
  ```
- [ ] **4.2** Rebuild `ex3-waveshare-nano` so `inject_version.py` bakes `v0.3.0-beta.1` into the release firmware binary, flash over USB-C (`SW0` OFF), and confirm `GET /api/system` reports `"firmware_version": "v0.3.0-beta.1"`.

### Step 5 — Flip `cblabs-org/echenon` Public & Publish Beta Release
- [ ] **5.1** Flip `cblabs-org/echenon` repository visibility to **Public** on GitHub.
- [ ] **5.2** Publish the GitHub Pre-Release for tag `v0.3.0-beta.1` using the [`CHANGELOG.md`](../CHANGELOG.md) `v0.3.0-beta.1` notes, attaching the pre-built `ex3-waveshare-nano` firmware binary (`firmware.bin`), `partitions.bin`, `bootloader.bin`, and pointing builders to [`docs/beta-build-guide.md`](beta-build-guide.md).
