---
schema_version: 1
wo_id: WO-09.11
pr_id: PR-09
title: Sync Release Audit Report with MCPServer Metrics & Live v0.3.0-dev.49 Bench
  Soak
complexity: mechanical
status: PENDING
allowed_files:
- docs/release-audit-v0.3.0-beta.1.md
forbidden_files:
- framework/**
- src/**
- web/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 0d92e9ebb510934cb719a587d58b5d15234ea94cec2031e7aebc7bb99adbefe4
completion: null
---

# WO-09.11 — Sync Release Audit Report with MCPServer Metrics & Live v0.3.0-dev.49 Bench Soak

## 1. Objective

Update [`docs/release-audit-v0.3.0-beta.1.md`](../../../../docs/release-audit-v0.3.0-beta.1.md) with the verified post-`WO-09.10` build/ELF footprint and the `🟢 MEASURED` `v0.3.0-dev.49` live hardware bench soak & `/api/mcp` verification captured on the attached Waveshare ESP32-S3-Nano (`/dev/cu.usbmodem101`).

## 2. Strict Boundary Box

- **Allowed files:** `docs/release-audit-v0.3.0-beta.1.md` only.
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, or any file outside `docs/release-audit-v0.3.0-beta.1.md`.
- **Privacy invariant (CRITICAL):** Preserve `<REDACTED_SSID>`, `<REDACTED_OUI>`, and `<REDACTED_SESSION_URL>` on lines `114` and `214`. NEVER include any literal home WiFi SSID, router BSSID/MAC address, personal session URL, or private LAN IP (`192.168.*`) anywhere in `docs/release-audit-v0.3.0-beta.1.md` — use `<LAN_IP>` / `http://echenon.local` instead.
- **Accuracy invariant:** Report ONLY the exact measured values provided below (`dram_min = 39,828 B` init peak vs `65,723–67,595 B` steady-state internal DRAM free, auth-disabled bench state noted candidly for `401`). Do not round up or invent any numbers.
- **Shell hygiene:** Keep `.agents/` untouched apart from Part II of this `WORKORDER.md` updated automatically by `~/.local/bin/pa wo verify WO-09.11`. Never invoke raw `pio` CLI in `run_command` or use `/tmp` / bash `$VAR` loops.

## 3. Coordinator-Verified Measurements (After `WO-09.10` `3173600c84abf2714e57e07bb262b3bdb29ce0e5`, Tag `v0.3.0-dev.49`)

- **Native Unit Tests (`run_tests` `environment: "native"`):** `269 / 269` passed, `0` failed, `0` errored, `0` skipped in `0.551 s` (`0.55 s`) across 20 suites (`29` GTest suites reported internally).
- **3-Environment Build Matrix (`build_project` `forceExecution: true` + `upload_firmware` image):**
  - `ex3-waveshare-nano`: `RAM: 58,996 B` (`18.0%` of `327,680 B`), `Flash: 1,787,295 B` (`28.4%` of `6,291,456 B` `app0`; initial `build_project` `1,786,835 B`; `firmware.bin` `1,787,696 B`).
  - `ex3-waveshare-nano-diag`: `RAM: 59,316 B` (`18.1%` of `327,680 B`), `Flash: 1,804,263 B` (`28.7%` of `6,291,456 B`).
  - `ex3-waveshare-nano-rig`: `RAM: 59,084 B` (`18.0%` of `327,680 B`), `Flash: 1,792,747 B` (`28.5%` of `6,291,456 B`).
- **ELF Section & Top-5 `.dram0` Symbol Inspection (`.pio/build/ex3-waveshare-nano/firmware.elf`):**
  - `.dram0.data`: `20,124 B`
  - `.dram0.bss`: `38,872 B`
  - **Static DRAM Total:** **`58,996 B`** (`18.0%` of `327,680 B`, **`268,684 B`** static headroom)
  - `.iram0.text`: `117,147 B`
  - `.flash.text`: `1,281,540 B`
  - `.flash.rodata`: `367,456 B`
  - Top 5 `.dram0` symbols unchanged: `g_cnxMgr` (`3,880 B`, `1.18%`), `app` (`3,344 B`, `1.02%`), `Redactor::_instance` (`3,172 B`, `0.97%`), `port_IntStack` (`3,072 B`, `0.94%`), `echenonModule` (`2,872 B`, `0.88%`); `mcpServer` (`_ZL9mcpServer`) in `.dram0.data` = `104 B`.
- **Live Hardware Bench Soak (`v0.3.0-dev.48` & `v0.3.0-dev.49` on `/dev/cu.usbmodem101`):**
  - **`v0.3.0-dev.48` pre-fix soak:** Continuous `8,235 s` (~2h 17m), 0 crashes/resets, `13/13` portal routes `200 OK`, BLE HRS/FTMS streaming verified; caught `POST /api/mcp -> 404` -> resolved by `WO-09.10`.
  - **`v0.3.0-dev.49` post-fix soak (`82.4 min` / `4,944 s` continuous):**
    - Boot log: `1` boot marker (`ESP32 App Foundation v0.3.0-dev.49 — Echenon — booting`), `0` panic/backtrace/WDT/brownout faults; LittleFS `/littlefs` and `/config` mounted cleanly; `MCPServer: Config applied: enabled=1 readOnly=1`; `Registry locked — tickables=10 webContributors=12 mcpContributors=6 dataSources=1 statusReporter=registered`; `EchenonModule: Registered MCP tools: echenon_get_telemetry, echenon_calibration, echenon_recalibration, echenon_upload_power_table, echenon_set_ftms_smoothing`; clean transition to `State -> RUNNING` at `6.83 s`.
    - `GET /api/system`: `200 OK`, `firmware_version: "0.3.0-dev.49"`, `uptime: 4944`, `heap_free: 65,723 B` (steady heartbeat `dram = 66,983–67,595 B`), `heap_min_free: 39,828 B` (startup transient high-water mark, `2.65×` the 15 KB framework floor; steady-state headroom `65.7–67.6 KB` well above 40 KB), `heap_max_alloc: 31,744 B`, `psram_free: 8,267,612 B / 8,388,608 B`, `chip_model: ESP32-S3` (rev 2, 2 cores), `chip_temp: 33.8 °C`, `ota_running_partition: app0`.
    - Portal routes: `14/14` routes (`/`, `/mcp.js` [`4,514 B`], `/echenon-dashboard.js`, `/calibration-wizard.js`, `/recalibration.js`, `/diagnostics.js`, `/ride`, `/ride.js`, `/session.js`, `/stored_rides.js`, `/upload_settings.js`, `/intervals_settings.js`, `/wahoo_settings.js`, `/manifest.json`) returned `200 OK` (`Content-Encoding: gzip`), plus `GET /api/echenon/live` -> `200 OK`. *(Auth note: board `/config` has Portal Auth `enabled: false`, so auth-enabled `401` enforcement rests on code inspection of the 23 `checkPortalAuth` call sites + `MCPServer` bearer middleware).*
    - MCP end-to-end: `GET /api/mcp/status` -> `200 OK` (`{"initialized": true, "enabled": true, "readOnly": true, "toolCount": 27}`); `POST /api/mcp` `initialize` -> `200 OK` (`serverInfo: {"name": "echenon", "version": "0.3.0-dev.49"}`); `POST /api/mcp` `tools/list` -> `200 OK` (`27` tools including all `5` production Echenon tools, excluding `echenon_rig_*`); `POST /api/mcp` `tools/call` (`echenon_get_telemetry`) -> `200 OK` (`isError: false`, live JSON payload); `POST /api/mcp` `tools/call` (`echenon_set_ftms_smoothing`) under default `readOnly: true` -> JSON-RPC `error.code: -32000` (`"Tool modifies state but server is in read-only mode"`).
    - Legacy `/api/echenon/rig/status` (`GET`) & `/api/echenon/rig/command` (`POST`) -> both `404`.
    - BLE scan (`BleakScanner`): `Echenon` advertising HRS `0000180d-...` & FTMS `00001826-...` (`-51 dBm`).

## 4. Technical Edits Required in `docs/release-audit-v0.3.0-beta.1.md`

1. **`§1` Metadata (`lines 8–16`):** Update target tag note to `v0.3.0-dev.49`, evaluated commit to `3173600c84abf2714e57e07bb262b3bdb29ce0e5`, `🟢 MEASURED Evidence` to summarize the live `v0.3.0-dev.49` bench soak & MCP gate on `/dev/cu.usbmodem101`, and `Overall Audit Verdict` to `PASS (Beta Channel Ready — Live v0.3.0-dev.49 Bench Soak & MCP Gate Verified; Awaiting Owner Steps 2–5)`.
2. **`§2` Matrix & `§3` (`D2`, `D3`, `D4`, `D6`, `D7`, `D8`):**
   - `D2`: Add `mcpServer` to the `static` composition-root instances in `src/main.cpp`.
   - `D3`: Document `mcpServer.setAppName("Echenon")`, `app.registerModule(&mcpServer)` before `app.begin(config)`, `mcpServer.begin(...)` after `timeManager.begin()` (`webContributors=12`, `mcpContributors=6`, `27` live MCP tools), `ESP_LOGI` in `EchenonModule::registerMCPTools()`, and `-32000` read-only hint in `tools/calibration-rig/calibration_sweep.py`.
   - `D4`: Update static DRAM (`58,996 B–59,316 B`), `mcpServer` `104 B`, and live `v0.3.0-dev.49` DRAM/PSRAM metrics alongside Phase 3 `v0.3.0-dev.45`.
   - `D6`, `D7`, `D8`: Update live `14/14` gzipped portal asset result (including `/mcp.js`), `0.55 s` (`0.551 s`) native duration, and `MCPServer` / `MCPToolRegistry` wiring.
   - Preserve privacy placeholders on `lines 114` and `214`.
3. **`§4.1–§4.3`:** Update native duration, 3-env RAM/Flash build table, and ELF section sizes from §3 above.
4. **`§4.5` & `§5` Step 1:** Record the complete `🟢 MEASURED` `v0.3.0-dev.48` + `v0.3.0-dev.49` live bench soak & MCP verification on `/dev/cu.usbmodem101` (with zero private SSID/MAC/LAN-IP leaks!), check `[x]` on Step `1.1`–`1.5`, and keep Steps `2`–`5` (`[ ]`) pending for the owner.

## 5. Verification

1. Verify `git grep -n -E "192\.168\.|3C:0F:02" -- docs/` returns 0 matches and `docs/release-audit-v0.3.0-beta.1.md` keeps `<REDACTED_SSID>|<REDACTED_OUI>|<REDACTED_SESSION_URL>`.
2. Keep `.agents/` untouched, run `~/.local/bin/pa wo verify WO-09.11`, and post the commit SHA + `receipt_id` to `@coordinator` via `post_message`.

## Completion Report

_Pending execution._
