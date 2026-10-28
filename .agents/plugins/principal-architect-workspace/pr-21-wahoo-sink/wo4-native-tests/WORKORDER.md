---
schema_version: 1
wo_id: WO-21.4
pr_id: PR-21
title: Native Unit Tests
complexity: standard
status: COMPLETED
allowed_files:
- test/test_native/test_wahoo_uploader.cpp
forbidden_files:
- src/**
- web/**
- docs/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 792702da23e31c727bd1b23c8f1fb1500a1b00e330564b37216e2c3105ac0523
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_df5b94c49242
  commit_sha: d079e3309a4d09c0bc12c5e477c28c936df54b28
  files_modified:
  - test/test_native/test_wahoo_uploader.cpp
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.518 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.4 — Native Unit Tests

## 1. Role & Objective

Author a comprehensive native unit test suite for `WahooUploader` and its integration within
`UploadQueue` in `test/test_native/test_wahoo_uploader.cpp`.

**Deliverables:**
- `MockWahooHttpClient` implementing `IHttpClient`.
- Deterministic test cases validating:
  1. `BuildAuthorizeUrlAndValidateSingleUseStateNonce`: Nonce length, encoding, and single-use consumption.
  2. `StateNonceTtlExpiryAndMillisWraparound`: State rejection past 10-minute TTL and signed 32-bit `millis()` wraparound.
  3. `RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt`: Write order verification in mock spy secret store.
  4. `RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens`: Atomic abort on NVS write failure.
  5. `RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor`: Redactor integration and saturation safety.
  6. `PostUploadMultipartFramingAndStatusResponses`: Framing check (`workout[workout_type_id]=1`, `.tcx.gz` filename and body), and status mapping (201/200 -> SUCCESS, 401 -> PERMANENT_ERROR, 429 -> RETRYABLE_ERROR, 500 -> RETRYABLE_ERROR).
  7. `PollUploadStatusReturnsSuccessImmediately`: Immediate synchronous completion.
  8. `ThreeSinkQueueProgression`: `UploadQueue` with Strava, Intervals.icu, and Wahoo registered simultaneously: fan-out produces 3 jobs, sequential execution, isolated sink failure, and single `.tcx.gz` deletion only after all three terminate.

## 2. Strict Boundary Box & Constraints

1. **Unity Build Symbol Scoping:** `SessionOverlay.cpp` already includes `WahooUploader.cpp` under `#ifdef UNIT_TEST` (via `test_session_manager.o`). Therefore, `test_wahoo_uploader.cpp` MUST only `#include "echenon/WahooUploader.h"` and NOT `#include "echenon/WahooUploader.cpp"`, preventing duplicate symbol linker errors.
2. **Deterministic Tests:** No network calls or real hardware access. All HTTP I/O and SecretStore operations must be intercepted by mock/spy classes.
3. **Execution Floor:** All 261 existing tests PLUS new Wahoo tests must pass in `pio test -e native --verbose`.

**Forbidden:** No edits outside `test/test_native/test_wahoo_uploader.cpp`.

## 3. Line-Item Instructions

- [ ] Create `test/test_native/test_wahoo_uploader.cpp`:
  - Include GoogleTest `<gtest/gtest.h>`.
  - Include headers: `echenon/IUploader.h`, `echenon/StravaUploader.h`, `echenon/IntervalsIcuUploader.h`, `echenon/WahooUploader.h`, `echenon/UploadQueue.h`.
  - Implement `OrderedSpySecretStore` recording chronological writes/deletes and supporting simulated failures.
  - Implement `MockWahooHttpClient` capturing form POSTs, multipart requests, and GET requests.
  - Author test `BuildAuthorizeUrlAndValidateSingleUseStateNonce`.
  - Author test `StateNonceTtlExpiryAndMillisWraparound`.
  - Author test `RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt`.
  - Author test `RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens`.
  - Author test `RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor`.
  - Author test `PostUploadMultipartFramingAndStatusResponses`.
  - Author test `PollUploadStatusReturnsSuccessImmediately`.
  - Author test `ThreeSinkQueueProgression` with mock `RideStore`, mock filesystem, and all 3 sinks active in `UploadQueue`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` to verify all test cases pass.
- Run `pa wo verify WO-21.4` to stamp cryptographic receipt.

## Completion Report

_Pending execution._
