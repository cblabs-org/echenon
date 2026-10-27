---
schema_version: 1
pr_id: PR-15
github_pr: 8
audit_round: 1
head_sha: 1ae6859e239d9c660fb59323126188b3f9d0ba9c
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-15 round=1 head_sha=1ae6859e239d9c660fb59323126188b3f9d0ba9c verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)

# Cold-Firewall Audit Report: PR-15 Round 1


## Executive Summary
Evaluation of PR-15 (`feat/pr-15-portal-ride`) at commit `1ae6859e239d9c660fb59323126188b3f9d0ba9c` against base `main`, the locked Part I PR Charter in `PROJECT.md`, and `SPEC.md`.
The implementation introduces the two-surface model (standalone daily-use `/ride` page + 6-tab device configuration portal), retires the `/overlay` concept across code and specification, and addresses all hardware verification findings (WO-15.6) including dark mode design token fixes and Apps/System tab restructuring.


## 1. Specification Alignment (spec_alignment)
- **Two-Surface Model (§1.1):**
  - Standalone daily-use `/ride` page created (`web/ride.html`, `web/ride.js`, `web/manifest.json`). Serves responsive split-screen (280–430px) layout with 2 Hz WebSocket live telemetry (`/ws/echenon/live`), start/pause/resume/stop session REST controls, last 5 rides history with TCX download, Strava upload mini-status, and ⚙️ portal link.
  - Portal (`/`) refactored from 9 tabs to 6 bottom navigation tabs: WiFi (10), Bluetooth (15), Echenon (50), Apps (60), Security (5), System.
  - Setup tab restructured to "Apps" tab (order 60, Feather grid icon) with "Strava" sub-pill; "Level Mapping" (order 25) and "Power Tuning" (order 30) sub-pills registered under parent "System".
  - Template App tab disabled via `app.disableDefaultAppTab()` in `src/main.cpp`.
  - Signal-logger UI clutter removed from production `web/echenon-dashboard.js`.
- **SPEC.md Overhaul (§11, §3.1–§3.5, §4.2, §12.1, §13, Appendix J):**
  - All occurrences of "overlay" replaced with "ride page" or "session UI" (`grep -i 'overlay' SPEC.md` yields zero matches).
  - §11 completely restructured for Ride Page (§11.1) and Portal Tabs (§11.2).
  - Test 7.9 added to §13.11 for P12 Android split-screen acceptance.
  - `docs/implementation-strategy.md` updated in lockstep.


## 2. Invariants & Architecture (invariants)
- **Flash Budget:** Total gzipped size of `/ride` assets is 2,784 bytes (HTML: 1,392 B, JS: 1,274 B, Manifest: 118 B), strictly below the 3 KB flash budget ceiling.
- **Embedded Footprint:** Firmware builds cleanly on `ex3-waveshare-nano` (RAM: 17.7% / 58,160 B, Flash: 26.6% / 1,672,171 B).
- **TimeManager Integration:** Properly conditionalized under `#ifdef WITH_TIME` in `src/main.cpp` and enabled in `platformio.ini`.
- **Route & Asset Serving:** Standalone `/ride`, `/ride.js`, and `/manifest.json` correctly registered with `serveGzipAsset` in `SessionOverlay.cpp`.
- **Secret Safety:** Strava Client Secret input is write-only, stored in NVS, never rendered in UI or logs. Empty Client ID is safely guarded in `StravaUploader::buildAuthorizeUrl`.


## 3. Edge Cases & Boundary Handling (edge_cases)
- **OAuth Empty Credentials:** `StravaUploader::buildAuthorizeUrl` returns empty string if `_clientId.empty()`, and the "Authorize with Strava" button is disabled until credentials are saved.
- **Power Table Availability:** `fetchActiveTable()` in `recalibration.js` handles failed responses or network disconnect gracefully without hanging on "Loading...".
- **Empty States:** Clear empty states for uncalibrated level mapping, missing ride history, and offline WebSocket connection.
- **Session Lifecycle Transitions:** Pause/resume and stop confirmation dialogs prevent accidental session termination.


## 4. Test Quality & Verification Floor (test_quality)
- **Verification Floor:** `pio test -e native` executed; 199/199 test cases passed 100% in 0.50s.
- `SessionOverlayTest.StravaCredentialsOAuthFlowTabsAndLiveFeedJson` updated to verify 0 tabs registered by SessionOverlay (migrated to `/ride` route and EchenonModule tabs).
- Target compilation verified: `pio run -e ex3-waveshare-nano` completed with 0 errors.


## 5. Scope & Repository Hygiene (scope_hygiene)
- All changed files are strictly contained within `charter.allowed_modules` (`src/echenon/`, `src/main.cpp`, `web/`, `include/portal_assets/`, `docs/`, `platformio.ini`, `SPEC.md`, `test/`).
- Working tree clean with zero uncommitted or untracked artifacts.
- No personal identifiers, API keys, or leaked credentials.


## Verdict
**APPROVED** — PR-15 meets all architectural invariants, passes all tests, conforms to the locked charter, and is ready for graduation.
