---
schema_version: 1
pr_id: PR-05
github_pr: null
audit_round: 3
head_sha: 740b154616764780b6975d513909af1e92578a85
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-05 round=3 head_sha=740b154616764780b6975d513909af1e92578a85 verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)

# Cold-Firewall Audit — PR-05 `pr-05-ftms-broadcast` — Round 3

**Branch:** `feat/pr-05-ftms-broadcast` → `main`
**HEAD:** `740b154616764780b6975d513909af1e92578a85`
**Test Suite:** 162/162 native tests PASS (`./.pio/build/native/program`)
**Working Tree:** Clean (no uncommitted changes)

---


## Verdict: ✅ APPROVED

All Round 3 live-hardware findings uncovered during physical Echelon EX-3 riding (`v0.3.0-dev.19` through `v0.3.0-dev.22`) have been cleanly engineered, unit-tested (`162/162` passing in `test_cadence_sensor.cpp` and `test_ftms_server.cpp`), and verified on hardware with **Xert**, **Echelon Fit**, and the **Echelon Beat** optical HR armband running simultaneously (`docs/measurements/phase3/C08-20261024-ftms-verification.md`).

---


## Round 3 Delta Verification (`1ee2824..740b154`)

1. **Physical Reed-Switch Two-Layer Debounce & Open-Release Hysteresis (`src/echenon/CadenceSensor.h`, `src/echenon/CadenceSensor.cpp`):**
   - **Layer A (Hardware IO-MUX Glitch Filter):** Enabled ESP-IDF `gpio_new_pin_glitch_filter` / `gpio_glitch_filter_enable` on `PIN_CADENCE` under `#if !defined(UNIT_TEST) && defined(ARDUINO)` and `SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER`, with clean teardown in `~CadenceSensor()`.
   - **Layer B (Immediate Leading-Edge Commit + `250 ms` Lockout + `50 ms` HIGH Release Hysteresis):** `onFallingEdgeISR(nowMicros)` commits the leading-edge microsecond timestamp immediately via `handlePulse(nowMicros)`, enforces `MIN_HW_INTERVAL_US = 250000` (`240 RPM` ceiling), and latches `_waitingForOpen = true` until `processHardwareState()` observes `REARM_HIGH_US = 50000` (`50 ms`) of continuous `HIGH` pin state. This eliminates the `~150 ms` opening-edge RC ramp threshold oscillation (`200.0 RPM` spikes) while preserving `injectPulse()` compatibility and sub-millisecond timing accuracy.
   - **Unit Test Coverage:** Verified by 5 dedicated unit tests in `test/test_native/test_cadence_sensor.cpp` (`HardwareReedSwitchRejectsOpeningEdgeGlitch`, `HardwareSustainedClosureBlocksOpeningGlitchBeyond250ms`, `HardwareClosureBouncePreservesLeadingEdgeTimestamp`, `HardwareCalibrationRig30msPulseCompatibility`, `HardwareMinInterval250msLockout`).

2. **`uint32_t micros()` 71.58-Minute Wraparound vs `millis()` Clock-Domain Fix (`src/echenon/CadenceSensor.cpp`):**
   - `CadenceSensor::tick(uint32_t currentMillis)` now evaluates `if (_newPulse)` first—computing `_rpm` from pure `uint32_t` microsecond difference (`_lastIntervalUs`) and latching `_lastPulseMs = currentMillis` in the `millis()` clock domain—before evaluating `else if (_hasFirstPulse && static_cast<int32_t>(currentMillis - _lastPulseMs) >= static_cast<int32_t>(TIMEOUT_MS))`.
   - **Unit Test Coverage:** Verified by `CadenceSensorTest.MicrosWraparoundAfter71MinutesPreservesRPMAndTimeout` at `currentMillis = 9,237,153 ms` (~2.56 hours uptime).

3. **Cross-Core `millis()` Underflow & Optical HR Grace Window (`src/echenon/HRMClient.cpp`, `src/echenon/FTMSServer.cpp`):**
   - `HRMClient::tick(uint32_t currentMillis)` and `FTMSServer::getEffectiveHrBpm(uint32_t now_ms)` now use signed `int32_t` elapsed-time comparisons (`static_cast<int32_t>(currentMillis - _lastValidHrMs) >= 10000` and `static_cast<int32_t>(now_ms - _lastHrMs) > static_cast<int32_t>(HR_STALE_TIMEOUT_MS)`), preventing unsigned subtraction underflow (`0xFFFFFFFE`) when the NimBLE host task on Core 0 timestamps an incoming HR notification 1–2 ms after `Application::tick()` latches `currentMillis` on Core 1.
   - **Unit Test Coverage:** Verified by `FTMSServerTest.CrossTaskFutureTimestampDoesNotUnderflowStaleCheck`.

---


## Five-Axis Summary

| Axis | Status | Notes |
|------|--------|-------|
| `spec_alignment` | ✅ PASS | Unified metric hierarchy (`LevelMapping` + `PowerEngine`), FTMS `0x1826` (`0x2ACC` = `0x00004482`, `0x2AD2` 11-byte 2 Hz notify, ADV `0x0485` + `01 20 00`), `/ws/echenon/live` (`peloton_resistance`, `speed_kph`), and live Xert + Echelon Fit + HR armband coexistence match Charter & SPEC |
| `invariants` | ✅ PASS | Zero BLE dependencies in `FTMSData.h`, ISR safety (`IRAM_ATTR` on ESP32 path), 250 ms phase offset relative to `LiveFeed`, stack pause/resume respected |
| `edge_cases` | ✅ PASS | Reed-switch rising-edge RC bounce, sustained slow-pedal closure (>250 ms), 71.58-minute `micros()` wraparound, cross-core `millis()` skew, Level 32 (`254` vs `255` sentinel), NaN/Inf, and HR staleness timeouts all handled and tested |
| `test_quality` | ✅ PASS | 162/162 native tests pass (`10 ms` total) with deterministic assertions covering all hardware bug regressions + live physical bike ride verification (`v0.3.0-dev.22`) |
| `scope_hygiene` | ✅ PASS | All modified files reside within `charter.allowed_modules` and `WO-05.4` `allowed_files`; clean working tree |
