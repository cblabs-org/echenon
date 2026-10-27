---
schema_version: 1
wo_id: WO-14.8
pr_id: PR-14
title: Audit Remediation — Event Viewer Schema, Runaway Telemetry & Ride Banner
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/EchenonModule.cpp
- web/diagnostics.js
- web/ride.html
- web/ride.js
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 70e0cbbe04087396c1d0a436b8bb8a3fbf3dda5449ad2f1b06ea22ba2d7a9a3f
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_f601adf48897
  commit_sha: a50d8f9ff2ddbad92888c79fb8f0ab4af8e3b947
  files_modified:
  - src/echenon/EchenonModule.cpp
  - web/diagnostics.js
  - web/ride.html
  - web/ride.js
  verification_summary: 'pio test -e native --verbose: ================ 233 test cases:
    233 succeeded in 00:00:00.426 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.8 — Audit Remediation: Event Viewer Schema, Runaway Telemetry & Ride Banner

## 1. Role & Objective

Resolve the three blocking defects identified in Round 1 Cold-Firewall Audit (`AUDIT-round-1.md`):

1. **`BLOCKING-1` (Event Viewer JSON Key Drift):**
   Update `web/diagnostics.js` to align with `DiagnosticsRoutes.cpp` / SPEC §13.4 by reading `ev.epoch || ev.epoch_s`, `ev.payload || ev.details`, `record_count_approx`, and string `calibration_type`.

2. **`BLOCKING-2` (Runaway Telemetry Polling):**
   Expose `"runaway": _runawayDetector.isRunaway()` in the production `/api/echenon/live` endpoint in `src/echenon/EchenonModule.cpp`. Update `web/diagnostics.js` `checkRunaway()` to poll `/api/echenon/live` instead of the diagnostic-only `/api/echenon/status`.

3. **`BLOCKING-3` (Ride Page Runaway Banner):**
   Add a high-visibility runaway alert banner to `web/ride.html` and wire `web/ride.js` `tl(d)` to toggle its visibility based on `d.runaway`.

## 2. Strict Boundary Box

- `src/echenon/EchenonModule.cpp`
- `web/diagnostics.js`
- `web/ride.html`
- `web/ride.js`

## 3. Verification

1. `pio test -e native --verbose` (all native tests pass).
2. Clean code formatting and zero heap allocations (`GRAVE-05`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

### Implementation Summary
- **BLOCKING-1 (Event Viewer Schema Alignment):**
  - Updated `web/diagnostics.js` to extract `ev.epoch || ev.epoch_s` for timestamps and `ev.payload || ev.details` for details payload.
  - Added support for `record_count_approx` with fallback to `record_count`.
  - Added support for string `calibration_type` values (`"initial"` -> "Initial", `"recalibration"` -> "Recalibration") alongside legacy numeric values (0 -> "Initial", 1 -> "Recalibration").
- **BLOCKING-2 (Production Runaway Telemetry):**
  - Exposed `"runaway": _runawayDetector.isRunaway()` in `/api/echenon/live` endpoint in `src/echenon/EchenonModule.cpp`.
  - Updated `checkRunaway()` in `web/diagnostics.js` to poll `/api/echenon/live` instead of the diagnostic-only `/api/echenon/status`.
- **BLOCKING-3 (Ride Page Runaway Banner):**
  - Added pulsing runaway alert banner `<div id="runaway-banner">` in `web/ride.html`.
  - Updated `tl(d)` in `web/ride.js` to toggle runaway banner display (`d.runaway === true || d.runaway === 'true'`).

### Verification Receipt
- Ran verification command: `pio test -e native --verbose` (233/233 tests passed).
- Receipt ID: `rcpt_f601adf48897`
- Commit SHA: `a50d8f9ff2ddbad92888c79fb8f0ab4af8e3b947`
- Tree Hash: `f601adf488978d48ec8c6def42ec8c7a6dc1e0bbdc7f4d5da10196818fd99bce`
