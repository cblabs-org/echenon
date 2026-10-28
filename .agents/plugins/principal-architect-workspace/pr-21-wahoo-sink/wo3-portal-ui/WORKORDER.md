---
schema_version: 1
wo_id: WO-21.3
pr_id: PR-21
title: Portal UI — Wahoo Pill & Stored Rides Badges
complexity: standard
status: COMPLETED
allowed_files:
- web/wahoo_settings.js
- web/stored_rides.js
forbidden_files:
- src/**
- test/**
- docs/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: b7ff9e3a73b6d2b1aa67419a8d6f0c4eda205f4c539cd93b6c7e6e6d880bca30
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_565992969d95
  commit_sha: 2c059c3a37b8185490986182e57a43007be32676
  files_modified:
  - web/stored_rides.js
  - web/wahoo_settings.js
  verification_summary: 'pio test -e native --verbose: ================ 261 test cases:
    261 succeeded in 00:00:00.497 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.3 — Portal UI: Wahoo Pill & Stored Rides Badges

## 1. Role & Objective

Create the Wahoo Cloud configuration pill (`web/wahoo_settings.js`) under the Apps tab and
update `web/stored_rides.js` to render per-sink badges and retry actions for Wahoo.

**Deliverables:**
- `web/wahoo_settings.js`:
  - Registers under the Apps tab (`order 30`).
  - Connection status badge: `Connected as <name>`, `Not Connected`, `Token Expired`.
  - Client credentials form: Client ID and Client Secret inputs with Save action (`POST /api/wahoo/credentials`).
  - `[Connect with Wahoo]` action: initiates OAuth flow via `GET /api/wahoo/auth-url` and redirects browser.
  - Auto-sync toggle: `auto_sync_wahoo` (synced via `/api/upload/settings`).
  - `[Disconnect]` action: calls `POST /api/wahoo/disconnect`.
- `web/stored_rides.js`:
  - Update `buildRemoteUrl()` to return `https://api.wahooligan.com/v1/workouts/` + `remoteId` (or Wahoo workout URL) when `serviceId === 'wahoo'`.
  - Ensure multi-sink rendering properly renders `Wahoo: ✅ Synced`, `Wahoo: ⏳ Pending`, or `Wahoo: ❌ Failed` badge with manual retry action.

## 2. Strict Boundary Box & Constraints

1. **PROGMEM Embedded Assets:** Assets in `web/` are converted to gzip byte arrays by `portalBuilder.py` at build time. NEVER call `upload_filesystem`.
2. **Design System Consistency:** Follow existing `web/upload_settings.js` and `web/intervals_settings.js` styles, typography, and card layouts.
3. **Vanilla JS:** Use vanilla ECMAScript 5/6 compatible JavaScript; do NOT introduce external web dependencies or frameworks.

**Forbidden:** No edits to `src/**`, `test/**`, or `docs/**`.

## 3. Line-Item Instructions

- [ ] Create `web/wahoo_settings.js`:
  - Use modular IIFE or standard web contributor pattern matching `intervals_settings.js` and `upload_settings.js`.
  - Fetch status from `/api/wahoo/status`.
  - Render Card container with header: "Wahoo Cloud", description: "Sync completed workouts directly to Wahoo Cloud (SYSTM / ELEMNT).".
  - If not configured: display Client ID & Secret form with "Save Credentials" button.
  - If configured but not authenticated: display "[Connect with Wahoo]" button linking to authorization URL.
  - If authenticated: display user details, token expiration status, auto-sync checkbox, and "[Disconnect]" button.
  - Handle error parameters in URL (e.g., `?wahoo_error=...`).
- [ ] Update `web/stored_rides.js`:
  - In `buildRemoteUrl(serviceId, remoteId)`:
    - Add branch for `sid === 'wahoo'`: return `'https://api.wahooligan.com/v1/workouts/' + remoteId;`.
  - Ensure per-sink status loop handles `service_id: "wahoo"` with appropriate label ("Wahoo").

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` to ensure all native tests continue passing.
- Run `pa wo verify WO-21.3` to stamp cryptographic receipt.

## Completion Report

All line-item deliverables completed and verified:
1. `web/wahoo_settings.js`:
   - Authored Wahoo Cloud settings panel conforming to modular IIFE structure matching `intervals_settings.js` and `upload_settings.js`.
   - Setup Guide: Instructions for creating application in Wahoo Developer Portal, setting redirect URI (`/api/wahoo/callback`), and scopes (`workouts_read workouts_write`).
   - Wahoo API Credentials card: Form inputs for Client ID and write-only Client Secret, with Save Credentials action (`POST /api/wahoo/credentials`).
   - Wahoo Cloud Integration card: Status badge (`Connected as <name>`, `Not Connected`, `Token Expired`), user & token expiry details, `[Connect with Wahoo]` action (`GET /api/wahoo/auth-url`), auto-sync toggle (`auto_sync_wahoo` via `/api/session/config` and `/api/upload/settings`), and `[Disconnect]` action (`POST /api/wahoo/disconnect`).
   - URL error handling: Detects and displays `wahoo_error` query parameters from hash or search query.
2. `web/stored_rides.js`:
   - Updated `buildRemoteUrl()` to return `'https://api.wahooligan.com/v1/workouts/' + remoteId` for `sid === 'wahoo'`.
   - Updated per-sink sync status rendering with `formatServiceLabel()` to display appropriate "Wahoo" service label while preserving `service_id: "wahoo"` for retry triggers.
