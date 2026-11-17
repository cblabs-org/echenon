# Release Audit Report — Echenon `v0.3.0-beta.1`

## 1. Release Metadata & Candidate Summary

| Field | Value |
|---|---|
| **Application** | Echenon (`cblabs-org/echenon`) |
| **Target Release Tag** | `v0.3.0-beta.1` *(Owner-gated per `.agents/rules/03-versioning.md`; current latest verified dev tag: `v0.3.0-dev.51`)* |
| **Evaluated Commit** | `802768575082b2e324637d542d7470e9d3a20e9d` (`PR-09` integration branch `feat/pr-09-integration`, `WO-09.14`, receipt `rcpt_1ea7627b8a9e`, tag `v0.3.0-dev.51`) |
| **Framework Submodule** | [`cblabs-org/esp32-app-foundation`](https://github.com/cblabs-org/esp32-app-foundation) `@ v1.0.0-beta.2` (`02d0b2de` / `02d0b2dea141313a0ce06b62f75bf2701559990a`, **Public**: `isPrivate: false`; aligned in `WO-09.12` with upstream `#320`, `#321`, `#230`, `#329`) |
| **Platform Toolchain** | `pioarduino/platform-espressif32` `55.03.311` (`ESP-IDF 5.5.5`, `Arduino 3.3.11`) |
| **Date** | 2026-11-17 |
| **Audit Mode** | Consumer Application Compliance & Beta Release Quality Gate (`Beta` channel) |
| **Target Hardware** | v1 Passive Tap Board / Waveshare ESP32-S3-Nano (`um_nanos3`, ESP32-S3R8 — 16 MB Flash, 8 MB Octal PSRAM) |
| **Build Environments** | `ex3-waveshare-nano` (production beta), `ex3-waveshare-nano-diag` (signal logger + synthetic HR), `ex3-waveshare-nano-rig` (calibration rig), `native` (host unit test suite) |
| **`WITH_OTA` Status** | **Disabled for Beta** (`; -D WITH_OTA` commented out in [`platformio.ini`](../platformio.ini#L67)) — Beta firmware updates are performed exclusively via USB-C with the mechanical `SW0` (`VIN`) power interlock switched **OFF** (see [`docs/beta-build-guide.md`](beta-build-guide.md)); post-beta OTA headroom tracks upstream `cblabs-org/esp32-app-foundation#230` |
| **🟢 MEASURED Evidence** | **Live `/dev/cu.usbmodem101` bench verification** on Waveshare ESP32-S3-Nano: `v0.3.0-dev.51` (`81 s` live serial soak, `monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log`, `App version: v0.3.0-dev.51`, `0` panics / `0` WDT resets, `0` `KeyManager: DeviceCrypto not initialized.` errors, `RidePersistence: Scanned 0 persisted rides (0 bytes total)` with zero literal `zu`), `v0.3.0-dev.50` (`70 s` live soak verifying SoftAP Captive Portal `192.168.4.1` + `http://echenon.local` and `#329` `%u` UART fixes), alongside preserved `v0.3.0-dev.48` (`8,235 s`) + `v0.3.0-dev.49` (`4,944 s` / `82.4 min` provisioned STA soak with Ring-2 `MCPServer`, `14/14` gzipped portal routes, `27` live MCP tools, and BLE HRS/FTMS streaming) and committed Phase 2 & Phase 3 HIL evidence (`v0.3.0-dev.44` / `v0.3.0-dev.45`) |
| **Overall Audit Verdict** | **PASS (Beta Channel Ready — `cblabs-org/esp32-app-foundation` `v1.0.0-beta.2` Public & Aligned, `WO-09.14` Newlib-Nano & Post-`DeviceCrypto` Boot Keystore Fixes Integrated, `v0.3.0-dev.51` Bench Verified Alongside `v0.3.0-dev.49` Provisioned Soak; Awaiting Owner Launch Gate Steps 3–5)** |

---

## 2. 10-Dimension Release Audit Matrix (`Beta` Channel)

Every dimension from the [ESP32 App Foundation Release Audit Protocol](../.agents/skills/framework-expert/guides/release-audit.md) has been evaluated against the integrated `PR-09` tree (`WO-09.1` through `WO-09.14`, including `WO-09.12` `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` `02d0b2de` + `pioarduino/platform-espressif32` `55.03.311` alignment and `WO-09.14` newlib-nano `%zu`/`%llu` + post-`DeviceCrypto` uploader boot ordering fixes at `802768575082b2e324637d542d7470e9d3a20e9d`, receipt `rcpt_1ea7627b8a9e`, tag `v0.3.0-dev.51`):

| Dim | Dimension Name | Status | Evidence Quality | Primary Evidence & Verification Basis |
|---|---|---|---|---|
| **D1** | **Documentation & House Style** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` + 🔴 `JUDGED` | All 63 authored C++ files in `src/` carry Apache-2.0 SPDX headers (`0` missing); `0` missing Doxygen `@brief`, `0` undocumented constants, `0` `TAG` mismatches, `0` directory/file mismatches; public OSS docs ([`LICENSE`](../LICENSE), [`README.md`](../README.md), [`CHANGELOG.md`](../CHANGELOG.md), [`CONTRIBUTING.md`](../CONTRIBUTING.md), [`SPEC.md`](../SPEC.md), [`AGENTS.md`](../AGENTS.md), [`docs/beta-build-guide.md`](beta-build-guide.md)) complete and synced in `WO-09.12` to public `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` (`isPrivate: false`) and `.agents/skills/framework-portal-design`. |
| **D2** | **Coding Discipline & Encapsulation** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` + 🔴 `JUDGED` | `0` `extern Application` in `src/`; `0` stored `Application*`; `0` `Application.hpp` in headers; `0` sibling module imports; `0` public member variables; `0` Law-of-Demeter violations; all top-level composition-root objects (including `mcpServer` at [`src/main.cpp:46`](../src/main.cpp#L46)) declared `static`; all 13 `std::function` uses in `src/echenon/` carry `// GRAVE-05:` cold-path/boot-bound justification comments; single `extern uint32_t millis();` in [`src/echenon/FTMSServer.cpp:37`](../src/echenon/FTMSServer.cpp#L35-L38) guarded by `#ifdef UNIT_TEST`. |
| **D3** | **Framework Best Practices** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` + 🟢 `MEASURED` | Single `#include "AppFoundation.h"` facade in [`src/main.cpp:8`](../src/main.cpp#L8) and [`src/echenon/EchenonModule.h:10`](../src/echenon/EchenonModule.h#L10); all modules including `mcpServer` (`mcpServer.setAppName("Echenon")`) and `timeManager.setEventBus(&app.getEventBus())` ([`src/main.cpp:187`](../src/main.cpp#L187), `WO-09.12` / `#320`) registered before `app.begin(config)` ([`src/main.cpp:107-195`](../src/main.cpp#L107-L195)), with `mcpServer.begin(...)` wired after `timeManager.begin()` ([`src/main.cpp:202-216`](../src/main.cpp#L202-L216)) and `intervalsUploader().begin()` + `wahooUploader().begin()` deferred until after `app.begin(config)` alongside `stravaUploader.begin()` ([`src/main.cpp:238-242`](../src/main.cpp#L238-L242), `WO-09.14`); live `v0.3.0-dev.51` boot verifies `0` `KeyManager: DeviceCrypto not initialized.` errors and `RidePersistence: Scanned 0 persisted rides (0 bytes total)` (zero literal `zu`), alongside `v0.3.0-dev.50` `#329` `%u` UART fixes and `v0.3.0-dev.49` `webContributors=12`, `mcpContributors=6`; `EchenonModule` implements `ITickable`, `IConfigurable`, `IWebContributor`, and `IMCPContributor` (5 production / 7 rig tools). |
| **D4** | **DRAM & Memory Budget** | **PASS** | 🟡 `MECHANICAL` + 🟢 `MEASURED` | `v0.3.0-dev.51` static internal DRAM is `59,516 B / 327,680 B` (`18.2%`) on `ex3-waveshare-nano` (`.dram0.data` `20,428 B` incl. `mcpServer` = `104 B`, `.dram0.bss` `39,088 B`, `DIRAM` `161,307 B` / `47.2%`, `IRAM` `16,384 B` / `100.0%`); largest `.dram0` symbol is `g_cnxMgr` (`3,880 B`, `1.18%` of DRAM, well below the 10% guidance ceiling); `SignalLogic::formatBurstCsvHeader()` uses zero-heap stack base-10 formatting (`char startUsBuf[21]`); `CONFIG_SPIRAM=y` (`8 MB` Octal PSRAM); live `v0.3.0-dev.51` `81 s` soak measured `8,599,311 B` boot free heap (`min-free 8,558,752 B`), `8,376,307 B` steady `free_heap` at `uptime=30s`/`60s`, `dram=108,775 B`, `dram_min=75,880 B`, and `DataManager` worker HWM `10,732 B free (of 12,288)`, alongside preserved `v0.3.0-dev.49` STA soak (`65,723 B` free internal DRAM at `4,944 s`, `dram_min = 39,828 B` `2.65×` the 15 KB floor, `8,267,612 B` free PSRAM) and Phase 3 `v0.3.0-dev.45` ([`pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md#L79-L83): `102,895 B` free, `dram_min = 60,844 B`, `8,267,732 B` PSRAM). |
| **D5** | **Security** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | `0` committed secrets; `0` `KeyManager` bypasses (`WO-09.14` deferred `intervalsUploader().begin()` / `wahooUploader().begin()` post-`DeviceCrypto`, `0` `KeyManager: DeviceCrypto not initialized.` boot errors on `v0.3.0-dev.51`, and guarded `_athleteId` against empty `KEY_ATHLETE_ID` lookups); `0` `setInsecure()` in `src/`; 23 `AuthManager::checkPortalAuth()` guards across all sensitive REST routes in `EchenonModule`, `SignalLogger`, `DiagnosticsRoutes`, and `SessionOverlay`; all `/api/echenon/rig/*` HTTP endpoints removed (`0` matches) and migrated to authenticated `IMCPContributor` tools; `0` personal SSIDs/BSSIDs/URLs outside `.agents/`. |
| **D6** | **Web Portal & Design System** | **PASS** | 🟡 `MECHANICAL` + 🔴 `JUDGED` + 🟢 `MEASURED` | `0` bare hex colors in `web/*.js` (all CSS colors use `var(--token, #fallback)` design tokens; only standalone PWA `web/ride.html` `:root` token definitions and `web/manifest.json` metadata contain literal hex); `0` missing ARIA tabpanel roles; Feather SVG iconography; `window.apiFetch` auth wrapper used across portal scripts; `web/intervals_settings.js` updated in `WO-09.12` for dual tab IDs (`tab-intervals-icu` / `tab-intervals.icu`) matching `#329` tab slugification; live `v0.3.0-dev.50` verified SoftAP captive portal fallback (`192.168.4.1`, `http://echenon.local`) alongside `v0.3.0-dev.49` `14/14` gzipped portal routes `200 OK` (including `/mcp.js` `4,514 B`). |
| **D7** | **Build Integrity & Test Coverage** | **PASS** | 🟡 `MECHANICAL` | **`271 / 271`** native unit tests pass (`0` failed, `0` errored, `0` skipped in **`0.677 s`** via PlatformIO MCP `run_tests`, adding `BurstCsvFormatHeaderAbove32BitTimestamp` and `ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId` in `WO-09.14`) across all 20 non-dormant test suites in `test/test_native/`; all 3 ESP32-S3 environments (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`) compile and link cleanly (`success: true`) on `v0.3.0-dev.51` (`pioarduino/platform-espressif32` `55.03.311`, `ESP-IDF 5.5.5`, `Arduino 3.3.11`). |
| **D8** | **Parallel Mechanism Detection** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | `0` stale `lib_deps`; no duplicate drivers or parallel infrastructure — reuses `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` (`02d0b2de`) `EncoderModule` (PCNT), `BLEManager` (`IBLEServiceProvider` / `IBLEClientProvider` / `ScanInhibit`), `ConfigManager`, `KeyManager`, `AuthManager`, `EventBus`, `TimeManager` (incl. `#320` DHCP Option 42/100/101), `DataManager`, `MCPToolRegistry`, and `MCPServer`. |
| **D9** | **Config Shadowing** | **PASS** | 🟡 `MECHANICAL` + 🟠 `TRACED` | `platformio.ini` pins `pioarduino/platform-espressif32` `55.03.311` (`ESP-IDF 5.5.5`, `Arduino 3.3.11`) while preserving `board = um_nanos3`; `SDKCONFIG_DEFAULTS` (`sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults`), `CMakeLists.txt` (`EXCLUDE_COMPONENTS` + `lwip_dhcp_tz_hooks.h`), and `#320` DHCP NTP/TZ flags (`CONFIG_LWIP_SNTP_MAX_SERVERS=3`, `CONFIG_LWIP_DHCP_GET_NTP_SRV=y`, `CONFIG_LWIP_DHCP_MAX_NTP_SERVERS=2`, `CONFIG_LWIP_HOOK_DHCP_EXTRA_OPTION_DEFAULT=y`) verified intentional alongside `CONFIG_SPIRAM=y`, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4`, `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (SPEC §7.8), and headless operation (SPEC §12.3). |
| **D10** | **Breaking Change & Migration** | **PASS WITH BETA WAIVER** | 🟡 `MECHANICAL` + 🟠 `TRACED` | First external beta release (`v0.3.0-beta.1`); partition table ([`hardware/ex3-waveshare-nano/partitions.csv`](../hardware/ex3-waveshare-nano/partitions.csv)) unchanged (`app0`/`app1` 6 MB each, `config` 1 MB, `storage` 2.75 MB); `CalibrationRig` HTTP routes migrated to MCP (`echenon_rig_status`, `echenon_rig_command`); OTA upgrade boot-log check waived for beta because `WITH_OTA` is intentionally disabled in favor of USB-C updates with `SW0` `VIN` isolated (post-beta OTA headroom tracks upstream `cblabs-org/esp32-app-foundation#230`). |

> [!NOTE]
> **Evidence Grading Disclosure:** No dimension is graded solely from `🔴 JUDGED` evidence. Every dimension is anchored by `🟡 MECHANICAL` automated checks and/or `🟠 TRACED` code-path inspection (`WO-09.1`–`WO-09.14`), with `🟢 MEASURED` hardware telemetry from committed Phase 2/3 HIL runs, the historical `v0.3.0-dev.48` + `v0.3.0-dev.49` provisioned STA soak & MCP gate, the `v0.3.0-dev.50` (`v1.0.0-beta.2` + `ESP-IDF 5.5.5`) SoftAP/#329 check, and the latest live `v0.3.0-dev.51` (`WO-09.14`, `/dev/cu.usbmodem101`, `81 s` soak) hardware verification.

---

## 3. Detailed Per-Dimension Findings & Analysis

### Dimension 1 — Documentation & House Style (`PASS`)

- **File Headers (`🟡 MECHANICAL`):** All 63 C++ source and header files under `src/` (`src/main.cpp` + 62 files in `src/echenon/`) include the canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` header block (`find src -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) | wc -l` = `63`; `grep -rnL "SPDX-License-Identifier: Apache-2.0" src/` returns only `src/CMakeLists.txt`).
- **Doxygen & Constants (`🟡 MECHANICAL`):** `release-audit-checks.sh` reports `missing_doxygen: PASS (0 hits)`, `undocumented_constants: PASS (0 hits)`, `tag_mismatch: PASS (0 hits)`, and `directory_file_mismatch: PASS (0 hits)`.
- **Public Repository & Builder Documentation (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - [`LICENSE`](../LICENSE) — Apache-2.0 license text (`12,554 B`).
  - [`README.md`](../README.md) — Beta tester recruitment guide, hardware overview, v1 stock runaway limitation disclosure, mechanical `VIN`/`USB-C` power switch procedure, and public `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2`) submodule clone instructions (`14,130 B`).
  - [`CHANGELOG.md`](../CHANGELOG.md) — `v0.3.0-beta.1` release notes and milestone history (`6,416 B`).
  - [`CONTRIBUTING.md`](../CONTRIBUTING.md) — Developer setup, `cblabs-org/esp32-app-foundation` submodule instructions, native test floor, and PR workflow (`6,856 B`).
  - [`SPEC.md`](../SPEC.md) & [`AGENTS.md`](../AGENTS.md) — Synced in `WO-09.12` (`5383b781087e` + `178027cc8a7d3bbd2ff09af1fe5510814f1edb58`, receipt `rcpt_d73d60391631`) to `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2`, `02d0b2de`, `isPrivate: false`) and `.agents/skills/framework-portal-design`.
  - [`docs/beta-build-guide.md`](beta-build-guide.md) — Standalone v1 Passive Tap Board builder guide with BOM (`C1 ×5` 100 nF ceramic decoupling errata synchronized with [`docs/hardware_specification.md`](hardware_specification.md)), netlist, stripboard assembly, and `SW0` (`VIN`) safety interlock instructions (`29,695 B`).
- **Advisory Notes (Non-Blocking):**
  - `release-audit-checks.sh` flags `missing_file_header` on the 20 unit test files in `test/test_native/*.cpp` and 1 standalone BLE peripheral test sketch in `tools/ble-test/control-peripheral/src/main.cpp`. All 63 production/diagnostic/rig firmware files in `src/` pass 100%.
  - `member_naming` hits reported by the script (`115` hits) reside entirely inside the upstream `framework/` submodule (`0` hits in `src/`).

### Dimension 2 — Coding Discipline & Encapsulation (`PASS`)

- **Zero Global `Application` Coupling (`🟡 MECHANICAL`):**
  - `grep -rn "extern Application" src/` → **0 matches**.
  - `stored_application_ptr: PASS (0 hits)`, `application_hpp_in_header: PASS (0 hits)`, `sibling_module_import: PASS (0 hits)`, `public_member_variables: PASS (0 hits)`, `law_of_demeter: PASS (0 hits)`.
  - All top-level module instances in [`src/main.cpp:15-58`](../src/main.cpp#L15-L58) (`bleManager`, `encoderModule`, `timeManager`, `timePrecondition`, `app`, `echenonModule`, `mcpServer` at [`src/main.cpp:46`](../src/main.cpp#L46), `rideStore`, `sessionManager`, `uploadQueue`, `sessionOverlay`, `ridePersistence`, `stravaUploader`) are declared `static` at file scope.
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

- **Facade & Lifecycle Ordering (`🟡 MECHANICAL` + `🟠 TRACED` + `🟢 MEASURED`):**
  - Application code includes `"AppFoundation.h"` ([`src/main.cpp:8`](../src/main.cpp#L8), [`src/echenon/EchenonModule.h:10`](../src/echenon/EchenonModule.h#L10)).
  - Every `app.registerModule(...)` and `bleManager.registerServiceProvider(...)` / `registerClientProvider(...)` call in [`src/main.cpp:107-190`](../src/main.cpp#L107-L190) — including `mcpServer.setAppName("Echenon"); app.registerModule(&mcpServer);` ([`src/main.cpp:108-109`](../src/main.cpp#L108-L109)) and `timeManager.setEventBus(&app.getEventBus());` ([`src/main.cpp:187`](../src/main.cpp#L187), added in `WO-09.12` for `cblabs-org/esp32-app-foundation#320` DHCP NTP/TZ support) — executes before `app.begin(config)` ([`src/main.cpp:195`](../src/main.cpp#L195)), respecting the framework's compact-and-lock registration contract.
  - Ring-2 `mcpServer.begin(...)` is invoked after `timeManager.begin()` ([`src/main.cpp:202-216`](../src/main.cpp#L202-L216)).
- **`WO-09.14` Post-`DeviceCrypto` Uploader Boot Ordering & Newlib-Nano Format Fixes (`🟠 TRACED` + `🟢 MEASURED`):**
  - **Uploader Initialization Ordering (`src/main.cpp` & `IntervalsIcuUploader.cpp`):** [`src/main.cpp:238-242`](../src/main.cpp#L238-L242) defers `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` until after `app.begin(config)` right alongside `stravaUploader.begin()` (initializing credentials after `DeviceCrypto` / `KeyManager` are ready, eliminating boot-time `KeyManager: DeviceCrypto not initialized.` errors), while [`src/echenon/IntervalsIcuUploader.cpp:255-262`](../src/echenon/IntervalsIcuUploader.cpp#L255-L262) adds an `_athleteId` non-empty guard so an absent `KEY_ATHLETE_ID` keystore entry preserves `_athleteId` loaded by `applyConfig()`.
  - **`RidePersistence` `%zu` Replacement (`src/echenon/RidePersistence.cpp`):** Replaced unsupported `newlib-nano` `%zu` format specifiers at [`L195`](../src/echenon/RidePersistence.cpp#L195-L196), [`L325`](../src/echenon/RidePersistence.cpp#L325-L327), and [`L355`](../src/echenon/RidePersistence.cpp#L356-L357) with `%u` + `static_cast<unsigned>(...)`.
  - **`SignalLogic` `%llu` Replacement (`src/echenon/SignalLogic.h`):** Replaced `%llu` in [`formatBurstCsvHeader()`](../src/echenon/SignalLogic.h#L492-L510) (`L492-500`) with a zero-heap stack base-10 conversion (`char startUsBuf[21]`) formatted via `%s`, preventing 32-bit `newlib-nano` `va_list` misalignment / `LoadProhibited` crash hazards when `burstStartUs >= 2^32` (`> 71.5 min` uptime).
  - **Live UART Verification (`v0.3.0-dev.51` + `v0.3.0-dev.50` + `v0.3.0-dev.49`):** Live `v0.3.0-dev.51` UART boot confirms **`0` occurrences** of `KeyManager: DeviceCrypto not initialized.` and **`I (1194) RidePersistence: Scanned 0 persisted rides (0 bytes total)`** (zero literal `zu`), complementing `v0.3.0-dev.50` `#329` `%u` formatting verification (`WebServerManager: Global body-size middleware registered (default=4096, 10 overrides)`, `MCPBuiltinTools: Registered 8 built-in tools`, `MCPServer: MCP Server started with 27 tools (8 built-in)`) and `v0.3.0-dev.49` `Registry locked — tickables=10 webContributors=12 mcpContributors=6 dataSources=1 statusReporter=registered`.
- **Calibration Persistence & Physics Synchronization (`🟠 TRACED`):**
  - [`EchenonModule::syncPhysicsCalibration()`](../src/echenon/EchenonModule.cpp#L287-L293) propagates fitted OLS potentiometer endpoints (`_calibrationWizard.getPotMin()`, `_calibrationWizard.getPotMax()`) and recalibration eddy-current scaling (`_recalibrationUI.applyToPhysicsParams(params)`) into `_powerEngine.setPhysicsParams(params)`.
  - Invoked on initial `tick()` startup ([`src/echenon/EchenonModule.cpp:89`](../src/echenon/EchenonModule.cpp#L89)), `applyConfig()` ([`src/echenon/EchenonModule.cpp:321`](../src/echenon/EchenonModule.cpp#L321)), `resetConfig()` ([`src/echenon/EchenonModule.cpp:383`](../src/echenon/EchenonModule.cpp#L383)), REST calibration record/reset and recalibration point/reset routes ([`src/echenon/EchenonModule.cpp:502-608`](../src/echenon/EchenonModule.cpp#L502-L608)), and MCP `echenon_calibration` / `echenon_recalibration` tools ([`src/echenon/EchenonModule.cpp:863-930`](../src/echenon/EchenonModule.cpp#L863-L930)), followed by `_configManager->saveModuleConfig(this)`.
- **MCP Tool Registration & Rig Read-Only Hint (`🟠 TRACED` + `🟢 MEASURED`):**
  - `EchenonModule::registerMCPTools(MCPToolRegistry&)` ([`src/echenon/EchenonModule.cpp:782-1100`](../src/echenon/EchenonModule.cpp#L782-L1100)) registers 5 core tools in production builds (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`) plus 2 rig tools under `WITH_CALIBRATION_RIG` (`echenon_rig_status`, `echenon_rig_command`), emits an `ESP_LOGI(TAG, "Registered MCP tools: ...")` boot summary ([`src/echenon/EchenonModule.cpp:1096-1099`](../src/echenon/EchenonModule.cpp#L1096-L1099)), and sets accurate `mutatesState` flags for read-only vs state-mutating operations.
  - [`tools/calibration-rig/calibration_sweep.py:133-134`](../tools/calibration-rig/calibration_sweep.py#L133-L134) surfaces an actionable hint when `MCPServer` returns JSON-RPC `-32000` (`read-only` mode).

### Dimension 4 — DRAM & Memory Budget (`PASS`)

- **Static DRAM & Flash Footprint (`🟡 MECHANICAL`):**
  - Across all 3 ESP32-S3 build environments (`v0.3.0-dev.51` on `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` and `pioarduino/platform-espressif32` `55.03.311` / `ESP-IDF 5.5.5`), static internal RAM usage is `59,516 B`–`59,836 B` (`18.2%`–`18.3%` of `327,680 B`), leaving `267,844 B`–`268,164 B` (`> 267 KB`) of static DRAM headroom prior to dynamic stack/heap initialization (`DIRAM` `161,307 B`–`161,947 B` / `47.2%`–`47.39%`; `IRAM` `16,384 B` / `100.0%`).
- **ELF Largest-Symbol Check (`🟡 MECHANICAL`):**
  - Inspected via `xtensa-esp32s3-elf-size -A` and `xtensa-esp32s3-elf-objdump -t` on `.pio/build/ex3-waveshare-nano/firmware.elf` (`v0.3.0-dev.51`): the largest `.dram0` symbol is `g_cnxMgr` at `3,880 B` (`1.18%` of 320 KB internal DRAM), well below the 10% (`~32.7 KB`) advisory threshold; `mcpServer` (`_ZL9mcpServer`) occupies `104 B` in `.dram0.data`.
- **PSRAM & Runtime Heap Headroom (`🟡 MECHANICAL` + `🟢 MEASURED`):**
  - `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, and `CONFIG_SPIRAM_SPEED_80M=y` are active in [`hardware/ex3-waveshare-nano/sdkconfig.defaults:9-12`](../hardware/ex3-waveshare-nano/sdkconfig.defaults#L9-L12).
  - `RideStore` allocates its 14,400-record ring buffer in Octal PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`), keeping internal DRAM free for NimBLE and WiFi/TLS; `SignalLogic::formatBurstCsvHeader()` uses a stack-only `char startUsBuf[21]` buffer (`0 B` heap allocation).
  - **Live `v0.3.0-dev.51` Hardware Bench Verification (`/dev/cu.usbmodem101`, `ESP-IDF 5.5.5`, `8MB PSRAM`, `81 s` live serial soak, `monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log`):**
    - `App version: v0.3.0-dev.51`, `0` panics / `0` WDT resets; boot heap free `8,599,311 B` (min-free `8,558,752 B`).
    - `uptime=30s` and `uptime=60s` heartbeat: `free_heap=8,376,307 B`, `dram=108,775 B`, `dram_min=75,880 B` (well above the 40 KB Echenon SPEC mandate and 15 KB framework floor).
    - `DataManager: Worker stack high water mark: 10,732 B free (of 12,288)`.
  - **Preserved `v0.3.0-dev.49` Provisioned STA Bench Soak Baseline (`/dev/cu.usbmodem101`, `4,944 s` / `82.4 min` continuous):**
    - **Internal DRAM free (`GET /api/system` at `uptime: 4944`):** `65,723 B` (steady heartbeat `dram = 66,983–67,595 B`, well above the 40 KB Echenon SPEC mandate)
    - **Internal DRAM minimum free (`heap_min_free`):** `39,828 B` (startup transient peak during TLS/BLE/portal init, `2.65×` the 15 KB framework guidance floor)
    - **Internal DRAM max allocatable block (`heap_max_alloc`):** `31,744 B`
    - **PSRAM free:** `8,267,612 B` / `8,388,608 B`
  - **Committed Phase 3 Baseline (`v0.3.0-dev.45`, [`docs/measurements/phase3/evidence/pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md#L79-L83)):**
    - **Internal DRAM free:** `102,895 B` (`dram_min = 60,844 B`, `psram_free = 8,267,732 B` / `8,388,608 B`)

### Dimension 5 — Security (`PASS`)

- **Portal Authentication Coverage (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - Every sensitive application HTTP endpoint enforces `AuthManager::checkPortalAuth(request, response)` (`23` call sites verified across `src/echenon/`):
    - [`src/echenon/EchenonModule.cpp`](../src/echenon/EchenonModule.cpp#L463-L748) — 13 routes: `/api/echenon/calibration/{start,stop,record-level,reset,status}`, `/api/echenon/recalibration/{point,points,reset}`, `/api/echenon/power-table/{upload,active}`, `/api/echenon/live`, `/api/echenon/ftms-smoothing` (`GET` & `POST`).
    - [`src/echenon/SignalLogger.cpp`](../src/echenon/SignalLogger.cpp#L363-L505) — 5 routes (`WITH_SIGNAL_LOGGER` diagnostic builds): `/api/echenon/status` (`GET`), `/api/echenon/burst/start` (`POST`), `/api/echenon/ride-log.csv` (`GET`), `/api/echenon/events.csv` (`GET`), and `/api/echenon/burst.csv` (`GET`).
    - [`src/echenon/DiagnosticsRoutes.cpp`](../src/echenon/DiagnosticsRoutes.cpp#L132-L373) — 4 routes: `/api/echenon/diagnostics/events` (`GET` & `DELETE`), `/api/echenon/diagnostics/rides` (`GET`), `/api/echenon/diagnostics/rides/*` (`GET` & `DELETE`).
    - [`src/echenon/SessionOverlay.cpp`](../src/echenon/SessionOverlay.cpp#L1271) — `checkAuth_()` helper guarding all `/api/session/*`, `/api/strava/*`, `/api/intervals/*`, `/api/wahoo/*`, and `/api/upload/*` endpoints (with OAuth redirect callbacks `/api/strava/callback` and `/api/wahoo/callback` additionally protected by single-use 128-bit cryptographic state nonces and 600 s TTLs).
- **CalibrationRig HTTP Surface Elimination (`🟡 MECHANICAL`):**
  - `grep -rn "/api/echenon/rig/" src/ web/ tools/` returns **0 matches**. All rig control and telemetry operate through the framework's authenticated `/api/mcp` endpoint (`echenon_rig_status`, `echenon_rig_command`) or local USB Serial.
- **Credential Storage & Redaction (`🟡 MECHANICAL` + `🟠 TRACED` + `🟢 MEASURED`):**
  - `committed_secrets: PASS (0 hits)`, `key_manager_bypass: PASS (0 hits)`.
  - Third-party cloud tokens/keys (Strava, Intervals.icu, Wahoo Cloud) are stored in `KeyManager` (`DeviceCrypto` AES-GCM NVS keystore) and registered with `Redactor::registerSecret()` upon load and refresh.
  - Per `WO-09.14`, `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` execute post-`app.begin(config)` alongside `stravaUploader.begin()` ([`src/main.cpp:238-242`](../src/main.cpp#L238-L242)), guaranteeing `DeviceCrypto` / `KeyManager` initialization before secret reads (`0` `KeyManager: DeviceCrypto not initialized.` errors observed in `v0.3.0-dev.51` live boot logs), and [`IntervalsIcuUploader::loadCredentials()`](../src/echenon/IntervalsIcuUploader.cpp#L255-L262) preserves `applyConfig()` `_athleteId` when `KEY_ATHLETE_ID` is absent from the keystore.
  - Repository-wide privacy check (`git grep -n -E "<REDACTED_SSID>|<REDACTED_OUI>|<REDACTED_SESSION_URL>" -- ":!.agents"`) returns **0 matches**.

### Dimension 6 — Web Portal & Design System (`PASS`)

- **Design Token Compliance (`🟡 MECHANICAL`):**
  - `grep -rn -E "#[0-9a-fA-F]{3,8}" web/ | grep -v "var(--"` confirms **0 bare hex colors** across all 10 JavaScript portal modules (`web/*.js`).
  - Every color reference in `web/calibration-wizard.js`, `web/diagnostics.js`, `web/echenon-dashboard.js`, `web/intervals_settings.js`, `web/recalibration.js`, `web/ride.js`, `web/session.js`, `web/stored_rides.js`, `web/upload_settings.js`, and `web/wahoo_settings.js` uses `var(--token, #fallback)`.
  - The only non-`var(--...)` hex strings in `web/` are the `:root` CSS custom property declarations and `<meta name="theme-color">` in standalone full-screen [`web/ride.html:1-2`](../web/ride.html#L1-L2) and PWA [`web/manifest.json:1`](../web/manifest.json#L1).
- **Accessibility, Iconography & Authenticated Fetch (`🟡 MECHANICAL` + `🔴 JUDGED` + `🟢 MEASURED`):**
  - `missing_aria_role: PASS (0 hits)`.
  - Top-level tabs (`Echenon`, `Apps`) use standard 24×24 Feather SVG `<polyline>` / `<rect>` primitives ([`src/echenon/EchenonModule.cpp:1105-1122`](../src/echenon/EchenonModule.cpp#L1105-L1122)).
  - [`web/intervals_settings.js:17-18,220-234`](../web/intervals_settings.js#L17-L239) adds defensive dual tab ID support (`tab-intervals-icu` / `tab-intervals.icu`) aligned in `WO-09.12` with upstream `cblabs-org/esp32-app-foundation` `v1.0.0-beta.2` (`#329`) non-alphanumeric tab slugification.
  - Portal scripts use `window.apiFetch` for authenticated API requests, including `POST /api/echenon/calibration/reset` in [`web/calibration-wizard.js`](../web/calibration-wizard.js).
  - Live `v0.3.0-dev.50` verified SoftAP Captive Portal startup (`Access Point 'echenon' is online. IP: 192.168.4.1`, `WebServerManager: HTTP server started on port 80`, `EchenonModule: mDNS responder started: http://echenon.local`) alongside preserved `v0.3.0-dev.49` STA verification of `14/14` gzipped portal routes returning `200 OK` (including `/mcp.js` `4,514 B`).

### Dimension 7 — Build Integrity & Test Coverage (`PASS`)

- **Native Unit Test Suite (`🟡 MECHANICAL`):**
  - Executed via PlatformIO MCP `run_tests` (`pio test -e native --verbose`, `v0.3.0-dev.51` / `WO-09.14`): **271 test cases, 271 passed, 0 failed, 0 errored, 0 skipped** (**`0.677 s`**) across 20 test suites in `test/test_native/` (adding `BurstCsvFormatHeaderAbove32BitTimestamp` in [`test/test_native/test_signal_logger.cpp`](../test/test_native/test_signal_logger.cpp) and `ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId` in [`test/test_native/test_intervals_uploader.cpp`](../test/test_native/test_intervals_uploader.cpp)). Every suite has `> 0` executing test cases (zero dormant suites).
- **Multi-Environment Firmware Build Matrix (`🟡 MECHANICAL`):**
  - Executed via `platformio-mcp_platformio` `build_project` on `v0.3.0-dev.51` (`pioarduino/platform-espressif32` `55.03.311`, `ESP-IDF 5.5.5`, `Arduino 3.3.11`) across all 3 hardware environments (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`): all 3 environments build and link with `success: true`.

### Dimension 8 — Parallel Mechanism Detection (`PASS`)

- **Framework Reuse Verification (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - `stale_lib_deps: PASS (0 hits)`.
  - Reuses public `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` (`02d0b2de`, `isPrivate: false`) kernel and optional modules rather than bespoke parallels:
    - Rotary knob decoding uses `EncoderModule` (`PCNT` hardware quadrature unit configured via `BOARD_ENCODERS` in [`hardware/ex3-waveshare-nano/board.h:34-43`](../hardware/ex3-waveshare-nano/board.h#L34-L43)).
    - BLE GATT server (`HRMRepeater`, `FTMSServer`) and central (`HRMClient`) roles integrate through `BLEManager` (`IBLEServiceProvider`, `IBLEClientProvider`) and use `BLEManager::ScanInhibit` during cloud TLS uploads in `UploadQueue`.
    - Wall-clock sync uses `TimeManager` (including `#320` DHCP Option 42 NTP and RFC 4833 Options 100/101 timezone acquisition wired via `timeManager.setEventBus(&app.getEventBus())` in `WO-09.12`).
    - Background cloud uploads use `IDataSource` / `DataManager` (`FetchContext` + `DeadlineToken` + `FrameworkSecureClient`).
    - MCP JSON-RPC transport, bearer auth middleware, read-only guard, and portal `/mcp.js` integration reuse Ring-2 `MCPServer` and `MCPToolRegistry`.

### Dimension 9 — Config Shadowing (`PASS`)

- **SDKCONFIG & Build Flag Audit (`🟡 MECHANICAL` + `🟠 TRACED`):**
  - [`platformio.ini:6`](../platformio.ini#L6) pins `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip` (`ESP-IDF 5.5.5`, `Arduino 3.3.11`) while preserving `board = um_nanos3` ([`platformio.ini:51`](../platformio.ini#L51)) for raw ESP32-S3 GPIO numbering (`1..18`) in `hardware/ex3-waveshare-nano/board.h`.
  - `platformio.ini` chains `SDKCONFIG_DEFAULTS="sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"`.
  - [`sdkconfig.defaults:245-254`](../sdkconfig.defaults#L245-L254) and [`CMakeLists.txt:3-49`](../CMakeLists.txt#L3-L49) incorporate `WO-09.12` alignment with `cblabs-org/esp32-app-foundation#320`:
    - `CONFIG_LWIP_SNTP_MAX_SERVERS=3`, `CONFIG_LWIP_DHCP_GET_NTP_SRV=y`, `CONFIG_LWIP_DHCP_MAX_NTP_SERVERS=2`, `CONFIG_LWIP_HOOK_DHCP_EXTRA_OPTION_DEFAULT=y`
    - `EXCLUDE_COMPONENTS` pruning and `ESP_IDF_LWIP_HOOK_FILENAME="lwip_dhcp_tz_hooks.h"` injection on `lwip`
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
  - Submodule remote switched in `WO-09.12` (`5383b781087e` + `178027cc8a7d3bbd2ff09af1fe5510814f1edb58`, receipt `rcpt_d73d60391631`) to public `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` (`02d0b2de`, `isPrivate: false`), tracking upstream `#320`, `#321`, `#230`, and `#329`.
- **Beta OTA Waiver (`🟠 TRACED`):**
  - Per `PROJECT.md` owner design decision and [`SPEC.md`](../SPEC.md), `WITH_OTA` is disabled for the v1 Beta release (`v0.3.0-beta.1`). Beta builders flash updates over USB-C with `SW0` (`VIN`) switched OFF. Consequently, the OTA upgrade boot-log capture check is waived for `v0.3.0-beta.1` and deferred to the post-beta OTA enablement milestone (tracking upstream `cblabs-org/esp32-app-foundation#230` for OTA memory headroom via temporary module unloading).

---

## 4. Verification Output Summary

### 4.1 Native Unit Test Suite (`pio test -e native --verbose`)

| Environment | Test Suites (`test/test_native/`) | Total Cases | Passed | Failed | Errored | Skipped | Duration | Result |
|---|---|---|---|---|---|---|---|---|
| `native` (`v0.3.0-dev.51` / `WO-09.14`) | 20 suites (`test_cadence_sensor`, `test_diagnostics_module`, `test_diagnostics_routes`, `test_ftms_server`, `test_hr_encoding`, `test_hr_parsing`, `test_intervals_uploader`, `test_level_mapping`, `test_power_engine`, `test_quadrature_generator`, `test_recalibration`, `test_ride_persistence`, `test_runaway_detector`, `test_session_manager`, `test_signal_logger`, `test_signal_logic`, `test_strava_uploader`, `test_tcx_generator`, `test_upload_queue`, `test_wahoo_uploader`) via PlatformIO MCP `run_tests` (adding `BurstCsvFormatHeaderAbove32BitTimestamp` & `ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId`) | **271** | **271** | **0** | **0** | **0** | **`0.677 s`** | ✅ **PASS** |

### 4.2 Multi-Environment Firmware Build Matrix (`v0.3.0-dev.51` — `pioarduino/platform-espressif32` `55.03.311`, `ESP-IDF 5.5.5`, `Arduino 3.3.11`)

| Environment | Feature Flags Beyond Base | Build Time | Build Status | Static RAM (`327,680 B` max) | Flash (`6,291,456 B` `app0`) | Total Image Size |
|---|---|---|---|---|---|---|
| `ex3-waveshare-nano` | Production v1 Beta (`WITH_BLE`, `WITH_ENCODER`, `WITH_HR_RELAY`, `WITH_FTMS`, `WITH_SESSION`, `WITH_STRAVA`, `WITH_TIME`) | `112.42 s` | ✅ `success: true` | **59,516 B** (`18.2%`) | **1,798,083 B** (`28.6%`) | `1,798,340 B` |
| `ex3-waveshare-nano-diag` | Base + `-DWITH_SIGNAL_LOGGER -DWITH_HR_SYNTHETIC` | `98.12 s` | ✅ `success: true` | **59,836 B** (`18.3%`) | **1,815,191 B** (`28.9%`) | `1,815,448 B` |
| `ex3-waveshare-nano-rig` | Base + `-DWITH_CALIBRATION_RIG` | `102.17 s` | ✅ `success: true` | **59,604 B** (`18.2%`) | **1,804,279 B** (`28.7%`) | `1,804,536 B` |

### 4.3 ELF Section & Top `.dram0` Symbol Inspection (`v0.3.0-dev.51`)

> **Toolchain Note:** Extracted from the `v0.3.0-dev.51` (`802768575082b2e324637d542d7470e9d3a20e9d`, `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` `02d0b2de`, `pioarduino 55.03.311` / `ESP-IDF 5.5.5`) build reports and `.pio/build/ex3-waveshare-nano/firmware.elf` (`xtensa-esp32s3-elf-size -A` / `xtensa-esp32s3-elf-objdump -t`).

| Environment | `DIRAM` Total (% / Breakdown) | `IRAM` | `.flash.text` | `.flash.rodata` |
|---|---|---|---|---|
| `ex3-waveshare-nano` | `161,307 B` (`47.2%`: `.text` `101,791 B`, `.bss` `39,088 B`, `.data` `20,428 B`) | `16,384 B` (`100.0%`) | `1,287,632 B` | `371,848 B` |
| `ex3-waveshare-nano-diag` | `161,947 B` (`47.39%`: `.text` `102,111 B`, `.bss` `39,392 B`, `.data` `20,444 B`) | `16,384 B` (`100.0%`) | `1,300,964 B` | `375,288 B` |
| `ex3-waveshare-nano-rig` | `161,395 B` (`47.22%`: `.text` `101,791 B`, `.bss` `39,176 B`, `.data` `20,428 B`) | `16,384 B` (`100.0%`) | `1,292,852 B` | `372,824 B` |

**Primary Production ELF Sections (`ex3-waveshare-nano`):**

| ELF Section | Size (Bytes) | Description |
|---|---|---|
| `.dram0.data` | `20,428 B` | Initialized static internal DRAM (includes `mcpServer` `_ZL9mcpServer` = `104 B`) |
| `.dram0.bss` | `39,088 B` | Zero-initialized static internal DRAM |
| **Static DRAM Total (`.data` + `.bss`)** | **`59,516 B`** | **`18.2%` of `327,680 B` (`268,164 B` static headroom)** |
| `.iram0.text` | `117,147 B` | Instruction RAM (ISR-safe & critical hot code) |
| `.flash.text` | `1,287,632 B` | XIP Flash code segment |
| `.flash.rodata` | `371,848 B` | Flash read-only data & gzipped portal assets |

**Top 5 `.dram0` Symbols (`ex3-waveshare-nano`):**

| Rank | Symbol Name | Section | Size (Bytes) | % of 320 KB Internal DRAM | Threshold (`< 10%`) |
|---|---|---|---|---|---|
| 1 | `g_cnxMgr` | `.dram0.bss` | `3,880 B` | `1.18%` | ✅ PASS |
| 2 | `app` (`Application`) | `.dram0.bss` | `3,344 B` | `1.02%` | ✅ PASS |
| 3 | `Redactor::_instance` | `.dram0.bss` | `3,172 B` | `0.97%` | ✅ PASS |
| 4 | `port_IntStack` | `.dram0.data` | `3,072 B` | `0.94%` | ✅ PASS |
| 5 | `echenonModule` (`EchenonModule`) | `.dram0.bss` | `2,872 B` | `0.88%` | ✅ PASS |

### 4.4 Static Hygiene, Encapsulation & Security Audit Checks

| Check Description | Command Executed | Observed Result | Verdict |
|---|---|---|---|
| Authored `src/` file count & Apache-2.0 headers | `find src -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \| wc -l` / `grep -rnL "SPDX-License-Identifier: Apache-2.0" src/` | `63` C++ files; `0` missing headers | ✅ PASS |
| Global `extern Application` ban | `grep -rn "extern Application" src/` | `0` matches | ✅ PASS |
| Unscoped `extern` check in `src/` | `grep -rn "extern " src/` | `1` match (`FTMSServer.cpp:37`, guarded by `#ifdef UNIT_TEST`) | ✅ PASS |
| `std::function` `GRAVE-05` justification comments | `grep -rn -B 2 -A 1 "std::function" src/` | `13` occurrences across 6 headers; all `13` carry `// GRAVE-05:` comments | ✅ PASS |
| Newlib-nano `%zu` / `%llu` specifier removal (`WO-09.14`) | `grep -rn "%zu\|%llu" src/` | `0` active format strings (`1` explanatory comment at `SignalLogic.h:494`) | ✅ PASS |
| Legacy `/api/echenon/rig/` HTTP endpoint removal | `grep -rn "/api/echenon/rig/" src/ web/ tools/` | `0` matches | ✅ PASS |
| `AuthManager::checkPortalAuth()` route coverage | `grep -rn "checkPortalAuth" src/` | `23` guarded endpoints/helpers (`EchenonModule`: 13, `SignalLogger`: 5, `DiagnosticsRoutes`: 4, `SessionOverlay`: 1) | ✅ PASS |
| Bare `#hex` color check in `web/` | `grep -rn -E "#[0-9a-fA-F]{3,8}" web/ \| grep -v "var(--"` | `0` matches in `web/*.js` (only `web/ride.html` `:root` / `<meta>` and `web/manifest.json`) | ✅ PASS |
| Personal SSID / BSSID / URL redaction | `git grep -n -E "<REDACTED_SSID>\|<REDACTED_OUI>\|<REDACTED_SESSION_URL>" -- ":!.agents"` | `0` matches | ✅ PASS |
| Framework submodule visibility & pin (`WO-09.12`) | `git -C framework describe --tags --exact-match HEAD` / `.gitmodules` | `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2` (`02d0b2de`, `isPrivate: false`) | ✅ PASS |
| `WITH_OTA` disabled for Beta | `grep -rn "WITH_OTA" platformio.ini` | `; -D WITH_OTA` commented out at `platformio.ini:67` | ✅ PASS |

### 4.5 Hardware-in-the-Loop (HIL) Evidence & Live Bench Soak (`🟢 MEASURED`)

- **Live `v0.3.0-dev.51` Bench Verification on `/dev/cu.usbmodem101` (`ex3-waveshare-nano`, `ESP-IDF 5.5.5`, `8MB PSRAM`, `WO-09.14`):**
  - **`81 s` Live Serial Soak (`monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log`):** Confirmed `App version: v0.3.0-dev.51` with `0` panics and `0` WDT resets.
  - **Post-`DeviceCrypto` Uploader Boot Ordering Verified (`0` Errors):** **`0` occurrences** of `KeyManager: DeviceCrypto not initialized.` (all 3 cloud uploaders — `StravaUploader`, `IntervalsIcuUploader`, `WahooUploader` — query `KeyManager` after `DeviceCrypto` initialization in `app.begin(config)`).
  - **`RidePersistence` `%u` Format Fix Verified Live on UART:** **`I (1194) RidePersistence: Scanned 0 persisted rides (0 bytes total)`** (exact integer formatted cleanly; zero literal `zu` anywhere in UART logs).
  - **Heap & Stack Stability:**
    - Boot heap free: `8,599,311 B` (min-free `8,558,752 B`).
    - `uptime=30s` and `uptime=60s` heartbeat: `free_heap=8,376,307 B`, `dram=108,775 B`, `dram_min=75,880 B`.
    - `DataManager: Worker stack high water mark: 10,732 B free (of 12,288)`.
- **Preserved `v0.3.0-dev.50` `v1.0.0-beta.2` Bench Verification (`/dev/cu.usbmodem101`, `70 s` live serial soak):**
  - Verified upstream `v1.0.0-beta.2` (`#329`) newlib-nano `%u` UART fixes (`WebServerManager: Global body-size middleware registered (default=4096, 10 overrides)`, `MCPBuiltinTools: Registered 8 built-in tools`, `MCPServer: MCP Server started with 27 tools (8 built-in)`), `SPI SRAM memory test OK`, and first-boot SoftAP Captive Portal fallback (`WifiManager: No credentials found. Falling back to AP mode.`, `Access Point 'echenon' is online. IP: 192.168.4.1`, `WebServerManager: HTTP server started on port 80`, `EchenonModule: mDNS responder started: http://echenon.local`).
- **Preserved Historical `v0.3.0-dev.48` & `v0.3.0-dev.49` Provisioned STA Bench Soak Baseline (`/dev/cu.usbmodem101`, `ex3-waveshare-nano`):**
  - **`v0.3.0-dev.48` Pre-Fix Soak:** Continuous `8,235 s` (~2h 17m) uptime with `0` crashes/resets, `13/13` portal routes `200 OK`, BLE HRS/FTMS streaming verified, and `/api/echenon/rig/* -> 404`; caught `POST /api/mcp -> 404` due to missing Ring-2 `MCPServer` wiring, resolved in `WO-09.10` (`3173600c84abf2714e57e07bb262b3bdb29ce0e5`).
  - **`v0.3.0-dev.49` Post-Fix Provisioned STA/HTTP/MCP/BLE Soak (`82.4 min` / `4,944 s` continuous):**
    - **Boot & Registry:** `1` boot marker (`ESP32 App Foundation v0.3.0-dev.49 — Echenon — booting`), `0` panic/backtrace/WDT/brownout faults; LittleFS `/littlefs` and `/config` mounted cleanly; `MCPServer: Config applied: enabled=1 readOnly=1`; `Registry locked — tickables=10 webContributors=12 mcpContributors=6 dataSources=1 statusReporter=registered`; `EchenonModule: Registered MCP tools: echenon_get_telemetry, echenon_calibration, echenon_recalibration, echenon_upload_power_table, echenon_set_ftms_smoothing`; clean transition to `State -> RUNNING` at `6.83 s`.
    - **`GET /api/system` (`200 OK` at `uptime: 4944`):** `firmware_version: "0.3.0-dev.49"`, `heap_free: 65,723 B` (steady heartbeat `dram = 66,983–67,595 B`), `heap_min_free: 39,828 B` (startup transient peak, `2.65×` the 15 KB framework floor; steady-state headroom `65.7–67.6 KB` well above 40 KB), `heap_max_alloc: 31,744 B`, `psram_free: 8,267,612 B / 8,388,608 B`, `chip_model: ESP32-S3` (rev 2, 2 cores), `chip_temp: 33.8 °C`, `ota_running_partition: app0`.
    - **Portal & REST (`http://echenon.local` / `<LAN_IP>`):** `14/14` routes (`/`, `/mcp.js` [`4,514 B`], `/echenon-dashboard.js`, `/calibration-wizard.js`, `/recalibration.js`, `/diagnostics.js`, `/ride`, `/ride.js`, `/session.js`, `/stored_rides.js`, `/upload_settings.js`, `/intervals_settings.js`, `/wahoo_settings.js`, `/manifest.json`) returned `200 OK` (`Content-Encoding: gzip`), plus `GET /api/echenon/live -> 200 OK`, and legacy `/api/echenon/rig/status` (`GET`) & `/api/echenon/rig/command` (`POST`) -> both `404`. *(Auth note: board `/config` has Portal Auth `enabled: false`, so auth-enabled `401` enforcement rests on code inspection of the 23 `checkPortalAuth` call sites + `MCPServer` bearer middleware.)*
    - **MCP End-to-End (`/api/mcp`):** `GET /api/mcp/status -> 200 OK` (`{"initialized": true, "enabled": true, "readOnly": true, "toolCount": 27}`); `POST /api/mcp` `initialize -> 200 OK` (`serverInfo: {"name": "echenon", "version": "0.3.0-dev.49"}`); `tools/list -> 200 OK` (`27` tools including all `5` production Echenon tools, excluding `echenon_rig_*`); `tools/call` (`echenon_get_telemetry`) `-> 200 OK` (`isError: false`, live JSON payload); `tools/call` (`echenon_set_ftms_smoothing`) under default `readOnly: true` -> JSON-RPC `error.code: -32000` (`"Tool modifies state but server is in read-only mode"`).
    - **BLE Advertising (`BleakScanner`):** `Echenon` advertising HRS `0000180d-...` & FTMS `00001826-...` (`-51 dBm`).
- **Committed Hardware Verification Baseline:**
  - **Phase 2 BLE Strap & Coexistence Verification:**
    - [`docs/measurements/phase2/wo5-strap-acceptance/evidence.md`](measurements/phase2/wo5-strap-acceptance/evidence.md) — Physical HRM strap discovery, connection, notification relay, and disconnect recovery verified on hardware.
    - [`docs/measurements/phase2/wo6-coexistence/evidence.md`](measurements/phase2/wo6-coexistence/evidence.md) — Concurrent WiFi STA + WebSocket portal + BLE Central (`HRMClient`) + BLE Peripheral (`HRMRepeater` / `FTMSServer`) RF coexistence verified.
  - **Phase 3 Multi-Sink & Portal Soak Verification (`/dev/cu.usbmodem101`):**
    - [`docs/measurements/phase3/evidence/pr19-hardware-verification.md`](measurements/phase3/evidence/pr19-hardware-verification.md) (`v0.3.0-dev.44`) — Dual-sink (`StravaUploader` + `IntervalsIcuUploader`) pipeline, LittleFS `/littlefs` (2.75 MB) & `/config` (1.0 MB) mounts, `KeyManager` + `DeviceCrypto` keystore, and BLE HR/FTMS coexistence verified.
    - [`docs/measurements/phase3/evidence/pr21-wahoo-verification.md`](measurements/phase3/evidence/pr21-wahoo-verification.md) (`v0.3.0-dev.45`) — Triple-sink (`Strava`, `Intervals.icu`, `Wahoo Cloud`) registration, OAuth2 128-bit state nonce generation, gzipped portal asset serving (`ETag` + `Content-Encoding: gzip`), and runtime memory stability (`dram_free = 102,895 B`, `dram_min = 60,844 B`, `psram_free = 8,267,732 B`) verified on target hardware.

---

## 5. Owner Launch Gate Checklist (Explicit Execution Order)

Per [`.agents/rules/03-versioning.md`](../.agents/rules/03-versioning.md) and programme governance, `beta` tags, `main` merges, `deploy_release.py` execution, and public repository flips require explicit human execution. Complete the steps below **in exact sequential order**:

### Step 1 — Mandatory Pre-Tag 60 s Live Bench Soak Gate (`ex3-waveshare-nano`) — ✅ VERIFIED (`v0.3.0-dev.51` + `v0.3.0-dev.49` STA Baseline)
> [!CAUTION]
> **Power Interlock Safety Rule:** Before connecting a USB-C cable between your computer and the Waveshare ESP32-S3-Nano on an installed v1 Passive Tap Board, **verify that mechanical switch `SW0` (`VIN`) is switched OFF** (isolating the bike's 9 V / MP1584EN 5 V rail from USB `VBUS`). Never connect `VIN` and `USB-C` simultaneously.

- [x] **1.1** Verify `SW0` (`VIN` disconnect switch) is **OFF**, then connect the Waveshare ESP32-S3-Nano via USB-C (`/dev/cu.usbmodem101`).
- [x] **1.2** Tag pre-flash dev build, build, and flash `ex3-waveshare-nano` (`v0.3.0-dev.51`, `WO-09.14`, `cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2`, `ESP-IDF 5.5.5`) to the board (building on the `v0.3.0-dev.50` SoftAP/`#329` verification and `v0.3.0-dev.49` provisioned STA baseline).
- [x] **1.3** Monitor serial boot output for **>= 60 seconds** (`81 s` live serial soak completed on `v0.3.0-dev.51` [`monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log`]; `70 s` completed on `v0.3.0-dev.50`; `4,944 s` / `82.4 min` provisioned STA soak completed on `v0.3.0-dev.49`) and confirm:
  - `v0.3.0-dev.51`: `App version: v0.3.0-dev.51`, `0` panics and `0` WDT resets; **`0` occurrences** of `KeyManager: DeviceCrypto not initialized.`; **`I (1194) RidePersistence: Scanned 0 persisted rides (0 bytes total)`** (zero literal `zu`).
  - `v0.3.0-dev.51`: Boot heap free `8,599,311 B` (min-free `8,558,752 B`), `uptime=30s`/`60s` heartbeat `free_heap=8,376,307 B`, `dram=108,775 B`, `dram_min=75,880 B`, and `DataManager: Worker stack high water mark: 10,732 B free (of 12,288)`.
  - `v0.3.0-dev.50`: `#329` newlib-nano `%u` UART fixes verified (`WebServerManager: Global body-size middleware registered (default=4096, 10 overrides)`, `MCPBuiltinTools: Registered 8 built-in tools`, `MCPServer: MCP Server started with 27 tools (8 built-in)`).
  - `v0.3.0-dev.49` historical STA baseline: clean boot to `RUNNING` (`6.83 s`), LittleFS `/config` + `/littlefs` mounts, `GET /api/system` `heap_free = 65,723 B` (`heap_min_free = 39,828 B`), `8,267,612 B / 8,388,608 B` Octal PSRAM.
- [x] **1.4** Verify Portal Auth, Captive Portal & MCP Smoke Check:
  - `v0.3.0-dev.50`: First-boot SoftAP Captive Portal fallback verified (`WifiManager: No credentials found. Falling back to AP mode.`, `Access Point 'echenon' is online. IP: 192.168.4.1`, `WebServerManager: HTTP server started on port 80`, `EchenonModule: mDNS responder started: http://echenon.local`).
  - `v0.3.0-dev.49` STA baseline: `GET /api/echenon/live -> 200 OK` (with 23 `checkPortalAuth` call sites + `MCPServer` bearer middleware), `14/14` gzipped portal routes `200 OK` (`/mcp.js` `4,514 B`), `GET /api/mcp/status` (`27` tools, `readOnly: true`), `POST /api/mcp` (`initialize`, `tools/list`, `tools/call` → `echenon_get_telemetry`, `-32000` `readOnly` guard), and legacy `/api/echenon/rig/* -> 404`.
- [x] **1.5** Confirm BLE advertising (`v0.3.0-dev.49` baseline: `Echenon` advertising FTMS `0x1826` and HRS `0x180D` at `-51 dBm`) and clean hardware boot to `RUNNING`.

### Step 2 — Verify Upstream Framework Submodule Visibility — ✅ VERIFIED (`cblabs-org/esp32-app-foundation` `@ v1.0.0-beta.2`)
- [x] **2.1** Confirm `cblabs-org/esp32-app-foundation` (referenced in `.gitmodules` at `framework`, pinned to `v1.0.0-beta.2` / `02d0b2de` in `WO-09.12`) is **Public** on GitHub (`isPrivate: false`) and accessible to external clones without authentication.

### Step 3 — Review Step 6A Audit & Merge `PR-09` (`PR #20`) to `main`
- [ ] **3.1** Review the Principal Architect Step 6A Cold-Firewall Audit report and this release audit (`docs/release-audit-v0.3.0-beta.1.md`).
- [ ] **3.2** Merge `feat/pr-09-integration` (`PR #20` / `PR-09`) into `main` and pull `main` locally.

### Step 4 — Cut & Push the `v0.3.0-beta.1` Tag (Owner CLI)
- [ ] **4.1** On clean `main` HEAD, verify the latest tag and create the annotated beta tag:
  ```bash
  git checkout main && git pull origin main
  git describe --tags --abbrev=0
  git tag -a v0.3.0-beta.1 -m "Echenon v0.3.0-beta.1 — v1 Passive Tap Board Beta Release"
  git push origin v0.3.0-beta.1
  ```
- [ ] **4.2** Rebuild `ex3-waveshare-nano` so `inject_version.py` bakes `v0.3.0-beta.1` into the release firmware binary, flash over USB-C (`SW0` OFF), confirm `GET /api/system` reports `"firmware_version": "v0.3.0-beta.1"`, and run `deploy_release.py` interactively from the owner terminal if cutting signed release artifacts (per Rule 03).

### Step 5 — Flip `cblabs-org/echenon` Public & Publish Beta Release
- [ ] **5.1** Flip `cblabs-org/echenon` repository visibility to **Public** on GitHub.
- [ ] **5.2** Publish the GitHub Pre-Release for tag `v0.3.0-beta.1` using the [`CHANGELOG.md`](../CHANGELOG.md) `v0.3.0-beta.1` notes, attaching the pre-built `ex3-waveshare-nano` firmware binary (`firmware.bin`), `partitions.bin`, `bootloader.bin`, and pointing builders to [`docs/beta-build-guide.md`](beta-build-guide.md).
