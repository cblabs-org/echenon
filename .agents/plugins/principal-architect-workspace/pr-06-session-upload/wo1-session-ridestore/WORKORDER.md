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
spec_hash: c5f472b86c41c6e7ab5eaee32e0041ac30b56d66e40c2551674e9852f384acbe
completion: null
---

# WO-06.1: RideStore PSRAM/LittleFS Storage & SessionManager Lifecycle (Auto-Start Toggle + 5 s Default)

## 1. Objective & Scope

Implement `RideStore` (`src/echenon/RideStore.h` / `.cpp`) for PSRAM-backed 1 Hz ride sample storage with 30-second LittleFS crash-recovery checkpoints and 10-ride history ring, and `SessionManager` (`src/echenon/SessionManager.h` / `.cpp`) implementing the `IDLE -> ACTIVE <-> PAUSED -> ENDED` state machine with configurable `autoStartEnabled` (default `true`) and `autoStartDelaySec` (default `5 s`).

## 2. Detailed Requirements

### 2.1 `RideStore` (`src/echenon/RideStore.h` & `.cpp`, `SPEC.md` §9.2–§9.3)
1. **Packed 8-Byte `RideRecord`:**
   ```cpp
   struct __attribute__((packed)) RideRecord {
       uint16_t timestamp_offset_s; // Seconds since session start (0..65535)
       uint16_t power_watts;        // Instantaneous power (W)
       uint8_t  cadence_rpm;        // Instantaneous cadence (RPM, 0..255)
       uint8_t  heart_rate_bpm;     // Instantaneous HR (BPM, 0 if invalid)
       uint8_t  resistance_level;   // Echelon discrete level (1..32)
       uint8_t  flags;              // bit 0: hr_valid, bit 1: pause_boundary
   };
   static_assert(sizeof(RideRecord) == 8, "RideRecord must be 8 bytes");
   ```
2. **Session Header & Summary (`RideSummary`):**
   - `uint32_t session_id` (Unix epoch start time in seconds, or monotonic counter if NTP not synced yet, with `bool ntp_valid`).
   - `uint32_t start_epoch_s`, `uint16_t duration_s`, `uint16_t active_duration_s`, `uint16_t record_count`.
   - `uint16_t avg_power_w`, `uint16_t max_power_w`, `uint8_t avg_cadence_rpm`, `uint8_t avg_hr_bpm`, `uint8_t max_hr_bpm`, `uint16_t total_work_kj`.
3. **PSRAM Buffer & LittleFS Checkpointing:**
   - Allocate up to `MAX_RECORDS = 14400` (`4 hours` @ `1 Hz` = `115,200 bytes`) using `ps_malloc` on ESP32 (`malloc` in native tests).
   - Every `30` appended records (`CHECKPOINT_INTERVAL_RECORDS = 30`), flush the new records to `/rides/active.bin` (injectable filesystem callback/abstraction so native unit tests can test checkpointing and crash recovery in memory or temp dir).
   - `trimTrailingZeroCadence(uint16_t trim_seconds)`: removes trailing records where `cadence_rpm == 0` up to `trim_seconds` (`SPEC.md` §9.3) and recomputes summary metrics.
   - `finalizeSession()`: writes completed ride to `/rides/ride_<session_id>.bin`, removes `/rides/active.bin`, and maintains at most `MAX_STORED_RIDES = 10` completed rides (evicting the oldest).
   - `recoverInterruptedSession()`: on boot, if `/rides/active.bin` exists with `>= 1` record, reconstructs its summary, finalizes it into `/rides/ride_<session_id>.bin`, and deletes `/rides/active.bin`.

### 2.2 `SessionManager` (`src/echenon/SessionManager.h` & `.cpp`, `SPEC.md` §9.1)
1. **Interfaces:** Implements `ITickable` and `IConfigurable` (namespace `"echenon_session"`).
2. **Configurable Parameters (Owner Decision 2026-10-24):**
   - `autoStartEnabled` (`bool`, default `true`): enables/disables pedalling auto-start.
   - `autoStartDelaySec` (`uint16_t`, default **`5`**, range `1..60`): continuous `cadence > 0` seconds required to auto-start when `autoStartEnabled == true`.
   - `inactivityTimeoutSec` (`uint16_t`, default **`300`**, range `30..3600`): continuous `cadence == 0` seconds while `ACTIVE` before auto-ending and trimming the trailing `inactivityTimeoutSec` of zero-cadence records.
3. **State Machine & Manual Controls:**
   - `bool startSession(uint32_t now_epoch_s = 0)` (`IDLE -> ACTIVE`)
   - `bool pauseSession()` (`ACTIVE -> PAUSED`)
   - `bool resumeSession()` (`PAUSED -> ACTIVE`, sets `pause_boundary` flag on first resumed record)
   - `bool endSession(bool trim_inactivity = false)` (`ACTIVE/PAUSED -> ENDED -> IDLE`, finalizes `RideStore`, publishes `"echenon.session"` event on `EventBus`, and notifies optional completion listener for `UploadQueue`).
4. **1 Hz Sampling (`tick()`):**
   - Samples current `(power_watts, cadence_rpm, heart_rate_bpm, hr_valid, resistance_level)` every `1000 ms` while `ACTIVE` and appends a `RideRecord` to `RideStore`.

### 2.3 Native Unit Tests (`test/test_native/test_session_manager.cpp`)
- Test `sizeof(RideRecord) == 8`.
- Test auto-start after `5 s` of `cadence > 0` when `autoStartEnabled == true`.
- Test that `cadence > 0` for `< 5 s` does NOT start a session.
- Test that when `autoStartEnabled == false`, pedalling indefinitely does NOT start a session, while manual `startSession()` / `pauseSession()` / `resumeSession()` / `endSession()` work cleanly.
- Test `300 s` inactivity auto-end and verify trailing zero-cadence records are trimmed from `RideStore` and excluded from average power/cadence/duration.
- Test `30 s` checkpoint flush and boot crash recovery (`recoverInterruptedSession()`).
- Test 10-ride FIFO eviction in `RideStore`.

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus the new `test_session_manager.cpp` suite.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
