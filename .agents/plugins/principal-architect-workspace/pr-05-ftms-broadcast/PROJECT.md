---
schema_version: 1
charter:
  pr_id: "PR-05"
  slug: "pr-05-ftms-broadcast"
  title: "FTMS Telemetry, read-only"
  branch: "feat/pr-05-ftms-broadcast"
  base_branch: "main"
  allowed_modules:
    - "src/echenon/"
    - "docs/"
    - "framework/"
    - "platformio.ini"
  charter_locked: false
status: PLANNED
---

## 2. Scope

_See legacy content below._


# PR-05: FTMS Telemetry, read-only (Phase 3 — FTMS telemetry) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 and PR-03 merged
> **Dev Phase:** Phase 3 — FTMS telemetry
> **Depends on:** PR-02, PR-03
> **Spec sections:** [SPEC](../../../../SPEC.md) §7.1–§7.8 (FTMS server, read-only in v1, Indoor Bike Data, multi-client), §8.5 (coexistence), §13.6 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3
> **Parallel with:** PR-04a, PR-06

## 1. Mandate

Add a **read-only** FTMS GATT service to the BLE stack from PR-03. The bike becomes
visible to external fitness apps. **v1 has no resistance control of any kind**
(owner decision 2026-10-12): Control Point targets are v2 (PR-08).

1. **`FTMSServer`** (`IBLEServiceProvider` + `ITickable`) — FTMS service `0x1826`,
   Indoor Bike Data notifications at 2 Hz, Fitness Machine Feature with
   **Target Setting Features = 0** (SPEC §7.3).
2. **FTMS v1.0 conformance check (first WO)** — Read the FTMS v1.0 service
   requirement table and record the answer with a citation:
   - **Control Point `0x2AD9`:** if it may be omitted when no target settings are
     supported, **v1 omits it**. If it is required, expose it with Request Control
     only and answer every target opcode with **0x02** (Op Code Not Supported).
   - **`0x2AD6` / `0x2AD8` / Fitness Machine Status `0x2ADA`:** whether each is
     mandatory, optional or excluded when the matching target-setting bit is clear.
     Include or omit each accordingly.
   *(Chief Architect recollection, unverified: the Control Point is conditional
   on a target setting being supported, and the range characteristics are tied
   to their target-setting bits. Verify against the spec, not this note.)*
   Target opcodes and Fitness Machine Status are v2 (PR-08).
3. **App behaviour with read-only FTMS** — For Zwift, TrainerRoad, Xert EBC and
   at least two others from SPEC §7.7, record: pairing, telemetry shown, and how
   the app behaves when it cannot control resistance (falls back to a manual /
   "level" mode, refuses ERG workouts, warns, or fails to pair). Evidence committed.
4. **Progressive coexistence test** — 3 peripheral connections (Echelon tablet on HR,
   plus FTMS clients) and the HR central with WiFi and the portal active (SPEC §13.6
   test 3.12). Confirm the 4-connection / 6-slot budget and record free DRAM (SPEC §7.8). The final coexistence test
   belongs to PR-09.

**Hardware:** No new hardware. The PR-01 fly-wire / Y-harness rig is sufficient;
the PR-04a board works too if it has landed. App tests (Zwift, Xert, TrainerRoad on
the P12 or another client) are `complexity: interactive` WOs with evidence committed.

After this PR merges, fitness apps see **`Echenon-XXXX`** and display live power,
cadence, resistance level and HR. No app can change resistance.

## Prior Art

- [wo4-ftms/WORKORDER.md](../archive-pa-decomposition/wo4-ftms/WORKORDER.md) — predates the B-01 corrections and the v1 read-only decision
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## 3. Work Order Ledger
<!-- PA fills this in during planning -->

## 4. Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
