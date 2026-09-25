> ⚠️ **SUPERSEDED.** This work order was part of the v1/initial programme plan. The programme was restructured on 2026-09-25 per the Chief Architect Design Audit. See the current programme at `../PROGRAM.md` and `../PROGRAM-PLAN.md`.

# PR-04: Session Recording

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (`power-engine` branch tip)
> **Spec sections:** §9.1–9.6
> **Parallel with:** PR-03 (no file overlap)

## Mandate

Implement the session recording pipeline:

1. **`SessionManager`** — auto-start on first cadence pulse, auto-end after 180s
   of zero cadence, manual start/stop override. EventBus integration for
   `session/started`, `session/ended`, `session/paused` events.
2. **`RideStore`** — PSRAM ring buffer (`ps_malloc`) holding ~27,000 data points
   (90 min at 2 Hz). 10-byte `RideDataPoint` struct.
3. **`TCXGenerator`** — streaming XML writer producing 1 Hz trackpoints (not 2 Hz)
   for Xert MPA compatibility. Zero-watt seconds included. Written to LittleFS
   at `/sessions/<timestamp>.tcx`.

After this PR merges, rides are automatically detected and recorded. TCX files
are written to flash on session end, ready for upload.

## Prior Art

- [wo6-session/WORKORDER.md](../archive-pa-decomposition/wo6-session/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
