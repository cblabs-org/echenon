---
schema_version: 1
wo_id: WO-15.3
pr_id: PR-15
title: Ride Page — Standalone /ride
complexity: standard
status: PENDING
allowed_files:
- web/ride.html
- web/ride.js
- web/manifest.json
- src/echenon/SessionOverlay.cpp
- src/echenon/SessionOverlay.h
- include/portal_assets/ride_html.hpp
- include/portal_assets/ride_js.hpp
- include/portal_assets/manifest_json.hpp
- platformio.ini
forbidden_files:
- src/main.cpp
- src/echenon/EchenonModule.cpp
- src/echenon/EchenonModule.h
- web/echenon-dashboard.js
- web/calibration-wizard.js
- web/recalibration.js
- web/upload_settings.js
- SPEC.md
- docs/**
required_skills:
- framework-expert
- framework-portal-design
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: 9fba032ad1979acfcd2b770416007530e0dcb322874e62d48c50f317edafd7c4
completion: null
---

# WO-15.3: Ride Page — Standalone /ride

> **Parent PR:** [PR-15 PROJECT.md](../PROJECT.md)
> **Depends on:** WO-15.1 (cleanup), WO-15.2 (setup tab)
> **Branch:** `pr-15-portal-ride/wo3-ride-page` (base: `pr-15-portal-ride/wo2-setup-tab`)

---

## 1. Objective

Create a standalone HTML page at `/ride` for daily-use session control, optimised for split-screen with the Echelon fitness app (primary: Android 280–430px split-screen / iPad Slide Over; secondary: full phone screen). This is a **separate HTML page** — NOT a portal SPA tab. It is served by `SessionOverlay::registerRoutes()`.

Key features:
- Dark theme by default, large touch targets, high-contrast glanceable metrics
- WebSocket live telemetry from `/ws/echenon/live` at 2 Hz
- Session start/pause/stop via REST API
- Upload queue mini-status
- Ride history with TCX download
- PWA manifest for "Add to Home Screen"
- ⚙️ gear icon → portal

**Flash budget: ride page ≤ 3 KB gzipped total (HTML + JS + manifest)**

## 2. Strict Boundary Box

### Allowed Files
| File | Operations |
|---|---|
| `web/ride.html` | **Create** — standalone HTML page |
| `web/ride.js` | **Create** — ride page JavaScript |
| `web/manifest.json` | **Create** — PWA manifest |
| `src/echenon/SessionOverlay.cpp` | Register `/ride` route serving the page; register routes for gzipped assets |
| `src/echenon/SessionOverlay.h` | Update if needed |
| `include/portal_assets/ride_html.hpp` | **Auto-generated** by `portalBuilder.py` pre-build |
| `include/portal_assets/ride_js.hpp` | **Auto-generated** by `portalBuilder.py` pre-build |
| `include/portal_assets/manifest_json.hpp` | **Auto-generated** by `portalBuilder.py` pre-build |
| `platformio.ini` | Only if build flags needed for new assets |

### Forbidden
- Do NOT modify portal files (echenon-dashboard.js, calibration-wizard.js, etc.)
- Do NOT modify EchenonModule files
- Do NOT modify SPEC.md or docs/

## 3. Detailed Task Checklist

### 3.1 Create web/ride.html
- [ ] Standalone HTML5 page (NOT a portal SPA tab — no portal framework JS loaded)
- [ ] `<meta name="viewport" content="width=device-width, initial-scale=1.0">`
- [ ] `<link rel="manifest" href="/manifest.json">`
- [ ] `<meta name="theme-color" content="#121212">` (dark theme)
- [ ] Dark theme by default — dark background (`#121212`), light text
- [ ] Responsive layout: primary target 280–430px (CSS media queries / container queries)
- [ ] Minimal HTML structure:
  - Header: "Echenon" title + ⚙️ gear icon (link to `/`)
  - Session state badge: IDLE / ACTIVE / PAUSED (colour-coded)
  - Live metrics grid: cadence, power, speed, HR, level (big numbers)
  - Session controls: Start / Pause / Stop buttons
  - Current ride stats: elapsed time, distance, avg power
  - Upload queue mini-status
  - Ride history (last 5 rides)
- [ ] Inline critical CSS within `<style>` tag (no external stylesheet — flash budget)
- [ ] Use design-system-compatible dark colour scheme:
  - Background: `#121212`, cards: `#1e1e1e`
  - Primary accent: `#2196F3` (Material Blue)
  - Success: `#4CAF50`, Warning: `#FF9800`, Danger: `#f44336`
  - Text: `#ffffff` / `rgba(255,255,255,0.7)` for secondary
- [ ] Large touch targets (min 48px height for buttons)
- [ ] `<script src="/ride.js"></script>` at end of body

### 3.2 Create web/ride.js
- [ ] **WebSocket connection** to `/ws/echenon/live`:
  - Connect on page load
  - Parse 2 Hz JSON telemetry frames
  - Update metrics display: cadence (RPM), power (W), speed (km/h), HR (BPM), level
  - Reconnect on disconnect with exponential backoff
  - Show connection status indicator (connected / reconnecting)
- [ ] **Session controls** via REST API:
  - Start: `POST /api/session/start`
  - Pause: `POST /api/session/pause`
  - Stop: `POST /api/session/stop`
  - Update button states based on session state (only valid actions enabled)
  - Confirm before stopping an active session
- [ ] **Session state badge**:
  - Poll `/api/session/status` or derive from WebSocket data
  - IDLE (grey), ACTIVE (green pulse), PAUSED (amber)
- [ ] **Current ride stats** (during active/paused session):
  - Elapsed time (formatted HH:MM:SS, ticking)
  - Distance (km, calculated or from API)
  - Average power (W)
- [ ] **Upload queue mini-status**:
  - Fetch from `/api/strava/status`
  - Show: ✓ uploaded / ⏳ pending / ✗ failed counts
  - Compact single-line display
- [ ] **Ride history** (last 5 rides):
  - Fetch from `/api/session/history`
  - Show date, duration, avg power for each
  - TCX download link for each ride
  - Empty state: "No rides yet — start your first session!"
- [ ] **Empty states** with helpful messaging throughout
- [ ] Keep file size minimal — no frameworks, vanilla JS only

### 3.3 Create web/manifest.json
- [ ] PWA manifest:
  ```json
  {
    "name": "Echenon Ride",
    "short_name": "Ride",
    "start_url": "/ride",
    "display": "standalone",
    "background_color": "#121212",
    "theme_color": "#121212",
    "icons": [
      { "src": "/icon-192.png", "sizes": "192x192", "type": "image/png" },
      { "src": "/icon-512.png", "sizes": "512x512", "type": "image/png" }
    ]
  }
  ```
- [ ] Icons: generate minimal SVG-based PNG icons (or use inline data URIs if size allows)
- [ ] Note: icon routes may reference placeholder paths — actual icons can be added later if flash budget is tight

### 3.4 Register /ride Route in SessionOverlay
- [ ] In `SessionOverlay.cpp` at the top, add includes for the new asset headers (guarded by `#ifndef UNIT_TEST`):
  ```cpp
  #include "include/portal_assets/ride_html.hpp"
  #include "include/portal_assets/ride_js.hpp"
  #include "include/portal_assets/manifest_json.hpp"
  ```
- [ ] In `SessionOverlay::registerRoutes()`, register routes using the same `serveGzipAsset()` pattern as `/session.js`:
  ```cpp
  server.on("/ride", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      return serveGzipAsset(request, response, ride_html_gz, ride_html_gz_len, ride_html_gz_etag, "text/html");
  });
  server.on("/ride.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      return serveGzipAsset(request, response, ride_js_gz, ride_js_gz_len, ride_js_gz_etag, "application/javascript");
  });
  server.on("/manifest.json", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      return serveGzipAsset(request, response, manifest_json_gz, manifest_json_gz_len, manifest_json_gz_etag, "application/manifest+json");
  });
  ```
- [ ] The `/ride` route serves a standalone HTML page — NOT a portal SPA tab

### 3.5 Asset Registration & Auto-Generation
- [ ] The framework's `portalBuilder.py` pre-build script automatically gzips all files in `web/` and generates `.hpp` headers in `include/portal_assets/`
- [ ] After creating the web files, `pio run` will generate:
  - `include/portal_assets/ride_html.hpp` (variable: `ride_html_gz`, length: `ride_html_gz_len`, etag: `ride_html_gz_etag`)
  - `include/portal_assets/ride_js.hpp` (variable: `ride_js_gz`, length: `ride_js_gz_len`)
  - `include/portal_assets/manifest_json.hpp` (variable: `manifest_json_gz`, length: `manifest_json_gz_len`)
- [ ] **Do NOT manually create or edit gzipped asset headers** — the build handles it
- [ ] **Flash budget check:** after build, inspect the generated `.hpp` files — total `_gz_len` values across all three must be ≤ 3072 bytes

### 3.6 Flash Budget Verification
- [ ] After generating gzipped assets, verify total size:
  ```
  # Total of ride_html_gz, ride_js_gz, manifest_json_gz must be ≤ 3072 bytes
  ```
- [ ] If over budget, optimize:
  - Shorten CSS class names
  - Minify HTML/JS manually
  - Remove non-essential features
  - Use shorter variable names in JS

## 4. Verification

- [ ] `pio test -e native` passes with zero failures
- [ ] Build succeeds for the default environment (`pio run`)
- [ ] `/ride` route is registered and serves the page
- [ ] Page loads correctly as standalone HTML (not portal SPA)
- [ ] WebSocket connection establishes to `/ws/echenon/live`
- [ ] Session controls (start/pause/stop) call correct API endpoints
- [ ] Upload queue status fetches from `/api/strava/status`
- [ ] Ride history fetches from `/api/session/history`
- [ ] PWA manifest is valid (can be verified with browser DevTools)
- [ ] Total gzipped size ≤ 3 KB
- [ ] Page is responsive at 280px and 430px widths

---

## Completion Report

*(Populated by `pa-engineer` after `pa wo verify WO-15.3`)*
