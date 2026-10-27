---
schema_version: 1
pr_id: PR-14
github_pr: 11
audit_round: 2
head_sha: 5034043cf6d8c18cdef8cf5c3f78712bf35ccc98
auditor_harness: antigravity
verdict: APPROVED
findings:
- id: NON-BLOCKING-1
  axis: spec_alignment
  file_path: docs/measurements/pr14-hw-verification.md
  line_range: L5-L7
  summary: Hardware verification was performed as a 5-minute smoke test (305s) rather
    than the 60-minute ride mandated by Charter §1.5, with full endurance testing
    deferred per owner agreement.
  remediation: Execute and document a full 60-minute endurance ride on the physical
    rig during the next scheduled training session or release milestone.
---

<!-- pa:audit pr_id=PR-14 round=2 head_sha=5034043cf6d8c18cdef8cf5c3f78712bf35ccc98 verdict=APPROVED blocking=0 major=0 minor=1 -->

**Audit Verdict:** APPROVED (0 blocking findings, 1 non-blocking finding)

# PR-14 Cold-Firewall Audit (Round 2)

**Verdict:** APPROVED (0 blocking findings, 1 non-blocking finding)  
**Evaluated Branch HEAD:** `5034043cf6d8c18cdef8cf5c3f78712bf35ccc98`  
**Target:** `feat/pr-14-diagnostics` against `main` (`03be4c1`)  

---


## 1. Executive Summary & Audit Scorecard

PR-14 delivers the complete architecture for Diagnostics & Run-Data Persistence across the embedded C++ firmware and web portal interfaces:
- **`DiagnosticsModule`**: Compact, PSRAM-backed 800-byte ring buffer recording structured 8-byte diagnostic events (boot, session lifecycle, runaway alerts with duration, calibration events, firmware version changes) with UTC epoch timestamps.
- **`RidePersistence`**: Gzip-compressed LittleFS ride persistence under `/rides/persist/` with PSRAM-allocated streaming encoder, configurable storage cap (`diag_ride_cap_kb`, default 512 KB, clamped 64..2048 KB), and oldest-first FIFO eviction.
- **`DiagnosticsRoutes`**: REST endpoints (`/api/echenon/events`, `/api/echenon/runs`, `/api/echenon/runs/<id>.csv.gz`, and `DELETE /api/echenon/runs/<id>`) streaming bounded JSON and CSV chunks with authentication per SPEC §11.4.
- **Portal & Ride Telemetry Integration**: Portal diagnostics tab event viewer, live runaway polling, and a visual pulsating runaway warning banner on the `/ride` page.
- **Verification Floor**: 233 native unit tests passing in 0.66s; 305-second interactive smoke ride verified on physical ESP32-S3 hardware (`ex3-waveshare-nano`).

### Remediation Status from Round 1
All three blocking findings from Round 1 (AUDIT-round-1.md) have been verified as fully resolved:
1. **BLOCKING-1 (Resolved in WO-14.8, commit `27df6ec`):** `web/diagnostics.js` now reads `ev.payload || ev.details`, resolves `record_count_approx || record_count`, handles both string (`"initial"`, `"recalibration"`) and numeric calibration types, and renders `ev.epoch || ev.epoch_s` timestamps correctly without blank columns.
2. **BLOCKING-2 (Resolved in WO-14.8, commit `27df6ec`):** `_runawayDetector.isRunaway()` boolean is exposed unconditionally on `/api/echenon/live` in `src/echenon/EchenonModule.cpp`, and `web/diagnostics.js` polls this production-safe endpoint every 5 seconds.
3. **BLOCKING-3 (Resolved in WO-14.8, commit `27df6ec`):** A visual pulsing warning banner (`#runaway-banner`) was added to `web/ride.html` and wired to `d.runaway` in `web/ride.js` (`tl(d)`), satisfying Charter §1.4.
4. **NON-BLOCKING-1 (Carried forward):** Hardware verification documented in `docs/measurements/pr14-hw-verification.md` verified 305s on hardware; 60-minute endurance ride deferred to regular training schedule per owner agreement.

| Audit Axis | Status | Key Observations |
|---|---|---|
| **spec_alignment** | **PASS** | Implementation fulfills all requirements of PR-14 Charter §1.1–§1.4 and SPEC §13/§14. Event schema and runaway alerts are fully aligned between firmware and frontend. |
| **invariants** | **PASS** | Complies strictly with GRAVE-05 (static free function `onSessionEndPersist`), PSRAM event buffer budget (800 B ≤ 4 KB), PSRAM `StreamingGzipEncoder` (~5.2 KB), streaming HTTP chunks (≤ 2048 B), LittleFS block writes, and CSS tokens (`--card-bg`, `--text-muted`). |
| **edge_cases** | **PASS** | Ring buffer circular wrap-around preserves chronological order; out-of-bounds query parameters clamped; storage cap clamping [64..2048 KB]; NTP epoch validity thresholding (1.7B) safely filters unsynced clocks. |
| **test_quality** | **PASS** | 233 native unit tests passing in 0.66s covering ring buffer wrap, query filters, cap eviction, and gzip round-trips with rigorous assertions. |
| **scope_hygiene** | **PASS** | All modified code files reside within `charter.allowed_modules`. Clean git working tree; zero stray files or personal/internal identifiers. |

---


## 2. Normative Axis Evaluation

### 2.1 Specification Alignment
- **PR-14 Charter §1.1 & SPEC §13:** `DiagnosticsModule` implements the 8-byte packed `DiagnosticEvent` structure with `EVENT_LOG_CAPACITY = 100` (800 bytes in PSRAM). Event recording methods for boot, session start/end, runaway, calibration, and firmware change match the spec.
- **PR-14 Charter §1.2 & SPEC §14:** `RidePersistence` stores completed rides in LittleFS under `/rides/persist/` with gzip compression via PSRAM-allocated `StreamingGzipEncoder` (5.2 KB PSRAM, zero DRAM pressure). Oldest-first FIFO eviction based on `start_epoch_s` respects the configurable cap (`diag_ride_cap_kb`, default 512 KB, clamped 64..2048 KB).
- **PR-14 Charter §1.3 & SPEC §13.4:** REST endpoints `GET /api/echenon/events`, `GET /api/echenon/runs`, `GET /api/echenon/runs/<id>.csv.gz`, and `DELETE /api/echenon/runs/<id>` are registered with authentication per SPEC §11.4. Chunked streaming guarantees memory safety during downloads.
- **PR-14 Charter §1.4:** Runaway alert surfacing is complete across all layers:
  - In portal diagnostics tab: warning banner driven by polling `/api/echenon/live`.
  - In `/ride` UI: pulsing warning banner toggled by WebSocket / HTTP telemetry (`d.runaway`).
  - In event log: runaway alert edge transitions recorded with duration.

### 2.2 Architectural Invariants
- **GRAVE-05 (No Heap `std::function`):** `onSessionEndPersist` in `src/main.cpp` is implemented as a static free function pointer passed to `EventBus::subscribe`.
- **PSRAM Buffer Budget:** Ring buffer requires 800 bytes in PSRAM (budget ≤ 4 KB). `StreamingGzipEncoder` allocates memory exclusively in PSRAM (`MALLOC_CAP_SPIRAM`), keeping DRAM pressure at zero.
- **Chunked HTTP Responses:** CSV streaming buffers chunks to 2048 bytes; JSON event and run listing stream in chunks ≤ 512 bytes, eliminating heap exhaustion in the `httpd` task.
- **Flash Wear Mitigation:** Completed rides are written as block-appends on session completion; no rewrite-in-place; eviction deletes whole oldest files.
- **CSS Design Tokens:** `web/diagnostics.js` adheres strictly to system tokens `var(--card-bg)`, `var(--text-muted)`, `var(--border)`, and `var(--primary)`. No forbidden tokens (`--bg-card`, `--text-secondary`) are present.
- **SignalLogger Isolation:** `SignalLogger` remains isolated behind `WITH_SIGNAL_LOGGER` without coupling to `DiagnosticsModule`.

### 2.3 Edge Cases & Robustness
- **Ring Buffer Circular Wrap:** Preserves strict oldest-first chronological ordering across head wrap boundaries.
- **NTP Epoch Transitions:** `getValidEpoch()` cleanly suppresses un-synced clock values prior to epoch 1,700,000,000.
- **Malformed Query Parameters:** Invalid `from`, `to`, or `limit` parameters safely fall back or clamp to valid ranges.
- **Storage Cap Enforcement:** Handles multiple evictions in a single pass if needed to accommodate large rides.
- **Gzip Integrity:** Full RFC 1952 header, CRC32, and ISIZE validation.

### 2.4 Test Quality & Verification Floor
- 233 native unit tests run and pass in 0.66s.
- `test_diagnostics_module.cpp` (14 tests) verifies lifecycle, encoding, wrapping, query filtering, and capacity clamping.
- `test_diagnostics_routes.cpp` (11 tests) verifies string conversions, payload formatting, and JSON generation.
- `test_ride_persistence.cpp` (8 tests) verifies save/load round-trips, oldest-first FIFO eviction under cap pressure, directory scanning, and deletion.
- Physical bench verification documented in `docs/measurements/pr14-hw-verification.md` on ESP32-S3 hardware.

### 2.5 Scope Hygiene & Allowed Modules
- All modified code files reside within `charter.allowed_modules`:
  `src/echenon/`, `src/main.cpp`, `test/`, `web/`, `docs/`, `SPEC.md`, `platformio.ini`.
- Clean working tree with zero uncommitted changes and no temporary debug artifacts.

---


## 3. Findings & Observations

### Blocking Findings
None. All Round 1 blocking findings (BLOCKING-1, BLOCKING-2, BLOCKING-3) have been remediated and verified.

### Non-Blocking Observations

#### NON-BLOCKING-1: Hardware Verification Endurance Duration Deferred (`spec_alignment`)
- **Location:** `docs/measurements/pr14-hw-verification.md:L5-L7`
- **Context:** Charter §1.5 specified a 60-minute ride verification. A 5-minute smoke ride (305 seconds) was conducted on hardware, with the 60-minute endurance ride deferred per owner agreement.
- **Remediation:** Execute the full 60-minute endurance ride on the physical rig during the next scheduled training session or release milestone to verify long-term thermal and LittleFS wear characteristics.
