---
name: echenon-firmware-wo7
description: Handoff instructions for Work Order 7 — Session Overlay
complexity: architect
---

# WO7 — Session Overlay (Standalone Web Page)

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `SessionOverlay` — a standalone web page at `/overlay` designed as a floating training computer. It shows live power, cadence, HR, resistance, rolling power chart, and session controls via 2 Hz WebSocket push. Also implement a `Session` portal tab for session history within the framework's SPA.

### Strict Boundary Box
- **DO NOT** implement Strava upload logic — that is WO8
- **DO NOT** implement calibration UI — that is WO9
- **DO NOT** modify sensor, power, BLE, or session engine code (WO2–WO6)
- The overlay is a standalone HTML page — NOT a portal tab
- The Session History IS a portal tab using IWebContributor

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §11 (Session Overlay), §11.1–11.4
- Read the framework-expert skill's web portal system docs:
  - `systems/web-portal/overview.md` — SPA architecture
  - `systems/web-portal/html-js-fragments.md` — gzip+PROGMEM pipeline (MANDATORY)
  - `systems/web-portal/routes.md` — PsychicHttp route patterns
  - `systems/web-portal/shared-utilities.md` — Design system tokens and components
- Read the framework design system skill for branding

### Line-Item Instructions

- [ ] Create `web/overlay.html`:
  - Minimal standalone HTML page (NOT a portal tab — has its own `<html>`, `<head>`, `<body>`)
  - Viewport configured for small floating window (mobile-first responsive)
  - Loads `overlay.js` and `overlay.css`
  - Dark theme matching the framework design system

- [ ] Create `web/overlay.js`:
  - WebSocket client: connects to `ws://<host>/ws/session`
  - Renders live data on each 2 Hz message:
    - Power (large, central), cadence, heart rate, resistance level bar
    - Session state indicator (● REC / ○ Idle)
    - Elapsed time counter
    - Start/Stop button (calls REST API)
    - HR zone indicator (Z1–Z5 colour-coded)
    - Running averages and peaks
  - Rolling power chart: lightweight `<canvas>` drawing, last 5 minutes
  - Post-ride summary view when session ends (avg/max stats, upload status)
  - Auto-reconnect on WebSocket disconnect

- [ ] Create `web/overlay.css`:
  - Dark theme, high-contrast for glanceability during exercise
  - Large numbers (power, HR) — readable from arm's length on a tablet
  - Responsive: works in a small floating window

- [ ] Create `web/session.js`:
  - Standard portal tab JS following the `tab-changed` event contract
  - IIFE wrapper, prefixed element IDs (`session-*`)
  - Session history list: fetch from `GET /api/session/history`
  - Each entry shows: date, duration, avg power, upload status
  - Download button per session: `GET /api/session/download?id=<id>&format=tcx`
  - Discard button per session

- [ ] Create `src/echenon/SessionOverlay.h` / `.cpp`:
  - Implement `IWebContributor`:
    - `registerRoutes()` — serve overlay HTML/JS/CSS via `serveGzipAsset()`, register REST API endpoints, create WebSocket handler
    - `registerTabs()` — register "Session" tab with `/session.js` script
  - REST endpoints: `POST /api/session/start`, `POST /api/session/stop`, `GET /api/session/status`, `GET /api/session/history`, `GET /api/session/download`
  - WebSocket endpoint `/ws/session`: push JSON at 2 Hz with current metrics
  - PSRAM allocation for chart buffer: `ps_malloc()`

- [ ] Wire into `EchenonModule`:
  - Add `SessionOverlay` as member
  - Register as `IWebContributor`
  - Feed live data to overlay's WebSocket push in `tick()`

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. The gzip+PROGMEM pipeline must be verified: `web/` files are compiled to `include/portal_assets/` headers. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
