---
schema_version: 1
wo_id: WO-05.4
pr_id: PR-05
title: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak
complexity: interactive
status: COMPLETED
allowed_files:
- docs/measurements/phase3/C08-20261024-ftms-verification.md
- src/echenon/CadenceSensor.h
- src/echenon/CadenceSensor.cpp
- src/echenon/ResistanceSensor.h
- src/echenon/ResistanceSensor.cpp
- src/echenon/LevelMapping.h
- src/echenon/LevelMapping.cpp
- src/echenon/CalibrationWizard.h
- src/echenon/CalibrationWizard.cpp
- src/echenon/PowerEngine.h
- src/echenon/PowerEngine.cpp
- src/echenon/RecalibrationUI.h
- src/echenon/RecalibrationUI.cpp
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
- src/echenon/HRMClient.h
- src/echenon/HRMClient.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- test/test_native/test_cadence_sensor.cpp
- test/test_native/test_ftms_server.cpp
- test/test_native/test_level_mapping.cpp
- test/test_native/test_power_engine.cpp
- test/test_native/test_recalibration.cpp
forbidden_files: []
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: a504e241d67ca8a5d383e1b5d3e053bd1ab284b7d5cac148f8d18b3e9f7148f4
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_cad6c148b554
  commit_sha: 6db31cc9c6ac5529efc6c516685926679ffa2658
  files_modified:
  - docs/measurements/phase3/C08-20261024-ftms-verification.md
  - src/echenon/CalibrationWizard.cpp
  - src/echenon/CalibrationWizard.h
  - src/echenon/EchenonModule.cpp
  - src/echenon/EchenonModule.h
  - src/echenon/LevelMapping.cpp
  - src/echenon/LevelMapping.h
  - src/echenon/PowerEngine.cpp
  - src/echenon/PowerEngine.h
  - src/echenon/RecalibrationUI.cpp
  - src/echenon/RecalibrationUI.h
  - src/echenon/ResistanceSensor.h
  - test/test_native/test_level_mapping.cpp
  - test/test_native/test_power_engine.cpp
  - test/test_native/test_recalibration.cpp
  verification_summary: 'pio test -e native: ================ 167 test cases: 167
    succeeded in 00:00:01.242 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-05.4: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak

## 1. Objective & Scope

Flash `ex3-waveshare-nano` onto the connected ESP32-S3-Nano bench unit, verify the live FTMS `0x1826` GATT profile and 2 Hz `0x2AD2` notifications, validate client pairing and telemetry display on the **Lenovo Tab P12** (and macOS/iOS), and record a 5-minute multi-client coexistence soak (`SPEC.md` §13.6 tests `1.4–1.7`, `3.1–3.11`) in `docs/measurements/phase3/C08-20261024-ftms-verification.md`.

## 2. Detailed Requirements

1. **Flash & Boot Verification:**
   - Flash `ex3-waveshare-nano` via `pio-manager` MCP tools and verify clean serial boot with both `HRMRepeater` (`0x180D`) and `FTMSServer` (`0x1826`) registered on `BLEManager`.
2. **GATT Structure & Payload Inspection (`SPEC.md` §13.6 `3.1–3.5`):**
   - Verify BLE advertisement includes `0x1826` (`FTMS`) and `0x180D` (`Heart Rate`).
   - Read `0x2ACC` (`Fitness Machine Feature`) and confirm 8-byte payload `82 44 00 00 00 00 00 00` (`Target Setting Features == 0`).
   - Subscribe to `0x2AD2` (`Indoor Bike Data`) and confirm 11-byte notifications at `2 Hz` (`500 ms` interval) with `Flags = 0x0264`, live Virtual Speed (`0.01 km/h`), Cadence (`0.5 RPM`), Resistance (`0–254`), Power (`W`), and Heart Rate (`BPM`).
   - Verify `/ws/echenon/live` WebSocket stream includes `peloton_resistance` (`0–100`) and `speed_kph`.
3. **Client Compatibility & Lenovo Tab P12 Coexistence Soak (`SPEC.md` §13.6 `3.7–3.11`):**
   - Test pairing and live telemetry reception on the **Lenovo Tab P12** (plus macOS/iOS client apps as available).
   - If any target client app refuses to pair without `0x2AD9`, enable `FTMS_STUB_CONTROL_POINT=1` in `FTMSServer.h`, re-verify, and document the finding.
   - Run a 5-minute coexistence soak (FTMS client + HR relay + `/ws/echenon/live` WebSocket open simultaneously), verifying zero disconnects, zero watchdog resets, and stable free internal SRAM.
4. **Evidence Report:**
   - Record all captures, GATT hex dumps, latency measurements, and coexistence metrics in `docs/measurements/phase3/C08-20261024-ftms-verification.md`.
5. **Audit Round 1 Remediation (`AUDIT-round-1.md`):**
   - **BLOCKING-1 (`src/echenon/EchenonModule.cpp` L109–L113):** Replace `getInternalLevel()` in `_ftmsServer->updateTelemetry(...)` with the unified `0..254` mapping gated by `_calibrationWizard.isPosDone()`:
     ```cpp
     uint8_t ftmsInternalRes = _calibrationWizard.isPosDone()
         ? LevelMapping::normalizedToInternal(
               LevelMapping::echelonContinuousToNormalized(continuousLevel))
         : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
     ```
     Add unit tests in `test/test_native/test_ftms_server.cpp` asserting that Echelon Level 32 maps to `internal_resistance = 254` (encoding `resistance = 254` in `0x2AD2`, not `0`) when calibrated, and `INTERNAL_RESISTANCE_UNCALIBRATED` (`255` -> `0`) when uncalibrated.
   - **Non-Blocking Polish (`FTMSServer.h` / `FTMSServer.cpp`):**
     - `NON-BLOCKING-1` & `NON-BLOCKING-2`: Change `getProviderId()` to `"ftms"`, update `contributeAdvertising()` to use `Priority::Required` for `0x1826`, `setAppearance(0x0485)` (`kIndoorBikeAppearance`), and `addServiceData16(0x1826, svcData, 3, Priority::Optional)`.
     - `NON-BLOCKING-3`: Clarify `updateTelemetry()` doc comment in `FTMSServer.h` ("must be called from main loop").
     - `NON-BLOCKING-4`: Add `onStackPaused()` / `onStackResumed()` overrides (pausing/resuming notifications during OTA) and add a `NimBLECharacteristicCallbacks` write handler inside `#if FTMS_STUB_CONTROL_POINT` on `0x2AD9`.
6. **Live BLE (`bleak`) + Calibration Rig Verification:**
   - Tag `v0.3.0-dev.18`, build & flash `ex3-waveshare-nano-rig` (to drive synthetic cadence + resistance via `/api/echenon/rig/command`), run a Python script in `tmp/` using `bleak` + `websockets` to:
     - Read `0x2ACC` (`82 44 00 00 00 00 00 00`) over real BLE
     - Subscribe to `0x2AD2` (`Indoor Bike Data`) and capture 2 Hz (`500 ms`) 11-byte notifications across multiple rig states (e.g. 0 RPM idle, 80 RPM at Levels 1, 16, 32) confirming Level 32 reports `resistance = 254`!
     - Connect to `ws://192.168.1.100/ws/echenon/live` and capture live JSON frames containing `peloton_resistance` and `speed_kph`.
   - Then build & flash production `ex3-waveshare-nano` (`v0.3.0-dev.18`), re-verify BLE `0x2ACC` and `0x2AD2` via `bleak`, update `docs/measurements/phase3/C08-20261024-ftms-verification.md`, and run `pa wo verify WO-05.4`.
7. **Physical Reed Switch Debounce & Open-Release Hysteresis (`src/echenon/CadenceSensor.h`, `src/echenon/CadenceSensor.cpp`, `test/test_native/test_cadence_sensor.cpp`):**
   - During live physical pedalling on the Echelon EX-3 bike (`~41 RPM`), the crank reed switch stays closed (`LOW`) for `~100–200 ms` while the magnet passes and then opens (`LOW -> HIGH`), charging the `100 nF` RC filter capacitor (`τ ≈ 1.5 ms`). As the slow rising ramp crosses the ESP32 GPIO digital input threshold (`~1.65 V`), threshold oscillation triggers `attachInterrupt(..., FALLING)` a second time at `dt ≈ 150 ms` (`> DEBOUNCE_LOCKOUT_US = 25 ms`), causing `CadenceSensor` to alternate between true RPM (`~41 RPM`) and `200.0 RPM` (`MAX_RPM` clamp).
   - Implement a hardware + software two-layer fix in `CadenceSensor` (while keeping `injectPulse(uint32_t pulseMicros)` direct to `handlePulse()` so all existing unit tests remain intact):
     - **Layer A — ESP-IDF Hardware Pin Glitch Filter (`driver/gpio_filter.h`):**
       - Under `#if !defined(UNIT_TEST) && defined(ARDUINO)` in `CadenceSensor.cpp`, include `<driver/gpio_filter.h>` (when `SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER` is enabled on ESP32-S3).
       - In `CadenceSensor::begin()`, configure and enable the hardware IO-MUX pin glitch filter on `_pin`:
         ```cpp
         gpio_pin_glitch_filter_config_t filterCfg = {
             .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
             .gpio_num = static_cast<gpio_num_t>(_pin),
         };
         if (gpio_new_pin_glitch_filter(&filterCfg, &_glitchFilter) == ESP_OK && _glitchFilter) {
             gpio_glitch_filter_enable(_glitchFilter);
         }
         ```
       - In `CadenceSensor::~CadenceSensor()`, disable and delete `_glitchFilter` if non-null (`gpio_glitch_filter_disable(_glitchFilter); gpio_del_glitch_filter(_glitchFilter); _glitchFilter = nullptr;`). Store `void* _glitchFilter = nullptr;` (or `gpio_glitch_filter_handle_t` under `#if !defined(UNIT_TEST) && defined(ARDUINO)`) on `CadenceSensor`.
     - **Layer B — Immediate Leading-Edge Commit + `250 ms` Minimum Interval Lockout + Open-Release Rearm Hysteresis:**
       - Constants on `CadenceSensor`:
         - `REARM_HIGH_US = 50000` (`50 ms` continuous `HIGH` open-state rearm hysteresis after closure before accepting the next falling edge).
         - `MIN_HW_INTERVAL_US = 250000` (`250 ms` = `240 RPM` minimum revolution interval between hardware pulses — suppresses all reed-switch opening-edge bounces/RC ramp threshold oscillations occurring `20–200 ms` after closure without requiring a delayed main-loop `LOW` poll that can miss short closures).
       - Hardware ISR & State Machine methods (public so they can be unit-tested in `test_cadence_sensor.cpp`):
         - `void onFallingEdgeISR(uint32_t nowMicros)`:
           - Called from `handleISR()` on `FALLING` interrupt.
           - If `_waitingForOpen` is `true`, return immediately.
           - If `_hasFirstPulse` is `true` and `(uint32_t)(nowMicros - _lastPulseMicros) < MIN_HW_INTERVAL_US`, return immediately.
           - Commit the pulse immediately via `handlePulse(nowMicros)` (capturing the exact leading-edge microsecond timestamp without waiting for a main-loop `LOW` poll) and set `_waitingForOpen = true; _highTimingActive = false; _highStableStartMicros = 0;`.
         - `void processHardwareState(uint32_t currentMicros, bool pinIsLow)`:
           - Called at the top of `tick(uint32_t currentMillis)` when `#if !defined(UNIT_TEST) && defined(ARDUINO)` and `_pin >= 0` (passing `static_cast<uint32_t>(micros())` and `digitalRead(_pin) == LOW`).
           - **Open-State Release Hysteresis:** If `_waitingForOpen` is `true`:
             - If `pinIsLow` is `true`:
               - Still closed; reset `_highTimingActive = false;`.
             - Else (`pinIsLow == false`, i.e., `HIGH`):
               - If `!_highTimingActive`:
                 - `_highTimingActive = true; _highStableStartMicros = currentMicros;`
               - Else if `(uint32_t)(currentMicros - _highStableStartMicros) >= REARM_HIGH_US`:
                 - Line has been continuously `HIGH` for `50 ms` — re-arm: `_waitingForOpen = false; _highTimingActive = false;`.
         - Ensure `reset()` also resets `_waitingForOpen = false`, `_highTimingActive = false`, and `_highStableStartMicros = 0`.
     - Add comprehensive native unit tests in `test/test_native/test_cadence_sensor.cpp` covering:
       1. Physical reed-switch revolution with rising-edge `FALLING` glitch at `+150 ms` (confirming the `+150 ms` opening-edge glitch is rejected and two revolutions `1460 ms` apart produce `41.10 RPM` with zero `200.0 RPM` spikes).
       2. Contact bounce on closure (`onFallingEdgeISR` at `0 ms`, `1 ms`, `2 ms` -> commits immediately on the `0 ms` leading-edge timestamp and ignores subsequent bounces).
       3. `CalibrationRig` `30 ms` `LOW` pulse compatibility at `80 RPM` (`750 ms` period).
       4. Sub-250 ms (`MIN_HW_INTERVAL_US`) hardware lockout (even after `50 ms` `HIGH` rearm, any pulse with `dt < 250 ms` is suppressed).
       5. Sustained closure (> 250 ms at very slow pedalling) where `_waitingForOpen` blocks the opening-edge glitch even when `dt >= 250 ms`.
8. **Timestamp Wraparound & Cross-Task `millis()` Underflow Fixes (`src/echenon/CadenceSensor.cpp`, `src/echenon/HRMClient.cpp`, `src/echenon/FTMSServer.cpp`, `test/test_native/test_cadence_sensor.cpp`, `test/test_native/test_ftms_server.cpp`):**
   - **Fix 8a (`CadenceSensor::tick(uint32_t currentMillis)` in `src/echenon/CadenceSensor.cpp`):**
     - `pulseMicros` wraps at `2^32 µs` (`4,294,967 ms` = `71.58 min`), so `_lastPulseMs = pulseMicros / 1000` in `handlePulse()` cannot be compared against `currentMillis` (`millis()`) once uptime exceeds 71.58 minutes.
     - In `CadenceSensor::tick(uint32_t currentMillis)`, process `if (_newPulse)` **first**:
       - Compute `_rpm` from `deltaMicros` (pure `uint32_t` microsecond difference), latch `_lastPulseMs = currentMillis` (in the `millis()` clock domain), and clear `_newPulse = false`.
     - Check `else if (_hasFirstPulse && static_cast<int32_t>(currentMillis - _lastPulseMs) >= static_cast<int32_t>(TIMEOUT_MS))` **second**:
       - Zero `_rpm = 0.0f` and `_newPulse = false` only when no new pulse has arrived for `>= TIMEOUT_MS` (`3000 ms`) in the `currentMillis` clock domain.
   - **Fix 8b (`HRMClient::tick()` in `src/echenon/HRMClient.cpp` and `FTMSServer::getEffectiveHrBpm()` in `src/echenon/FTMSServer.cpp`):**
     - When the NimBLE host task receives an HR notification 1–2 ms after `Application::tick()` samples `currentMillis = millis()`, `n.receivedAtMs` is `currentMillis + 1` (or `+ 2`), causing unsigned `currentMillis - _lastValidHrMs` in `HRMClient::tick(currentMillis)` and `now_ms - _lastHrMs` in `FTMSServer::getEffectiveHrBpm(now_ms)` to underflow to `0xFFFFFFFF` (`4,294,967,295`) and fire an immediate false stale timeout (`0 BPM`) in the same tick.
     - Change `HRMClient::tick(uint32_t currentMillis)` to use signed elapsed-time comparison:
       `if (static_cast<int32_t>(currentMillis - _lastValidHrMs) >= 10000)`
     - Change `FTMSServer::getEffectiveHrBpm(uint32_t now_ms)` to use signed elapsed-time comparison:
       `if (static_cast<int32_t>(now_ms - _lastHrMs) > static_cast<int32_t>(HR_STALE_TIMEOUT_MS)) return 0;`
   - **Unit Tests:**
     - In `test/test_native/test_cadence_sensor.cpp`, add a test verifying that `CadenceSensor` continues computing accurate RPM (`> 0`) and timing out properly when `currentMillis` is past multiple 71.58-minute `micros()` wraparounds (e.g., `currentMillis = 9237153` ms while `pulseMicros` wrapped to `647218000` µs).
     - In `test/test_native/test_ftms_server.cpp`, add a test verifying that `FTMSServer::getEffectiveHrBpm(now_ms)` retains the valid HR BPM (does not underflow to `0`) when `sample.receivedAtMs` is 1–5 ms ahead of `now_ms` (`now_ms = 1000`, `sample.receivedAtMs = 1002`).
9. **Fundamental-First Sensor-to-Telemetry Pipeline & Formula-Parameter Calibration (`ResistanceSensor`, `LevelMapping`, `CalibrationWizard`, `PowerEngine`, `RecalibrationUI`, `EchenonModule`):**
   - **9a. `ResistanceSensor` Unrounded Float Wiper (`src/echenon/ResistanceSensor.h`):**
     - Add `float getFilteredWiperFloat() const { return _initialized ? _filteredWiper : 0.0f; }` alongside `uint16_t getFilteredWiper() const` so the continuous EMA float is preserved without premature `uint16_t` rounding.
   - **9b. `LevelMapping` Canonical `C07` Constants & Direct Float Helpers (`src/echenon/LevelMapping.h`, `src/echenon/LevelMapping.cpp`, `test/test_native/test_level_mapping.cpp`):**
     - Update the baseline constants in `LevelMapping.h` from the stale `C03` values (`157..1313`) to the `C07` 320-point sweep model (`docs/power-physics-model.md` §1 & `PowerEngine.h`):
       - `constexpr uint16_t WORKING_RANGE_MIN = 171;`
       - `constexpr uint16_t WORKING_RANGE_MAX = 1333;`
       - `constexpr float B03_SLOPE = (1333.0f - 171.0f) / 31.0f; // 37.483871f`
       - `constexpr float B03_INTERCEPT = 171.0f - B03_SLOPE; // 133.516129f`
     - Add direct normalized and continuous level helpers accepting `float` wiper and optional `(potMin, potMax)` bounds:
       - `inline float wiperToNormalized(float wiper, float potMin = static_cast<float>(WORKING_RANGE_MIN), float potMax = static_cast<float>(WORKING_RANGE_MAX))`: returns `0.0f` if non-finite or `potMax <= potMin`, else `std::clamp((wiper - potMin) / (potMax - potMin), 0.0f, 1.0f)`.
       - `float wiperToLevel(float wiper, float potMin = static_cast<float>(WORKING_RANGE_MIN), float potMax = static_cast<float>(WORKING_RANGE_MAX))`: returns `normalizedToEchelonContinuous(wiperToNormalized(wiper, potMin, potMax))` (and keep `float wiperToLevel(uint16_t wiper)` overload delegating to `wiperToLevel(static_cast<float>(wiper))`).
       - Update `wiperToEchelon(uint16_t wiper)` (and add `uint8_t wiperToEchelon(float wiper, float potMin = static_cast<float>(WORKING_RANGE_MIN), float potMax = static_cast<float>(WORKING_RANGE_MAX))`) to compute `static_cast<uint8_t>(std::clamp(std::round(wiperToLevel(wiper, potMin, potMax)), 1.0f, 32.0f))` directly from `wiperToLevel` — eliminating the double integer quantization through `wiperToInternal` (`0..255`).
       - Add `uint8_t wiperToInternal(float wiper, float potMin = static_cast<float>(WORKING_RANGE_MIN), float potMax = static_cast<float>(WORKING_RANGE_MAX))` delegating to `levelToInternal(wiperToLevel(wiper, potMin, potMax))`.
   - **9c. `CalibrationWizard` OLS Linear Endpoint Regression (`src/echenon/CalibrationWizard.h`, `src/echenon/CalibrationWizard.cpp`, `test/test_native/test_recalibration.cpp`):**
     - Rather than interpolating a jagged 32-point lookup table (which bakes in $\pm 2\text{--}4\text{ ADC count}$ bang-bang motor stop jitter), use Ordinary Least Squares (OLS) linear regression across recorded `(level, _table[level - 1])` entries (where `_table[level - 1] > 0`) to fit the linear potentiometer endpoints `pot_min` (at $L = 1$) and `pot_max` (at $L = 32$):
       - `bool fitLinearEndpoints(float& outPotMin, float& outPotMax) const`:
         - Collect all levels $L \in \{1..32\}$ where `_table[L - 1] > 0`.
         - If `_posDone` is `false` or fewer than 2 valid levels exist (or denominator $\sum (L - \bar{L})^2 \le 0$ or fitted `outPotMax <= outPotMin`), set `outPotMin = static_cast<float>(LevelMapping::WORKING_RANGE_MIN)` (`171.0f`), `outPotMax = static_cast<float>(LevelMapping::WORKING_RANGE_MAX)` (`1333.0f`), and return `false`.
         - Otherwise compute OLS slope $m = \frac{\sum (L_i - \bar{L})(w_i - \bar{w})}{\sum (L_i - \bar{L})^2}$ and intercept $b = \bar{w} - m\bar{L}$, set `outPotMin = b + m * 1.0f` and `outPotMax = b + m * 32.0f`, and return `true` (if `outPotMax <= outPotMin`, fall back to defaults and return `false`).
       - Cache `_potMin = 171.0f` and `_potMax = 1333.0f` on `CalibrationWizard`, updating them automatically whenever `recordLevel`, `setTableEntry`, `setPosDone`, or `reset` is called (via a private `updateFittedEndpoints()` helper), and expose:
         - `float getPotMin() const { return _potMin; }`
         - `float getPotMax() const { return _potMax; }`
         - `float wiperToNormalized(float wiper) const { return LevelMapping::wiperToNormalized(wiper, _potMin, _potMax); }`
         - `float wiperToLevel(float wiper) const { return LevelMapping::wiperToLevel(wiper, _potMin, _potMax); }`
         - `uint8_t wiperToEchelon(float wiper) const { return LevelMapping::wiperToEchelon(wiper, _potMin, _potMax); }`
   - **9d. `PowerEngine` Direct Normalized Evaluation (`src/echenon/PowerEngine.h`, `src/echenon/PowerEngine.cpp`, `test/test_native/test_power_engine.cpp`):**
     - Add `static float computePowerFromNormalized(float p_tilde, float cadence_rpm, const PowerPhysicsParams& params = PowerPhysicsParams{})` which validates finite inputs and `cadence_rpm > 0.0f`, clamps `p_tilde` to `[0.0f, 1.0f]`, and calls `evaluateKlossPower(p_tilde, cadence_rpm, params)`.
     - Add member `float computePowerFromNormalized(float p_tilde, float cadenceRPM) const` which uses `computePowerFromNormalized(p_tilde, cadenceRPM, _physicsParams)` when `_usePhysicsModel` is `true`, and `computePowerTable(1.0f + 31.0f * std::clamp(p_tilde, 0.0f, 1.0f), cadenceRPM)` when a custom CSV table is active.
     - Add member `void tickFromNormalized(float p_tilde, float cadenceRPM)` which sets `_lastPower = computePowerFromNormalized(p_tilde, cadenceRPM)`.
     - In `PowerEngine::loadDefault()`, keep `_physicsParams` (or reset `a_scale = A`, `pot_min = POT_MIN_DEFAULT`, `pot_max = POT_MAX_DEFAULT` on explicit default reset while allowing `setPhysicsParams` to customize `pot_min`, `pot_max`, `a_scale` while `_usePhysicsModel == true`).
   - **9e. `RecalibrationUI` Formula Scale-Factor Fitting (`src/echenon/RecalibrationUI.h`, `src/echenon/RecalibrationUI.cpp`, `test/test_native/test_recalibration.cpp`):**
     - Add `bool applyToPhysicsParams(PowerPhysicsParams& params) const` to `RecalibrationUI`:
       - Instead of abandoning `Physics P3r` for a sparse 32x31 table when recalibration points are recorded, fit a least-squares multiplicative scale factor on the eddy-current coefficient `params.a_scale` across all valid recorded points (`pt.cadenceRPM > 0.0f && pt.reportedPower > 0.0f`):
         - For each point, determine $\tilde{p}_i$: if `pt.wiperCount > 0 && params.pot_max > params.pot_min`, use `std::clamp((static_cast<float>(pt.wiperCount) - params.pot_min) / (params.pot_max - params.pot_min), 0.0f, 1.0f)`; else use `std::clamp((static_cast<float>(pt.echelonLevel) - 1.0f) / 31.0f, 0.0f, 1.0f)`.
         - Compute baseline eddy-current power at unit scale $B_i = \exp(\beta u_i) \frac{\text{cad}_i^2}{1 + (\text{cad}_i / \text{cad}_c)^2}$ where $u_i = 1 - \cos(\theta_{\text{sweep}} \tilde{p}_i)$, and target eddy power $Y_i = \max(P_{\text{reported},i} - c_{\text{fric}} \cdot \text{cad}_i, 0.0f)$.
         - Compute the ordinary least-squares estimator through the origin: $a_{\text{scale}} = \frac{\sum B_i Y_i}{\sum B_i^2}$, clamped to a sane physical range $[0.25 \cdot A,\; 4.0 \cdot A]$ (`[0.00122f, 0.01954f]`).
         - If at least one valid point has $B_i > 0$, update `params.a_scale` and return `true`; otherwise set `params.a_scale = PowerEngine::A` and return `false`.
     - Keep `applyToPowerTable(PowerTable& table) const` intact for legacy unit test compatibility.
   - **9f. `EchenonModule` Fundamental-First Propagation (`src/echenon/EchenonModule.h`, `src/echenon/EchenonModule.cpp`):**
     - Add a private helper `void syncPhysicsCalibration()` on `EchenonModule` that:
       - Copies `PowerPhysicsParams params = _powerEngine.getPhysicsParams();`
       - Sets `params.pot_min = _calibrationWizard.getPotMin();` and `params.pot_max = _calibrationWizard.getPotMax();`
       - Calls `_recalibrationUI.applyToPhysicsParams(params);`
       - Calls `_powerEngine.setPhysicsParams(params);`
     - Call `syncPhysicsCalibration()` in `applyConfig()`, `resetConfig()`, after `/api/echenon/calibration/record-level` completes, and after `/api/echenon/recalibration/point` and `/api/echenon/recalibration/reset` (in `/api/echenon/recalibration/point`, call `syncPhysicsCalibration()` instead of `setTable(tbl)` so `_usePhysicsModel` stays `true` and the 320-point `Physics P3r` surface is scaled smoothly rather than disabled!).
     - Expose `float getNormalizedPosition() const { return _calibrationWizard.wiperToNormalized(_resistanceSensor.getFilteredWiperFloat()); }` and `float getContinuousLevel() const { return _calibrationWizard.wiperToLevel(_resistanceSensor.getFilteredWiperFloat()); }` on `EchenonModule`, and update `getEchelonLevel()` and `getInternalLevel()` to use `_resistanceSensor.getFilteredWiperFloat()` with `_calibrationWizard.getPotMin()` / `getPotMax()`.
     - In `EchenonModule::tick(uint32_t currentMillis)`:
       - Read the two fundamental continuous states once per tick:
         ```cpp
         const float wiperFloat = _resistanceSensor.getFilteredWiperFloat();
         const float pTilde = _calibrationWizard.wiperToNormalized(wiperFloat);
         const float cadenceRpm = _cadenceSensor.getRPM();
         ```
       - Evaluate power directly from `(pTilde, cadenceRpm)`:
         `_powerEngine.tickFromNormalized(pTilde, cadenceRpm);`
       - Derive all projections directly from `(pTilde, _powerEngine.getLastPower())` and quantize only at the sinks:
         - `uint8_t pelotonRes = _calibrationWizard.isPosDone() ? echenon::PowerEngine::computePelotonResistanceFromNormalized(pTilde, _powerEngine.getPhysicsParams()) : 0;`
         - `float speedKph = echenon::PowerEngine::computeVirtualSpeedKph(_powerEngine.getLastPower());`
         - `uint8_t ftmsInternalRes = _calibrationWizard.isPosDone() ? LevelMapping::normalizedToInternal(pTilde) : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;`
         - `int16_t ftmsPowerWatts = static_cast<int16_t>(std::round(_powerEngine.getLastPower()));` (fixing the previous `static_cast<int16_t>` float truncation).

## 3. Acceptance Criteria
- `BLOCKING-1` and non-blocking findings remediated with unit tests passing (`pio test -e native`).
- Hardware verification report `docs/measurements/phase3/C08-20261024-ftms-verification.md` updated with real `bleak` GATT `0x2ACC` & `0x2AD2` captures (including Level 32 = 254), interval timing, and `/ws/echenon/live` WebSocket payloads.
- `pio test -e native` passes cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
