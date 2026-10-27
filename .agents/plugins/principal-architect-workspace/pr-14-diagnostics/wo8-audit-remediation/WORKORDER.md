---
schema_version: 1
wo_id: WO-14.8
pr_id: PR-14
title: Audit Remediation — Event Viewer Schema, Runaway Telemetry & Ride Banner
complexity: standard
status: PENDING
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
completion: null
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
Pending execution.
