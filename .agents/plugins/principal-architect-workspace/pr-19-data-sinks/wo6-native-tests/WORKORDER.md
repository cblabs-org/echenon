---
schema_version: 1
wo_id: WO-19.6
pr_id: PR-19
title: Native Unit Tests
complexity: standard
status: COMPLETED
allowed_files:
- test/test_native/test_upload_queue.cpp
- test/test_native/test_intervals_uploader.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: a44ff5747bfbad150990c6e22baecec96b766bc05e6235ac641dee079d0c72dd
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_74e9923f9126
  commit_sha: ff8982ec0eb68444ec70158b0ac428720e0104a1
  files_modified:
  - test/test_native/test_intervals_uploader.cpp
  - test/test_native/test_upload_queue.cpp
  verification_summary: 'pio test -e native --verbose: ================ 261 test cases:
    261 succeeded in 00:00:00.531 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-19.6 — Native Unit Tests

## 1. Role & Objective

Add comprehensive native unit tests covering the multi-sink `UploadQueue` dispatch
and `IntervalsIcuUploader` request building/response parsing.

## 2. Strict Boundary Box & Constraints

1. Only modify `test/test_native/test_upload_queue.cpp` and create `test/test_native/test_intervals_uploader.cpp`.
2. Follow existing test patterns: `MockUploader`, `InMemoryRideFileSystem`, `InMemorySecretStore`, GTest.
3. Existing tests in `test_upload_queue.cpp` must continue to pass unchanged.

**Forbidden:** No `src/echenon/` edits. No `web/` edits.

## 3. Line-Item Instructions

### 3.1 test_upload_queue.cpp Additions
- [ ] Multi-sink fan-out: register 2 MockUploaders, enqueue session, verify 2 jobs created with different service_ids.
- [ ] Sequential execution: verify only one sink processes per `stepQueue()`.
- [ ] Shared `.tcx.gz` lifecycle: verify TCX generated once, deleted only when all sink jobs terminal.
- [ ] Per-sink job lookup: `findJob(session_id, service_id)` returns correct job.
- [ ] Backward compat: single-uploader via `setUploader()` still works.
- [ ] JSON serialization: `service_id` serialized and deserialized correctly.
- [ ] Legacy v1 JSON: missing `service_id` defaults to `"strava"`.
- [ ] Sink failure isolation: one sink failing doesn't block the other.
- [ ] `allSinkJobsTerminal()` returns true only when all jobs for session are COMPLETE/FAILED.
- [ ] `discardSession()` removes all jobs across sinks.
- [ ] Precondition eval: generic multi-sink precondition check.

### 3.2 test_intervals_uploader.cpp (new)
- [ ] Include `IntervalsIcuUploader.h` and `IntervalsIcuUploader.cpp`.
- [ ] Mock `IHttpClient` for request/response testing.
- [ ] `isConfigured()`: true when both athlete_id and api_key set, false otherwise.
- [ ] `needsTokenRefresh()`: always false.
- [ ] `refreshToken()`: always SUCCESS.
- [ ] `postUpload()` success: verify Basic Auth header format, URL path, content type.
- [ ] `postUpload()` 401: returns PERMANENT_ERROR.
- [ ] `postUpload()` 429: returns RETRYABLE_ERROR.
- [ ] `postUpload()` 500: returns RETRYABLE_ERROR.
- [ ] `pollUploadStatus()`: always SUCCESS.
- [ ] `setApiCredentials()`: persists via ISecretStore, registers with Redactor.
- [ ] `disconnect()`: clears credentials from ISecretStore.
- [ ] Config serialization: `applyConfig()`/`serializeConfig()` round-trip.
- [ ] `base64Encode()`: verify known test vectors.
- [ ] `testConnection()` success: parses athlete name.
- [ ] `testConnection()` failure: returns error message.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.6` → `pio test -e native --verbose`. ALL tests must pass.

## Completion Report

_Pending execution._
