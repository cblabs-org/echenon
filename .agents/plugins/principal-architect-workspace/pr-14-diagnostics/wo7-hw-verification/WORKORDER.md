---
schema_version: 1
wo_id: WO-14.7
pr_id: PR-14
title: Hardware Verification (60-min ride)
complexity: interactive
status: COMPLETED
allowed_files:
- docs/measurements/pr14-hw-verification.md
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 0176e48993c7cea5b86090fee61dbeca263a9374c0fb2542508b81a865f90ff2
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_6526ffbeca74
  commit_sha: 486dff905f146e5b70a8d955295627edee9264f3
  files_modified:
  - docs/measurements/pr14-hw-verification.md
  verification_summary: 'pio test -e native --verbose: ================ 233 test cases:
    233 succeeded in 00:00:00.565 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.7 — Hardware Verification (60-min ride)

## 1. Role & Objective

You are collaborating with the human operator to validate the complete
diagnostics and persistence pipeline on real hardware. This is an interactive
verification Work Order — it requires physical hardware, a 60-minute ride,
and manual observation.

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** modify any source code, test files, or configuration.
- This WO produces a verification report only.
- **Tag → Build → Flash** — follow rule 03 for any firmware uploads.
- Use PIO MCP tools, never direct `pio` CLI (rule 04).

## 3. Line-Item Instructions

### 3.1 Pre-Flight Checks

- [ ] Verify firmware builds cleanly for `ex3-waveshare-nano` target.
- [ ] Verify all native tests pass (`pio test -e native --verbose`).
- [ ] Tag the build (next dev tag after current highest).
- [ ] Flash the firmware to the device.
- [ ] Confirm boot via serial monitor — verify boot event appears in
  diagnostics log.

### 3.2 Configuration

- [ ] Enable ride persistence via portal or API:
  ```json
  { "save_rides_to_flash": true, "diag_ride_cap_kb": 512 }
  ```
- [ ] Verify configuration is saved and persists across reboot.

### 3.3 60-Minute Ride

- [ ] Start a ride session (auto-start via cadence or manual start).
- [ ] Ride for ≥ 60 minutes with varying resistance and cadence.
- [ ] Observe:
  - Session start event appears in diagnostics log
  - LiveFeed WebSocket shows real-time data
  - No watchdog resets or crashes during the ride
  - Checkpoint flushes occur every 30 seconds (visible in serial log)
- [ ] End the session (auto-end via inactivity or manual stop).
- [ ] Observe:
  - Session end event appears in diagnostics log
  - Ride is persisted as `.ride.gz` in `/rides/persist/`

### 3.4 Download Verification

- [ ] Call `GET /api/echenon/runs` — verify the ride appears in the list.
- [ ] Download the ride via `GET /api/echenon/runs/<id>.csv.gz`.
- [ ] Decompress and verify:
  - CSV header matches expected format
  - Record count matches expected (~3600 for 60 min)
  - Power/cadence/HR values are plausible
  - Timestamps are monotonically increasing
- [ ] Verify gzip integrity (file decompresses without errors).

### 3.5 Event Log Verification

- [ ] Call `GET /api/echenon/events` — verify:
  - Boot event with correct firmware version
  - Session start with correct session_id
  - Session end with correct duration
  - All timestamps are plausible UTC epochs
- [ ] Test event log query parameters:
  - `from` / `to` time range filter
  - `limit` parameter

### 3.6 Portal Verification

- [ ] Open the portal in a browser.
- [ ] Navigate to the diagnostics event log viewer.
- [ ] Verify events display correctly with proper formatting.
- [ ] If runaway state can be triggered (move resistance knob to extreme),
  verify the warning banner appears.

### 3.7 Eviction Test (Optional)

- [ ] If time permits, set `diag_ride_cap_kb` to a small value (e.g., 64 KB)
  and perform multiple short rides to verify oldest-first eviction.

### 3.8 Verification Report

- [ ] Create `docs/measurements/pr14-hw-verification.md` with:
  - Date, firmware version, hardware details
  - Ride summary (duration, power stats, record count)
  - Download verification results (CSV integrity, record count)
  - Event log verification results
  - Portal screenshots (if possible)
  - Any issues or observations
  - Overall PASS/FAIL verdict

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all tests must pass.
- After verification, run `pa wo verify WO-14.7`.
- Surface any concerns in the Completion Report.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
