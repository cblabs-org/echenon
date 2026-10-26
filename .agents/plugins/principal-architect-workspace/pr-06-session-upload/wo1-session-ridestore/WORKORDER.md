---
schema_version: 1
wo_id: WO-06.1
pr_id: PR-06
title: RideStore PSRAM/LittleFS Storage & SessionManager Lifecycle (Auto-Start Toggle
  + 5 s Default)
complexity: standard
status: PENDING
allowed_files:
- src/echenon/RideStore.h
- src/echenon/RideStore.cpp
- src/echenon/SessionManager.h
- src/echenon/SessionManager.cpp
- test/test_native/test_session_manager.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: 2e9c46e54bdc8ebcf61dcab65a01ec788bc1024b7c86bced62c9a5ba916e2291
completion: null
---

# WO-06.1: RideStore PSRAM/LittleFS Storage & SessionManager Lifecycle (Auto-Start Toggle + 5 s Default)

## 1. Objective & Scope

Implement `RideStore` (`src/echenon/RideStore.h` / `.cpp`) for PSRAM-backed 1 Hz ride sample storage (averaged from 2 Hz sensor ticks and storing the canonical `0..254` `internal_resistance` coordinate) with 30-second LittleFS crash-recovery checkpoints and 5-ride history ring, and `SessionManager` (`src/echenon/SessionManager.h` / `.cpp`) implementing the `IDLE -> ACTIVE <-> PAUSED -> ENDED` state machine with configurable `autoStartEnabled` (default `true`), `autoStartDelaySec` (default `5 s`), `inactivityTimeoutSec` (default `180 s`), wraparound-safe signed `int32_t` `millis()` timers, and mid-ride NTP wall-clock back-computation (`SPEC.md` §5.3 & §9.1–§9.4).

## 2. Detailed Requirements

### 2.1 `RideStore` (`src/echenon/RideStore.h` & `.cpp`, `SPEC.md` §5.3 & §9.2–§9.3)
1. **Packed 8-Byte Fundamental-First `RideRecord`:**
   ```cpp
   static constexpr uint8_t RIDE_FLAG_HR_VALID              = (1u << 0);
   static constexpr uint8_t RIDE_FLAG_RESISTANCE_CALIBRATED = (1u << 1);
   static constexpr uint8_t RIDE_FLAG_PAUSE_BOUNDARY        = (1u << 2);

   struct __attribute__((packed)) RideRecord {
       uint16_t timestamp_offset_s;  // Seconds since session start (0..65535)
       uint16_t power_watts;         // 1 Hz average power from 2 Hz samples (W)
       uint8_t  cadence_rpm;         // 1 Hz average cadence from 2 Hz samples (RPM, 0..255)
       uint8_t  heart_rate_bpm;      // Instantaneous HR (BPM, 0 if invalid)
       uint8_t  internal_resistance; // Canonical 0..254 = round(254 * pTilde), 255 = UNCALIBRATED
       uint8_t  flags;               // bit 0: hr_valid, bit 1: resistance_calibrated, bit 2: pause_boundary
   };
   static_assert(sizeof(RideRecord) == 8, "RideRecord must be 8 bytes");
   ```
   - Provide inline helpers on `RideRecord` (using `LevelMapping.h`):
     - `float normalizedResistance() const` (`internal_resistance <= 254 ? (float)internal_resistance / 254.0f : 0.0f`)
     - `uint8_t echelonLevel() const` (`LevelMapping::internalToEchelon(internal_resistance)`)
     - `uint8_t pelotonLevel() const` (`LevelMapping::internalToPeloton(internal_resistance)`)
2. **Session Header & Summary (`RideSummary`):**
   - `uint32_t session_id` (Unix epoch start time in seconds when NTP is synced, or monotonic counter if NTP not synced yet, with `bool ntp_valid`).
   - `uint32_t start_epoch_s`, `uint16_t duration_s`, `uint16_t active_duration_s`, `uint16_t record_count`.
   - `uint16_t avg_power_w`, `uint16_t max_power_w`, `uint8_t avg_cadence_rpm`, `uint8_t avg_hr_bpm`, `uint8_t max_hr_bpm`, `uint16_t total_work_kj`.
3. **PSRAM Buffer & LittleFS Checkpointing:**
   - Allocate up to `MAX_RECORDS = 14400` (`4 hours` @ `1 Hz` = `115,200 bytes`) using `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` / `ps_malloc` on ESP32 (`malloc` fallback in native tests).
   - Every `30` appended records (`CHECKPOINT_INTERVAL_RECORDS = 30`), flush the new records to `/ride_active.bin` (or `/rides/active.bin` via injectable filesystem callback/abstraction so native unit tests can test checkpointing and crash recovery in memory or temp dir).
   - `trimTrailingZeroCadence(uint16_t trim_seconds)`: removes trailing records where `cadence_rpm == 0` up to `trim_seconds` (`SPEC.md` §9.2–§9.3) and recomputes summary metrics (`avg_power_w`, `max_power_w`, `avg_cadence_rpm`, `avg_hr_bpm`, `max_hr_bpm`, `total_work_kj`, `duration_s`, `active_duration_s`).
   - `finalizeSession()`: writes completed ride to `/rides/ride_<session_id>.bin`, removes `/ride_active.bin`, and maintains at most `MAX_STORED_RIDES = 5` completed rides (`SPEC.md` §9.2, evicting the oldest).
   - `discardSession(uint32_t session_id)`: deletes a stored session (or active buffer) from memory and LittleFS (`SPEC.md` §11.3/§11.5).
   - `recoverInterruptedSession(uint16_t min_records = 60)`: on boot, if `/ride_active.bin` exists with `>= min_records` records (`SPEC.md` §9.2, configurable threshold for unit testing), reconstructs its summary, finalizes it into `/rides/ride_<session_id>.bin`, and deletes `/ride_active.bin`.

### 2.2 `SessionManager` (`src/echenon/SessionManager.h` & `.cpp`, `SPEC.md` §9.1–§9.4)
1. **Interfaces:** Implements `ITickable` and `IConfigurable` (namespace `"echenon_session"`).
2. **Configurable Parameters (Owner Decision 2026-10-24 & `SPEC.md` §9.2):**
   - `autoStartEnabled` (`bool`, default `true`, config key `"session_auto_start"`): enables/disables pedalling auto-start.
   - `autoStartDelaySec` (`uint16_t`, default **`5`**, range `1..60`, config key `"session_auto_start_s"`): continuous `cadence > 0` seconds required to auto-start when `autoStartEnabled == true`.
   - `inactivityTimeoutSec` (`uint16_t`, default **`180`**, range `30..3600`, config key `"session_inactivity_s"`): continuous `cadence == 0` seconds while `ACTIVE` before auto-ending and trimming the trailing zero-cadence records (`SPEC.md` §9.2).
3. **Wraparound-Safe Signed `int32_t` Timers & Mid-Ride NTP Back-Computation (`PR-05` `WO-05.4` Lesson & `SPEC.md` §9.2):**
   - All `millis()` comparisons (`500 ms` sample interval, auto-start delay, inactivity timeout) MUST use signed `(int32_t)(now_ms - last_ms) >= threshold_ms` arithmetic.
   - `void updateWallClock(uint32_t now_epoch_s)`: if an active/paused session started before NTP sync (`start_epoch_s < 1700000000`) and `now_epoch_s >= 1700000000`, dynamically back-computes `start_epoch_s = now_epoch_s - elapsed_s` and marks `ntp_valid = true`.
4. **State Machine & Manual Controls:**
   - `bool startSession(uint32_t now_epoch_s = 0, uint32_t now_ms = 0)` (`IDLE -> ACTIVE`)
   - `bool pauseSession(uint32_t now_ms = 0)` (`ACTIVE -> PAUSED`)
   - `bool resumeSession(uint32_t now_ms = 0)` (`PAUSED -> ACTIVE`, sets `RIDE_FLAG_PAUSE_BOUNDARY` flag on first resumed record)
   - `bool endSession(bool trim_inactivity = false)` (`ACTIVE/PAUSED -> ENDED -> IDLE`, finalizes `RideStore`, publishes `"echenon.session"` event on `EventBus`, and notifies optional completion listener for `UploadQueue`).
   - `bool discardActiveSession()` (`ACTIVE/PAUSED -> IDLE` without saving to history or queuing upload).
5. **`2 Hz` Sampling $\to$ `1 Hz` Anti-Aliased Record (`SPEC.md` §9.3–§9.4):**
   - Samples current `(power_watts, cadence_rpm, heart_rate_bpm, hr_valid, internal_resistance, resistance_calibrated)` every `500 ms` (`2 Hz`) while `ACTIVE`. Every second (`2` consecutive `500 ms` samples), averages `power_watts = (p0 + p1 + 1) / 2` and `cadence_rpm = (c0 + c1 + 1) / 2`, and appends the `1 Hz` `RideRecord` to `RideStore`.

### 2.3 Native Unit Tests (`test/test_native/test_session_manager.cpp`)
- Test `sizeof(RideRecord) == 8` and verify `internal_resistance` (`0..254`, `255 = UNCALIBRATED`) round-trips cleanly to `normalizedResistance()`, `echelonLevel()`, and `pelotonLevel()`.
- Test `2 Hz` (`500 ms`) sampling averaging into `1 Hz` `RideRecord` entries.
- Test auto-start after `5 s` of `cadence > 0` when `autoStartEnabled == true`, and verify `< 5 s` does NOT start a session.
- Test that when `autoStartEnabled == false`, pedalling indefinitely does NOT start a session, while manual `startSession()` / `pauseSession()` / `resumeSession()` / `endSession()` / `discardActiveSession()` work cleanly.
- Test `180 s` default inactivity auto-end and verify trailing zero-cadence records are trimmed from `RideStore` and excluded from average power/cadence/duration.
- Test mid-ride NTP sync back-computation (`start_epoch_s = now_epoch_s - elapsed_s`) and `millis()` `uint32_t` wraparound across `0xFFFFFFFF -> 0x00000100`.
- Test `30 s` checkpoint flush, boot crash recovery (`recoverInterruptedSession()`), and 5-ride FIFO eviction in `RideStore`.

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus the new `test_session_manager.cpp` suite.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
