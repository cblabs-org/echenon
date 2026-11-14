---
schema_version: 1
wo_id: WO-09.10
pr_id: PR-09
title: Wire Ring-2 MCPServer in Composition Root & Rig Read-Only Hint
complexity: standard
status: PENDING
allowed_files:
- src/main.cpp
- src/echenon/EchenonModule.cpp
- tools/calibration-rig/calibration_sweep.py
forbidden_files:
- framework/**
- web/**
- platformio.ini
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 833aa94e44b45633741d491138a864517ccb027236eb7bf9592d26f2872214ad
completion: null
---

# WO-09.10 — Wire Ring-2 MCPServer in Composition Root & Rig Read-Only Hint

## 1. Objective

Live bench verification of `v0.3.0-dev.48` on `/dev/cu.usbmodem101` revealed that although `Application` discovers `IMCPContributor` (`EchenonModule` and framework contributors), the Ring-2 `MCPServer` module was never instantiated, registered, or started in [`src/main.cpp`](../../../../src/main.cpp), causing `POST /api/mcp` and `GET /api/mcp/status` to return `404 Not Found`.

This Work Order wires Ring-2 `MCPServer` in the composition root (`src/main.cpp`), adds the `framework-expert` Rule 6 tool-registration log in [`src/echenon/EchenonModule.cpp`](../../../../src/echenon/EchenonModule.cpp), and adds an actionable read-only `-32000` error hint in [`tools/calibration-rig/calibration_sweep.py`](../../../../tools/calibration-rig/calibration_sweep.py).

## 2. Strict Boundary Box

- **Allowed files:**
  - `src/main.cpp`
  - `src/echenon/EchenonModule.cpp`
  - `tools/calibration-rig/calibration_sweep.py`
- **Forbidden files:** `framework/**`, `web/**`, `platformio.ini`, or any file outside the allowlist above.
- **Rules & Governance:** Keep `.agents/` completely unmodified (aside from updating Part II of this `WORKORDER.md` via `~/.local/bin/pa wo verify WO-09.10`). Never run `pio` CLI directly in `run_command`; never use `/tmp` or bash `$VAR` loops.

## 3. Technical Specification

### 3.1 `src/main.cpp` — Instantiate, Register & Begin Ring-2 `MCPServer`
1. **Include (`line 12`, right after `#include "echenon/EchenonModule.h"`):**
   - Add `#include "MCPServer.hpp"`.
   - **Critical LDF invariant:** Use the bare header `#include "MCPServer.hpp"` (NOT `"MCPServer/MCPServer.hpp"`), so PlatformIO's `lib_ldf_mode = deep+` matches `framework/modules/MCPServer/library.json` via `lib_extra_dirs = framework/modules` and compiles/links `libMCPServer.a` (`MCPServer.cpp`, `MCPBuiltinTools.cpp`, `MCPToolRegistry.cpp`), consistent with `#include "EncoderModule.hpp"`.
2. **Static module instance (`line 45`, right after `static EchenonModule echenonModule;`):**
   - Declare `static MCPServer mcpServer;` (no global/non-static `Application` references; preserves `Dimension 2` encapsulation).
3. **Registration before `app.begin(config);` (around `line 106` or before `line 189`):**
   - Call `mcpServer.setAppName("Echenon");`
   - Call `app.registerModule(&mcpServer);` (so `MCPServer` participates in `ITickable`, `IConfigurable` namespace `"mcp"`, and `IWebContributor` routes `/mcp.js`, `/api/mcp`, `/api/mcp/status`, `/api/mcp/config`, plus System -> MCP tab before `app.begin(config)` compacts and locks the registries).
4. **Startup initialization after `app.begin(config);` and `#ifdef WITH_TIME timeManager.begin(); #endif` (right after `line 200`):**
   - Call `mcpServer.begin(...)` passing all contributors and optional managers:
     ```cpp
     mcpServer.begin(
         app.getMcpContributors(),
         &app.getConfigManager(),
     #ifdef WITH_TIME
         &timeManager,
     #else
         nullptr,
     #endif
         &app.getWifiManager(),
         &app.getAuthManager(),
         &app.getDataManager()
     );
     ```

### 3.2 `src/echenon/EchenonModule.cpp` — Rule 6 Registration `ESP_LOGI`
- At the end of `EchenonModule::registerMCPTools(MCPToolRegistry& registry)` right before the closing `}` (`lines 1096–1097`), log all registered tool names via `ESP_LOGI(TAG, ...)`:
  - Under `#ifdef WITH_CALIBRATION_RIG`: log the 7 registered tools (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`, `echenon_rig_status`, `echenon_rig_command`).
  - Under `#else`: log the 5 production tools (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`).

### 3.3 `tools/calibration-rig/calibration_sweep.py` — Actionable Read-Only `-32000` Error Hint
- In `RigHTTP._call_tool()` (`lines 130–132`), inspect `rpc_resp["error"]` before raising the generic `MCP JSON-RPC error`:
  - When `err` indicates read-only mode (`(isinstance(err, dict) and (err.get("code") == -32000 or "read-only" in str(err.get("message", "")).lower())) or "read-only" in str(err).lower()`), raise a `RuntimeError` explaining clearly that `MCPServer` defaults to `readOnly: true` and giving both concrete remediation steps:
    1. `curl -X POST http://<host>/api/mcp/config -H "Content-Type: application/json" -d '{"readOnly":false}'` (plus `-H "Authorization: Bearer <token>"` if Portal Auth is enabled), or
    2. Disable read-only mode in the Web Portal under **System -> MCP**.

## 4. Verification

1. Run `python3 -m py_compile tools/calibration-rig/calibration_sweep.py` to confirm clean Python syntax.
2. Confirm `git status --short` shows only allowed files modified and `.agents/` untouched.
3. Run `~/.local/bin/pa wo verify WO-09.10` (`pio test -e native --verbose`, stamps receipt and commits on `pr-09-integration/wo10-mcp-server-wiring`).
4. Report the resulting commit SHA and `receipt_id` to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
