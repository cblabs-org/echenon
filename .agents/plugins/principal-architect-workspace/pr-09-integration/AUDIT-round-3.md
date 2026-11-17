---
schema_version: 1
pr_id: PR-09
github_pr: 20
audit_round: 3
head_sha: a53a7960acbba46157cb545da4ead72b1726c782
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-09 round=3 head_sha=a53a7960acbba46157cb545da4ead72b1726c782 verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)


## Summary

Cold-firewall Round 3 verification of **PR-09** (`feat/pr-09-integration`, GitHub PR #20, `head_sha: a53a7960acbba46157cb545da4ead72b1726c782`) against `main`, `SPEC.md`, the locked `PROJECT.md` Part I Charter (`WO-09.1` through `WO-09.11`), and prior audit rounds (`AUDIT-round-1.md`, `AUDIT-round-2.md`) is **APPROVED** with **0 blocking and 0 non-blocking findings**.

Round 3 specifically verifies `WO-09.10` (`3173600c84ab`) and `WO-09.11` (`1ab081beb1fb`) alongside the full 11-work-order PR diff:
- **`WO-09.10` (`src/main.cpp`, `src/echenon/EchenonModule.cpp`, `tools/calibration-rig/calibration_sweep.py`):**
  - Ring-2 `MCPServer` is included via bare `#include "MCPServer.hpp"` (`src/main.cpp:12`), resolving cleanly through PlatformIO LDF (`deep+`) against `framework/modules/MCPServer/library.json`.
  - `static MCPServer mcpServer;` is declared at file scope (`src/main.cpp:46`), configured with `mcpServer.setAppName("Echenon")`, and registered via `app.registerModule(&mcpServer)` (`src/main.cpp:108-109`) prior to `app.begin(config)` (`src/main.cpp:196`).
  - `mcpServer.begin(...)` is called after `timeManager.begin()` (`src/main.cpp:206-217`) with `app.getMcpContributors()`, `&app.getConfigManager()`, `&timeManager` (under `#ifdef WITH_TIME`, else `nullptr`), `&app.getWifiManager()`, `&app.getAuthManager()`, and `&app.getDataManager()`.
  - `EchenonModule::registerMCPTools()` (`src/echenon/EchenonModule.cpp:1096-1099`) logs all registered MCP tools via `ESP_LOGI(TAG, ...)` for both `WITH_CALIBRATION_RIG` (7 tools) and production `#else` (5 tools) builds.
  - `RigHTTP._call_tool()` (`tools/calibration-rig/calibration_sweep.py:130-143`) inspects JSON-RPC error `-32000` / `"read-only"` and raises an actionable `RuntimeError` detailing both `POST /api/mcp/config` (`{"readOnly":false}`) and Web Portal (`System -> MCP`) remediation steps.
- **`WO-09.11` (`docs/release-audit-v0.3.0-beta.1.md`):**
  - Synchronizes the release audit report with post-`MCPServer` build and ELF metrics (`ex3-waveshare-nano`: `58,996 B` static RAM / `18.0%`, `1,787,295 B` Flash / `28.4%`; `.dram0.data` = `20,124 B`, `.dram0.bss` = `38,872 B`, `mcpServer` = `104 B`; `-diag`: `59,316 B` RAM / `1,804,263 B` Flash; `-rig`: `59,084 B` RAM / `1,792,747 B` Flash).
  - Records the `v0.3.0-dev.48` (`8,235 s`) and `v0.3.0-dev.49` (`4,944 s` / `82.4 min`) live bench soak and `/api/mcp` end-to-end verification on `/dev/cu.usbmodem101`, including `14/14` gzipped portal routes (`/mcp.js` `200 OK`), `GET /api/mcp/status` (`27` tools, `readOnly: true`), `POST /api/mcp` (`initialize`, `tools/list`, `echenon_get_telemetry`, and `-32000` `readOnly` rejection on `echenon_set_ftms_smoothing`), `/api/echenon/rig/* -> 404`, and BLE advertising (`0x180D`, `0x1826`).
  - Candidly discloses the `heap_min_free = 39,828 B` startup transient high-water mark (`2.65x` the `15,000 B` framework floor, with steady-state free internal DRAM at `65,723-67,595 B` well above the 40 KB target) and the bench `/config` Portal Auth `enabled: false` state (`401` enforcement backed by code inspection of the 23 `checkPortalAuth` call sites and `MCPServer` bearer middleware).
  - Marks Owner Launch Gate Step `1.1`-`1.5` complete (`[x]`) while preserving Steps `2`-`5` (`[ ]`) as owner-gated.



## Five-Axis Evaluation

### 1. `spec_alignment` — PASS
- **Ring-2 `MCPServer` & `IMCPContributor` Wiring (`SPEC.md` §3.4, `WO-09.1`, `WO-09.10`):** `src/main.cpp` wires `MCPServer` into the composition root before `app.begin(config)` and initializes it after `timeManager.begin()`, exposing `/api/mcp`, `/api/mcp/status`, `/api/mcp/config`, and `/mcp.js` alongside the 5 production (7 rig) `EchenonModule` MCP tools. Legacy `/api/echenon/rig/*` HTTP routes remain completely removed (`0` matches across `src/`, `web/`, and `tools/`).
- **Hardware Specification, Builder Guide & Beta OTA Deferral (`SPEC.md` §1.3, §3.4, §4.4, §4.6, `WO-09.5`, `WO-09.6`):** `hardware/ex3-waveshare-nano/board.h`, `docs/beta-build-guide.md`, `docs/hardware_specification.md`, `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, and `SPEC.md` consistently specify `C1 x5` (`100 nF` to `GND` on `GPIO 4, 5, 8, 9, 10`), the mandatory `SW0` mechanical `VIN` / `USB-C` interlock, and USB-C beta updates while `WITH_OTA` is disabled.
- **Release Audit Synchronization (`WO-09.8`, `WO-09.9`, `WO-09.11`):** `docs/release-audit-v0.3.0-beta.1.md` accurately reflects the `SignalLogger` route paths, post-`WO-09.10` ELF/build numbers, and live `v0.3.0-dev.49` bench evidence.

### 2. `invariants` — PASS
- **Service Module Pattern & Encapsulation (`WO-09.1`-`WO-09.3`, `WO-09.10`):** Zero `extern Application` occurrences in `src/`; all composition-root modules in `src/main.cpp` (including `static MCPServer mcpServer;`) are file-scope `static`; `FTMSServer.cpp` guards `extern uint32_t millis();` under `#if !defined(ESP_PLATFORM)` and `#ifdef UNIT_TEST`; all 13 `std::function` uses in `src/echenon/` carry `// GRAVE-05:` cold-path/boot-bound justifications; all 7 MCP tool handlers use stateless capture-less C-style lambdas; all 63 C++ files in `src/` carry Apache-2.0 SPDX headers.
- **Portal Auth & MCP Security Boundary (`WO-09.1`, `WO-09.10`):** All 23 sensitive HTTP routes/helpers across `EchenonModule` (13), `SignalLogger` (5), `DiagnosticsRoutes` (4), and `SessionOverlay` (`checkAuth_()`) enforce `AuthManager::checkPortalAuth(request, response)`, and `MCPServer` receives `&app.getAuthManager()` with secure-by-default `readOnly = true`.
- **Design System Token Invariant (`WO-09.4`):** Zero bare hex colors exist across all 10 `web/*.js` files (`grep -rn -E "#[0-9a-fA-F]{3,8}" web/ | grep -v "var(--"` matches only `:root` / `<meta>` in standalone `web/ride.html` and `web/manifest.json`).
- **Rule 03 Versioning Invariant:** Only `dev` tags (`v0.3.0-dev.49`) were created during hardware verification; cutting `v0.3.0-beta.1` and merging to `main` remain strictly owner-gated in Steps 2-5.

### 3. `edge_cases` — PASS
- **Read-Only MCP Guard & Rig Error Handling (`WO-09.10`):** `MCPServer` defaults to `readOnly: true` and blocks state-mutating tools (`mutatesState = true`) with JSON-RPC code `-32000`. `tools/calibration-rig/calibration_sweep.py` catches `-32000` / `"read-only"` responses and raises a clear, actionable error instructing the operator how to disable read-only mode via REST or the portal UI.
- **Bounds Clamping, Null Safety & Calibration Persistence (`WO-09.1`):** All REST and MCP handlers validate inputs (`level` in `[1, 32]`, `ftms_power_smooth_s` clamped to `[0.0f, 10.0f]`, non-empty valid CSV power tables), check injected pointers against `nullptr`, synchronize `_powerEngine` via `syncPhysicsCalibration()`, and persist via `_configManager->saveModuleConfig(this)`.

### 4. `test_quality` — PASS
- **Native Verification Floor:** All 269 unit tests across 20 test suites (`29` GTest suites) in `test/test_native/` pass (`0` failed, `0` skipped) and `python3 -m py_compile tools/calibration-rig/calibration_sweep.py` passes cleanly.
- **Multi-Environment Firmware Matrix & Live Hardware Gate:** All 3 ESP32-S3 environments (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`) compile cleanly with `18.0%-18.1%` static internal DRAM usage (`58,996 B-59,316 B`), and `v0.3.0-dev.49` completed an `82.4 min` (`4,944 s`) continuous bench soak on `/dev/cu.usbmodem101` with zero crashes, verified `/api/mcp` JSON-RPC tool execution, `14/14` gzipped portal routes, and active BLE HRS/FTMS advertising.

### 5. `scope_hygiene` — PASS
- **Charter Boundary & Zero Submodule Diff:** Every modified or added file in `git diff --name-status main...a53a7960acbba46157cb545da4ead72b1726c782` resides strictly within the locked `PROJECT.md` Charter `allowed_modules` (plus `.agents/plugins/principal-architect-workspace/pr-09-integration/`). `git diff --name-only main...a53a7960acbba46157cb545da4ead72b1726c782 -- framework/` is empty (`0` files modified in `framework/`).
- **Privacy & Redaction Hygiene (`WO-09.7`, `WO-09.9`, `WO-09.11`):** Repository-wide checks outside `.agents/` confirm zero private WiFi SSIDs, router BSSID OUIs, or personal session URLs, and `docs/release-audit-v0.3.0-beta.1.md` contains zero private LAN IPs or MAC addresses (using `<LAN_IP>`, `<REDACTED_SSID>`, `<REDACTED_OUI>`, and `<REDACTED_SESSION_URL>`).



## Findings

None. All 11 work orders (`WO-09.1` through `WO-09.11`) are verified and ready for owner review and release gating.
