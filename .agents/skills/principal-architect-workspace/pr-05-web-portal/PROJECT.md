# PR-05: Web Portal (Overlay + Calibration + Session History)

> **Programme:** Echenon Firmware
> **PR target base:** PR-03 + PR-04 (merge both before starting)
> **Spec sections:** §6.3 (calibration UI), §11.1–11.4
> **Parallel with:** PR-06 (partial — PR-06 only needs PR-04, not PR-03)

## Mandate

Build the complete web UI layer using the framework's gzip+PROGMEM pipeline:

1. **`SessionOverlay`** — a **standalone** page at `/overlay` (NOT a portal tab)
   designed for floating-window use on the Lenovo P12 tablet. WebSocket at 2 Hz
   for live power, cadence, HR, resistance, rolling power chart, session controls.
2. **Calibration portal tab** — potentiometer calibration wizard (set min/max
   from live ADC), power tier selector dropdown, community matrix selection,
   custom CSV upload, reference power meter coefficient fitting.
3. **Session History portal tab** — list past sessions, show summary stats,
   download TCX, discard sessions.

All JS assets go through `web/` → `portalBuilder.py` → `include/portal_assets/`
→ `serveGzipAsset()`. **No files in `data/www/`.**

After this PR merges, users can open `echenon.local/overlay` in a floating
browser window to see live training data, calibrate power via the portal, and
review past sessions.

## Prior Art

- [wo7-overlay/WORKORDER.md](../archive-pa-decomposition/wo7-overlay/WORKORDER.md)
- [wo9-calibration-portal/WORKORDER.md](../archive-pa-decomposition/wo9-calibration-portal/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
