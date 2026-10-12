# PR-14: Diagnostics & Run-Data Persistence (v1)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Release:** **v1 Passive**
> **PR target base:** `main` after PR-06 merged
> **Depends on:** PR-06 (it sets the LittleFS storage budget and owns `RideStore`); PR-02 (runaway detection)
> **Spec sections:** [SPEC](../../../../SPEC.md) §11.5 (trust boundaries), §12 (build envs); diagnostics sections to be added by this PR

## Mandate

The owner decided on 2026-10-12 to keep monitoring and diagnostics in v1.

1. **Production diagnostics module.**
   - Live tap monitoring, the event log, and a bounded decimated ride log.
   - Sized so `RideStore` (PR-06) still has its PSRAM.
   - Refactor out of `SignalLogger`. Full kHz burst capture stays in the
     `ex3-waveshare-nano-diag` env only (created in PR-03 WO1).
2. **Save to flash (option, default off).**
   - Persist runs to LittleFS as gzip, written in blocks to limit wear.
   - A configurable cap within the 2.75 MiB partition, which is shared with the
     upload queue and the audit log. The oldest run is dropped first.
   - The setting lives in the Echenon IConfigurable namespace.
3. **Query interface.**
   - `GET /api/echenon/runs` (list), `GET /api/echenon/runs/<id>.csv.gz`, `DELETE /api/echenon/runs/<id>`.
   - Time-windowed, paged ride-log and event queries (`from`, `to`, `limit`) replace whole-buffer dumps.
   - Streamed in chunks; a download must never reboot the device.
   - Optional MCP tools that mirror these queries.
   - Authentication per SPEC §11.5.
4. **Runaway alert surfacing.** The PR-02 detection appears in the portal and in the persisted event log.
5. **Tests.** Native tests for paging, the ring/cap policy and gzip framing. One interactive check: a 60-minute ride persisted and downloaded.

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED — v1
