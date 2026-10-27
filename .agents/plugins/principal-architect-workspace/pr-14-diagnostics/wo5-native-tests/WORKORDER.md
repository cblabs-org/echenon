---
schema_version: 1
wo_id: WO-14.5
pr_id: PR-14
title: Native Tests
complexity: standard
status: COMPLETED
allowed_files:
- test/test_native/test_diagnostics_module.cpp
- test/test_native/test_ride_persistence.cpp
- test/test_native/test_diagnostics_routes.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 5a584088892be40521437a5787e9464528ef04fb1bc82691b4f305665b20cb66
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_3634caebb0d7
  commit_sha: becaccf06b4b636924886ccd3b1ef8f57fdefb2c
  files_modified:
  - test/test_native/test_diagnostics_module.cpp
  - test/test_native/test_diagnostics_routes.cpp
  - test/test_native/test_ride_persistence.cpp
  verification_summary: 'pio test -e native --verbose: ================ 232 test cases:
    232 succeeded in 00:00:00.501 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.5 — Native Tests

## 1. Role & Objective

You are an expert senior embedded C++ engineer. Write comprehensive native unit
tests for the DiagnosticsModule, RidePersistence, and DiagnosticsRoutes classes
introduced in WO-14.1 through WO-14.3. Target: ≥ 25 new `RUN_TEST` cases
(GoogleTest `TEST()` macros) across the test files.

Read the `framework-expert` skill at
`/Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md`.

Also study the existing test patterns in `test/test_native/`:
- `test_session_manager.cpp` — EventBus integration, InMemoryRideFileSystem, unity build
- `test_runaway_detector.cpp` — pure logic testing
- `test_strava_uploader.cpp` — injectable spy/mock pattern
- `test_upload_queue.cpp` — filesystem abstraction testing

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** touch any source files (`src/`), `main.cpp`, web files, or config.
- **No heap `std::function`** (GRAVE-05 — applies to test harness code too;
  use function pointers or templates for callbacks).
- Follow existing test patterns: unity-build style `#include "*.cpp"` for
  source files, GoogleTest framework, `InMemoryRideFileSystem` for filesystem.

## 3. Line-Item Instructions

### 3.1 `test_diagnostics_module.cpp` — DiagnosticsModule Tests (≥ 12 tests)

- [ ] Include pattern (unity build):
  ```cpp
  #include <gtest/gtest.h>
  #include "echenon/DiagnosticsModule.h"
  #include "echenon/DiagnosticsModule.cpp"
  ```

- [ ] Test cases (minimum):
  1. `TEST(DiagnosticsModuleTest, BeginAllocatesBuffer)` — `begin()` returns true, `getEventCount()` == 0
  2. `TEST(DiagnosticsModuleTest, RecordBootEvent)` — `recordBoot()`, verify event type, epoch, uptime payload
  3. `TEST(DiagnosticsModuleTest, RecordSessionStartEvent)` — verify session_id in payload
  4. `TEST(DiagnosticsModuleTest, RecordSessionEndEvent)` — verify duration and record_count payload
  5. `TEST(DiagnosticsModuleTest, RecordRunawayAlertEvent)` — verify duration_ms encoding
  6. `TEST(DiagnosticsModuleTest, RecordCalibrationDoneEvent)` — verify calibration_type
  7. `TEST(DiagnosticsModuleTest, RecordFirmwareChangeEvent)` — verify major/minor/patch encoding
  8. `TEST(DiagnosticsModuleTest, RingBufferWraps)` — fill > capacity, verify oldest evicted
  9. `TEST(DiagnosticsModuleTest, GetEventReturnsOldestFirst)` — verify ordering after wrap
  10. `TEST(DiagnosticsModuleTest, ClearEventsResetsCount)` — clear, verify empty
  11. `TEST(DiagnosticsModuleTest, QueryEventsTimeFilter)` — query with from/to bounds
  12. `TEST(DiagnosticsModuleTest, QueryEventsLimitFilter)` — query with limit < available
  13. `TEST(DiagnosticsModuleTest, QueryEventsEmptyRange)` — no events in range → 0 returned
  14. `TEST(DiagnosticsModuleTest, EventPayload24BitUptime)` — test max 24-bit uptime encoding

### 3.2 `test_ride_persistence.cpp` — RidePersistence Tests (≥ 8 tests)

- [ ] Include pattern:
  ```cpp
  #include <gtest/gtest.h>
  #include "echenon/RideStore.h"
  #include "echenon/RideStore.cpp"
  #include "echenon/RidePersistence.h"
  #include "echenon/RidePersistence.cpp"
  ```

- [ ] Test cases (minimum):
  1. `TEST(RidePersistenceTest, SaveAndLoadRide)` — round-trip: save ride, load, verify header + records
  2. `TEST(RidePersistenceTest, DeleteRide)` — save, delete, verify not found
  3. `TEST(RidePersistenceTest, StoredRidesList)` — save 3 rides, verify list
  4. `TEST(RidePersistenceTest, OldestFirstEviction)` — save rides exceeding cap, verify oldest evicted
  5. `TEST(RidePersistenceTest, CapEnforcement)` — set small cap, verify eviction triggers
  6. `TEST(RidePersistenceTest, DisabledDoesNotSave)` — `setEnabled(false)`, verify saveRide is no-op or returns false
  7. `TEST(RidePersistenceTest, BeginScansExistingFiles)` — pre-populate filesystem, call begin(), verify index
  8. `TEST(RidePersistenceTest, ClearRemovesAll)` — save, clear, verify empty

### 3.3 `test_diagnostics_routes.cpp` — DiagnosticsRoutes Tests (≥ 5 tests)

- [ ] Test the route handler logic WITHOUT PsychicHttp dependencies.
  Follow the `SessionOverlay` pattern: test the JSON serialization and
  event-type string conversion helpers directly.

- [ ] Include pattern:
  ```cpp
  #include <gtest/gtest.h>
  #include "echenon/DiagnosticsModule.h"
  #include "echenon/DiagnosticsModule.cpp"
  #include "echenon/RidePersistence.h"
  #include "echenon/RidePersistence.cpp"
  #include "echenon/DiagnosticsRoutes.h"
  #include "echenon/DiagnosticsRoutes.cpp"
  ```

- [ ] Test cases (minimum):
  1. `TEST(DiagnosticsRoutesTest, EventTypeToString)` — all 6 event types map correctly
  2. `TEST(DiagnosticsRoutesTest, BootPayloadDecode)` — decode boot event payload to JSON fields
  3. `TEST(DiagnosticsRoutesTest, SessionEndPayloadDecode)` — decode session_end payload
  4. `TEST(DiagnosticsRoutesTest, RunawayPayloadDecode)` — decode runaway duration
  5. `TEST(DiagnosticsRoutesTest, FirmwareChangePayloadDecode)` — decode version string

### 3.4 Test Quality Requirements

- [ ] Each test is self-contained: creates its own module/filesystem instances.
- [ ] No test depends on another test's side effects.
- [ ] Use `ASSERT_*` for preconditions, `EXPECT_*` for assertions.
- [ ] Test boundary conditions: empty buffer, full buffer, max values (0xFFFF,
  0xFFFFFF), zero values.
- [ ] Total: ≥ 25 `TEST()` cases across all three files.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — ALL tests (existing + new) must pass.
- After verification, run `pa wo verify WO-14.5`.
- Surface any concerns in the Completion Report.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
