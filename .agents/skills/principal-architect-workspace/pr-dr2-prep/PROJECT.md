# DR-2 Prep: Active-Design Bench Evidence — v2

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Owner:** Chief Architect (Tier 1). Evidence gathering for DR-2, not a phase PR (like [`pr-dr1-prep`](../pr-dr1-prep/PROJECT.md)).
> **PR target base:** `main`
> **Depends on:** DR-1 closed (bench rig); timing at the owner's discretion. Does not block v1
> **Feeds:** [DR-2 record](../reviews/DR-2-design-freeze.md) §1 items P1–P5
> **Evidence home:** `docs/measurements/`, using the register's naming convention

## Mandate

Gather the bench evidence DR-2 needs before any active board is designed. Bench
rigs only (split encoder loom, H-bridge breakout); **nothing is installed** and
no product firmware changes.

| # | Item | Why DR-2 needs it |
|---|---|---|
| P1 | **Encoder-emulation timing** toward the console: minimum quadrature pulse width and edge spacing accepted | DR-2 §2.10, §2.12 |
| P2 | **Emulation during a seek**: console behaviour when emulated clicks arrive mid-seek | DR-2 §2.10, §2.12 |
| P3 | **Wiper in the dead zone and during the stock runaway** (A09-20261012 #3) | DR-2 §2.4, §2.5 |
| P4 | **H-bridge drive with the console disconnected**: current, stall, coast, direction mapping | DR-2 §2.3, §2.5 |
| P5 | **Pot-reference replica**: ≈ 4.7 kΩ / 4.7 kΩ divider vs the console's 1.66 V / 2.4 kΩ Green source | DR-2 §2.2, §2.9 |

Any test that can drive the motor (P3, P4) needs the owner's go-ahead in the
session and a kill switch within reach.

## Work Order Ledger
<!-- PA fills this in after DR-2 -->

## Session Log
<!-- PA enriches this progressively -->

### 2026-10-12: Stub created (Chief Architect)
- Created when the owner restructured the programme into v1 Passive / v2 Active
  releases (2026-10-12, in chat; recorded in [PROGRAM.md](../PROGRAM.md)). This
  stub is **not** an approved decomposition.

## Status
**Plan Status:** PROVISIONAL — decomposed after DR-2
