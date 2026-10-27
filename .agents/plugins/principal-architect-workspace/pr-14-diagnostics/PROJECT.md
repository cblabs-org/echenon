---
schema_version: 1
charter:
  pr_id: PR-14
  programme_id: prog-default
  slug: pr-14-diagnostics
  title: Diagnostics & Run-Data Persistence
  branch: feat/pr-14-diagnostics
  base_branch: main
  depends_on:
  - PR-06
  - PR-02
  allowed_modules:
  - src/echenon/
  - src/main.cpp
  - test/
  - web/
  - docs/
  - SPEC.md
  - platformio.ini
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: 26aaafe1dd55bab0490bbc71e7fecdabffbdaf645fcc4c070b4d7e3f952e4108
status: STAGED_FOR_AUDIT
approved_at: '2026-10-27T17:09:34.409212Z'
github_pr: null
work_orders:
- id: WO-14.1
  slug: wo1-diagnostics-module
  title: DiagnosticsModule + Bounded Event Log
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 116a292f54c853b07c6496d5f99fae1ef7afd56e
- id: WO-14.2
  slug: wo2-ride-persistence
  title: Run-Data Persistence (LittleFS + Gzip)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: a37b3d2af5722b5a3f09c37fc7c54b43eb9ff204
- id: WO-14.3
  slug: wo3-query-api
  title: Query REST API
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: bf25fea79673678681878e6b318521892d33eeb8
- id: WO-14.4
  slug: wo4-portal-integration
  title: Portal Integration + Runaway Alert Surfacing
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 64aa76e7110c039abe65a22244395521080e75f6
- id: WO-14.5
  slug: wo5-native-tests
  title: Native Tests
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: becaccf06b4b636924886ccd3b1ef8f57fdefb2c
- id: WO-14.6
  slug: wo6-spec-doc-sync
  title: SPEC + Doc Sync
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: d90a8252d3d63ff11ef0c444780a927219d2182d
- id: WO-14.7
  slug: wo7-hw-verification
  title: Hardware Verification (60-min ride)
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: 486dff905f146e5b70a8d955295627edee9264f3
---

# PR-14: Diagnostics & Run-Data Persistence

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md))
> **Release:** v1 Passive
> **PR target base:** `main` at `03be4c1`
> **Depends on:** PR-06 ✅ (RideStore, SessionManager, RideRecord, IRideFileSystem),
>   PR-02 ✅ (RunawayDetector)
> **Spec sections:** SPEC.md §9.3 (RideRecord), §11.4 (trust boundaries), §12 (build system);
>   new diagnostics sections to be added by this PR

---

## 1. Mandate

The owner decided on 2026-10-12 to keep monitoring and diagnostics in v1
(PROGRAM.md §6, 2026-10-12 session log entry).

### 1.1 Production Diagnostics Module

A new, purpose-built `DiagnosticsModule` (not a refactor of `SignalLogger`)
providing:

- **Tap health summary:** Green reference voltage monitoring and
  `RunawayDetector` state surfacing.
- **Bounded event log:** A PSRAM ring buffer (~100 entries, ~4 KB) recording
  runaway alerts (with duration), calibration completions, boot events (with
  uptime), session start/end (with summary), and firmware version changes.
  All entries carry UTC epoch timestamps.
- The full kHz burst `SignalLogger` remains untouched behind
  `WITH_SIGNAL_LOGGER` in the `ex3-waveshare-nano-diag` env only.

**Rationale (D1):** `SignalLogger` is a complex Phase-0 capture engine (10 ADC
channels, DMA, burst mode, decimation, ~7.5 MB PSRAM). Surgery to extract a
"light" version creates coupling risk. A new `DiagnosticsModule` is cleaner,
purpose-built, and independently testable.

### 1.2 Save to Flash (Option, Default Off)

Persist completed rides to LittleFS as `.ride.gz` (gzip-compressed binary
RideFileHeader + RideRecord[]):

- **Block writes:** Append checkpoint chunks during the ride (reusing the
  existing `RideStore` checkpoint mechanism), finalize with gzip on session end.
- **Configurable cap:** `diag_ride_cap_kb` (default `512` KB) within the
  2,816 KiB `storage` partition, which is shared with the upload queue and
  the audit log.
- **Oldest-first eviction:** When the cap is exceeded, the oldest stored ride
  is deleted to make room.
- **Config toggle:** `save_rides_to_flash` (`bool`, default `false`) in the
  Echenon `IConfigurable` namespace.

**Rationale (D2):** Binary persistence matches the existing `RideStore` format
(RideFileHeader + packed RideRecord[]). CSV conversion happens on-the-fly
during API download, minimizing flash writes.

**Rationale (D3):** At ~30 KB/ride (gzipped 60-min ride), 512 KB stores ~17
rides before eviction. This leaves ample headroom for the upload queue (~50 KB
per `tcx.gz`) and the audit log.

### 1.3 Query Interface

- `GET /api/echenon/runs` — JSON list of stored rides with summary metadata
  (session_id, start_epoch, duration, avg/max power, avg cadence, avg/max HR,
  total_work_kj, record_count).
- `GET /api/echenon/runs/<id>.csv.gz` — Download a stored ride as gzipped CSV
  (streamed in chunks; must never reboot the device).
- `DELETE /api/echenon/runs/<id>` — Delete a stored ride.
- `GET /api/echenon/events` — Event log with `from`, `to`, `limit` query
  parameters for time-windowed, paged queries.
- All endpoints authenticated per SPEC §11.4.
- Optional MCP tools: `get_diagnostic_events`, `get_stored_runs`.

### 1.4 Runaway Alert Surfacing

The PR-02 `RunawayDetector` (already ticking in `EchenonModule`) is surfaced:

- In the portal Echenon dashboard as a warning banner when `isRunaway()`.
- In the `/ride` page as a visual alert.
- In the persisted event log (with runaway duration).

### 1.5 Tests

- Native unit tests for: event log ring buffer (wrap, eviction, capacity),
  run-data persistence (gzip round-trip, cap enforcement, oldest-first
  eviction), paged query parameters (`from`/`to`/`limit`), CSV export format.
- Target: ≥ 25 new `RUN_TEST` cases.
- One interactive check (WO-14.7): a 60-minute ride with `save_rides_to_flash:
  true`, persisted and downloaded via the API.

---

## 2. Invariants

1. **Flash wear.** All writes use block-append (no rewrite-in-place). The
   checkpoint interval is ≥ 30 s. Oldest-first eviction, not circular rewrite.
2. **No heap `std::function`.** (GRAVE-05.)
3. **No full-buffer allocation in API responses.** All downloads streamed in
   chunks (≤ 4 KB per write to the HTTP response).
4. **PSRAM budget.** `DiagnosticsModule`'s event log is ≤ 4 KB. It does not
   compete with `RideStore`'s 115 KB active buffer.
5. **`SignalLogger` untouched.** It stays behind `WITH_SIGNAL_LOGGER`. No
   coupling between `DiagnosticsModule` and `SignalLogger`.
6. **Authentication.** All mutation endpoints (`DELETE`) and data endpoints
   require authentication per SPEC §11.4.
7. **CSS design tokens.** Use `--card-bg`, `--text-muted` (not `--bg-card` /
   `--text-secondary`).

---

## 3. Work Order Ledger

| WO | Title | Complexity | Status |
|---|---|---|---|
| WO-14.1 | DiagnosticsModule + Bounded Event Log | standard | PENDING |
| WO-14.2 | Run-Data Persistence (LittleFS + Gzip) | standard | PENDING |
| WO-14.3 | Query REST API | standard | PENDING |
| WO-14.4 | Portal Integration + Runaway Alert Surfacing | standard | PENDING |
| WO-14.5 | Native Tests | standard | PENDING |
| WO-14.6 | SPEC + Doc Sync | mechanical | PENDING |
| WO-14.7 | Hardware Verification (60-min ride) | interactive | PENDING |

---

## 4. Session Log

### 2026-10-27: Charter written (CA Session 2)
- Charter authored by Chief Architect based on 2026-10-12 owner mandate and
  approved plan (CA Session 2 artifact).
- Owner decisions D1–D4 approved at plan approval:
  - D1: New `DiagnosticsModule` (not SignalLogger refactor)
  - D2: Binary `.ride.gz` with CSV-on-download
  - D3: 512 KB default cap
  - D4: Event log scope (runaway, calibration, boot, session, firmware version)
- All dependencies met (PR-02, PR-06 on `main`).
- Next: lock charter, start Tier 2 PA for WO decomposition and execution.
