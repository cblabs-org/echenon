---
schema_version: 1
wo_id: WO-09.1
pr_id: PR-09
title: MCP Contributor Tools, Portal Auth Guards, Persistence Fixes & Encapsulation
complexity: standard
status: PENDING
allowed_files:
- src/main.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/echenon/SignalLogger.h
- src/echenon/SignalLogger.cpp
- hardware/ex3-waveshare-nano/board.h
- tools/calibration-rig/calibration_sweep.py
forbidden_files:
- framework/**
- web/**
- docs/**
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: aaa5d04ba5411af94cadbb58de2041a6ddb030d3d0a9b9afbcf89f84737eb91d
completion: null
---

# WO-09.1 — MCP Contributor Tools, Portal Auth Guards, Persistence Fixes & Encapsulation

## 1. Objective

Close PR-09's Dimension 5 (Security & Auth), Dimension 2 (Encapsulation & GRAVE-05), MCP Contributor integration, Calibration NVS persistence bugs, and frozen v1 `board.h` pin finalisation before downstream house-style and portal work orders run:

1. Implement `IMCPContributor` on `EchenonModule` using stateless lambdas only (`void* context`, zero `std::function`).
2. Delete HTTP routes `/api/echenon/rig/status` and `/api/echenon/rig/command` (`#ifdef WITH_CALIBRATION_RIG`) and migrate CalibrationRig control 100% onto MCP (`echenon_rig_status`, `echenon_rig_command`), updating `tools/calibration-rig/calibration_sweep.py` to call `POST /api/mcp` JSON-RPC 2.0 (with optional `--mcp-token` / `ECHENON_MCP_TOKEN` Bearer auth).
3. Inject `AuthManager*` into `EchenonModule` and guard all active `/api/echenon/*` portal endpoints on `EchenonModule` and `SignalLogger` with `checkPortalAuth(request, response)`, plus fix calibration/recalibration config persistence and add `POST /api/echenon/calibration/reset`.
4. Eliminate both `extern Application app;` couplings in `EchenonModule.cpp`, mark top-level module instances `static` in `src/main.cpp`, and finalise `hardware/ex3-waveshare-nano/board.h` for Frozen v1 + reserved UART0 GPIO 43/44.

## 2. Strict Boundary Box

- **Allowed files:** `src/main.cpp`, `src/echenon/EchenonModule.h`, `src/echenon/EchenonModule.cpp`, `src/echenon/SignalLogger.h`, `src/echenon/SignalLogger.cpp`, `hardware/ex3-waveshare-nano/board.h`, `tools/calibration-rig/calibration_sweep.py`.
- **Forbidden files:** `framework/**`, `web/**`, `docs/**`, and any C++/header file not listed above.
- **GRAVE-05:** `MCPToolHandler` is `void(*)(void* context, JsonObject& params, JsonObject& result)`. Pass stateless capture-less lambdas (`[](void* ctx, JsonObject& params, JsonObject& result) { ... }`) with `this` as `context`. Never store `std::function` in `EchenonModule` or MCP handlers.
- **PlatformIO MCP:** Always use `call_mcp_tool` (`ServerName: "platformio-mcp_platformio"`) for interactive `build_project` and `run_tests` checks — never run `pio` directly in `run_command`.
- **Shell hygiene:** Never use bash `$VAR` loops or `/tmp` paths in `run_command`.

## 3. Technical Specification

### 3.1 `src/echenon/EchenonModule.h`
1. Forward-declare `class AuthManager;` and `class MCPToolRegistry;` near the top alongside `ConfigManager` / `EventBus`.
2. Inherit `public IMCPContributor` (`AppFoundation.h` already includes `interfaces/IMCPContributor.hpp`; `Application::registerModule(&echenonModule)` auto-detects `IMCPContributor` via `std::is_base_of_v` without extra registration in `src/main.cpp`). Update class Doxygen comment to mention `IMCPContributor`.
3. Under public interfaces add:
   ```cpp
   // --- IMCPContributor ---
   void registerMCPTools(MCPToolRegistry& registry) override;
   ```
4. Under Dependency Injection add:
   ```cpp
   void setAuthManager(AuthManager* am) { _authManager = am; }
   ```
5. In `private:`, add `AuthManager* _authManager = nullptr;` next to `_configManager` / `_eventBus`.
6. Ensure clean conditional helper or inline guard readability; keep all existing public accessors intact.

### 3.2 `src/echenon/EchenonModule.cpp` — `IMCPContributor::registerMCPTools`
1. `#include "MCPServer/MCPToolRegistry.hpp"` at the top (`AuthManager.hpp` is already included on line 16).
2. Implement `void EchenonModule::registerMCPTools(MCPToolRegistry& registry)` registering tools using **stateless lambdas** (`context = this`):
   - `echenon_get_telemetry` (`mutatesState = false`, no schema):
     Returns `cadence_rpm`, `power_watts`, `speed_kph`, `hr` (from `#ifdef WITH_BLE` `_hrmClient.getHeartRateBpm()`, falling back `#ifdef WITH_SESSION` to `_sessionManager->getHeartRateBpm()`), `level_echelon`, `level_internal`, `continuous_level`, `p_tilde` (`getNormalizedPosition()`), `pot_filtered` (`getFilteredWiper()`), `pos_done` (`_calibrationWizard.isPosDone()`), `session_state`, and `runaway` (`_runawayDetector.isRunaway()`).
   - `echenon_calibration` (`mutatesState = true`):
     Schema: `{"type":"object","properties":{"action":{"type":"string","enum":["start","stop","record","reset"]},"level":{"type":"integer","minimum":1,"maximum":32}},"required":["action"]}`.
     Actions:
     - `"start"` -> `self->_calibrationWizard.start()`
     - `"stop"` -> `self->_calibrationWizard.stop()`
     - `"record"` -> require `params["level"].is<int>()` (`1..32`); call `self->_calibrationWizard.recordLevel(lvl)`; if `true`, call `self->syncPhysicsCalibration(); if (self->_configManager) self->_configManager->saveModuleConfig(self);`, else set `result["isError"] = true` and `result["error"] = "Invalid level or calibration not active"`.
     - `"reset"` -> `self->_calibrationWizard.reset(); self->syncPhysicsCalibration(); if (self->_configManager) self->_configManager->saveModuleConfig(self);`
     - Else set `result["isError"] = true` and `result["error"]`.
     Populate `active`, `step`, `complete`, `pos_done`.
   - `echenon_recalibration` (`mutatesState = true`):
     Schema: `{"type":"object","properties":{"action":{"type":"string","enum":["add_point","reset"]},"level":{"type":"integer","minimum":1,"maximum":32},"power":{"type":"number","minimum":0}},"required":["action"]}`.
     Actions:
     - `"add_point"` -> validate `level` (`is<int>()`) and `power` (`is<float>() || is<int>()`); call `self->_recalibrationUI.addPoint(lvl, pwr)`; if `true`, call `self->syncPhysicsCalibration(); if (self->_configManager) self->_configManager->saveModuleConfig(self);`, else set `result["isError"] = true` and `result["error"]`.
     - `"reset"` -> `self->_recalibrationUI.reset(); self->syncPhysicsCalibration(); if (self->_configManager) self->_configManager->saveModuleConfig(self);`
     - Else set `result["isError"] = true` and `result["error"]`.
     Populate `count`, `max`.
   - `echenon_upload_power_table` (`mutatesState = true`):
     Schema: `{"type":"object","properties":{"csv":{"type":"string"}},"required":["csv"]}`.
     Validate non-empty string `csv`; parse via `PowerTable newTable; bool ok = newTable.parseCSV(csv, strlen(csv));`. If `ok`, call `self->_powerEngine.setTable(newTable); result["success"] = true;`, else set `result["isError"] = true; result["error"] = "Failed to parse CSV power table";`.
   - `#ifdef WITH_FTMS` -> `echenon_set_ftms_smoothing` (`mutatesState = true`):
     Schema: `{"type":"object","properties":{"seconds":{"type":"number","minimum":0,"maximum":10}},"required":["seconds"]}`.
     Validate `seconds` numeric, clamp `0.0f..10.0f`, set `self->_ftmsPowerSmoothS`, update `self->_ftmsServer->setPowerSmoothingWindow(val)`, call `if (self->_configManager) self->_configManager->saveModuleConfig(self);`, set `result["ftms_power_smooth_s"] = self->_ftmsPowerSmoothS`.
   - `#ifdef WITH_CALIBRATION_RIG`:
     - `echenon_rig_status` (`mutatesState = false`, no schema):
       Populate `state`, `level`, `target`, `cadence`, `steps_remaining`, `settle_timeout`, `pot`, `derived_level`, `gpio` (`enc_a_pin/val`, `enc_b_pin/val`, `cad_pin/val`), `adc` (`pot_raw`, `cad_sense`, `enc_vcc`, `enc_a_sense`, `enc_b_sense`), AND `status_line` (`self->_calibrationRig.handleStatus().c_str()`).
     - `echenon_rig_command` (`mutatesState = true`):
       Schema: `{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}`.
       Trim string; if empty set `result["isError"] = true; result["error"] = "ERROR empty command";`, else call `self->_calibrationRig.handleCommand(String(cmd.c_str())); result["command"] = cmd; result["status_line"] = self->_calibrationRig.handleStatus().c_str();`.

### 3.3 `src/echenon/EchenonModule.cpp` — HTTP Routes, Auth Guards & Persistence Fixes
1. **Delete HTTP rig routes:** Completely remove `/api/echenon/rig/status` and `/api/echenon/rig/command` (`EchenonModule.cpp` lines ~409-471) so CalibrationRig is controlled strictly via MCP.
2. **AuthManager guards:** At the top of EVERY active `/api/echenon/*` route handler in `EchenonModule::registerRoutes()` (leave `/echenon-dashboard.js`, `/calibration-wizard.js`, `/recalibration.js`, `/diagnostics.js` static JS routes unguarded so the portal shell can load them), enforce:
   ```cpp
   if (_authManager && !_authManager->checkPortalAuth(request, response)) {
       return ESP_OK;
   }
   ```
   (Verify `DiagnosticsRoutes.cpp:128-130`: `checkPortalAuth` already sends HTTP `401` when returning `false`).
   Guarded routes:
   - `POST /api/echenon/calibration/start`
   - `POST /api/echenon/calibration/stop`
   - `POST /api/echenon/calibration/record-level`
   - `POST /api/echenon/calibration/reset` *(new route — see #3)*
   - `GET  /api/echenon/calibration/status`
   - `POST /api/echenon/recalibration/point`
   - `GET  /api/echenon/recalibration/points`
   - `POST /api/echenon/recalibration/reset`
   - `POST /api/echenon/power-table/upload`
   - `GET  /api/echenon/power-table/active`
   - `GET  /api/echenon/live`
   - `#ifdef WITH_FTMS` `GET /api/echenon/ftms-smoothing` and `POST /api/echenon/ftms-smoothing`
3. **Add `POST /api/echenon/calibration/reset`:** Guard with `_authManager->checkPortalAuth(request, response)`, call `_calibrationWizard.reset(); syncPhysicsCalibration(); if (_configManager) { _configManager->saveModuleConfig(this); }`, and return `200` `{"success":true,"active":false,"pos_done":false}`.
4. **Fix missing NVS persistence:** In `POST /api/echenon/calibration/record-level`, `POST /api/echenon/recalibration/point`, and `POST /api/echenon/recalibration/reset`, call `if (_configManager) { _configManager->saveModuleConfig(this); }` whenever `ok` / reset succeeds right after `syncPhysicsCalibration()`.

### 3.4 `src/echenon/SignalLogger.h` & `src/echenon/SignalLogger.cpp` — Auth Guards
1. Forward-declare `class AuthManager;` in `SignalLogger.h`.
2. Update `void registerRoutes(PsychicHttpServer& server, AuthManager* authManager = nullptr);`.
3. In `SignalLogger.cpp` (under `#if !defined(UNIT_TEST)`), `#include "AuthManager.hpp"`.
4. Guard all 5 routes (`/api/echenon/status`, `/api/echenon/burst/start`, `/api/echenon/ride-log.csv`, `/api/echenon/events.csv`, `/api/echenon/burst.csv`) at entry with:
   ```cpp
   if (authManager && !authManager->checkPortalAuth(request, response)) {
       return ESP_OK;
   }
   ```
   (Capture `[this, authManager]` in each lambda).
5. In `EchenonModule.cpp` under `#ifdef WITH_SIGNAL_LOGGER`, pass `_authManager`:
   `_signalLogger.registerRoutes(server, _authManager);`

### 3.5 Encapsulation (`EchenonModule.cpp` & `src/main.cpp`)
1. In `EchenonModule.cpp`:
   - Remove `extern Application app;` at line ~234 (`#ifdef WITH_SIGNAL_LOGGER`): `_eventBus` is already injected via `setEventBus()`; replace with `if (_eventBus) { _signalLogger.setEventBus(_eventBus); }` (under `s_eventBusWired` once).
   - Remove `extern Application app;` at line ~511 (`registerRoutes`): replace `if (_ridePersistence)` with `if (_ridePersistence && _authManager) { static echenon::DiagnosticsRoutes diagRoutes(_diagnostics, *_ridePersistence); _diagRoutes = &diagRoutes; diagRoutes.registerRoutes(server, *_authManager); }`.
2. In `src/main.cpp`:
   - Wire `echenonModule.setAuthManager(&app.getAuthManager());` alongside `setConfigManager` and `setEventBus` before `app.registerModule(&echenonModule);`.
   - Mark `static EncoderModule encoderModule;`, `static Application app;`, and `static EchenonModule echenonModule;`.

### 3.6 `hardware/ex3-waveshare-nano/board.h`
1. Update the pin map banner comment (`line 10`) and GPIO 10 comment (`line 28`) from `"Phase 0/1"` / `"Phase 0/1 only"` to `"Phase 0/1 & DR-1 Frozen v1 Passive Tap Board, SPEC §4.4.1–§4.4.2"`.
2. Add explicit UART0 reservation constants matching SPEC §4.4.1–§4.4.2:
   ```cpp
   // Reserved UART0 pins for future ANT+ module (SPEC §4.4.1–§4.4.2) — leave unconnected in v1
   constexpr int8_t PIN_UART0_TX_RESERVED = 43;
   constexpr int8_t PIN_UART0_RX_RESERVED = 44;
   ```

### 3.7 `tools/calibration-rig/calibration_sweep.py`
1. Update `RigHTTP` (`tools/calibration-rig/calibration_sweep.py`) to communicate exclusively via MCP (`POST http://{host}/api/mcp` JSON-RPC 2.0 `tools/call`):
   - Support optional `mcp_token: str | None = None` in `RigHTTP.__init__` (seeded from `--mcp-token` CLI flag and `ECHENON_MCP_TOKEN` environment variable) and pass `Authorization: Bearer <token>` when set.
   - Add `_call_tool(self, name: str, arguments: dict) -> dict` sending `{"jsonrpc": "2.0", "id": <seq>, "method": "tools/call", "params": {"name": name, "arguments": arguments}}`, validating no JSON-RPC `"error"` and not `result.get("isError")`, and parsing JSON from `result["content"][0]["text"]`.
   - Route `send_command(cmd)` through `_call_tool("echenon_rig_command", {"command": cmd.strip()})` returning `status_line`.
   - Route `get_status_json()` and `get_status_line()` through read-only `_call_tool("echenon_rig_status", {})` (returning full dict or `status_line`).
2. Inspect `calibration_sweep.py` for any host `/tmp` paths (leave Android target `/sdcard/...` adb paths intact; ensure host dump temp files use `tempfile.gettempdir()` or a configurable/cwd-safe path, never hardcoded `/tmp`).

## 4. Verification

1. Via `call_mcp_tool` (`ServerName: "platformio-mcp_platformio"`):
   - Run `build_project` on `ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, and `ex3-waveshare-nano-rig` — all 3 MUST compile cleanly with 0 errors.
   - Run `run_tests` (`environment: "native"`).
2. Run `python3 -m py_compile tools/calibration-rig/calibration_sweep.py` and `grep -rn "extern Application" src/` (must return 0 matches).
3. Run `~/.local/bin/pa wo verify WO-09.1` to run the native test suite, stamp the cryptographic receipt, and commit on the stacked branch.
4. Post completion summary to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
