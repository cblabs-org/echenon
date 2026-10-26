---
schema_version: 1
wo_id: WO-06.3
pr_id: PR-06
title: IUploader & Split-Stage UploadQueue IDataSource State Machine
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/IUploader.h
- src/echenon/UploadQueue.h
- src/echenon/UploadQueue.cpp
- test/test_native/test_upload_queue.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: a87c8dd6edf21092832a5148ad080fa190b3e24674027ea7ad1f48f639384c9f
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_6b0877f52aa9
  commit_sha: 4e988fe56fa2ad6aa18699427ebbd70ffa36a990
  files_modified:
  - src/echenon/IUploader.h
  - src/echenon/UploadQueue.cpp
  - src/echenon/UploadQueue.h
  - test/test_native/test_upload_queue.cpp
  verification_summary: 'pio test -e native: ================ 190 test cases: 190
    succeeded in 00:00:03.136 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-06.3: IUploader & Split-Stage UploadQueue IDataSource State Machine

## 1. Objective & Scope

Implement the `IUploader` capability interface (`src/echenon/IUploader.h`) and the split-stage `UploadQueue` (`src/echenon/UploadQueue.h` / `.cpp`) implementing `IDataSource`, `IConfigurable`, and `IPreconditionContributor` (`SPEC.md` §10.1–§10.2, §10.6).

## 2. Detailed Requirements

### 2.1 `IUploader` Capability Interface (`src/echenon/IUploader.h`, `SPEC.md` §10.2)
Define a clean, mockable, capability-only interface for cloud activity uploaders (zero raw socket ownership inside `IUploader` implementations; all HTTP execution flows through an injected `IHttpTransport` or `buildRequest` / `handleResponse` contract):
- `enum class UploadStageResult { SUCCESS, RETRYABLE_ERROR, PERMANENT_ERROR, IN_PROGRESS };`
- `virtual const char* serviceId() const = 0;` (`"strava"`)
- `virtual bool isConfigured() const = 0;`
- `virtual bool isAuthenticated() const = 0;`
- `virtual bool needsTokenRefresh(uint32_t now_epoch_s) const = 0;`
- `virtual UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) = 0;`
- `virtual UploadStageResult postUpload(uint32_t session_id, const std::string& tcx_gz_path, uint32_t timeout_ms, std::string& out_upload_id, std::string& err_msg) = 0;`
- `virtual UploadStageResult pollUploadStatus(const std::string& upload_id, uint32_t timeout_ms, std::string& out_activity_id, std::string& err_msg) = 0;`

### 2.2 Split-Stage `UploadQueue` (`src/echenon/UploadQueue.h` & `.cpp`)
1. **Framework Interfaces (`SPEC.md` §10.2):**
   - Implements `IDataSource` (when building with framework headers; provide clean standalone/mockable methods so native unit tests exercise the exact state machine), `IConfigurable` (`"echenon_upload"`), and `IPreconditionContributor`.
2. **Persistent Queue (`MAX_QUEUE_ENTRIES = 5`):**
   - Each `UploadJob` tracks: `session_id`, `UploadStage` (`STAGE_GENERATE_TCX_GZ`, `STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`, `STAGE_COMPLETE`, `STAGE_FAILED`), `attempt_count` (max `5`), `next_attempt_ms`, `remote_upload_id`, `remote_activity_id`, and `last_error`.
   - Persists queue state to `/upload_queue.json` (or `/rides/upload_queue.json`) across reboots.
   - `bool discardSession(uint32_t session_id)`: removes a queued/failed session from the queue and deletes its pending `.tcx.gz` / `.bin` artifacts (`SPEC.md` §11.3/§11.5).
3. **Strict Single-Stage Execution per `fetchUpdate()` (`15 s` `DeadlineToken` Budget):**
   - All `millis()` comparisons (`next_attempt_ms`, backoff timers) MUST use wraparound-safe signed `(int32_t)(now_ms - target_ms) >= 0` arithmetic (`PR-05` `WO-05.4` lesson).
   - Each call to `stepQueue(uint32_t remaining_budget_ms, uint32_t now_ms)` (invoked by `fetchUpdate(const FetchContext& ctx)`) executes **at most ONE stage** of the front ready job:
     1. **`STAGE_GENERATE_TCX_GZ`:** Calls `TCXGenerator` (passing a `DeadlineCheckFn` bound to `ctx.deadline`) to stream `/rides/ride_<id>.bin` into `/rides/ride_<id>.tcx.gz` (no network I/O). Advances to `STAGE_REFRESH_TOKEN` (if `uploader->needsTokenRefresh()`) or directly to `STAGE_POST_UPLOAD`.
     2. **`STAGE_REFRESH_TOKEN`:** Acquires `BLEManager::ScanInhibit` (guarded under `#ifdef WITH_BLE`), calls `uploader->refreshToken(min(remaining_budget_ms, 8000))`, and advances to `STAGE_POST_UPLOAD` on `SUCCESS`.
     3. **`STAGE_POST_UPLOAD`:** Acquires `BLEManager::ScanInhibit` (`#ifdef WITH_BLE`), calls `uploader->postUpload(..., min(remaining_budget_ms, 10000))`, stores `remote_upload_id`, and advances to `STAGE_POLL_STATUS` (scheduled `+3000 ms` later).
     4. **`STAGE_POLL_STATUS`:** Acquires `BLEManager::ScanInhibit` (`#ifdef WITH_BLE`), polls `uploader->pollUploadStatus(..., min(remaining_budget_ms, 8000))`. On `SUCCESS`, marks job `STAGE_COMPLETE`, deletes temporary `.tcx.gz` file, and publishes `"echenon.upload"` event on `EventBus`.
   - **Retry & Precondition Handling:**
     - On `RETRYABLE_ERROR`, increments `attempt_count` and schedules exponential backoff (`30 s`, `60 s`, `120 s`, `300 s`). After `5` failed attempts (or `PERMANENT_ERROR`), transitions to `STAGE_FAILED`.
     - Contributes preconditions (`"strava_configured"`, `"ntp_synced"`) via `IPreconditionContributor`.

### 2.3 Native Unit Tests (`test/test_native/test_upload_queue.cpp`)
- Test split-stage progression using a mock `IUploader`: verify that 4 separate `stepQueue()` invocations advance a job through `GENERATE_TCX_GZ -> REFRESH_TOKEN -> POST_UPLOAD -> POLL_STATUS -> COMPLETE` (never running two network stages in a single call).
- Test token refresh skip when `needsTokenRefresh() == false`.
- Test exponential backoff and `STAGE_FAILED` after 5 retryable failures or 1 permanent failure.
- Test `discardSession(session_id)` removal and queue capacity (`5` sessions) + JSON serialization/deserialization round-trip.

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus `test_upload_queue.cpp`.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
