---
schema_version: 1
wo_id: WO-14.3
pr_id: PR-14
title: Query REST API
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/DiagnosticsRoutes.h
- src/echenon/DiagnosticsRoutes.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 3d7a4478317ca44c41ccf3c90afc2767e7873e4cd587b0daeb90ff98f832c538
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_587713c5907e
  commit_sha: bf25fea79673678681878e6b318521892d33eeb8
  files_modified:
  - src/echenon/DiagnosticsRoutes.cpp
  - src/echenon/DiagnosticsRoutes.h
  verification_summary: 'pio test -e native --verbose: ================ 199 test cases:
    199 succeeded in 00:00:00.614 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.3 — Query REST API

## 1. Role & Objective

You are an expert senior embedded C++ engineer. Implement the diagnostics and
run-data query REST API endpoints as a standalone route-registration helper
class. These endpoints will be registered by `EchenonModule::registerRoutes()`
in WO-14.4.

Read the `framework-expert` skill at
`/Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md`.
Relevant references:
- `systems/web-portal/routes.md` for REST route registration patterns
- `systems/web-portal/dram-tuning.md` for chunked streaming rules
- `security/` for authentication requirements

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** touch `EchenonModule`, `main.cpp`, web HTML/JS files, or test files.
- **No heap `std::function`** (GRAVE-05).
- **No full-buffer allocation in API responses** — ALL responses streamed in
  ≤ 4 KB chunks.
- **Authentication required** on all endpoints per SPEC §11.4.
- The route helper class takes dependency pointers via constructor injection —
  it does NOT own `DiagnosticsModule`, `RidePersistence`, or `AuthManager`.

## 3. Line-Item Instructions

### 3.1 DiagnosticsRoutes Class

- [ ] Create `src/echenon/DiagnosticsRoutes.h` / `.cpp`:
  ```cpp
  namespace echenon {

  // Forward declarations
  class DiagnosticsModule;
  class RidePersistence;

  class DiagnosticsRoutes {
  public:
      DiagnosticsRoutes(DiagnosticsModule& diag,
                        RidePersistence& persistence);

      /// Register all diagnostic routes on the given server.
      /// authManager is used for authentication checks.
      void registerRoutes(PsychicHttpServer& server, AuthManager& auth);

  private:
      DiagnosticsModule& _diag;
      RidePersistence&   _persistence;
  };
  } // namespace echenon
  ```

### 3.2 `GET /api/echenon/events`

- [ ] Returns JSON array of diagnostic events.
- [ ] Query parameters:
  - `from` (uint32_t epoch, optional): inclusive lower bound
  - `to` (uint32_t epoch, optional): inclusive upper bound
  - `limit` (size_t, optional, default 100, max 100): max events returned
- [ ] Response format (chunked JSON streaming):
  ```json
  {
    "events": [
      {
        "epoch": 1698000000,
        "type": "BOOT",
        "type_id": 0,
        "payload": { "uptime_s": 12345 }
      }
    ],
    "count": 1,
    "total": 42
  }
  ```
- [ ] Payload field varies by `type`:
  - `BOOT`: `{ "uptime_s": <uint32_t> }`
  - `SESSION_START`: `{ "session_id": <uint32_t> }`
  - `SESSION_END`: `{ "duration_s": <uint16_t>, "record_count_approx": <uint16_t> }`
  - `RUNAWAY_ALERT`: `{ "duration_ms": <uint32_t> }`
  - `CALIBRATION_DONE`: `{ "calibration_type": "initial"|"recalibration" }`
  - `FIRMWARE_CHANGE`: `{ "version": "<major>.<minor>.<patch>" }`
- [ ] Stream the JSON response using `PsychicStreamResponse` or chunked
  transfer encoding. Do NOT build the entire JSON in memory.
- [ ] Authentication: require valid session/token via `AuthManager`.

### 3.3 `GET /api/echenon/runs`

- [ ] Returns JSON array of stored rides with summary metadata.
- [ ] Response format:
  ```json
  {
    "runs": [
      {
        "session_id": 12345,
        "start_epoch": 1698000000,
        "file_size_bytes": 28672,
        "record_count": 3600
      }
    ],
    "count": 3,
    "total_size_kb": 84
  }
  ```
- [ ] Authentication required.

### 3.4 `GET /api/echenon/runs/<id>.csv.gz`

- [ ] Stream a stored ride as gzipped CSV.
- [ ] **On-the-fly conversion**: Load the binary `.ride.gz` file, decompress,
  and re-encode as CSV while streaming. The CSV format:
  ```
  elapsed_s,power_w,cadence_rpm,hr_bpm,resistance,flags
  0,150,80,120,128,3
  1,155,82,121,129,3
  ```
- [ ] Streaming in ≤ 4 KB chunks. Use `PsychicStreamResponse` with
  `Content-Type: application/gzip` and
  `Content-Disposition: attachment; filename="ride_<id>.csv.gz"`.
- [ ] If the session_id is not found, return HTTP 404 with JSON error.
- [ ] Authentication required.

### 3.5 `DELETE /api/echenon/runs/<id>`

- [ ] Delete a stored ride by session_id.
- [ ] Returns `{ "ok": true }` on success, HTTP 404 if not found.
- [ ] Authentication required.

### 3.6 Event Type String Conversion

- [ ] Implement a helper `const char* diagEventTypeToString(DiagEventType type)`
  returning canonical uppercase strings: `"BOOT"`, `"SESSION_START"`,
  `"SESSION_END"`, `"RUNAWAY_ALERT"`, `"CALIBRATION_DONE"`, `"FIRMWARE_CHANGE"`.

### 3.7 Streaming Implementation Notes

- [ ] All responses must use chunked transfer encoding or `PsychicStreamResponse`.
- [ ] The CSV conversion and gzip re-compression happen in a streaming fashion:
  read binary records in batches, format CSV lines, compress with miniz
  streaming deflation, write chunks to the response.
- [ ] Maximum memory for any single API response: ≤ 4 KB working buffer.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all existing tests must pass.
- After verification, run `pa wo verify WO-14.3`.
- Surface any concerns in the Completion Report.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
