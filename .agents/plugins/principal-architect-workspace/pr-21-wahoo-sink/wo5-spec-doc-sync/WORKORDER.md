---
schema_version: 1
wo_id: WO-21.5
pr_id: PR-21
title: SPEC & Documentation Sync
complexity: mechanical
status: COMPLETED
allowed_files:
- SPEC.md
- docs/data-sinks.md
forbidden_files:
- src/**
- web/**
- test/**
required_skills: []
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: b06a3ed6217e2bb6eb324be700b30a570950fdb6db3a5f23a82c06bc0120399f
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_87a73884958e
  commit_sha: be5e696c35ce6e51da67d7785bf5d75c257ab49b
  files_modified:
  - SPEC.md
  - docs/data-sinks.md
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.487 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.5 — SPEC & Documentation Sync

## 1. Role & Objective

Update the architectural specification (`SPEC.md`) and data sinks documentation (`docs/data-sinks.md`)
to incorporate Wahoo Cloud as Sink #3 within Echenon's pluggable multi-sink architecture.

**Deliverables:**
- Update `SPEC.md` §10.3 and §10.5:
  - Document `WahooUploader` OAuth 2.0 flow, authorization endpoints, token rotation, and multipart workout creation.
  - Update data sinks summary table to reflect Wahoo Cloud as fully implemented.
  - Update portal tabs specification in §11.2 (Wahoo Cloud pill, order 30).
- Update `docs/data-sinks.md`:
  - Detailed architectural guide for Wahoo Cloud data sink.
  - BYOA setup, credentials, OAuth endpoints, and payload formats.
  - Troubleshooting and error handling taxonomy.

## 2. Strict Boundary Box & Constraints

1. **Maintain Invariants:** Keep all existing markdown headings and cross-references intact.
2. **Preserve Integrity:** Accurate technical parameters matching `WahooUploader` implementation.

**Forbidden:** No edits to `src/**`, `web/**`, or `test/**`.

## 3. Line-Item Instructions

- [ ] In `SPEC.md`:
  - In §10.3 / §10.5, update the Data Sinks provider table to mark Wahoo Cloud as active/supported.
  - Detail `WahooUploader` OAuth 2.0 endpoints (`https://api.wahooligan.com/oauth/authorize`, `/oauth/token`, `/v1/workouts`), credentials (`wahoo.client_secret`, `wahoo.refresh_token`, `wahoo.access_token`), and `workout[workout_type_id]=1`.
  - In §11.2, ensure Wahoo Cloud portal pill (order 30) is documented under the Apps tab.
- [ ] In `docs/data-sinks.md`:
  - Add Wahoo Cloud section detailing:
    - Overview and purpose.
    - BYOA registration steps at Wahoo Developer Portal.
    - Flow architecture (OAuth 2.0, token exchange, refresh, TCX multipart upload).
    - Status code handling and retry policy.
    - Configuration keys (`client_id`, `enabled`, `user_name`, `expires_at`).

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` to ensure no documentation or build breakage.
- Run `pa wo verify WO-21.5` to stamp cryptographic receipt.

## Completion Report

### Summary of Completed Work
Updated `SPEC.md` and `docs/data-sinks.md` to fully document Wahoo Cloud as Sink #3 in Echenon's pluggable multi-sink architecture, matching the implementation delivered across WO-21.1 through WO-21.4.

### Key Changes & Documentation Synchronized
1. **`SPEC.md` Architectural Specification:**
   - **§1.4 (Scope & Non-Goals):** Updated non-goals to reflect active on-device direct upload support for Strava, Intervals.icu, and Wahoo Cloud.
   - **§2.1 (System Architecture & Module Table):** Documented `WahooUploader` implementing `IUploader` and `IConfigurable` (`"echenon_wahoo"`, schema v1) and updated `SessionOverlay` description to include `/api/wahoo/*` routes.
   - **§10.1 & §10.2 (Multi-Sink UploadQueue Architecture):** Updated documentation and code snippets to include `auto_sync_wahoo` and `wahoo` as a registered sink identifier.
   - **§10.3 (Settings Pages):** Updated Apps tab pill hierarchy tree to include `Wahoo Cloud` (`/wahoo_settings.js`, order 30) and multi-sink toggle list.
   - **§10.3b (Wahoo Cloud Sink — OAuth 2.0 Sink #3):** Added dedicated specification section covering:
     - Bring-Your-Own-App (BYOA) developer setup and OAuth scopes (`user_read workouts_write`).
     - Authorization endpoints (`https://api.wahooligan.com/oauth/authorize`, `/oauth/token`).
     - Cryptographically secure 128-bit (32 hex) CSRF state nonce generation and single-use 10-minute TTL validation.
     - Token exchange and rotated refresh-token-first persistence writing `wahoo.refresh_token` before `wahoo.access_token` and metadata.
     - Multipart workout creation endpoint (`POST https://api.wahooligan.com/v1/workouts`) with `workout[workout_type_id]=1`, `workout[file]`, and boundary framing.
     - Synchronous status handling (`pollUploadStatus()` completes immediately with `SUCCESS` without async polling round-trips).
     - HTTP status code mapping to `UploadStageResult` (200/201 SUCCESS, 401 PERMANENT_ERROR, 429/5xx RETRYABLE_ERROR).
     - REST configuration routes (`/api/wahoo/status`, `/api/wahoo/credentials`, `/api/wahoo/auth-url`, `/api/wahoo/callback`, `/api/wahoo/disconnect`).
   - **§10.5 (Downstream Platform Sync — Provider Tier Taxonomy):** Promoted Wahoo Cloud to **Tier B (Core live)**; removed from Tier C.
   - **§11.2 (Portal Tabs & REST API Routes):** Added Wahoo Cloud pill (order 30) to Apps tab table and documented all 5 `/api/wahoo/*` REST endpoints.

2. **`docs/data-sinks.md` Multi-Sink Design Guide:**
   - **Section 1 & 2:** Updated multi-sink architecture diagram to show 3 registered uploaders plus future webhook sink; added Wahoo Cloud to Tier B table and removed from Tier C table.
   - **Section 4 & 5:** Added Wahoo Cloud encrypted NVS keys (`wahoo.client_secret`, `wahoo.refresh_token`, `wahoo.access_token`) under namespace `echenon_wahoo`, auto-sync toggle `auto_sync_wahoo`, and Apps tab pill order (order 30).
   - **Section 6 & 7:** Updated JSON payload examples for `/api/upload/sinks` and `/api/session/history` showing Wahoo Cloud sync status; updated sequential queue drain diagram to include Wahoo stages.
   - **Section 8 (Wahoo Cloud Sink — Sink #3):** Added comprehensive guide with subsections on:
     - Architectural role as Sink #3 in the 4-sink registry (`UploadQueue`).
     - Step-by-step BYOA registration on Wahoo Developer Portal.
     - Scope requirements (`user_read workouts_write`).
     - OAuth 2.0 flow sequence diagram, CSRF state nonce defense, and rotated refresh-token-first crash resilience.
     - Redactor secret registration for all sensitive tokens.
     - Multipart TCX upload framing and synchronous workout creation response handling.
     - HTTP error code taxonomy and backoff retry policy.
     - Multi-sink interaction and shared `.tcx.gz` file lifecycle.
     - Configuration keys and secret storage partition.

### Verification
- Ran `pio test -e native --verbose`: all 269 tests across 29 test suites passed in 0.54s with zero errors or warnings.
- Ran `pa wo verify WO-21.5`: verified scope hygiene, computed canonical tree hash `87a73884958e8b09befa44cce25118ea6eeac7e3db02f8db9deed4d22d529e50`, and committed receipt `rcpt_87a73884958e`.
