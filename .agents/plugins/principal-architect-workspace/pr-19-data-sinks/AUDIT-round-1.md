---
schema_version: 1
pr_id: PR-19
github_pr: 17
audit_round: 1
head_sha: 16c0c7aa429c383090a44ac5e2fe58a92afe82a7
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-19 round=1 head_sha=16c0c7aa429c383090a44ac5e2fe58a92afe82a7 verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)

# Cold-Firewall Audit Report: PR-19 Round 1


## Executive Summary
Evaluation of PR-19 (`feat/pr-19-data-sinks`) at commit `16c0c7aa429c383090a44ac5e2fe58a92afe82a7` against base `main` (`cfff3b3`), the locked Part I PR Charter in `PROJECT.md`, and `SPEC.md`.
The implementation delivers the pluggable multi-sink data upload architecture, the Intervals.icu API-key reference sink, the Stored Rides portal management pill in the Echenon tab, modular per-sink configuration pills under the Apps tab, and complete hardware soak verification on the Waveshare ESP32-S3-Nano platform.

---


## 1. Specification Alignment (spec_alignment) — PASS
- **Multi-Sink UploadQueue Engine (§10.1–§10.2):**
  - Evolved `UploadQueue` from a single-uploader architecture to a multi-sink registry supporting up to `MAX_SINKS = 4` registered `IUploader` implementations via `registerSink(IUploader*)`.
  - Extended `UploadJob` with a `service_id` field forming a composite `(session_id, service_id)` key.
  - Implemented automatic fan-out in `enqueueSession(session_id)` across all configured and enabled sinks.
  - Maintained strict sequential execution: at most one sink performs TLS I/O at any time, bounded by inter-stage yielding and gated by `BLEManager::ScanInhibit("upload_queue")`.
  - Shared `.tcx.gz` lifecycle: payload is generated exactly once per session in `STAGE_GENERATE_TCX_GZ` and retained until all registered sink jobs for that session achieve terminal state (`STAGE_COMPLETE` or `STAGE_FAILED`).
  - Added per-sink auto-sync toggle (`isEnabled()`) allowing individual sinks to be skipped during automatic post-ride fan-out while remaining available for manual retry.
- **Intervals.icu API-Key Sink (§10.3a):**
  - Implemented `IntervalsIcuUploader` conforming to `IUploader` and `IConfigurable` (`"echenon_intervals"`).
  - Implements personal API key authentication using HTTP Basic Auth (`Basic base64(API_KEY:{api_key})`).
  - Synchronous upload behavior: `postUpload()` delivers compressed `.tcx.gz` binary payloads with `Content-Type: application/octet-stream` to `https://intervals.icu/api/v1/athlete/{id}/activities`; `pollUploadStatus()` returns `SUCCESS` immediately with the resulting activity URL.
  - `refreshToken()` is a deterministic no-op returning `SUCCESS`.
  - API key and athlete ID are persisted encrypted in NVS via `KeyManagerSecretStore` (`intervals.api_key`, `intervals.athlete_id`) and registered with `Redactor::registerSecret()`.
- **Stored Rides Portal Pill (§11.2):**
  - Dedicated Stored Rides pill registered under the **Echenon** tab (`/stored_rides.js`, order 30) via `EchenonModule::registerTabs`.
  - Renders recent rides with start timestamp, elapsed duration, average power, distance, and heart rate.
  - Per-sink sync status badges (`COMPLETE`, `PENDING`, `FAILED`, `NONE`) with clickable remote links.
  - Interactive actions for one-click TCX download (`GET /api/session/download?id=<n>`), deletion (`DELETE /api/echenon/runs/<id>`), and per-sink retry / force-push (`POST /api/session/upload` with `{session_id, service_id}`).
- **Connected Apps Tab Restructuring (§11.2, §10.3):**
  - Modular per-sink configuration pills registered under the **Apps** tab: Strava (`/upload_settings.js`, order 10) and Intervals.icu (`/intervals_settings.js`, order 20).
  - Responsive configuration interface with connection status indicators, inline credential forms, test connection triggers, disconnect actions, and auto-sync preference toggles.
- **Specification & Documentation Synchronization (§10.5, §11, docs/data-sinks.md):**
  - Fully articulated 4-tier provider sync taxonomy in SPEC.md §10.5 (Tier A: downstream Strava sync including Xert/Relive/VeloViewer; Tier B: direct on-device uploaders Strava & Intervals.icu; Tier C: manual/future export for Garmin Connect and Wahoo Cloud; Tier D: BLE FTMS real-time clients including Zwift and Xert EBC).
  - Authored comprehensive multi-sink design specification in `docs/data-sinks.md`.

---


## 2. Architectural Invariants (invariants) — PASS
- **Radio & BLE Coexistence (ADR-11):**
  - TLS operations during network stages (`STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`) run under an active `BLEManager::ScanInhibit("upload_queue")` guard.
  - Sequential single-sink execution prevents concurrent TLS handshakes and ensures zero dropped packets on BLE FTMS (`0x1826`) and HRM (`0x180D`) streams during background uploads.
- **Zero Heap std::function (GRAVE-05):**
  - Sinks are maintained in a fixed static pointer array (`IUploader* _sinks[MAX_SINKS]{}`).
  - Route handlers and EventBus subscriptions utilize non-capturing lambdas and static callback functions with explicit `user_ctx`.
  - Precondition contributor functions (`evalSinkConfigured`, `describeSinkConfigured`, `evalNtpSynced`, `describeNtpSynced`) are static members with `void* ctx` passing.
- **PSRAM Memory Safety & Stack Frame Budget:**
  - Streaming TCX generation and gzip compression occur strictly within PSRAM allocations (`StreamingGzipEncoder`), maintaining the < 4 KB stack frame budget for `PsychicHttpServer` worker tasks.
  - Shared `.tcx.gz` prevents redundant compression passes and bounds LittleFS storage consumption.
- **Secret Storage & Redactor Integrity:**
  - All sensitive credentials (Strava OAuth client secret/refresh token, Intervals.icu API key) are stored encrypted in NVS via `KeyManager`.
  - Secrets are registered with `Redactor::registerSecret()` on load, save, and update, preventing credential leakage in debug logs or serial streams.
- **Embedded Target Resource Footprint:**
  - Hardware target compilation (`ex3-waveshare-nano`) verified:
    - RAM Utilization: 58,524 bytes (17.9% of 327,680 bytes).
    - Flash Utilization: 1,714,863 bytes (27.3% of 6,291,456 bytes).
    - Clean linkage and factory binary generation (`firmware.factory.bin`).

---


## 3. Edge Cases & Boundary Conditions (edge_cases) — PASS
- **Queue Saturation & Eviction:**
  - `UploadQueue` bounds memory consumption to `MAX_QUEUE_ENTRIES = 5`.
  - When capacity is reached, deterministic eviction prioritizes completed jobs first, followed by failed jobs, ensuring active in-flight uploads are never displaced.
- **HTTP Error Code Taxonomy & Retry Logic:**
  - HTTP 200/201 maps to `SUCCESS`.
  - HTTP 401 (invalid credentials / revoked token) maps to `PERMANENT_ERROR`, preventing futile retries.
  - HTTP 429 (rate limiting) and 5xx (server errors) map to `RETRYABLE_ERROR`, triggering exponential backoff (`computeBackoffMs`) capped at 5 attempts.
- **Sink Failure Isolation:**
  - Failure or rate-limiting of one sink does not disrupt or abort uploads for other configured sinks.
  - Verified by `UploadQueueTest.SinkFailureIsolationOneFailDoesNotBlockOther`.
- **Shared Artifact Deletion Guard:**
  - `allSinkJobsTerminal(session_id)` ensures `.tcx.gz` temporary files are preserved until all participating sinks have either completed or permanently failed.
- **Legacy Serialization Compatibility:**
  - LittleFS queue deserialization gracefully defaults missing `service_id` keys to `"strava"`, preventing state loss during firmware upgrades.
- **Malformed Input Rejection:**
  - REST handlers (`/api/intervals/credentials`, `/api/session/upload`) strictly validate JSON structure and reject missing or empty parameters with HTTP 400.

---


## 4. Test Quality & Verification Floor (test_quality) — PASS
- **Native Verification Floor:**
  - Executed `pio test -e native --verbose`: 261 / 261 test cases passed 100% across 28 test suites in 0.62 seconds.
- **Intervals.icu Test Suite (`test_intervals_uploader.cpp` — 16 tests):**
  - Verifies credential validation, no-op token refresh, HTTP Basic Auth formatting, upload request construction, status code mapping (200, 401, 429, 500), synchronous poll completion, Redactor registration, RFC 4648 Base64 encoding test vectors, and config serialization round-trip.
- **UploadQueue Multi-Sink Test Suite (`test_upload_queue.cpp` — 16 tests, 11 new):**
  - Exercises multi-sink fan-out, sequential step queue progression, shared `.tcx.gz` lifecycle, composite key lookups, backward compatibility with `setUploader()`, sink failure isolation, JSON serialization with `service_id`, legacy v1 JSON fallback, terminal state semantics, session discard across multiple sinks, and multi-sink precondition evaluation.
- **Hardware Soak Verification (WO-19.8):**
  - Physical execution on Waveshare ESP32-S3-Nano (`/dev/cu.usbmodem101`).
  - Serial boot log inspection confirms LittleFS mount (`/littlefs`, `/config`), module registration, WiFi/mDNS connectivity, dual-sink queue initialization, BLE FTMS advertising, and central connection without brownouts or panics (`docs/measurements/phase3/evidence/pr19-dual-sink-serial.log`, `docs/measurements/phase3/evidence/pr19-hardware-verification.md`).

---


## 5. Scope & Repository Hygiene (scope_hygiene) — PASS
- **Allowed Modules Compliance:**
  - All modified files fall strictly within `charter.allowed_modules`:
    - `src/echenon/`
    - `src/main.cpp`
    - `web/`
    - `test/`
    - `docs/`
    - `SPEC.md`
- **Clean Repository Tree:**
  - `git status --porcelain` is clean with zero untracked artifacts or uncommitted modifications.
- **Sanitization:**
  - Zero hardcoded passwords, personal API keys, athlete credentials, or internal tokens present in repository diff or tests.

---


## Verdict
**APPROVED** — PR-19 satisfies all specification requirements, fulfills all architectural invariants, passes all automated tests and hardware soak verification, and conforms strictly to the locked PR Charter. The PR is ready for graduation.
