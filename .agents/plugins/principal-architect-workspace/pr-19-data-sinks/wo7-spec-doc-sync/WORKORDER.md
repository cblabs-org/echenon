---
schema_version: 1
wo_id: WO-19.7
pr_id: PR-19
title: SPEC & Doc Sync
complexity: mechanical
status: PENDING
allowed_files:
- SPEC.md
- docs/data-sinks.md
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 4c024611faf82c5b4f7b4b05438e0938124defb1e1985d19369558287f3b25a0
completion: null
---

# WO-19.7 — SPEC & Doc Sync

## 1. Role & Objective

Update SPEC.md and create design documentation reflecting the multi-sink architecture,
Intervals.icu integration, Stored Rides pill, and provider sync strategy.

## 2. Strict Boundary Box & Constraints

1. Only modify `SPEC.md` and create `docs/data-sinks.md`.
2. Preserve all existing SPEC.md content not related to upload services.
3. Section references: §10 (Upload Services), §10.5 (Downstream Platform Sync), §11.2 (Portal Tabs), §14 (Ride Persistence).

**Forbidden:** No source code edits. No `web/` edits. No `test/` edits.

## 3. Line-Item Instructions

### 3.1 SPEC.md §10 Updates
- [ ] Update §10.1 to describe multi-sink `UploadQueue` architecture (registerSink, MAX_SINKS=4, sequential execution).
- [ ] Update §10.2 to describe `UploadJob` with `service_id` composite key and shared `.tcx.gz` lifecycle.
- [ ] Add §10.3a for Intervals.icu: API-key auth, POST endpoint, synchronous processing.
- [ ] Add/update §10.5 "Downstream Platform Sync" with Tier A/B/C/D taxonomy from the PR charter.
- [ ] Document per-sink auto-sync toggles (`auto_sync_strava`, `auto_sync_intervals`).

### 3.2 SPEC.md §11 Updates
- [ ] Update §11.2 portal tab structure to include Stored Rides pill under Echenon tab.
- [ ] Document per-sink Apps pills: Strava (order 10), Intervals.icu (order 20).
- [ ] Document REST API extensions: `/api/upload/sinks`, `/api/upload/status`, `/api/intervals/*`.

### 3.3 docs/data-sinks.md (new)
- [ ] Create design document covering:
  - Multi-sink architecture overview.
  - Provider sync strategy (Tier A/B/C/D).
  - Adding a new sink: implementation checklist.
  - Per-sink configuration and secret storage.
  - Portal pill registration pattern.
  - REST API conventions.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.7` → `pio test -e native --verbose` (doc-only change, tests must still pass).

## Completion Report

_Pending execution._
