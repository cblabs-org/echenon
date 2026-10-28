---
schema_version: 1
wo_id: WO-21.3
pr_id: PR-21
title: Portal UI — Wahoo Pill & Stored Rides Badges
complexity: standard
status: PENDING
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
completion: null
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

_Pending execution._
