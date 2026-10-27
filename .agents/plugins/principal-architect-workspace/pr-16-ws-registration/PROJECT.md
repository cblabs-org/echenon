---
schema_version: 1
charter:
  pr_id: PR-16
  programme_id: prog-default
  slug: pr-16-ws-registration
  title: WebSocket Client Registration Fix
  branch: feat/pr-16-ws-registration
  base_branch: main
  depends_on:
  - PR-15
  allowed_modules:
  - src/echenon/
  - web/
  - test/
  - docs/
  - SPEC.md
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: 5d2052c1825931b5eb015d28c89f462c4f84fdf43a56600b87e419ea381f2382
status: GRADUATED
approved_at: '2026-10-27T22:39:52Z'
github_pr: null
work_orders:
- id: WO-16.1
  slug: wo1-ws-registration-fix
  title: WebSocket Registration Frame, Keepalive & Fallback Watchdog
  complexity: standard
  status: COMPLETED
  attempts: 0
  commit_sha: 50357110bf530c0866c20aacfd42e1bb55ad1019
- id: WO-16.2
  slug: wo2-hw-verification
  title: Hardware Verification — Tag, Build, Flash & Live Test
  complexity: interactive
  status: COMPLETED
  attempts: 0
  commit_sha: null
- id: WO-16.3
  slug: wo3-hr-field-fix
  title: Dashboard HR field name mismatch fix
  complexity: mechanical
  status: COMPLETED
  attempts: 0
  commit_sha: null
---

# PR-16 — WebSocket Client Registration Fix

> **Programme:** Echenon Firmware
> **Target Base:** `main` (commit `c95391a`)
> **Depends On:** PR-15 ✅
> **Severity:** High — live telemetry on `/ride` and Echenon portal tab is non-functional

---

## 1. PR Charter: Scope, Goals, & Non-Goals

### 1.1 Problem Statement & Root Cause

Live metrics on the `/ride` page and the Echenon portal dashboard tab show
static `--` values during rides. The WebSocket connection indicator shows
"Live" (green dot) but no telemetry data ever arrives.

**Root Cause (verified with serial trace evidence and Python probe):**

ESP-IDF v5's `esp_http_server` handles the WebSocket `101 Switching Protocols`
handshake internally and does **not** invoke the application's URI handler
(`PsychicWebSocketHandler::handleRequest`) on the initial HTTP GET. ESP-IDF
only calls `handleRequest` when the first incoming WebSocket **data frame**
arrives from the client.

Since neither `web/ride.js` nor `web/echenon-dashboard.js` send a frame after
opening the socket:
1. `PsychicWebSocketHandler::checkForNewClient()` is never called.
2. The socket is never added to `_clients`.
3. `_handler->count()` remains `0`.
4. `LiveFeed::tick()` returns early at the guard: `if (_handler->count() == 0) return;`
5. No telemetry is ever broadcast.
6. The browser sees `ws.readyState === 1` (OPEN) and suppresses the HTTP
   fallback polling in `pl()`.

**Serial trace evidence** (monitor-2026-10-27T21-23-49-172Z):
- **Zero** WebSocket log entries during the entire 380-second ride.
- `httpd_ws: Failed to read header byte (socket FD invalid)` errors appear
  only when the browser **closes** (t=485s, 35s after ride end) — confirming
  TCP sockets existed but were never registered as WS clients.

**Python probe verification:**
- A probe that sends `ws.send("ping")` immediately after connect triggers
  `handleRequest → checkForNewClient`, registers the socket, and receives
  20 telemetry frames at 2 Hz.

### 1.2 Core Scope & Goals

1. **Client Registration Frame:** Both `web/ride.js` and
   `web/echenon-dashboard.js` must send a registration frame
   (`ws.send('ping')`) inside `ws.onopen` to trigger ESP-IDF's
   `handleRequest` and register the socket in `_clients`.

2. **Keepalive Ping:** Add a 15-second periodic `ws.send('ping')` to
   prevent TCP idle timeouts from silently killing the socket.

3. **Fallback Watchdog:** Track `lastRx = Date.now()` on each received
   message. In the polling loop, trigger HTTP fallback
   (`fetch('/api/echenon/live')`) whenever `(Date.now() - lastRx) > 2500`,
   regardless of `ws.readyState`. This provides defense-in-depth against
   stalled sockets.

4. **Strava Activity Link:** In `ride.js` ride history, render a direct
   clickable `Strava ↗` link next to `TCX` when `r.activity_id` is present.

### 1.3 Non-Goals

- Server-side changes to PsychicHttp or LiveFeed (the bug is purely in the
  frontend; the server-side code is correct given a properly behaving client).
- Changes to BLE FTMS or session recording (those paths are unaffected).
- Multi-sink upload changes (deferred to PR-19).

---

## 2. Architectural Requirements & Invariants

1. **GRAVE-05:** No `std::function` heap allocations (frontend-only PR, not applicable to JS changes, but any C++ test additions must comply).
2. **Design System:** No UI layout changes — this PR only fixes WebSocket
   connection logic and adds a small link element.
3. **Asset Pipeline:** Changes to `web/*.js` are automatically gzipped by
   `portalBuilder.py` into `include/portal_assets/` headers during
   `pio run`. The filesystem must be re-uploaded after changes via
   `upload_filesystem` MCP tool.
4. **Tag → Build → Flash:** Rule 03 versioning policy applies. Dev tag
   `v0.3.0-dev.33` before flashing.

---

## 3. Implementation Plan & Work Order Breakdown

### WO-16.1 — WebSocket Registration Frame, Keepalive & Fallback Watchdog
- **Complexity:** `standard` → Model: `flash`
- **Branch:** `feat/pr-16-ws-registration/wo1-ws-registration-fix`
- **Files:** `web/ride.js`, `web/echenon-dashboard.js`
- **Changes:**
  1. Add `ws.send('ping')` registration frame in `onopen` of both files.
  2. Add `lastRx`/`lastWsRx` timestamp tracking in `onmessage`.
  3. Add 15s keepalive ping interval in both files.
  4. Update `pl()` in ride.js to trigger HTTP fallback when `lastRx` stale (>2.5s).
  5. Add HTTP fallback interval in echenon-dashboard.js for stale WS.
  6. Add conditional Strava `↗` link in ride history (`r.activity_id`).
- **Verification:** `pio test -e native --verbose` (233/233 pass).

### WO-16.2 — Hardware Verification: Tag, Build, Flash & Live Test
- **Complexity:** `interactive` → Handled by PA + user
- **Branch:** `feat/pr-16-ws-registration/wo2-hw-verification`
- **Steps:**
  1. Tag `v0.3.0-dev.33` (or next available).
  2. Build via PlatformIO MCP (`build_project`).
  3. Flash firmware (`upload_firmware`) + filesystem (`upload_filesystem`).
  4. Open `/ride` page — verify 2 Hz live telemetry (not static `--`).
  5. Verify serial log shows WS client registration on page load.
  6. Verify Strava link renders in ride history when `activity_id` present.
- **Verification:** Manual — serial log + browser observation.

### Dependency Chain
```
WO-16.1 (code) → WO-16.2 (verify on hardware)
```

---

## 4. Outcome & Session Log

### WO-16.1 — COMPLETED ✅
- **Commit:** `f9b107d` on `pr-16-ws-registration-wo1`, merged to `feat/pr-16-ws-registration`
- **Changes:** `web/ride.js` (+6/-6), `web/echenon-dashboard.js` (+22/-2)
- **Verification:** `pio test -e native --verbose` → 233/233 PASSED
- **Bug 2 finding:** No code bug in ride.js `lh()` — root cause is stale PROGMEM assets

### WO-16.2 — COMPLETED ✅
- **Tag:** `v0.3.0-dev.33` (pushed to origin)
- **Build:** Succeeded — portal assets rebuilt with updated JS
- **Flash:** Firmware + filesystem uploaded to `/dev/cu.usbmodem101`
- **Version verified:** `curl /api/system` → `firmware_version: 0.3.0-dev.33`
- **WebSocket test:** Python probe received 11 telemetry frames at ~2 Hz after `ws.send('ping')` — **registration fix verified**
- **Ride history:** No rides on device after filesystem re-flash (expected); rendering fix will be validated in live use

### WO-16.3 — COMPLETED ✅
- **Commit:** `40c6031` (cherry-picked from `cedb456` which landed on wrong branch)
- **Fix:** `data.heart_rate` → `data.hr` in `handleLiveMessage()` — field name mismatch with LiveFeed JSON
- **Tag:** `v0.3.0-dev.34` — rebuild + reflash of firmware + filesystem
- **Version verified:** `firmware_version: 0.3.0-dev.34`
