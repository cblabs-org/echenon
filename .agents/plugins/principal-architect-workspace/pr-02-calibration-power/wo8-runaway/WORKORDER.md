---
wo_id: WO-02.8
pr_id: PR-02
programme_id: prog-echenon
title: "Runaway Detection"
slug: wo8-runaway
branch: "calibration-power/wo8-runaway"
base_branch: "calibration-power/wo5-rig-firmware"
complexity: standard
status: IN_PROGRESS
attempts: 1
spec_hash: null
allowed_files:
  - src/echenon/RunawayDetector.h
  - src/echenon/RunawayDetector.cpp
  - src/echenon/EchenonModule.h
  - src/echenon/EchenonModule.cpp
  - test/test_native/test_runaway_detector.cpp
  - web/echenon-dashboard.js
verification_command: "pio test -e native && pio run -e ex3-waveshare-nano"
required_skills:
  - /Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md
---

# WO-02.8 — Runaway Detection

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo8-runaway` from `calibration-power/wo5-rig-firmware`
> **Charter sections:** §9 of the PR-02 Work Order Ledger

## 1. Objective

Implement stock-runaway detection: when the display-path wiper stays outside
the working range (165–1327 counts) for longer than `runaway_timeout_s`
(default 10 s), publish a `runaway.detected` event on the EventBus and set
a portal alert. Include native tests and a dashboard alert banner.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/RunawayDetector.h` — new
- `src/echenon/RunawayDetector.cpp` — new
- `src/echenon/EchenonModule.h` — modify (add RunawayDetector member, getter)
- `src/echenon/EchenonModule.cpp` — modify (tick RunawayDetector, persist config, add route)
- `test/test_native/test_runaway_detector.cpp` — new
- `web/echenon-dashboard.js` — modify (add runaway alert banner)

### Read only (reference)
- `src/echenon/ResistanceSensor.h` — live wiper readings, `isInWorkingRange()`
- `src/echenon/LevelMapping.h` — `WORKING_RANGE_MIN`, `WORKING_RANGE_MAX` constants
- `hardware/ex3-waveshare-nano/board.h`

### Out of scope
- Motor-sense corroboration on GPIO 6/7 (optional, nice-to-have — record
  in completion report if implemented)
- CalibrationRig / sweep (WO-02.6, WO-02.7)
- Bench simulation test (deferred to WO-02.7 bench session)

## 3. Specification

### 3.1 RunawayDetector (SPEC §5.2)

```cpp
namespace echenon {

class RunawayDetector {
public:
    // Default timeout: 10 seconds (owner decision 2026-10-20)
    static constexpr uint16_t DEFAULT_TIMEOUT_S = 10;

    RunawayDetector() = default;

    // Configuration
    void setTimeoutSeconds(uint16_t seconds);
    uint16_t getTimeoutSeconds() const;

    // Called every tick with the current filtered wiper count
    // and current millis. Returns true if a runaway was just detected
    // (transition from not-detected to detected).
    bool tick(uint32_t currentMillis, uint16_t filteredWiper);

    // State queries
    bool isRunaway() const;        // currently in runaway state
    bool wasTriggered() const;     // has been triggered at least once since reset
    uint32_t getRunawayDurationMs() const;  // how long in runaway state

    // Reset the runaway state (e.g. when wiper returns to range)
    void reset();

    // Working range constants (from LevelMapping)
    static constexpr uint16_t RANGE_MIN = 165;
    static constexpr uint16_t RANGE_MAX = 1327;

private:
    uint16_t _timeoutS = DEFAULT_TIMEOUT_S;
    uint32_t _outOfRangeStartMs = 0;
    bool _outOfRange = false;
    bool _runawayDetected = false;
    bool _wasTriggered = false;
};

} // namespace echenon
```

**Behaviour:**
- When the filtered wiper is **outside** [165, 1327]:
  - If not already tracking: record the start time, set `_outOfRange = true`
  - If already tracking: check if `currentMillis - _outOfRangeStartMs >= _timeoutS * 1000`
  - If timeout exceeded: set `_runawayDetected = true`, `_wasTriggered = true`,
    return `true` (one-shot transition detection)
- When the filtered wiper **returns to** [165, 1327]:
  - Reset the out-of-range timer
  - Clear `_outOfRange`
  - Clear `_runawayDetected` (the runaway is over)
  - `_wasTriggered` stays true (latched until explicit reset)
- `tick()` returns `true` only on the first tick where the transition from
  not-runaway to runaway occurs (edge detection for EventBus publishing)

### 3.2 EventBus Integration

When `tick()` returns `true` (runaway just detected), publish an event on
the EventBus. The topic should be `"runaway.detected"`.

In `EchenonModule::tick()`:
```cpp
if (_runawayDetector.tick(currentMillis, _resistanceSensor.getFilteredWiper())) {
    // Publish runaway event on EventBus
    // (requires EventBus reference — inject via setEventBus or access via app)
}
```

The EventBus publishing is target-only and should be guarded behind
`#ifndef UNIT_TEST`. If wiring the EventBus into EchenonModule requires
significant changes, it's acceptable to log via `ESP_LOGW` instead and
note the EventBus integration as deferred.

### 3.3 NVS Persistence

Add `runaway_timeout_s` to the `echenon` config namespace:
- In `applyConfig()`: read `config["runaway_timeout_s"]` → `_runawayDetector.setTimeoutSeconds()`
- In `serializeConfig()`: write `config["runaway_timeout_s"]`
- In `resetConfig()`: reset to `DEFAULT_TIMEOUT_S` (10)

### 3.4 Dashboard Alert Banner

Modify `web/echenon-dashboard.js` to show a runaway alert:
- Add a hidden alert banner at the top of the Live Metrics section
- Poll or check via the existing WebSocket feed — add a `runaway` boolean
  field to the WS JSON payload (requires adding it in `LiveFeed.cpp`)
- When `runaway` is true: show a red/warning banner:
  "⚠️ Stock runaway detected — turn the knob back to a valid level"
- When `runaway` clears: hide the banner
- Optionally add `runaway` to the `/api/echenon/live` GET response too

**LiveFeed changes (allowed — LiveFeed.cpp is part of EchenonModule.cpp's
compilation unit or a separate file modified via EchenonModule.cpp):**
- Add `bool runaway` parameter to `LiveFeed::tick()` or set it via a setter
- Include `"runaway": true/false` in the WS JSON payload

### 3.5 Native Tests (test_runaway_detector.cpp)

Required test cases:
1. **Initial state**: not runaway, not triggered
2. **In-range stays safe**: tick with wiper = 500 for 20 seconds → no runaway
3. **Out-of-range for < timeout**: tick with wiper = 100 for 5 seconds → no runaway
4. **Out-of-range for >= timeout**: tick with wiper = 100 for 10+ seconds → runaway detected
5. **Return to range cancels**: out of range for 5 s, then in range → timer resets.
   Going out again needs another full 10 s.
6. **Edge detection**: tick() returns true only on the first detection tick,
   false on subsequent ticks while still in runaway
7. **wasTriggered latches**: after detection, wasTriggered stays true even
   after wiper returns to range. Only explicit reset() clears it.
8. **Boundary values**: wiper = 165 is in range, wiper = 164 is out of range.
   Wiper = 1327 is in range, wiper = 1328 is out of range.
9. **Custom timeout**: set timeout to 5 s, verify detection at 5 s instead of 10

## 4. Verification

1. `pio test -e native` — all native tests pass
2. `pio run -e ex3-waveshare-nano` — target compiles with zero errors

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- Framework submodule drift — NEVER commit
- Known gotchas: `<cstddef>` before BOARD_HEADER; Arduino `#define DEFAULT 1`
- LiveFeed code is guarded behind `#ifndef UNIT_TEST`
