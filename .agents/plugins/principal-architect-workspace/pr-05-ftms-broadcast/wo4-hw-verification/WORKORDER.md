---
schema_version: 1
wo_id: WO-05.4
pr_id: PR-05
title: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak
complexity: interactive
status: IN_PROGRESS
allowed_files:
- docs/measurements/phase3/C08-20261024-ftms-verification.md
- src/echenon/CadenceSensor.h
- src/echenon/CadenceSensor.cpp
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
- src/echenon/EchenonModule.cpp
- test/test_native/test_cadence_sensor.cpp
- test/test_native/test_ftms_server.cpp
forbidden_files: []
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: 0bc01c5e104d5d0c50a820be5ad572793507bc750e351f7c781af89109f18e4d
completion: null
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
     - **Layer B — Two-State Closed-Hold Confirmation + Open-Release Rearm Hysteresis:**
       - Constants on `CadenceSensor`:
         - `CONFIRM_LOW_US = 5000` (`5 ms` post-falling-edge `LOW` confirmation window — compatible with both the physical reed switch `20–200 ms` closure and `CalibrationRig` `30 ms` `LOW` pulse).
         - `REARM_HIGH_US = 50000` (`50 ms` continuous `HIGH` open-state rearm hysteresis after closure before accepting the next falling edge).
         - `MIN_HW_INTERVAL_US = 250000` (`250 ms` = `240 RPM` minimum revolution interval between confirmed hardware pulses).
       - Hardware ISR & State Machine methods (public so they can be unit-tested in `test_cadence_sensor.cpp`):
         - `void onFallingEdgeISR(uint32_t nowMicros)`:
           - Called from `handleISR()` on `FALLING` interrupt.
           - If `_waitingForOpen` is `true` or `_hasPendingPulse` is `true`, return immediately.
           - If `_hasFirstPulse` is `true` and `(uint32_t)(nowMicros - _lastPulseMicros) < MIN_HW_INTERVAL_US`, return immediately.
           - Latch `_pendingPulseMicros = nowMicros; _hasPendingPulse = true;` (capturing the exact leading-edge microsecond timestamp).
         - `void processHardwareState(uint32_t currentMicros, bool pinIsLow)`:
           - Called at the top of `tick(uint32_t currentMillis)` when `#if !defined(UNIT_TEST) && defined(ARDUINO)` and `_pin >= 0` (passing `static_cast<uint32_t>(micros())` and `digitalRead(_pin) == LOW`).
           - **Pending Pulse Confirmation:** If `_hasPendingPulse` is `true` and `(uint32_t)(currentMicros - _pendingPulseMicros) >= CONFIRM_LOW_US`:
             - `_hasPendingPulse = false;`
             - If `pinIsLow` is `true`:
               - Confirmed genuine closure! Set `_waitingForOpen = true; _highTimingActive = false; _highStableStartMicros = 0;` and call `handlePulse(_pendingPulseMicros);`.
             - Else (`pinIsLow == false`):
               - Transient glitch or rising-edge RC threshold oscillation (`LOW -> HIGH` ramp is already `HIGH` 5 ms later) — discard without calling `handlePulse()`.
           - **Open-State Release Hysteresis:** If `_waitingForOpen` is `true`:
             - If `pinIsLow` is `true`:
               - Still closed; reset `_highTimingActive = false;`.
             - Else (`pinIsLow == false`, i.e., `HIGH`):
               - If `!_highTimingActive`:
                 - `_highTimingActive = true; _highStableStartMicros = currentMicros;`
               - Else if `(uint32_t)(currentMicros - _highStableStartMicros) >= REARM_HIGH_US`:
                 - Line has been continuously `HIGH` for `50 ms` — re-arm: `_waitingForOpen = false; _highTimingActive = false;`.
         - Ensure `reset()` also resets `_hasPendingPulse = false`, `_pendingPulseMicros = 0`, `_waitingForOpen = false`, `_highTimingActive = false`, and `_highStableStartMicros = 0`.
     - Add comprehensive native unit tests in `test/test_native/test_cadence_sensor.cpp` covering:
       1. Physical reed-switch revolution with rising-edge `FALLING` glitch at `+150 ms` (confirming the `+150 ms` opening-edge glitch is rejected and two revolutions `1460 ms` apart produce `41.10 RPM` with zero `200.0 RPM` spikes).
       2. Transient noise spike (`onFallingEdgeISR` where `pinIsLow` is `false` at `+5 ms` -> ignored).
       3. Contact bounce on closure (`onFallingEdgeISR` at `0 ms`, `1 ms`, `2 ms` -> confirms at `+5 ms` with exact `0 ms` leading-edge timestamp).
       4. `CalibrationRig` `30 ms` `LOW` pulse compatibility at `80 RPM` (`750 ms` period).
       5. Sub-250 ms (`MIN_HW_INTERVAL_US`) hardware lockout.

## 3. Acceptance Criteria
- `BLOCKING-1` and non-blocking findings remediated with unit tests passing (`pio test -e native`).
- Hardware verification report `docs/measurements/phase3/C08-20261024-ftms-verification.md` updated with real `bleak` GATT `0x2ACC` & `0x2AD2` captures (including Level 32 = 254), interval timing, and `/ws/echenon/live` WebSocket payloads.
- `pio test -e native` passes cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
