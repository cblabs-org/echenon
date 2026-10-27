---
schema_version: 1
wo_id: WO-02.3
pr_id: PR-02
title: CalibrationWizard + RecalibrationUI
complexity: standard
status: IN_PROGRESS
allowed_files:
- src/echenon/CalibrationWizard.h
- src/echenon/CalibrationWizard.cpp
- src/echenon/RecalibrationUI.h
- src/echenon/RecalibrationUI.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- web/calibration-wizard.js
- web/recalibration.js
- test/test_native/test_recalibration.cpp
- platformio.ini
forbidden_files: []
required_skills:
- /Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md
verification_command: pio test -e native && pio run -e ex3-waveshare-nano
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-02.3 — CalibrationWizard + RecalibrationUI

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo3-calibration-ui` from `calibration-power/wo2-power-engine`
> **Charter sections:** §5, §6 of the PR-02 Work Order Ledger

## 1. Objective

Build the portal pages for passive position calibration (CalibrationWizard)
and individual power-point recalibration (RecalibrationUI), the firmware
backend classes, API endpoints, and native tests.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/CalibrationWizard.h` — new
- `src/echenon/CalibrationWizard.cpp` — new
- `src/echenon/RecalibrationUI.h` — new
- `src/echenon/RecalibrationUI.cpp` — new
- `src/echenon/EchenonModule.h` — modify (add wizard/recal members, getters)
- `src/echenon/EchenonModule.cpp` — modify (register routes, tick, init)
- `web/calibration-wizard.js` — new portal tab script
- `web/recalibration.js` — new portal tab script
- `test/test_native/test_recalibration.cpp` — new
- `platformio.ini` — modify (native test deps, portal builder config if needed)

### Read only (reference)
- `src/echenon/ResistanceSensor.h` — live wiper readings
- `src/echenon/CadenceSensor.h` — live cadence
- `src/echenon/LevelMapping.h` — scale conversions
- `src/echenon/PowerEngine.h` / `PowerTable.h` — power table update API
- `hardware/ex3-waveshare-nano/board.h`

### Out of scope
- WebSocket feed (WO-02.4)
- CalibrationRig (WO-02.5)
- NVS persistence of cal_level_table and cal_recal_points is handled via
  the existing IConfigurable interface (applyConfig/serializeConfig). The
  calibration classes store their state in memory; EchenonModule persists
  them via ConfigManager. If ConfigManager integration is complex, stub
  the persistence calls and note it for WO-02.9.
- Peloton mapping (deferred to PR-05)

## 3. Specification

### 3.1 CalibrationWizard (SPEC §6.3 Passive Calibration)

A class that manages the state machine for passive position calibration.

**State machine:**
```
IDLE → STARTED → RECORDING_LEVEL(1..32) → COMPLETED
                  ↑__________________|
```

**API:**
- `start()` — begins calibration, moves to STARTED
- `recordLevel(uint8_t consoleLevel)` — records the current filtered wiper
  count at the given console level (1–32). Advances to the next level.
- `stop()` — cancels calibration, returns to IDLE
- `isActive() -> bool`
- `getCurrentStep() -> uint8_t` — which level we're waiting for (1–32)
- `isComplete() -> bool`
- `getTable() -> const uint16_t*` — pointer to the 32-entry wiper table
- `getTableEntry(uint8_t level) -> uint16_t` — wiper count at level (1-indexed)

**Behaviour:**
- The wizard needs a reference to `ResistanceSensor` to read the current
  filtered wiper when `recordLevel()` is called.
- On completion, stores the 32-entry `cal_level_table` (array of uint16_t
  wiper counts indexed by level-1).
- Sets `cal_pos_done = true`.
- The table can replace the B03 default fit in `ResistanceSensor` or
  `LevelMapping` — but for this WO, just store the table. The actual
  replacement of the B03 fit is a natural extension: `LevelMapping` could
  accept a custom table, but this is optional for this WO. Record in the
  completion report whether you implemented it.

**NVS keys (via ConfigManager):**
- `cal_level_table` — array[32] of uint16
- `cal_pos_done` — bool

### 3.2 RecalibrationUI (SPEC §6.3 Tier 3)

A class that manages individual power-point corrections.

**API:**
- `addPoint(uint8_t echelonLevel, float reportedPower)` — the rider enters
  a level and power from the console/app. The firmware records:
  - The entered `echelonLevel` and `reportedPower`
  - The live internal level `i` (from `LevelMapping`)
  - The live cadence RPM (from `CadenceSensor`)
  - Timestamp (millis)
- `getPoints() -> const std::vector<RecalPoint>&` — all stored points
- `getPointCount() -> size_t`
- `reset()` — clears all points
- `getMaxPoints() -> size_t` — maximum stored points (e.g. 100)

**RecalPoint struct:**
```cpp
struct RecalPoint {
    uint8_t echelonLevel;     // entered by rider
    float reportedPower;       // entered by rider (watts)
    uint8_t internalLevel;     // live i from LevelMapping
    float cadenceRPM;          // live cadence
    uint16_t wiperCount;       // live wiper
    uint32_t timestamp;        // millis()
};
```

**Update rule (PR-02 sets these values):**
- Maximum stored points: **100**
- Averaging window: **5 seconds** (points within 5s of each other at the
  same level are averaged)
- When a new point is added, if a point already exists at the same
  (level, RPM bin), the new value replaces the old (simple replacement
  rule for v1). More sophisticated averaging can be added later.
- The recalibration points can be applied to update the power table, but
  the actual table update logic is simple: for each point, update the
  nearest cell in the PowerTable. This is optional for this WO — record
  in the completion report whether you implemented automatic table update.

**NVS key:**
- `cal_recal_points` — serialised array of RecalPoint

### 3.3 REST API Endpoints

Register these in `EchenonModule::registerRoutes()`:

```
POST /api/echenon/calibration/start    → wizard.start()
POST /api/echenon/calibration/stop     → wizard.stop()
POST /api/echenon/calibration/record-level  → { "level": N }
GET  /api/echenon/calibration/status   → { "active", "step", "complete", "table": [...] }

POST /api/echenon/recalibration/point  → { "level": N, "power": W }
GET  /api/echenon/recalibration/points → { "points": [...], "count": N, "max": 100 }
POST /api/echenon/recalibration/reset  → reset all points

POST /api/echenon/power-table/upload   → CSV body → PowerTable parse
GET  /api/echenon/power-table/active   → current table preview JSON
```

### 3.4 Portal JavaScript

**web/calibration-wizard.js:**
- IIFE-wrapped, listens for `tab-changed` event on `tab-calibration`
- Shows a step-by-step UI: "Set console to Level N, then click Record"
- Calls `/api/echenon/calibration/start`, `/record-level`, `/stop`
- Shows progress (step N/32) and the recorded wiper table on completion
- Uses the framework design system tokens (see the `framework-design-system`
  skill for CSS classes)

**web/recalibration.js:**
- IIFE-wrapped, listens for `tab-changed` event on `tab-recalibration`
- Form: level input (1–32), power input (watts)
- Calls `/api/echenon/recalibration/point`, `/points`, `/reset`
- Shows list of recorded points
- Power table upload: file input + upload button calling
  `/api/echenon/power-table/upload`
- Power table preview: fetches `/api/echenon/power-table/active`

**Tab registration (in EchenonModule::registerTabs):**
- Add a "Calibration" tab (priority 60) loading `/calibration-wizard.js`
- Add a "Recalibration" tab (priority 70) loading `/recalibration.js`

**Asset serving (in EchenonModule::registerRoutes):**
- Serve both JS files using the gzip+PROGMEM pipeline:
  `#include "portal_assets/calibration-wizard_js.hpp"` and
  `#include "portal_assets/recalibration_js.hpp"` (generated by portalBuilder.py)

### 3.5 EchenonModule Integration

**Additions to EchenonModule.h:**
- `#include "echenon/CalibrationWizard.h"` and `"echenon/RecalibrationUI.h"`
- Members: `CalibrationWizard _calibrationWizard;` and `RecalibrationUI _recalibrationUI;`
- Pass sensor references to the wizard/recal classes in init

**Additions to EchenonModule.cpp:**
- Register all new API routes in `registerRoutes()`
- Register new tabs in `registerTabs()`
- Serve the JS assets
- Persistence via `applyConfig()`/`serializeConfig()`: serialize
  `cal_level_table`, `cal_pos_done`, and `cal_recal_points` into the
  `echenon` config namespace

### 3.6 Native Tests (test_recalibration.cpp)

Required test cases:
1. **Wizard start/stop**: starts in IDLE, start() → active, stop() → IDLE
2. **Wizard record all 32 levels**: record 1–32, verify complete
3. **Wizard table values**: verify recorded wiper counts match injected values
4. **RecalPoint storage**: add points, verify storage
5. **RecalPoint max limit**: add > 100 points, verify bounded to 100
6. **RecalPoint reset**: add points, reset, verify empty
7. **Averaging / replacement rule**: add two points at same level/RPM bin,
   verify replacement

## 4. Verification

1. `pio test -e native` — all native tests pass
2. `pio run -e ex3-waveshare-nano` — target compiles with zero errors

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- Framework submodule drift is expected — NEVER commit the submodule
- Known gotcha: include `<cstddef>` before `BOARD_HEADER` in standalone headers
- Known gotcha: Arduino `#define DEFAULT 1` collides with enum members — use
  `#ifdef DEFAULT` / `#undef DEFAULT` / `#endif` if needed
- The `web/` JS files must use IIFE wrappers and ID-prefixed element IDs
- JS files are compiled by portalBuilder.py into `include/portal_assets/`
  headers automatically at build time — just place them in `web/`
## Completion Report

_No completion report recorded._
