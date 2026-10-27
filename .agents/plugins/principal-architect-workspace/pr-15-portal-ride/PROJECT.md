---
schema_version: 1
charter:
  pr_id: PR-15
  programme_id: prog-default
  slug: pr-15-portal-ride
  title: Portal Rework & Ride Page
  branch: feat/pr-15-portal-ride
  base_branch: main
  depends_on:
  - PR-06
  allowed_modules:
  - src/echenon/
  - src/main.cpp
  - web/
  - include/portal_assets/
  - docs/
  - platformio.ini
  - SPEC.md
  - test/
  required_skills:
  - framework-expert
  - framework-design-system
  verification_floor:
  - pio test -e native
  charter_locked: true
  charter_hash: 50a5ba28b7c4b4fdd9d3c0ff6018eee2d68d19885ae6b794dc979f644715b695
status: GRADUATED
approved_at: '2026-10-27T07:19:12.619513Z'
github_pr: 8
work_orders:
- id: WO-15.1
  slug: wo1-portal-cleanup
  title: Portal Cleanup & Tab Restructure
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 911d660847a54e714a284338df175946338868b2
- id: WO-15.2
  slug: wo2-setup-tab
  title: Setup Tab — Apps & Calibration Pills
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 9c3e53ab2e4e865db749f9401db22bbe8de7d122
- id: WO-15.3
  slug: wo3-ride-page
  title: Ride Page — Standalone /ride
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 6dc83ed16337e72352a7dd648f046c3cc89cb5b3
- id: WO-15.4
  slug: wo4-spec-updates
  title: SPEC & Design Doc Updates
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 61a177371b074820c2a4f4bb3a057c077d85bcab
- id: WO-15.5
  slug: wo5-hw-verification
  title: Hardware Verification — P12 Split-Screen
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-15.6
  slug: wo6-portal-bugfixes
  title: Portal Bug Fixes from Hardware Verification
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 053511d0cc5522ea95b32132d5830a0c009eb846
---

# PR-15: Portal Rework & Ride Page (Phase 4, v1 Passive)

> **Parent:** [`PROGRAM.md`](../PROGRAM.md)
> **Phase:** 4 (`docs/implementation-strategy.md`) — **v1 "Passive" release**
> **Depends on:** PR-06 (`aecdc97`)
> **Before:** PR-09 (v1 Integration & Release)
> **Branch:** `feat/pr-15-portal-ride` (based on `main` after PR-06 merge)
> **Spec sections:** §11 (Session UI — Ride Page & Portal Tabs), §3.1–§3.5 (overlay → ride page rename), §4.2, §12.1, §13, Appendix J
> **UX Audit:** [`portal-ux-audit.md`](../../../../.gemini/antigravity/brain/4025aa8a-c556-4eac-ab5b-123ed68ccea5/portal-ux-audit.md)

---

# Part I — PR Charter (Chief Architect)

## 1. Mandate & Architectural Invariants

Rework the Echenon portal from 9 unusable bottom-bar tabs into a clean 6-tab layout, create a standalone `/ride` page for daily-use session control (split-screen / Slide Over / phone), and update the SPEC to replace the `/overlay` concept with `/ride`.

### 1.1 Two-Surface Model (Owner Decision 2026-10-26)

The Echenon UI is split into two distinct surfaces:

- **`/ride` (Standalone Ride Page):** Daily-use rider control surface. Live metrics via WebSocket (`/ws/echenon/live` at 2 Hz), session start/stop/pause, ride history, upload queue mini-status, TCX download. Responsive 280–430px (primary: Android split-screen / iPad Slide Over alongside Echelon app; secondary: full phone screen). PWA manifest for "Add to Home Screen". Dark theme by default. ⚙️ gear icon → portal.

- **`/` (Portal):** Device configuration and settings (one-time / rare). 6-tab bottom navigation: WiFi (10), Bluetooth (15), Echenon (50, default), Setup (60), Security (5), System. The Echenon tab shows device status summary + "Open Ride Page →" button. The Setup tab has PillBar navigation: Apps (Strava OAuth), Level Mapping (initial calibration wizard), Power Tuning (recalibration).

### 1.2 What's Removed
- **App tab** — framework template remnant → `app.disableDefaultAppTab()` in `main.cpp` before `app.begin()`
- **Signal logger content** in `echenon-dashboard.js` — burst capture controls, raw ADC channel tables, CSV export links → behind `WITH_SIGNAL_LOGGER` compile guard only (present in `-diag` environment)
- **`/overlay` page** — dropped from SPEC entirely. Split-screen and iPad Slide Over are native OS features that achieve the same result without OBS complexity.

### 1.3 What's Merged / Moved
- **Session controls + Upload queue status** → standalone `/ride` page (**NOT** portal pills)
- **Calibration + Recalibration** → portal "Setup" tab, pills "Level Mapping" + "Power Tuning"
- **Strava OAuth config** → portal "Setup" tab, "Apps" pill

### 1.4 Design System Compliance
All new and refactored UI must follow the `framework-portal-design` skill:
- CSS design tokens (`var(--primary)`, `var(--success)`, etc.)
- Card-based layout (16px radius, hairline borders)
- Feather icon style (24px, 2px stroke, currentColor)
- PillBar navigation for Setup tab sub-pills
- Empty states with helpful messaging
- Ride page: dark-first colour scheme, large touch targets, high-contrast metrics

## 2. Scope

### Allowed Files & Modules
- `src/main.cpp` (`disableDefaultAppTab`)
- `src/echenon/EchenonModule.cpp`, `src/echenon/EchenonModule.h`
- `src/echenon/SessionOverlay.cpp`, `src/echenon/SessionOverlay.h`
- `web/echenon-dashboard.js`, `web/session.js`, `web/upload_settings.js`
- `web/calibration-wizard.js`, `web/recalibration.js`
- `web/ride.html` (new), `web/ride.js` (new), `web/manifest.json` (new)
- `include/portal_assets/`
- `platformio.ini`
- `SPEC.md`, `docs/`

### Out of Scope
- Framework BLE tab modifications (framework-owned, not consumer scope)
- Strava upload logic changes (PR-06 scope, already complete)
- Diagnostics persistence (PR-14)
- Signal-logger C++ code changes (already behind `WITH_SIGNAL_LOGGER`)

---

# Part II — Principal Architect Decomposition & Execution

## 3. Work Order Ledger

| WO ID | Slug | Title | Complexity | Status | Attempts | Commit SHA |
|---|---|---|---|---|---|---|
| `WO-15.1` | `wo1-portal-cleanup` | Portal Cleanup & Tab Restructure | `standard` | `COMPLETED` | 1 | `7dc14ee` |
| `WO-15.2` | `wo2-setup-tab` | Setup Tab — Apps & Calibration Pills | `standard` | `COMPLETED` | 1 | `6dc83ed` |
| `WO-15.3` | `wo3-ride-page` | Ride Page — Standalone `/ride` | `standard` | `COMPLETED` | 1 | `61a1773` |
| `WO-15.4` | `wo4-spec-updates` | SPEC & Design Doc Updates | `standard` | `COMPLETED` | 1 | `ab3ee53` |
| `WO-15.5` | `wo5-hw-verification` | Hardware Verification — P12 Split-Screen | `interactive` | `IN_PROGRESS` | 1 | — |
| `WO-15.6` | `wo6-portal-bugfixes` | Portal Bug Fixes from Hardware Verification | `standard` | `COMPLETED` | 1 | `71e9222` |

### WO-15.1: Portal Cleanup & Tab Restructure
**Goal:** Remove the App tab, strip signal-logger content from production `echenon-dashboard.js`, rewrite the Echenon tab to show device status + "Open Ride Page →" button, and verify the resulting 6-tab bottom nav.

**Tasks:**
1. Call `app.disableDefaultAppTab()` in `main.cpp` before `app.begin()`
2. In `EchenonModule.cpp`, update `registerTabs()`:
   - Remove separate Calibration (order 60) and Recalibration (order 70) tab registrations
   - Keep Echenon tab at order 50
3. In `SessionOverlay.cpp`, remove Session (order 80) and Upload Services (order 90) top-level tab registrations
4. Rewrite `web/echenon-dashboard.js`:
   - Remove: Channel Map, Burst Capture controls, CSV Export links, Capture Status card, Recent Samples table
   - Keep: device status summary (firmware version, uptime, connection state, last ride)
   - Add: prominent "Open Ride Page →" button linking to `/ride`
   - Apply design-system CSS tokens
5. Verify portal shows exactly 6 bottom-bar tabs in correct order

**Verification:** `pio test -e native` passes; portal at `echenon.local` shows WiFi, Bluetooth, Echenon, Setup, Security, System tabs.

### WO-15.2: Setup Tab — Apps & Calibration Pills
**Goal:** Create a new "Setup" top-level tab (order 60) with PillBar sub-navigation containing Apps, Level Mapping, and Power Tuning pills.

**Tasks:**
1. In `EchenonModule.cpp`, register "Setup" tab at order 60 with a parent-tab script
2. Refactor `web/calibration-wizard.js` to render as "Level Mapping" pill panel:
   - Pre-populate fields with current calibration values from `/api/config`
   - Add inline help text explaining the calibration procedure
   - Add "Reset to Defaults" button
   - Apply design-system CSS tokens
3. Refactor `web/recalibration.js` to render as "Power Tuning" pill panel:
   - Same UX improvements as Level Mapping
4. Split `web/upload_settings.js`:
   - Strava OAuth config (client_id, client_secret, authorize button) → "Apps" pill
   - Queue status display → moved to `/ride` page (WO-15.3)
5. Apply PillBar navigation per `framework-portal-design` skill

**Verification:** Setup tab shows 3 pills; each renders correctly; existing calibration and Strava functionality preserved.

### WO-15.3: Ride Page — Standalone `/ride`
**Goal:** Create a standalone HTML page at `/ride` for daily-use session control, optimised for split-screen with the Echelon app.

**Tasks:**
1. Create `web/ride.html`:
   - Standalone HTML page (not a portal SPA tab)
   - Dark theme by default
   - `<link rel="manifest" href="/manifest.json">`
   - Responsive 280–430px (CSS media queries / container queries)
   - Viewport meta tag for mobile
2. Create `web/ride.js`:
   - Live metrics display: cadence, power, speed, HR, level (big glanceable numbers)
   - WebSocket connection to `/ws/echenon/live` (2 Hz telemetry)
   - Session controls: Start/Pause/Stop buttons via REST (`/api/session/*`)
   - Session state badge (IDLE / ACTIVE / PAUSED)
   - Current ride stats: elapsed time, distance, avg power
   - Upload queue mini-status (✓ uploaded / ⏳ pending / ✗ failed) via `/api/strava/status`
   - Ride history (last 5 rides, TCX download links) via `/api/session/history`
   - ⚙️ gear icon linking to portal (`/`)
   - Empty states with helpful messaging
3. Create `web/manifest.json`:
   - PWA manifest with `display: standalone`
   - Theme and background colours for dark mode
   - Icons (192px + 512px)
   - `start_url: "/ride"`
4. In `SessionOverlay.cpp`, register `/ride` route serving the standalone page
5. Register gzipped assets in `include/portal_assets/`

**Verification:** `/ride` page loads, shows live metrics, session controls work, PWA installable. Responds correctly at 280px and 430px widths.

**Flash budget:** ride page ≤ 3 KB gzipped total (HTML + JS + manifest).

### WO-15.4: SPEC & Design Doc Updates
**Goal:** Update SPEC.md to replace `/overlay` with `/ride`, restructure §11, and document the portal tab rework.

**Tasks (12 sections):**
1. §1.1 Purpose: "session overlay" → "ride page"; drop "floating training-computer" framing
2. §3.1 Three Output Paths: `📊 OVERLAY` → `📊 RIDE PAGE`
3. §3.2 Block Diagram: `Floating overlay` → `Ride page (echenon.local/ride)`
4. §3.3 Data Flow: "the overlay receives" → "the ride page receives"
5. §3.4 Module Decomposition: Update `SessionOverlay` description
6. §3.5 Foundation Integration: "WebSocket push to the overlay" → "to the ride page"
7. §4.2 Target Deployment: Keep split-screen concept, update route name
8. §11 Session Overlay: **Major rewrite** — rename to "Session UI", restructure into §11.1 Ride Page + §11.2 Portal Tabs, responsive wireframe, PWA, tab restructure
9. §12.1 Project Structure: `overlay.*` → `ride.*` in file tree
10. §13 Acceptance Tests: "overlay" → "ride page", add P12 split-screen test
11. Appendix J User Journeys: `echenon.local/overlay` → `echenon.local/ride` throughout
12. Update `docs/implementation-strategy.md` if it references overlay

**Verification:** `grep -ri 'overlay' SPEC.md` returns zero hits. All file paths in SPEC match actual project structure.

### WO-15.5: Hardware Verification — P12 Split-Screen
**Goal:** Flash updated firmware and verify portal + ride page on real hardware and real devices.

**Tasks:**
1. Tag, build, flash to bench (`/dev/cu.usbmodem101`)
2. Verify portal tab layout on phone browser (6 tabs, correct order)
3. Verify Echenon portal tab shows device status + "Open Ride Page →"
4. Verify Setup tab pills (Apps, Level Mapping, Power Tuning)
5. Open `/ride` on Lenovo Tab P12 in Android split-screen with Echelon app
6. Test PWA "Add to Home Screen" on P12
7. Test `/ride` on phone (full-screen standalone use)
8. iPad Slide Over test if iPad available
9. Verify session start/stop from `/ride` page while metrics streaming

**Verification:** Screenshot evidence committed to `docs/measurements/phase4/wo15-portal-ride/`.

### WO-15.6: Portal Bug Fixes from Hardware Verification
**Goal:** Fix 6 bugs found during WO-15.5 hardware verification and restructure tabs per owner's architectural decision:
1. **Restructure Setup → Apps tab & move Calibration to System:**
   - Bottom nav tab 5 becomes **"Apps"** (order 60, Feather `grid` icon), housing fitness app integrations.
   - Sub-pill `Strava` (order 10, `/upload_settings.js`) under parent "Apps".
   - Move `Level Mapping` and `Power Tuning` calibration pills under parent **"System"** (orders 25 and 30).
2. **CSS Dark Mode Fix:** Find-replace `--bg-card` → `--card-bg` and `--text-secondary` → `--text-muted` in `web/calibration-wizard.js`, `web/recalibration.js`, `web/upload_settings.js`.
3. **Strava BYOA UX:** In `web/upload_settings.js`, update `TAB_ID = 'tab-strava'`, reorder credentials above authorize button, disable authorize button until configured, add setup guide with dynamic callback URL. In `src/echenon/StravaUploader.cpp`, guard `_clientId.empty()`.
4. **Power Table Loading Fix:** In `web/recalibration.js`, add error handling to `fetchActiveTable()`.
5. **WITH_TIME Build Flag:** Add `-DWITH_TIME` to `platformio.ini` `ex3-waveshare-nano` build flags.
6. **Echenon Tab Content:** In `web/echenon-dashboard.js`, remove duplicate device info, show bike-specific status (BLE connection, calibration state, session state).

**Allowed Files:**
- `web/calibration-wizard.js`, `web/recalibration.js`, `web/upload_settings.js`
- `web/echenon-dashboard.js`
- `src/echenon/EchenonModule.cpp`
- `src/echenon/StravaUploader.cpp`
- `platformio.ini`

**Verification:** `pio test -e native` passes. Build succeeds for `ex3-waveshare-nano`. Manual dark mode toggle check on device.

## 4. Session Log

- **2026-10-26:** Chief Architect drafted PR-15 charter based on portal UX audit conducted live on bench with owner. Two-surface model finalised, 5-WO decomposition defined. Awaiting charter lock and execution approval.
- **2026-10-27:** PA: Read locked charter, rules, skills. Created 5 WORKORDER.md specs (WO-15.1–WO-15.5) with flat YAML schema, detailed task checklists, exact code references. Corrected portal asset naming (`.hpp` via `portalBuilder.py` auto-generation). Plan approved via `pa pr approve-plan PR-15` (5 WOs stamped). Dispatched WO-15.1 engineer on branch `pr-15-portal-ride/wo1-portal-cleanup`.
- **2026-10-27:** Chief Architect: WO-15.5 hardware verification (interactive). Device flashed v0.3.0-dev.25, portal and /ride page verified functional. 6 bugs found: (1) CSS dark mode broken (wrong variable names), (2) Setup tab same icon as System, (3) Strava auth button enabled without credentials, (4) Power table stuck on Loading, (5) WITH_TIME missing, (6) Echenon tab duplicates System tab content. Owner decision: Strava stays BYOA with improved UX; cloud proxy (Google Cloud) deferred. WO-15.6 created for bug fixes.
- **2026-10-27:** PA: Dispatched pa-engineer for WO-15.6 on pr-15-portal-ride/wo6-portal-bugfixes. Implemented all 6 tasks: T1 (CSS dark mode variables fixed in 3 JS files), T2 (restructured Setup to Apps tab with Feather grid icon and Strava sub-pill, moved calibration pills to System tab), T3 (Strava BYOA UX improvements with setup guide, credential ordering, disabled auth button, and StravaUploader empty clientId guard), T4 (recalibration power table error handling and empty state), T5 (-DWITH_TIME build flag added to ex3-waveshare-nano), T6 (Echenon tab reworked to show bike-specific BLE/calibration/session status). Verification passed (199/199 native unit tests, ex3-waveshare-nano build succeeded). Receipt rcpt_b467ac37e0ac stamped in commit 71e9222. Work order COMPLETED.

<!-- pa:graduation-summary start -->
### Graduation Summary (`PR-15` / `pr-15-portal-ride` — `GRADUATED`)

- **Completed Date:** 2026-10-27
- **Graduation Mode:** `external`
- **Latest Audit:** Round 1 — `APPROVED` (`1ae6859e239d`)

| WO-ID | Status | Attempts | Commit SHA |
|---|---|---|---|
| WO-15.1 | COMPLETED | 1 | `7dc14eeac13d` |
| WO-15.2 | COMPLETED | 1 | `6dc83ed16337` |
| WO-15.3 | COMPLETED | 1 | `61a177371b07` |
| WO-15.4 | COMPLETED | 1 | `ab3ee53044df` |
| WO-15.5 | IN_PROGRESS | 1 | `-` |
| WO-15.6 | COMPLETED | 1 | `71e9222da534` |
<!-- pa:graduation-summary end -->
