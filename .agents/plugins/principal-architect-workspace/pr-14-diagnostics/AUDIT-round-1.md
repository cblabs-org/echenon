---
schema_version: 1
pr_id: PR-14
github_pr: 11
audit_round: 1
head_sha: f5383b67cc60c1a6b119eaf7c3deb0e37a212f8a
auditor_harness: antigravity
verdict: CHANGES_REQUESTED
findings:
- id: BLOCKING-1
  axis: spec_alignment
  file_path: web/diagnostics.js
  line_range: L153-L188
  summary: Diagnostics event viewer JSON field mismatch — diagnostics.js accesses
    ev.epoch_s and ev.details instead of ev.epoch and ev.payload emitted by DiagnosticsRoutes
    and specified in SPEC §13.4, resulting in blank timestamp and details columns.
  remediation: Update web/diagnostics.js to read ev.epoch (with ev.epoch_s fallback),
    ev.payload (with ev.details fallback), and align field accesses for record_count_approx
    and calibration_type string values.
- id: BLOCKING-2
  axis: spec_alignment
  file_path: web/diagnostics.js
  line_range: L211-L229
  summary: Runaway warning banner polls /api/echenon/status, which is compiled only
    under WITH_SIGNAL_LOGGER in ex3-waveshare-nano-diag and 404s in production builds,
    preventing the banner from ever triggering.
  remediation: Add runaway boolean to /api/echenon/live or register an un-gated /api/echenon/status
    endpoint in EchenonModule, and update web/diagnostics.js to poll this production-safe
    endpoint.
- id: BLOCKING-3
  axis: spec_alignment
  file_path: web/ride.js
  line_range: L6-L23
  summary: PR-14 Charter §1.4 requires runaway alert surfacing on the /ride page as
    a visual alert, but web/ride.html and web/ride.js contain no visual element or
    telemetry handling for runaway state.
  remediation: Add a runaway alert UI indicator to web/ride.html and update web/ride.js
    tl(d) to toggle the alert when d.runaway is true.
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

<!-- pa:audit pr_id=PR-14 round=1 head_sha=f5383b67cc60c1a6b119eaf7c3deb0e37a212f8a verdict=CHANGES_REQUESTED blocking=3 major=1 minor=0 -->

**Audit Verdict:** CHANGES_REQUESTED (3 blocking, 1 non-blocking)

# PR-14 Cold-Firewall Audit (Round 1)

**Verdict:** CHANGES_REQUESTED (3 blocking findings, 1 non-blocking finding)  
**Evaluated Branch HEAD:** `f5383b67cc60c1a6b119eaf7c3deb0e37a212f8a`  
**Target:** `feat/pr-14-diagnostics` against `main`  

---


## 1. Executive Summary & Audit Scorecard

PR-14 delivers the core C++ embedded architecture for Diagnostics & Run-Data Persistence:
- A compact, PSRAM-backed ring buffer (`DiagnosticsModule`) recording structured 8-byte events (boot, session, runaway, calibration, firmware change).
- Gzip-compressed LittleFS ride persistence (`RidePersistence`) with FIFO oldest-first cap eviction and directory rebuilding.
- REST query endpoints (`DiagnosticsRoutes`) streaming chunked JSON and CSV responses with authentication per SPEC §11.4.
- 233 passing native unit tests covering ring buffer wrapping, query parameters, LittleFS persistence, cap enforcement, and gzip compression round-trips.

However, three blocking integration defects were identified in the web portal and ride UI layers that prevent the diagnostics event viewer and runaway safety alerts from functioning as required by Charter §1.4 and SPEC §13.4:
1. **Frontend-Backend JSON contract mismatch** in `web/diagnostics.js`: the script queries `ev.epoch_s` and `ev.details` instead of `ev.epoch` and `ev.payload` as specified in SPEC §13.4 and emitted by `DiagnosticsRoutes.cpp`. Consequently, every row in the portal event viewer renders as blank/empty (`Time: —`, `Details: —`).
2. **Dead polling endpoint** in `web/diagnostics.js`: `checkRunaway()` polls `/api/echenon/status`, which is conditionally compiled only behind `WITH_SIGNAL_LOGGER` in the diagnostic build environment. In production builds (`ex3-waveshare-nano`), this endpoint returns 404, permanently hiding the runaway banner.
3. **Missing runaway alert on `/ride`**: Charter §1.4 explicitly mandates surfacing runaway alerts "in the /ride page as a visual alert". No UI element or telemetry handler exists in `web/ride.html` or `web/ride.js` to surface this state.

| Audit Axis | Status | Key Observations |
|---|---|---|
| **spec_alignment** | **FAIL** | Core C++ logic matches SPEC §13/§14, but portal event rendering is broken due to JSON key drift, runaway banner polls a non-existent production endpoint, and `/ride` visual alert is missing (Charter §1.4). |
| **invariants** | **PASS** | Strict compliance with GRAVE-05 (static free function callbacks), PSRAM buffer sizing (800 B event log ≤ 4 KB budget), streaming HTTP chunks (≤ 2048 B), and design token usage (`--card-bg`, `--text-muted`). |
| **edge_cases** | **PASS** | Ring buffer wrap-around, out-of-bounds query parameters, storage cap clamping (64..2048 KB), and NTP epoch validity thresholding (1.7B) handled cleanly. |
| **test_quality** | **PASS** | 233 unit tests pass in 0.57s with rigorous assertions covering ring buffer wrap, chronological ordering, FIFO eviction, and gzip round-trips. |
| **scope_hygiene** | **PASS** | All modified files are strictly within `charter.allowed_modules`. Clean diff without stray or temporary artifacts. |

---


## 2. Normative Axis Evaluation

### 2.1 Specification Alignment
- **PR-14 Charter §1.1 & SPEC §13:** `DiagnosticsModule` implements the 8-byte packed `DiagnosticEvent` structure with `EVENT_LOG_CAPACITY = 100` (800 bytes in PSRAM). Event recording methods for boot, session start/end, runaway, calibration, and firmware change match the spec.
- **PR-14 Charter §1.2 & SPEC §14:** `RidePersistence` stores completed rides in LittleFS under `/rides/persist/` with gzip compression via PSRAM-allocated `StreamingGzipEncoder` (5.2 KB PSRAM, zero DRAM pressure). Oldest-first eviction based on `start_epoch_s` respects the configurable cap (`diag_ride_cap_kb`, default 512 KB, clamped 64..2048 KB).
- **PR-14 Charter §1.3 & SPEC §13.4:** REST endpoints `GET /api/echenon/events`, `GET /api/echenon/runs`, `GET /api/echenon/runs/<id>.csv.gz`, and `DELETE /api/echenon/runs/<id>` are registered with authentication.
- **PR-14 Charter §1.4:** Runaway alert surfacing fails in two areas:
  - In `web/diagnostics.js`, `checkRunaway()` polls `/api/echenon/status`, which is not compiled in production builds (`env:ex3-waveshare-nano`).
  - In `web/ride.html` / `web/ride.js`, no runaway visual indicator or handling exists.
- In addition, `web/diagnostics.js` expects JSON keys `epoch_s` and `details`, whereas `DiagnosticsRoutes.cpp` and SPEC §13.4 define `epoch` and `payload`.

### 2.2 Architectural Invariants
- **GRAVE-05 (No Heap `std::function`):** `onSessionEndPersist` is implemented as a static free function pointer passed to `EventBus::subscribe`.
- **PSRAM Buffer Budget:** Ring buffer requires 800 bytes PSRAM (budget ≤ 4 KB). `StreamingGzipEncoder` uses PSRAM heap allocation (`MALLOC_CAP_SPIRAM`).
- **Chunked HTTP Responses:** CSV streaming buffers chunks to 2048 bytes; JSON event and run listing stream in chunks ≤ 512 bytes.
- **Flash Wear Mitigation:** Rides are written as block-appends on session completion; no rewrite-in-place; eviction deletes whole oldest files.
- **CSS Design Tokens:** `web/diagnostics.js` strictly adheres to `var(--card-bg)` and `var(--text-muted)`. No forbidden tokens (`--bg-card`, `--text-secondary`) are present.

### 2.3 Edge Cases & Robustness
- **Ring Buffer Wrap:** Verified by unit tests to maintain strict oldest-first chronological ordering across wrap boundaries.
- **NTP Epoch Transitions:** `getValidEpoch()` cleanly suppresses un-synced clock values prior to epoch 1,700,000,000.
- **Malformed Query Parameters:** `std::strtoul` handles invalid values with safe fallbacks; `limit` is clamped to `[1, 100]`.
- **Gzip Integrity:** `TCXGenerator::compressGzip` and `decompressGzip` verify IEEE 802.3 CRC32 and ISIZE trailer.

### 2.4 Test Quality & Verification Floor
- 233 native unit tests run and pass in 0.57s.
- 14 tests in `test_diagnostics_module.cpp` cover lifecycle, encoding, wrapping, query filtering, and capacity clamping.
- 11 tests in `test_diagnostics_routes.cpp` test string conversions and payload decode round-trips.
- 8 tests in `test_ride_persistence.cpp` verify save/load round-trips, oldest-first FIFO eviction under cap pressure, directory scanning, and deletion.

### 2.5 Scope Hygiene & Allowed Modules
- All files modified in the PR diff reside within `charter.allowed_modules`:
  `src/echenon/`, `src/main.cpp`, `test/`, `web/`, `docs/`, `SPEC.md`, `platformio.ini`.
- No extraneous files, debug logs, or personal/internal identifiers present.

---


## 3. Findings & Required Remediations

### BLOCKING-1: Diagnostics Event Viewer JSON Key Mismatch (`spec_alignment`)
- **Location:** `web/diagnostics.js:L153-L188`
- **Defect:** `web/diagnostics.js` expects event objects to contain `ev.epoch_s` and `ev.details`:
  ```javascript
  formatTime(ev.epoch_s)
  if (!ev.details) return '—';
  ```
  However, `DiagnosticsRoutes.cpp` (L183-L188) and SPEC §13.4 define the JSON response as:
  ```json
  {"epoch": 1710000000, "type": "BOOT", "type_id": 0, "payload": {"uptime_s": 3600}}
  ```
  Because `ev.epoch_s` and `ev.details` are undefined, the portal event viewer displays `Time: —` and `Details: —` for all rows. Additionally, `record_count_approx` and string `calibration_type` are not mapped.
- **Remediation:** Update `web/diagnostics.js` to:
  1. Read `ev.epoch || ev.epoch_s`.
  2. Read `ev.payload || ev.details`.
  3. Map `d.record_count || d.record_count_approx`.
  4. Support string calibration types (`d.calibration_type === 'initial' || d.calibration_type === 0`).

### BLOCKING-2: Runaway Warning Banner Polls Missing Production Endpoint (`spec_alignment`)
- **Location:** `web/diagnostics.js:L211-L229`
- **Defect:** `checkRunaway()` in `web/diagnostics.js` attempts to fetch `/api/echenon/status`. This endpoint is defined exclusively in `src/echenon/SignalLogger.cpp` under `#ifdef WITH_SIGNAL_LOGGER`. In production firmware builds (`ex3-waveshare-nano`), `WITH_SIGNAL_LOGGER` is not compiled, causing the request to return 404 and the banner to remain permanently hidden.
- **Remediation:** Add `"runaway": _runawayDetector.isRunaway()` to the `/api/echenon/live` JSON endpoint in `src/echenon/EchenonModule.cpp` (or register a production-safe `/api/echenon/status` route), and update `web/diagnostics.js` to consume it.

### BLOCKING-3: Missing Runaway Alert on `/ride` Page (`spec_alignment`)
- **Location:** `web/ride.html`, `web/ride.js:L6-L23`
- **Defect:** PR-14 Charter §1.4 explicitly requires runaway alerts to be surfaced "in the /ride page as a visual alert". While `LiveFeed` transmits `"runaway": "true"/"false"` in the WebSocket telemetry frame, `web/ride.html` and `web/ride.js` contain no DOM container or event handling for runaway alerts.
- **Remediation:** Add a visual runaway warning container to `web/ride.html` and update `tl(d)` in `web/ride.js` to display/hide the alert when `d.runaway` is true/false.

### NON-BLOCKING-1: Hardware Verification Endurance Duration Deferred (`spec_alignment`)
- **Location:** `docs/measurements/pr14-hw-verification.md:L5-L7`
- **Defect:** Charter §1.5 specified a 60-minute ride verification. A 5-minute smoke ride (305 seconds) was conducted on hardware, with the 60-minute endurance ride deferred per owner agreement.
- **Remediation:** Execute the 60-minute ride test during the next scheduled training session or release milestone to confirm thermal and LittleFS wear endurance.
