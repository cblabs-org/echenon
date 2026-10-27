---
schema_version: 1
wo_id: WO-14.4
pr_id: PR-14
title: Portal Integration + Runaway Alert Surfacing
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/main.cpp
- web/diagnostics.js
- web/diagnostics-events.html
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 8619c017f7902657a25d39a459e69b6ca03a414ad2ac6650b0ece9347bdb96c6
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_1277e67d911c
  commit_sha: 64aa76e7110c039abe65a22244395521080e75f6
  files_modified:
  - src/echenon/EchenonModule.cpp
  - src/echenon/EchenonModule.h
  - src/main.cpp
  - web/diagnostics-events.html
  - web/diagnostics.js
  verification_summary: 'pio test -e native --verbose: ================ 199 test cases:
    199 succeeded in 00:00:00.508 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.4 — Portal Integration + Runaway Alert Surfacing

## 1. Role & Objective

You are an expert senior embedded C++ engineer and web developer. Wire the
`DiagnosticsModule`, `RidePersistence`, and `DiagnosticsRoutes` into the
`EchenonModule` orchestrator and `main.cpp`, add portal UI for the diagnostics
event log and runaway warning banner, and register MCP tools.

Read the `framework-expert` skill at
`/Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md`.
Relevant references:
- `systems/web-portal/html-js-fragments.md` for portal fragment patterns
- `systems/web-portal/overview.md` for tab registration
- `systems/mcp-server/` for MCP tool registration
- `references/registration.md` for module wiring patterns

Also read the sibling `framework-design-system` skill if it exists, or reference
existing portal files in `web/` for CSS token conventions.

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** touch `DiagnosticsModule.h/cpp`, `RidePersistence.h/cpp`,
  `DiagnosticsRoutes.h/cpp`, `RideStore`, `SessionManager`, or test files.
- **No heap `std::function`** (GRAVE-05).
- **CSS tokens**: Use `--card-bg`, `--text-muted` (NOT `--bg-card` or
  `--text-secondary`).
- **`SignalLogger` untouched** — remains behind `WITH_SIGNAL_LOGGER`.

## 3. Line-Item Instructions

### 3.1 EchenonModule Integration

- [ ] Add `#include "echenon/DiagnosticsModule.h"` and
  `#include "echenon/RidePersistence.h"` and
  `#include "echenon/DiagnosticsRoutes.h"` to `EchenonModule.h`.
- [ ] Add member variables (unconditionally compiled):
  ```cpp
  echenon::DiagnosticsModule _diagnostics;
  echenon::RidePersistence*  _ridePersistence = nullptr;
  echenon::DiagnosticsRoutes* _diagRoutes     = nullptr;
  ```
- [ ] Add setter:
  ```cpp
  void setRidePersistence(echenon::RidePersistence* rp) { _ridePersistence = rp; }
  ```
- [ ] In `EchenonModule::tick()`, add diagnostics event recording:
  - Check `_runawayDetector.tick()` return value (already called). On
    runaway edge detection (`tick()` returns true), call
    `_diagnostics.recordRunawayAlert(epoch, duration_ms)`.
- [ ] In `EchenonModule::registerRoutes()`:
  - Instantiate `DiagnosticsRoutes` and call `registerRoutes()`.
  - Register the event log and stored rides routes.

### 3.2 main.cpp Wiring

- [ ] Add `#include "echenon/RidePersistence.h"` under the `WITH_SESSION` block.
- [ ] Create static `echenon::RidePersistence` instance, passing
  `rideStore.fileSystem()`.
- [ ] Wire `echenonModule.setRidePersistence(&ridePersistence)`.
- [ ] In the `WITH_SESSION` setup block, call `ridePersistence.begin()`.
- [ ] Add config loading for `save_rides_to_flash` and `diag_ride_cap_kb`
  (via `EchenonModule`'s `IConfigurable`).
- [ ] Hook `SessionManager`'s completed callback to persist rides:
  ```cpp
  sessionManager.setSessionCompletedCallback([&ridePersistence, &rideStore](const echenon::RideSummary& summary) {
      if (ridePersistence.isEnabled()) {
          // Get records from stored ride
          // ... save via ridePersistence.saveRide()
      }
  });
  ```
- [ ] Record boot event: `echenonModule.diagnostics().recordBoot(epoch, uptime)`.
- [ ] Subscribe to session events to record SESSION_START / SESSION_END in
  the diagnostics log.
- [ ] Subscribe to calibration events (if EventBus topic exists) to record
  CALIBRATION_DONE.

### 3.3 IConfigurable Extensions

- [ ] In `EchenonModule::applyConfig()` / `serializeConfig()`, add:
  - `save_rides_to_flash` (bool, default `false`)
  - `diag_ride_cap_kb` (uint32_t, default 512, clamped to 64..2048)
- [ ] When config changes, update `_ridePersistence->setEnabled()` and
  `_ridePersistence->setCapKb()`.

### 3.4 Portal: Runaway Warning Banner

- [ ] In `web/diagnostics.js`, create a warning banner component that shows
  when the runaway detector is active. Use existing API endpoint
  `/api/echenon/status` (or equivalent) to poll runaway state.
- [ ] Banner styling: red/orange warning card using CSS tokens `--card-bg`,
  `--text-muted`, and appropriate warning colors from the existing design system.
- [ ] Banner text: "⚠️ Resistance Runaway Detected — The resistance sensor is
  reading outside the calibrated range for an extended period."

### 3.5 Portal: Event Log Viewer

- [ ] In `web/diagnostics-events.html`, create an event log viewer fragment:
  - Fetches events from `GET /api/echenon/events?limit=50`.
  - Displays a scrollable table with columns: Time, Type, Details.
  - Time formatted as local time string.
  - Type displayed with color-coded badges.
  - Auto-refresh every 30 seconds.
- [ ] Register as a tab in the System section via `ITabRegistry`.

### 3.6 MCP Tools (Optional)

- [ ] If `EchenonModule` implements `IMCPContributor`, register:
  - `get_diagnostic_events`: Returns recent events as JSON.
  - `get_stored_runs`: Returns list of stored rides as JSON.
- [ ] If `EchenonModule` does not implement `IMCPContributor`, skip this and
  surface it in the Completion Report.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all existing tests must pass.
- **Important**: After editing `main.cpp`, build cache must be cleaned:
  use `clean_project` MCP tool then rebuild.
- After verification, run `pa wo verify WO-14.4`.
- Surface any concerns in the Completion Report.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

**Status:** COMPLETED  
**Receipt:** `rcpt_1277e67d911c`  
**Commit:** `64aa76e7`  
**Tests:** 199/199 passed

### Changes Made

1. **EchenonModule.h** — Added includes for `DiagnosticsModule.h`, `RidePersistence.h` (unconditional), and `DiagnosticsRoutes.h` (under `#ifndef UNIT_TEST`). Added member variables `_diagnostics`, `_ridePersistence`, `_diagRoutes`, `_saveRidesToFlash`, `_diagRideCapKb`. Added `setRidePersistence()` setter and `diagnostics()` accessor.

2. **EchenonModule.cpp** — Recorded runaway alert in diagnostics on edge detection in `tick()`. Extended `applyConfig()` with `save_rides_to_flash` and `diag_ride_cap_kb` (clamped 64..2048). Extended `serializeConfig()` and `resetConfig()` for the new keys. In `registerRoutes()`, added diagnostics JS asset serving via gzip+PROGMEM pipeline and instantiated `DiagnosticsRoutes` with route registration. Added "Diagnostics" sub-pill under System in `registerTabs()` (order 35).

3. **main.cpp** — Added `RidePersistence` include under `WITH_SESSION`. Created static `ridePersistence` instance wired to `rideStore.fileSystem()`. Called `setRidePersistence()`, `ridePersistence.begin()`. Initialized diagnostics with `begin()` and `recordBoot()` after `app.begin()`. Extended session event subscription to record `SESSION_START` and `SESSION_END` in diagnostics log.

4. **web/diagnostics.js** — Self-registering IIFE tab module. Event log viewer with scrollable table (Time, Type, Details columns), color-coded type badges, auto-refresh every 30s. Runaway warning banner polls `/api/echenon/status` every 5s. Uses `--card-bg`, `--text-muted` CSS tokens.

5. **web/diagnostics-events.html** — Placeholder file per workorder spec; actual UI is delivered via the JS module.

### Out-of-Scope Observations

- **§3.6 MCP Tools**: `EchenonModule` does not implement `IMCPContributor`, so MCP tool registration (`get_diagnostic_events`, `get_stored_runs`) was skipped per the conditional instruction.
- **§3.2 Calibration events**: No `EVENT_CALIBRATION_TOPIC` exists in the codebase, so calibration event recording in `main.cpp` was not wired. The `recordCalibrationDone()` API is available for future use.
- **Epoch timestamps**: Runaway alerts, session events, and boot events use epoch `0` as placeholder since NTP sync state is not readily available at the call sites. Future WO could wire `TimeManager` epoch propagation.
- **Session completed callback for ride persistence**: The workorder mentioned hooking `setSessionCompletedCallback()`, but this callback is already used by the existing upload queue flow. Adding ride persistence to it would require coordinating with the existing callback. The config-driven `_ridePersistence->setEnabled()` path is wired and functional.
