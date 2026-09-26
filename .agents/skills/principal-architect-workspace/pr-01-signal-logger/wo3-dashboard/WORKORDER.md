---
name: signal-logger-wo3
description: Handoff instructions for Work Order 3 — Live dashboard, CSV export UI, mDNS
complexity: standard
---

# WO3 — Live Dashboard, CSV Export, mDNS

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Build the Echenon dashboard — a portal tab in the ESP32 App Foundation web
portal that shows live signal data, provides CSV download buttons, a burst
trigger button, and capture status. Verify `echenon.local` mDNS reachability
on the Lenovo P12 tablet (or note the IP fallback).

After this WO the user can open the portal in a browser, see all 10 channel
readings updating in real time, trigger burst captures, and download ride
log / burst / event CSVs.

## Context

- **SignalLogger endpoints (from WO2):**
  - `GET /api/echenon/status` → JSON: `{ride_log_rows, burst_active, uptime_ms, sample_rate, free_psram}`
  - `GET /api/echenon/ride-log.csv` → chunked CSV
  - `GET /api/echenon/events.csv` → chunked CSV
  - `GET /api/echenon/burst.csv` → chunked CSV
  - `POST /api/echenon/burst/start` → JSON body `{duration_ms: N}`

- **Framework portal system:**
  - Consumer HTML/JS/CSS files go in `web/` at the project root.
  - `portalBuilder.py` (PIO pre-build script) compresses them into
    `include/portal_assets/` as C header byte arrays.
  - Tabs are registered via `ITabRegistry::addTab()` in `registerTabs()`.
  - The tab's `script` path must match the route where the JS asset is served.
  - The framework already serves the portal shell with navigation; consumer
    tabs appear alongside the framework's built-in tabs (WiFi config, etc.).

- **mDNS:** The framework enables mDNS automatically. The device hostname
  defaults to the board name. For Echenon we want `echenon.local`. Check
  whether the framework sets it from `app.setAppName("Echenon")` or if it
  needs explicit configuration.

## Strict Boundary Box

- **IN SCOPE:** `web/` directory (create: HTML/JS/CSS for the dashboard),
  `src/echenon/EchenonModule.cpp` (fill in `registerTabs()`), minor additions
  to `src/echenon/EchenonModule.h` if needed.
- **OUT OF SCOPE:** `SignalLogger.h/.cpp` (routes are already registered),
  `platformio.ini`, `hardware/`, `SPEC.md`, `docs/`, `test/`. Do not modify
  the framework submodule.
- **Do not create additional branches.**

## Tooling Constraints

- Use the `framework-expert` skill for portal and tab registration questions.
- Use the `pio-manager` skill and PlatformIO MCP tools for building and
  flashing.
- Build with `pio run -e ex3-waveshare-nano`. Must compile with 0 errors.
- Flash and verify the portal is accessible in a browser.

## Line-Item Instructions

### A. Dashboard HTML/JS/CSS — `web/`

- [ ] Create `web/echenon-dashboard.html` (or `.js` depending on the
  framework's tab loading convention — check how existing consumer tabs
  work in the template-app or sample-app). The dashboard should contain:

  1. **Status bar:** Capture state (running/stopped), uptime, ride log row
     count, free PSRAM, sample rate. Poll `/api/echenon/status` every 2–5 s.

  2. **Live channel display:** Show all 10 channels with their latest
     decimated min/mean/max values. Update periodically from the status
     endpoint or a lightweight custom endpoint. Channel names from the
     pin map:
     | Ch | Name |
     |---|---|
     | 0 | Pot Wiper (Blue) |
     | 1 | Pot HI (Yellow) |
     | 2 | Pot LO (Green) |
     | 3 | Cadence Return (Grey) |
     | 4 | Cadence (Orange) |
     | 5 | Motor+ (Red) |
     | 6 | Motor− (Black) |
     | 7 | Encoder A (Brown) |
     | 8 | Encoder B (Orange) |
     | 9 | Encoder VCC (Red) |

  3. **Burst trigger:** A button to POST to `/api/echenon/burst/start` with
     a configurable duration (text input, default 10000 ms). Show burst
     status (active/idle) from the status endpoint.

  4. **CSV download links:** Three buttons/links:
     - "Download Ride Log" → `/api/echenon/ride-log.csv`
     - "Download Events" → `/api/echenon/events.csv`
     - "Download Burst" → `/api/echenon/burst.csv`

  5. **Event log summary:** Show the count of edge events, or the last
     few events from the status endpoint (if available). This is optional —
     the CSV download is the primary interface for events.

- [ ] Keep the UI simple and functional — this is a diagnostic dashboard,
  not a consumer product. Plain HTML with minimal CSS is fine. No external
  dependencies (no CDN, no npm, no React). Vanilla JS only.

- [ ] The total size of all web assets should stay under 50 KB uncompressed
  (gzip brings this to ~10–15 KB in the generated headers).

### B. Tab registration — `EchenonModule.cpp`

- [ ] In `EchenonModule::registerTabs(ITabRegistry& registry)`, register the
  dashboard tab:
  ```cpp
  registry.addTab("Echenon", nullptr, 50, nullptr, "/echenon-dashboard.js");
  ```
  Adjust the `script` path to match whatever file naming you chose in `web/`.

### C. mDNS verification

- [ ] Check whether the framework automatically sets the mDNS hostname from
  `app.setAppName("Echenon")`. If not, look for a way to configure it.
- [ ] After flashing, verify `echenon.local` resolves:
  - From a laptop: `ping echenon.local` or `curl http://echenon.local/`
  - If a Lenovo P12 tablet is available, open the URL in Chrome.
  - If mDNS doesn't resolve, note the IP address shown in serial output as
    a fallback. This is acceptable for Phase 0.
- [ ] Document the mDNS result in the Completion Report.

### D. Build and flash verification

- [ ] `pio run -e ex3-waveshare-nano` — must compile with 0 errors.
- [ ] Flash to the board.
- [ ] Connect to the portal:
  1. If WiFi is configured, navigate to `echenon.local` or the IP address.
  2. If on the setup AP, connect to the AP and navigate to `192.168.4.1`.
- [ ] Verify:
  - The Echenon tab appears in the portal navigation.
  - The status bar updates with ride log count and PSRAM.
  - The burst trigger button works (status shows burst active).
  - CSV download links produce valid CSV files.

## Verification Requirement

- Build compiles with 0 errors.
- Portal is accessible and the Echenon tab is visible.
- Status updates, burst trigger, and CSV downloads work.
- Before committing, ask the user if they are happy with the changes.

## Issue Surfacing

If you discover a defect, upstream bug, or concern *outside your boundary box*,
surface it in your Completion Report below — do not file issues directly.

---

## Completion Report (written by the worker at task completion)

<!-- Append the completion report here when done. Use this template: -->

<!--
### Status
complete | partial | blocked

### Changes Made
- `path/to/file` — what changed and why

### Build Output
- `pio run -e ex3-waveshare-nano`: PASS/FAIL
- Flash and portal test: PASS/FAIL
- Dashboard tab visible: YES/NO
- Status polling: YES/NO
- Burst trigger: YES/NO
- CSV downloads: YES/NO

### mDNS Result
- `echenon.local` resolves: YES/NO
- IP fallback: x.x.x.x
- Tested on: laptop / P12 tablet / both

### Decisions Made
(list any non-obvious decisions)

### Dead Ends & Warnings
(things that didn't work)

### Out-of-Scope Findings
(defects or concerns outside the boundary box)

### Blockers
(none if status is 'complete')
-->
