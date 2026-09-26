# PR-05: FTMS Telemetry (Phase 3 — FTMS telemetry)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 and PR-03 merged
> **Dev Phase:** Phase 3 — FTMS telemetry
> **Depends on:** PR-02, PR-03
> **Spec sections:** [SPEC](../../../../SPEC.md) §7.1–§7.8 (FTMS server, Control Point, Indoor Bike Data, multi-client), §8.5 (coexistence), §13.6 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3
> **Parallel with:** PR-04a, PR-06

## Mandate

Add the FTMS GATT service to the BLE stack from PR-03. The bike becomes visible to
external fitness apps.

1. **`FTMSServer`** (`IBLEServiceProvider` + `ITickable`) — FTMS service `0x1826`,
   Indoor Bike Data notifications at 2 Hz, Feature, Supported Resistance Level Range
   (`0x2AD6`) and Supported Power Range (`0x2AD8`). Resistance Target Setting is bit 2
   and Power Target Setting bit 3 of the Target Setting Features field (SPEC §7.3);
   both are mode-dependent (clear on the passive board and in Intercept mode).
2. **Control Point `0x2AD9` — access layer only** (audit B-01):
   - exposes the characteristic and handles **0x00 Request Control**;
   - answers every target opcode with result code **0x05** (Control Not Permitted)
     in Intercept mode and **0x02** (Op Code Not Supported) otherwise.
   Target opcodes and Fitness Machine Status belong to PR-08.
3. **Progressive coexistence test** — 3 peripheral connections (Echelon tablet on HR,
   plus FTMS clients) and the HR central with WiFi and the portal active (SPEC §13.6
   test 3.12). Confirm the 4-connection / 6-slot budget and record free DRAM (SPEC §7.8). The final coexistence test
   belongs to PR-09.

**Hardware:** No new hardware. The PR-01 fly-wire / Y-harness rig is sufficient;
the PR-04a board works too if it has landed. App tests (Zwift, Xert, TrainerRoad on
the P12 or another client) are `complexity: interactive` WOs with evidence committed.

After this PR merges, fitness apps see **`Echenon-XXXX`** and display live power,
cadence, resistance and HR.

## Prior Art

- [wo4-ftms/WORKORDER.md](../archive-pa-decomposition/wo4-ftms/WORKORDER.md) — predates the B-01 corrections and the Control Point split
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
