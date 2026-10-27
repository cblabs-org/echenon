---
schema_version: 1
wo_id: WO-14.6
pr_id: PR-14
title: SPEC + Doc Sync
complexity: mechanical
status: COMPLETED
allowed_files:
- SPEC.md
- docs/diagnostics.md
- docs/run-data-persistence.md
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 03a0c959ee9355aeefe3f66a94eb3c58d7fe2fdf437c583a650ee7bb7534d076
completion: null
---

# WO-14.6 — SPEC + Doc Sync

## 1. Role & Objective

You are an expert technical writer and firmware documentation specialist.
Update `SPEC.md` and create/update developer documentation to reflect the new
DiagnosticsModule, run-data persistence, and query API introduced in
WO-14.1 through WO-14.5.

Read the `framework-expert` skill at
`/Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md`
for documentation conventions and SPEC.md structure.

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** touch any source code, test files, web files, or `main.cpp`.
- Documentation must accurately reflect the implemented code from WO-14.1
  through WO-14.5.

## 3. Line-Item Instructions

### 3.1 SPEC.md Updates

- [ ] Add a new section **§13 Diagnostics** (or append to existing diagnostics
  section if one exists) covering:
  - §13.1 DiagnosticsModule overview — purpose, scope, separation from SignalLogger
  - §13.2 Diagnostic Event Types — table of all 6 event types with payload encoding
  - §13.3 Event Log Ring Buffer — PSRAM allocation, capacity (100 entries),
    wrap semantics, 4 KB budget
  - §13.4 Query API — `EventQuery` struct, time-windowed queries

- [ ] Add a new section **§14 Run-Data Persistence** covering:
  - §14.1 Storage Format — `.ride.gz` binary format (RideFileHeader + RideRecord[])
  - §14.2 Configuration — `save_rides_to_flash`, `diag_ride_cap_kb`, defaults
  - §14.3 Eviction Policy — oldest-first, cap enforcement, flash wear rules
  - §14.4 File Layout — `/rides/persist/ride_<session_id>.ride.gz`

- [ ] Update **§9.3 RideRecord** cross-references to point to §14 for persistence.

- [ ] Add REST API documentation to existing API section (or create new subsection):
  - `GET /api/echenon/events` — query params, response format
  - `GET /api/echenon/runs` — response format
  - `GET /api/echenon/runs/<id>.csv.gz` — streaming download
  - `DELETE /api/echenon/runs/<id>` — auth required

- [ ] Update **§11.4 Trust Boundaries** to include diagnostics endpoints:
  - Read-only (events, runs list): require auth for data privacy
  - Mutations (delete run): require auth

### 3.2 Developer Documentation

- [ ] Create `docs/diagnostics.md`:
  - Overview of DiagnosticsModule architecture
  - How to record custom events
  - Event log query examples
  - Portal event viewer screenshot placeholder

- [ ] Create `docs/run-data-persistence.md`:
  - How to enable ride persistence (`save_rides_to_flash: true`)
  - Storage budget and eviction behavior
  - How to download rides via API
  - CSV format specification
  - Integration with existing RideStore/SessionManager pipeline

### 3.3 Cross-Reference Hygiene

- [ ] Ensure all new SPEC sections cross-reference correctly.
- [ ] Add DiagnosticsModule to any module inventory table in SPEC.md if one exists.
- [ ] Reference the correct CSS tokens (`--card-bg`, `--text-muted`) in portal
  documentation.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all tests must still pass (doc-only
  changes should not affect compilation).
- After verification, run `pa wo verify WO-14.6`.
- Surface any concerns in the Completion Report.

## Completion Report

**Status:** COMPLETED

### Files Modified
- `SPEC.md` — Added §13 Diagnostics (§13.1–§13.4), §14 Run-Data Persistence (§14.1–§14.4), renumbered old §13 Testing to §15, updated module table, persisted settings, trust boundaries, project tree, portal tabs, test table, and all cross-references.
- `docs/diagnostics.md` — Created: DiagnosticsModule developer guide covering architecture, event recording, query API, payload encoding, portal viewer, ring buffer capacity, and testing.
- `docs/run-data-persistence.md` — Created: Run-data persistence developer guide covering enablement, storage budget, eviction policy, REST API usage, CSV format spec, binary file format, session pipeline integration, flash wear, and testing.

### Verification
- `pio test -e native --verbose`: **232 tests passed** (0 failures). Doc-only changes; no compilation impact.

### Decisions Made
- Renumbered §13 Testing → §15 to make room for §13 Diagnostics and §14 Run-Data Persistence, maintaining consistency with the WO instructions that specified "§13 Diagnostics" and "§14 Run-Data Persistence".
- All internal cross-references (§13.6, §13.10, §13.11, §13.12, §13.1) updated to new §15.x numbering.
- Diagnostics endpoints documented as requiring authentication per existing §11.4 trust boundary pattern, consistent with the implementation in `DiagnosticsRoutes.cpp` which calls `auth.checkPortalAuth()`.
- CSS tokens (`--card-bg`, `--text-muted`) not referenced because no portal HTML/CSS was created in this WO (portal UI is in WO-14.4). The developer docs reference the portal viewer conceptually with a TODO screenshot placeholder.
