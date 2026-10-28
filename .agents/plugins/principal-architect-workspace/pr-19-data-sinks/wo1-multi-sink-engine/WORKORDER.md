---
schema_version: 1
wo_id: WO-19.1
pr_id: PR-19
title: Multi-Sink UploadQueue Engine
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/UploadQueue.h
- src/echenon/UploadQueue.cpp
- src/echenon/IUploader.h
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 7ff73623be7bc127a72bc56c33d637cc8ce5e41879bc9b51e279c4ed2d057cd0
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_9d01f6b23d1e
  commit_sha: 8811905812b403bd236bf1eb2609aab2bca9fb51
  files_modified:
  - src/echenon/IUploader.h
  - src/echenon/UploadQueue.cpp
  - src/echenon/UploadQueue.h
  verification_summary: 'pio test -e native --verbose: ================ 234 test cases:
    234 succeeded in 00:00:00.554 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-19.1 — Multi-Sink UploadQueue Engine

## 1. Role & Objective

Refactor `UploadQueue` from its current single-`_uploader` pointer design to a
multi-sink registry capable of dispatching upload jobs across up to 4
`IUploader` implementations.

**Key Deliverables:**
- `registerSink(IUploader*)` API with `MAX_SINKS = 4` static array (no heap).
- `UploadJob` extended with `service_id` field creating `(session_id, service_id)` composite key.
- `enqueueSession()` fans out jobs to all registered, configured, and enabled sinks.
- Shared `.tcx.gz` lifecycle: generate once, delete when all sink jobs for that session terminate.
- JSON serialization with `service_id`, backward compat defaulting to `"strava"`.
- Sequential sink execution in registration order, one network stage at a time.

## 2. Strict Boundary Box & Constraints

1. **BLE Coexistence (ADR-11):** Every network stage must remain inside `BLEManager::ScanInhibit`. Only ONE sink does TLS I/O at a time.
2. **Zero Heap std::function (GRAVE-05):** No new `std::function` members.
3. **PSRAM Memory Safety:** `.tcx.gz` generation bounded to 4 KB scratch.
4. **Backward Compat:** `setUploader()`/`getUploader()` remain as convenience wrappers. `service_id` defaults to `"strava"` for legacy JSON.
5. **MAX_QUEUE_ENTRIES = 5** total jobs across all sinks.
6. **Single `.tcx.gz` per session:** Skip generation if file already exists from prior sink job.

**Forbidden:** No new `std::function` members. No `StravaUploader.*` edits. No `web/` or `test/` edits.

## 3. Line-Item Instructions

- [ ] Add `virtual bool isEnabled() const { return true; }` to `IUploader`.
- [ ] Replace `IUploader* _uploader` with `IUploader* _sinks[MAX_SINKS]` + `size_t _sinkCount`.
- [ ] Add `registerSink()`, `getSinkCount()`, `getSink()`, `findSinkById()`.
- [ ] Shim `setUploader()` → `registerSink()`, `getUploader()` → `_sinks[0]`.
- [ ] Extend `UploadJob` with `std::string service_id;`, `UploadEvent` with `char service_id[16]`.
- [ ] Add `findJob(session_id, service_id)` overloads, `allSinkJobsTerminal()`.
- [ ] Refactor `enqueueSession()` for multi-sink fan-out.
- [ ] Refactor `stepQueue()` to look up sink per job, skip TCX gen if file exists, defer `.tcx.gz` cleanup until all sink jobs terminal.
- [ ] Update JSON serialization with `service_id` (v1 compat: default `"strava"`).
- [ ] Rename preconditions from Strava-specific to generic multi-sink.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.1` → `pio test -e native --verbose`. All existing tests must pass.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
