---
schema_version: 1
wo_id: WO-02.1
pr_id: PR-02
title: CadenceSensor + ResistanceSensor + Level Mapping
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/CadenceSensor.h
- src/echenon/CadenceSensor.cpp
- src/echenon/ResistanceSensor.h
- src/echenon/ResistanceSensor.cpp
- src/echenon/LevelMapping.h
- src/echenon/LevelMapping.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/main.cpp
- hardware/ex3-waveshare-nano/board.h
- test/test_native/test_level_mapping.cpp
- test/test_native/test_cadence_sensor.cpp
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

# WO-02.1 — CadenceSensor + ResistanceSensor + Level Mapping

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo1-sensors` from `main`
> **Charter sections:** §1, §2, §3 of the PR-02 Work Order Ledger

## 1. Objective

Build the two production sensor classes (`CadenceSensor`, `ResistanceSensor`),
the canonical internal scale with external mappings (`LevelMapping`), wire
them into `EchenonModule::tick()`, and write native unit tests.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/CadenceSensor.h` — new
- `src/echenon/CadenceSensor.cpp` — new
- `src/echenon/ResistanceSensor.h` — new
- `src/echenon/ResistanceSensor.cpp` — new
- `src/echenon/LevelMapping.h` — new
- `src/echenon/LevelMapping.cpp` — new
- `src/echenon/EchenonModule.h` — modify (add sensor members)
- `src/echenon/EchenonModule.cpp` — modify (call sensors in tick)
- `src/main.cpp` — modify (if any wiring needed)
- `test/test_native/test_level_mapping.cpp` — new
- `test/test_native/test_cadence_sensor.cpp` — new
- `platformio.ini` — modify (native test deps only)

### Read only
- `hardware/ex3-waveshare-nano/board.h` — reference for pin constants

### Out of scope
- Power engine (WO-02.2)
- Calibration UI (WO-02.3)
- Peloton 0–100 mapping (deferred to PR-05, owner decision 2026-10-20)
- Any `PIN_RIG_*` additions to board.h (WO-02.5)
- WebSocket feed (WO-02.4)

## 3. Specification

### 3.1 CadenceSensor (SPEC §5.1)

A class that computes cadence RPM from pedal edge timing.

**Design (target-portable):**
- Constructor takes a GPIO pin number (default: `PIN_CADENCE` = 5)
- `begin()` — configures the GPIO as `INPUT` and attaches a hardware interrupt
  on `FALLING` edge (on the target). On native: no-op.
- `tick(uint32_t currentMillis)` — called from `EchenonModule::tick()`.
  Computes RPM from the last pulse interval. Handles timeout.
- `getRPM() -> float` — current cadence in RPM
- `getLastPulseInterval() -> uint32_t` — last pulse-to-pulse interval in µs
- `isActive() -> bool` — true if cadence > 0 (not timed out)

**Parameters (compile-time constants or class defaults):**
- Software debounce lockout: **25 ms** (25,000 µs)
- RPM formula: `RPM = 60,000,000 / Δt_µs`
- Timeout: **3 s** with no pulse → cadence = 0
- Clamp: 0–200 RPM

**ISR pattern:**
- The ISR sets a `volatile uint32_t _lastPulseMicros` and a
  `volatile bool _newPulse` flag. `tick()` reads these atomically.
- The ISR enforces the 25 ms debounce by comparing `micros()` to
  `_lastPulseMicros`.

**Native test strategy:**
- The class must be compilable on native. Guard ISR/GPIO calls behind
  `#ifdef ARDUINO` or `#ifndef UNIT_TEST`.
- Tests call `tick()` with synthetic timing (simulate pulse intervals by
  directly setting `_lastPulseMicros` via a `injectPulse(uint32_t micros)`
  test helper or friend/public for test).

### 3.2 ResistanceSensor (SPEC §5.2)

A class that reads the pot wiper ADC and derives the filtered wiper value.

**Design:**
- Constructor takes ADC pin (default: `PIN_POT_WIPER` = 1)
- `begin()` — configures ADC (on target). On native: no-op.
- `tick(uint32_t currentMillis)` — reads ADC, applies EMA filter.
- `getFilteredWiper() -> uint16_t` — the EMA-filtered wiper count
- `getRawWiper() -> uint16_t` — the latest raw reading (for diagnostics)
- `isInWorkingRange() -> bool` — filtered wiper is between 165 and 1327

**Parameters:**
- EMA at 20 Hz with α = 0.15:
  `filtered = 0.15 × sample + 0.85 × filtered`
- ADC read rate: on target, reads in `tick()` which runs at ~100+ Hz.
  The 20 Hz is the effective bandwidth of the filter, not the sample rate.

**Native test strategy:**
- Guard `analogRead()` behind `#ifndef UNIT_TEST`.
- Provide `injectSample(uint16_t adcValue)` for tests.
- Test EMA convergence: inject N identical samples, verify convergence to
  within 1 count.

### 3.3 LevelMapping (SPEC §5.3)

A stateless utility class (or namespace with free functions) for scale
conversions.

**Functions:**
```cpp
namespace LevelMapping {
    // Wiper → continuous level (float, clamped 1.0–32.0)
    // Uses B03 default fit: wiper = 39.1 × level + 97
    // level = (wiper - 97) / 39.1, clamped to [1, 32]
    float wiperToLevel(uint16_t wiper);

    // Continuous level → canonical internal scale i ∈ 0…255
    // i = round((level - 1) × 255 / 31)
    uint8_t levelToInternal(float level);

    // Internal scale → Echelon display level L ∈ 1…32
    // L = round(i × 31 / 255) + 1
    uint8_t internalToEchelon(uint8_t i);

    // Echelon level → internal scale
    // i = round((L - 1) × 255 / 31)
    uint8_t echelonToInternal(uint8_t echelonLevel);

    // Convenience: wiper → Echelon level
    uint8_t wiperToEchelon(uint16_t wiper);

    // Convenience: wiper → internal
    uint8_t wiperToInternal(uint16_t wiper);

    // Constants
    constexpr float B03_SLOPE = 39.1f;
    constexpr float B03_INTERCEPT = 97.0f;
    constexpr uint16_t WORKING_RANGE_MIN = 165;
    constexpr uint16_t WORKING_RANGE_MAX = 1327;
}
```

**Native tests (test_level_mapping.cpp):**
- Round-trip endpoints: `echelonToInternal(1)` → 0, `internalToEchelon(0)` → 1
- Round-trip endpoints: `echelonToInternal(32)` → 255, `internalToEchelon(255)` → 32
- Round-trip all 32 levels: `internalToEchelon(echelonToInternal(L)) == L` for L = 1…32
- `wiperToLevel(165)` ≈ 1.74 (near level 1 end), `wiperToLevel(1327)` ≈ 31.4
- `wiperToLevel(97)` clamps to 1.0 (below range)
- `wiperToLevel(1400)` clamps to 32.0 (above range)
- `wiperToEchelon(165)` produces a valid level in [1, 32]
- B03 fit accuracy: `wiperToLevel(97 + 39.1 * 16)` ≈ 16.0 (mid-range check)

### 3.4 EchenonModule Integration

**Additions to `EchenonModule.h`:**
- Add `CadenceSensor _cadenceSensor;` member
- Add `ResistanceSensor _resistanceSensor;` member
- Add public getters: `getCadenceRPM()`, `getEchelonLevel()`, `getInternalLevel()`, `getFilteredWiper()`

**Additions to `EchenonModule.cpp`:**
- In `tick()`: call `_cadenceSensor.tick(currentMillis)` and
  `_resistanceSensor.tick(currentMillis)`, then derive levels via
  `LevelMapping`.
- Call `_cadenceSensor.begin()` and `_resistanceSensor.begin()` once
  (guarded by a static bool, or move to an init method).

**Additions to `src/main.cpp`:** likely none — the sensors are internal to
`EchenonModule` and don't need external wiring.

### 3.5 PCNT Verification (SPEC §5.4)

Confirm that the `EncoderModule` PCNT configuration (in
`framework/modules/EncoderModule/EncoderModule.cpp`) does **not** enable
internal pull-ups or pull-downs on GPIO 8/9. Record the finding in the
Completion Report.

**Expected finding:** The PCNT API (`pcnt_chan_config_t`) uses `.edge_gpio_num`
and `.level_gpio_num` fields which do not configure pull resistors. The
framework does not call `gpio_set_pull_mode()` for encoder pins. Confirmed:
no internal pulls enabled.

## 4. Verification

1. `pio test -e native` — all native tests pass (including existing tests
   and the two new test files)
2. `pio run -e ex3-waveshare-nano` — target compiles with zero errors
3. PCNT finding recorded in Completion Report

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- `board.h` is read-only for this WO
- Peloton mapping is deferred to PR-05 — do NOT implement
- Framework submodule drift is expected — NEVER commit the submodule

---

## Completion Report

### Status
COMPLETE

### Changes Made
- `src/echenon/LevelMapping.h` & `src/echenon/LevelMapping.cpp`:
  - Implemented stateless conversions between ADC wiper counts, continuous level, internal scale `i ∈ 0…255`, and Echelon level `L ∈ 1…32`.
  - Defined linear B03 fit `wiper = 39.1 × level + 97`, `WORKING_RANGE_MIN = 165`, and `WORKING_RANGE_MAX = 1327`.
  - Exact endpoint mappings: `1 ↔ 0` and `32 ↔ 255`.
- `src/echenon/ResistanceSensor.h` & `src/echenon/ResistanceSensor.cpp`:
  - Implemented EMA filter with α = 0.15 over ADC wiper counts.
  - Snap-initialized on first sample to prevent ramp-up lag.
  - Added `isInWorkingRange()` checking `[165, 1327]`.
  - Added `injectSample()`, `reset()`, and hardware call isolation under `#if !defined(UNIT_TEST) && defined(ARDUINO)`.
- `src/echenon/CadenceSensor.h` & `src/echenon/CadenceSensor.cpp`:
  - Computes RPM from falling edge pulse intervals: `RPM = 60,000,000 / Δt_µs`.
  - Enforces 25 ms debounce lockout (`DEBOUNCE_LOCKOUT_US = 25000`).
  - 3 s timeout (`TIMEOUT_MS = 3000`) drops RPM to 0.0 without spurious spikes on resume.
  - Clamps output to 0–200 RPM.
  - Hardware ISR attachment guarded for target (`#if !defined(UNIT_TEST) && defined(ARDUINO)`), with `injectPulse()` test helper for deterministic native testing.
- `src/echenon/EchenonModule.h` & `src/echenon/EchenonModule.cpp`:
  - Added `_cadenceSensor` and `_resistanceSensor` members.
  - Added accessors: `cadenceSensor()`, `resistanceSensor()`, `getCadenceRPM()`, `getEchelonLevel()`, `getInternalLevel()`, `getFilteredWiper()`.
  - Initialized sensors on first tick (`begin()`) and polled every tick (`tick()`).
- `test/test_native/test_level_mapping.cpp`:
  - 10 test cases covering exact endpoints, 32-level bijection round-trip, working range calculations, clamping, B03 fit accuracy, and monotonic progression.
- `test/test_native/test_cadence_sensor.cpp`:
  - 12 test cases covering CadenceSensor initial state, single-pulse debounce lockout, valid 60/120 RPM calculations, 200 RPM clamping, 3 s timeout, timeout recovery, and ResistanceSensor EMA convergence and working range checks.

### Verification Results
1. Native Unit Tests (`run_tests` MCP tool on `native`):
   - **PASS**: 67 test cases succeeded in 0.30s (0 failed).
2. Target Compilation (`build_project` MCP tool on `ex3-waveshare-nano`):
   - **PASS**: RAM: 54,292 bytes, Flash: 1,536,351 bytes, zero errors.

### PCNT Verification Finding (SPEC §5.4)
Confirmed that `EncoderModule` PCNT configuration in `framework/modules/EncoderModule/EncoderModule.cpp` does **not** enable internal pull-ups or pull-downs on GPIO 8/9.
The ESP-IDF PCNT API configuration uses:
```cpp
pcnt_chan_config_t chanA_config = {};
chanA_config.edge_gpio_num = config.pinA;
chanA_config.level_gpio_num = config.pinB;
```
which only routes the pins to the pulse counter without configuring pull resistors. Furthermore, no calls to `gpio_set_pull_mode()`, `gpio_pullup_en()`, or `gpio_pulldown_en()` are made. Software pull resistors on GPIO 8/9 are confirmed to be disabled.

### Decisions & Notes
- Peloton 0–100 mapping was deferred to PR-05 per owner decision 2026-10-20; no stubs or implementations were added.
- `<cstddef>` was added to sensor headers before `BOARD_HEADER` inclusion so `size_t` in `board.h` is defined when building target translation units.

