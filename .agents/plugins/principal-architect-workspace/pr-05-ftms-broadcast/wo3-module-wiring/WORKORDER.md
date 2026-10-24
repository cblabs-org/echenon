---
schema_version: 1
wo_id: WO-05.3
pr_id: PR-05
title: EchenonModule, LiveFeed & platformio.ini Integration (WITH_FTMS)
complexity: standard
status: PENDING
allowed_files:
- src/echenon/LiveFeed.h
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/main.cpp
- platformio.ini
- test/test_native/test_ftms_server.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native && pio run -e ex3-waveshare-nano && pio run
  -e ex3-waveshare-nano-diag
max_attempts: 5
max_structural_failures: 2
spec_hash: 72e2adf1b51fa789a1af8a167904452f3f4a4012a066c490fc8b1f3795846059
completion: null
---

# WO-05.3: EchenonModule, LiveFeed & platformio.ini Integration (WITH_FTMS)

## 1. Objective & Scope

Wire `FTMSServer`, Peloton `0–100` resistance (`peloton_resistance`), and Virtual Road Speed (`speed_kph`) into `LiveFeed`, `EchenonModule`, `src/main.cpp`, and `platformio.ini` (`-DWITH_FTMS`), with a `250 ms` phase offset between the 2 Hz `/ws/echenon/live` WebSocket broadcast and the 2 Hz `FTMSServer` BLE notification tick.

## 2. Detailed Requirements

### 2.1 `LiveFeed.h` Enhancements (`SPEC.md` §5.3 & §11.3)
1. Extend `LiveFeedState` in `src/echenon/LiveFeed.h`:
   - Add `uint8_t peloton_resistance = 0;` (`0–100` Peloton scale from `computePelotonResistance(_continuousLevel)` when calibrated, else `0`).
   - Add `float speed_kph = 0.0f;` (from `PowerEngine::computeVirtualSpeedKph(power_watts)`).
2. Extend `LiveFeed::buildJson(const LiveFeedState& s)` to include:
   - `"peloton_resistance": <uint8>`
   - `"speed_kph": <float rounded to 1 decimal place>`
   while preserving all existing keys (`type`, `cadence_rpm`, `resistance_level`, `continuous_level`, `internal_resistance`, `power_watts`, `pot_adc`, `heart_rate_bpm`, `hr_connected`, `calibrated`, `power_tier`, `motor_active`, `runaway_detected`).

### 2.2 `EchenonModule` & `src/main.cpp` Integration
1. In `EchenonModule`:
   - Recompute `_pelotonResistance = _calibrationManager.isCalibrated() ? PowerEngine::computePelotonResistance(_continuousLevel) : 0` and `_speedKph = PowerEngine::computeVirtualSpeedKph(_currentPowerWatts)` inside `recomputePower()` and when resistance changes (so `_pelotonResistance` updates immediately even when `cadence == 0 RPM`!).
   - Under `#ifdef WITH_FTMS`:
     - Own or hold `FTMSServer _ftmsServer{_eventBus};` with accessor `FTMSServer& getFTMSServer() { return _ftmsServer; }`.
     - Push updated `(_speedKph, _currentCadenceRpm, _internalResistance, _currentPowerWatts)` to `_ftmsServer.updateTelemetry(...)` whenever cadence, resistance, or power updates.
     - Offset `_lastLiveFeedMs` initialization by `250 ms` relative to `FTMSServer`'s `500 ms` tick so BLE notifications and WebSocket JSON broadcasts interleave cleanly every `250 ms` instead of firing on the exact same millisecond.
2. In `src/main.cpp`:
   - Under `#if defined(WITH_BLE) && defined(WITH_FTMS)`:
     - Register `echenonModule.getFTMSServer()` with `bleManager.registerServiceProvider(&echenonModule.getFTMSServer())` (alongside `hrmRepeater` and `hrmClient`) before `app.begin()`.
     - Register `&echenonModule.getFTMSServer()` with `app.registerModule(&echenonModule.getFTMSServer())`.
3. In `platformio.ini`:
   - Add `-DWITH_FTMS` to `[env:ex3-waveshare-nano]` build flags.

### 2.3 Verification
- Add native unit tests in `test/test_native/test_ftms_server.cpp` verifying `LiveFeed::buildJson()` includes `peloton_resistance` and `speed_kph`, and verifying `peloton_resistance` updates at `0 RPM` when resistance changes.
- Verify `pio test -e native`, `pio run -e ex3-waveshare-nano`, and `pio run -e ex3-waveshare-nano-diag` all succeed cleanly.

## 3. Acceptance Criteria
- `pio test -e native && pio run -e ex3-waveshare-nano && pio run -e ex3-waveshare-nano-diag` exits `0` with all native unit tests passing and both firmware environments compiling cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
