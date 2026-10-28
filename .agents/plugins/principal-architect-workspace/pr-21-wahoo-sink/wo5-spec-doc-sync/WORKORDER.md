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

_Pending execution._
