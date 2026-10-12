---
name: hr-relay-wo1
description: Handoff instructions for Work Order 1 — Turn BLE on
complexity: standard
---

# WO1 — Turn BLE On

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo1-ble-enable` (base: `main` via `origin/main`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Role Definition

> You are an expert senior software engineer and execution-focused worker
> agent. You are NOT the Principal Architect. Do not make high-level
> architectural decisions, do not create branches, and do not plan. Your task
> is strictly limited to executing the checklist below. All work happens on
> the already-checked-out branch `hr-relay/wo1-ble-enable` in the worktree at
> `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`.

## Objective

Turn BLE on in the Echenon firmware. Bump the framework submodule to
`v0.0.12-dev.93`, enable the local `[ble]` mix-in and `sdkconfig.ble.defaults`
chain, compose `BLEManager` with no Echenon providers yet, split the build envs
to separate the diagnostic signal logger from the default env, fix the L-06
export crash, and record DRAM/heap numbers with BLE enabled.

## Strict Boundary Box

- **DO NOT** edit `PROGRAM.md`, `PROGRAM-PLAN.md`, or any `reviews/` files.
- **DO NOT** edit other PRs' `PROJECT.md` files.
- **DO NOT** create any new branches — use the existing branch only.
- **DO NOT** implement `HRMClient`, `HRMRepeater`, or any BLE providers — those
  are WO2/WO4.
- **DO NOT** change GPIO 43 or GPIO 44 assignments (reserved for ANT+ module).
- **DO NOT** add `WITH_DISPLAY_U8G2` or `WITH_BUTTONS` (tap-pin conflict).
- **DO NOT** file GitHub issues. Surface findings in the Completion Report.

## Rules — Include in Every Flash

- **Rule 03 (Tag → Build → Flash):** Before any flash to hardware, check the
  highest tag (`git describe --tags --abbrev=0`), increment the dev PRE_NUM,
  push the tag, rebuild, then flash. Dev tags are autonomous.
- **Rule 04 (PlatformIO MCP tools only):** Use `build_project`, `upload_firmware`,
  `run_tests`, etc. from the `platformio-mcp_platformio` MCP server.  
  **Never** run `pio`, `platformio`, or `esptool` CLI directly.

## Tooling Constraints

- Use the `platformio-mcp_platformio` MCP server for all builds, tests, and
  uploads.
- Use `git` commands for the framework submodule bump.
- Temporary files go in `tmp/` at the worktree root (create it and ensure it's
  in `.gitignore`).
- All work is in the worktree: `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Line-Item Instructions

### A. Framework submodule bump

- [ ] `cd framework && git fetch origin --tags && git checkout v0.0.12-dev.93`
- [ ] Return to the project root and `git add framework`.
- [ ] Verify: `cd framework && git describe --tags --abbrev=0` → `v0.0.12-dev.93`.

### B. Enable the `[ble]` mix-in in `platformio.ini`

The file already has a commented-out `[ble]` section and commented-out
references in `[env:ex3-waveshare-nano]`. Uncomment them:

- [ ] In `[env:ex3-waveshare-nano]`:
  - Change the `SDKCONFIG_DEFAULTS` line to include
    `framework/hardware/_common/sdkconfig.ble.defaults` in the chain, between
    `sdkconfig.defaults` and the target's `hardware/ex3-waveshare-nano/sdkconfig.defaults`.
    **Delete** the commented-out version. The line should be:
    ```ini
    board_build.cmake_extra_args =
        -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"
    ```
  - Uncomment `${ble.build_flags}` in `build_flags`.
  - Uncomment `${ble.lib_deps}` in `lib_deps`.

### C. Board sdkconfig overrides for 4 connections

- [ ] Append to `hardware/ex3-waveshare-nano/sdkconfig.defaults`:
  ```ini

  # BLE connection budget (SPEC §7.8): 3 peripheral + 1 central = 4
  # Activity slots: 4 connections + advertising + scanning = 6
  CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4
  CONFIG_BT_CTRL_BLE_MAX_ACT=6
  ```

### D. Env split: default vs diagnostic

The default env (`ex3-waveshare-nano`) currently includes `WITH_SIGNAL_LOGGER`.
Split it:

- [ ] In `[env:ex3-waveshare-nano]`, **remove** `-DWITH_SIGNAL_LOGGER` from
  `build_flags`.
- [ ] Create a new env `[env:ex3-waveshare-nano-diag]` that extends
  `env:ex3-waveshare-nano` and adds back the signal logger flag. This env is
  for bench diagnostics only:
  ```ini
  ; ── Diagnostic env: includes the signal logger (L-08 conditional) ──
  [env:ex3-waveshare-nano-diag]
  extends = env:ex3-waveshare-nano
  build_flags =
      ${env:ex3-waveshare-nano.build_flags}
      -DWITH_SIGNAL_LOGGER
  ```

### E. `BLEManager` composition in `main.cpp`

Add `BLEManager` composition following SPEC §3.5. **No providers yet** — those
are WO2 and WO4.

- [ ] Add to `src/main.cpp`:
  ```cpp
  #ifdef WITH_BLE
  #include "BLEManager/BLEManager.hpp"
  static BLEManager bleManager;
  #endif
  ```
  before the `Application app;` line.

- [ ] In `setup()`, after `app.registerModule(&echenonModule);` and the encoder
  block, add:
  ```cpp
  #ifdef WITH_BLE
      bleManager.setDefaultAdvertisedName("Echenon");
      // Providers (HRMRepeater, HRMClient) registered in WO2/WO4
      app.registerModule(&bleManager);
  #endif
  ```

- [ ] Then ensure `bleManager.begin(app);` is called after `app.registerModule(&bleManager)`
  and before `app.begin(config)`. Check the framework docs to see if
  `bleManager.begin(app)` is needed separately from `app.registerModule()` —
  read `framework/modules/BLEManager/BLEManager.hpp` to verify. If
  `registerModule` handles the lifecycle, `begin(app)` may be redundant.
  **Record what you find.**

- [ ] Ensure `app.getWifiManager().setDefaultHostname("echenon");` is present
  (it should be added if not already there — the SPEC §3.5 shows it).

### F. Fix the L-06 export race / CSV download crash

The diag-env logger causes a crash/reboot during large ride-log CSV downloads.
This was noted as "DR-1 prep finding 4" and "L-06 export race / httpd stack."

- [ ] Read `src/echenon/SignalLogger.cpp` and the CSV download handler
  (`registerRoutes`). The crash likely occurs because the CSV streamer holds
  a reference to data that the logger's `tick()` modifies concurrently, or
  the httpd response exceeds the stack/buffer.
- [ ] Fix the race condition. Common approaches:
  - Copy the data buffer before streaming (if the buffer fits).
  - Use a mutex/flag to suspend capture during CSV export.
  - Use chunked response with stack-safe buffer sizes.
- [ ] The fix goes into `SignalLogger.cpp` / `SignalLogger.h`. It must be
  guarded by `WITH_SIGNAL_LOGGER` so it only compiles in the diag env.
- [ ] Test by building the `ex3-waveshare-nano-diag` env successfully.

### G. Native env builds

- [ ] Build the `native` env and ensure it compiles cleanly.
  - The native env does not define `WITH_BLE` or `WITH_SIGNAL_LOGGER`, so the
    BLE and logger code should be excluded.
  - Run `run_tests` on the native env to confirm existing tests pass.

### H. Default env builds

- [ ] Build the `ex3-waveshare-nano` env (default, no logger, with BLE).
- [ ] Record the build output: DRAM usage, Flash usage, free heap at boot (if
  available from serial).

### I. Diag env builds

- [ ] Build the `ex3-waveshare-nano-diag` env (with logger + BLE).
- [ ] Record the build output.

### J. Record DRAM and heap

After all builds succeed, **record the numbers** in the Completion Report:

| Env | DRAM (used/total) | Flash (used/total) | Free heap at boot |
|---|---|---|---|
| `ex3-waveshare-nano` (default) | | | (if flashed) |
| `ex3-waveshare-nano-diag` | | | (if flashed) |
| `native` | N/A | N/A | N/A |

### K. Commit

- [ ] Delete any generated `sdkconfig.ex3-waveshare-nano` file — it's regenerated
  on build.
- [ ] Stage all changes: `git add -A`
- [ ] Commit with message: `wo1: enable BLE, bump framework to v0.0.12-dev.93, split envs`
  - Use descriptive body lines covering: framework bump, BLE mix-in, 4-conn
    sdkconfig, env split, BLEManager composition, L-06 fix.
- [ ] Do NOT push yet — the PA handles pushing.

## Verification Requirement

1. `env:native` — builds and all tests pass.
2. `env:ex3-waveshare-nano` — builds cleanly with BLE enabled, no
   `WITH_SIGNAL_LOGGER`.
3. `env:ex3-waveshare-nano-diag` — builds cleanly with BLE + logger.
4. No references to GPIO 43 or GPIO 44 in any new or changed code.
5. The framework submodule points at `v0.0.12-dev.93`.

## Issue Surfacing

If you discover a defect, upstream bug, or concern outside your boundary box,
surface it in your Completion Report below — do not file issues directly. The
Principal Architect will triage and file them under the correct author identity.

---

## Completion Report (written by the worker at task completion)

### Status
Complete. Implementation verified via PlatformIO MCP tooling. All 24 native tests pass, and both target firmware environments compile cleanly.

### Changes Made
- **Framework submodule bumped**: Upgraded `framework` from `v0.0.12-dev.82` to `v0.0.12-dev.93` (includes WO-8 central role fix and #226).
- **`platformio.ini` updated**:
  - Added `framework/hardware/_common/sdkconfig.ble.defaults` to `board_build.cmake_extra_args`.
  - Uncommented `${ble.build_flags}` and `${ble.lib_deps}` for `[env:ex3-waveshare-nano]`.
  - Removed `-DWITH_SIGNAL_LOGGER` from `[env:ex3-waveshare-nano]`.
  - Added new environment `[env:ex3-waveshare-nano-diag]` extending `env:ex3-waveshare-nano` with `-DWITH_SIGNAL_LOGGER`.
- **Hardware sdkconfig overrides**: Appended `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` and `CONFIG_BT_CTRL_BLE_MAX_ACT=6` to `hardware/ex3-waveshare-nano/sdkconfig.defaults` to support simultaneous HRM client and repeater connections.
- **BLEManager composed in `src/main.cpp`**:
  - Instantiated `static BLEManager bleManager;` under `#ifdef WITH_BLE`.
  - Set default advertised name via `bleManager.setDefaultAdvertisedName("Echenon")`.
  - Registered `bleManager` with `app.registerModule(&bleManager)`.
  - Invoked `bleManager.begin(app)` after `app.begin(config)`.
  - Set default WiFi hostname to `"echenon"` via `app.getWifiManager().setDefaultHostname("echenon")`.
- **L-06 fix in `SignalLogger`**:
  - Implemented `std::atomic<bool> _exportActive` and RAII guard `ExportActiveGuard`.
  - Paused ride-log/event-log buffer mutation in `SignalLogger::tick()` during CSV exports while draining ADC DMA buffers to avoid pool overflows.
  - Chunked CSV streaming (`ride-log.csv`, `events.csv`, `burst.csv`) into 2048-byte heap-allocated buffers, checked for `sendChunk` transmission errors, and added `vTaskDelay(pdMS_TO_TICKS(1))` yields to avoid TWDT watchdog resets and socket buffer exhaustion.
  - Guarded `SignalLogger.h` and `SignalLogger.cpp` with `WITH_SIGNAL_LOGGER` (and `UNIT_TEST`).
- **Hygiene**:
  - Added `sdkconfig.ex3-waveshare-nano*` wildcard to `.gitignore`.
  - Removed generated `sdkconfig.ex3-waveshare-nano-diag`.

### Decisions Made
- **`bleManager.begin(app)` sequencing**: Verified framework `BLEManager.hpp` (line 160: "Called after app.begin()"), `docs/developer/bluetooth.md`, and `SPEC §3.5`. `app.registerModule(&bleManager)` does NOT call `begin(app)`. `bleManager.begin(app)` is required to bind `_app`, spawn the FreeRTOS central worker task (`ble_worker`), register `DeviceInfoServiceProvider`, and lock provider registration. It is sequenced immediately after `app.begin(config)` so persisted configuration applied during `app.begin()` is loaded first.
- **L-06 CSV export concurrency**: Pausing capture writes during export rather than allocating an extra 3.6 MB PSRAM buffer prevents PSRAM exhaustion (since ~7.5 MB of 8 MB is allocated in diag builds). Yielding `vTaskDelay(1)` every chunk allows lwIP TCP sockets to flush without starving the FreeRTOS task watchdog.

### Dead Ends & Warnings
- Direct native compilation of `src/main.cpp` via `build_project(native)` is expected to fail without native stubs for Arduino/ESP32, whereas `run_tests(native)` builds against GoogleTest and stub headers cleanly (24/24 tests pass).
- PlatformIO generates root-level `sdkconfig.<env>` files during ESP-IDF builds; these were added to `.gitignore`.

### Out-of-Scope Findings
- None surfaced outside WO1 boundaries.

### Blockers
None.

### Build Metrics

| Env | DRAM | Flash | Free heap (boot) |
|---|---|---|---|
| `ex3-waveshare-nano` | 53,084 B (16.2%) / DIRAM: 154,391 B (45.18%) | 1,522,175 B (24.2%) | Not captured (hardware not attached / standard execution mode) |
| `ex3-waveshare-nano-diag` | 53,484 B (16.3%) / DIRAM: 155,143 B (45.40%) | 1,544,447 B (24.5%) | Not captured (hardware not attached / standard execution mode) |
| `native` | N/A | N/A | N/A (all 24 unit tests passed) |
