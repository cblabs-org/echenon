---
schema_version: 1
wo_id: WO-14a.4
pr_id: PR-14
title: "Remediation — Session Persistence Trigger + Epoch Propagation"
complexity: standard
status: COMPLETED
allowed_files:
  - src/main.cpp
  - src/echenon/EchenonModule.h
  - src/echenon/EchenonModule.cpp
forbidden_files: []
required_skills:
  - framework-expert
verification_command: "pio test -e native --verbose"
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-14.4a — Remediation: Session Persistence Trigger + Epoch Propagation

## 1. Role & Objective

Fix two blockers discovered during WO-14.4 integration that would cause
WO-14.7 hardware verification to fail:

1. Completed rides don't auto-persist to flash (callback not hooked)
2. Diagnostic event timestamps are epoch=0 until NTP syncs

## 2. Strict Boundary Box & Constraints

- **You may ONLY edit** the files listed in `allowed_files`.
- **No heap `std::function`** (GRAVE-05). Use EventBus static callbacks or
  direct function calls.
- **SignalLogger untouched.**
- All existing 232 tests must continue to pass.

## 3. Line-Item Instructions

### 3.1 Fix 1: Session Persistence Trigger

- [ ] In `main.cpp`, subscribe to `echenon::EVENT_SESSION_TOPIC` with a static
  callback that triggers ride persistence when a session ends:
  ```cpp
  // Pattern: static free function, no std::function capture
  static void onSessionEndPersist(const char*, const void* data, void*) {
      if (!data) return;
      const auto* ev = static_cast<const echenon::SessionStateChangeEvent*>(data);
      if (ev->newState != echenon::SessionState::ENDED || ev->sessionId == 0) return;
      if (!ridePersistence.isEnabled()) return;

      // Load the completed ride from rideStore's stored rides
      echenon::RideSummary summary;
      std::vector<echenon::RideRecord> records;
      if (rideStore.loadStoredRide(ev->sessionId, summary, records) && !records.empty()) {
          echenon::RideFileHeader header;
          header.magic = echenon::RIDE_FILE_MAGIC;
          header.version = echenon::RIDE_FILE_VERSION;
          header.flags = echenon::RIDE_HEADER_FLAG_FINALIZED |
                         (summary.ntp_valid ? echenon::RIDE_HEADER_FLAG_NTP_VALID : 0);
          header.session_id = summary.session_id;
          header.start_epoch_s = summary.start_epoch_s;
          ridePersistence.saveRide(ev->sessionId, header, records.data(),
                                   static_cast<uint16_t>(records.size()));
      }
  }
  ```
- [ ] Register this callback in `setup()` alongside the existing upload queue
  subscription:
  ```cpp
  app.getEventBus().subscribe(echenon::EVENT_SESSION_TOPIC, onSessionEndPersist);
  ```
- [ ] This must be a **static free function** (not a lambda with captures) to
  avoid GRAVE-05. The `ridePersistence` and `rideStore` statics are at file
  scope so they're accessible.

### 3.2 Fix 2: Epoch Propagation for Diagnostic Events

- [ ] In `EchenonModule.h`, add a method to receive the current epoch:
  ```cpp
  void updateEpoch(uint32_t epoch_s) { _currentEpoch = epoch_s; }
  uint32_t getCurrentEpoch() const { return _currentEpoch; }
  ```
  Add member: `uint32_t _currentEpoch = 0;`

- [ ] In `EchenonModule::tick()`, use `_currentEpoch` when recording runaway
  alerts instead of 0:
  ```cpp
  _diagnostics.recordRunawayAlert(_currentEpoch, _runawayDetector.getRunawayDurationMs());
  ```

- [ ] In `main.cpp`, propagate epoch to EchenonModule. Check how TimeManager
  works — look for `TimeManager::getEpoch()` or similar. If available under
  `WITH_TIME`:
  ```cpp
  // In the WITH_TIME + WITH_SESSION block, after timeManager.begin():
  // Periodically update epoch from TimeManager
  ```
  
  Alternatively, use the same approach as `SessionManager::updateWallClock()`:
  - In the existing session event subscription or a periodic check, call
    `echenonModule.updateEpoch(time(nullptr))` when `time(nullptr) > 1700000000`
    (valid SNTP epoch threshold).

- [ ] Fix the boot event in `main.cpp` — currently records with epoch=0.
  Change to record boot event AFTER time is available, or re-record boot
  event when NTP first syncs. The simplest approach:
  - Keep the initial `recordBoot(0, millis()/1000)` at boot
  - Add a one-shot epoch update: in the tick loop or a time callback, when
    `time(nullptr)` first becomes valid, call
    `echenonModule.updateEpoch(time(nullptr))` and optionally
    `echenonModule.diagnostics().recordBoot(time(nullptr), millis()/1000)`
    to record a proper boot event with real epoch.

- [ ] For session start/end events in the EventBus callback in `main.cpp`,
  use `time(nullptr)` if valid, otherwise 0:
  ```cpp
  uint32_t epoch = time(nullptr);
  if (epoch < 1700000000u) epoch = 0;
  ```

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all 232 tests must pass.
- After verification, run `pa wo verify WO-14.4a`.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

- **status**: COMPLETED
- **commit_sha**: (to be stamped on commit)
- **files_modified**:
  - `src/main.cpp` — Added `getValidEpoch()` helper, `onSessionEndPersist()` static free function for ride persistence on session end, epoch propagation in session event lambda and loop, boot event epoch fix.
  - `src/echenon/EchenonModule.h` — Added `updateEpoch()`/`getCurrentEpoch()` methods and `_currentEpoch` member.
  - `src/echenon/EchenonModule.cpp` — Changed `recordRunawayAlert` to use `_currentEpoch` instead of hard-coded 0.
- **verification_summary**: 232/232 native tests passed. All changes within allowed_files boundary. No heap std::function used — persistence callback is a static free function (GRAVE-05 safe).
- **decisions_made**:
  - Used a shared `getValidEpoch()` static helper (threshold 1700000000) to avoid repeating the epoch-validity check in 4+ call sites.
  - Epoch propagation happens every loop iteration via `echenonModule.updateEpoch(getValidEpoch())` — `time(nullptr)` is a cheap syscall, no throttling needed.
  - Boot event records `getValidEpoch()` at setup time (will be 0 pre-NTP, real epoch post-NTP on warm reboot).
