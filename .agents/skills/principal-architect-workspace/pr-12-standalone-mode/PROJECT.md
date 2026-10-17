# PR-12: Console-Off Standalone Mode (mode 2, UC6) — v2

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-07 and PR-10 merged
> **Dev Phase:** Phase 6 — Console-off standalone
> **Depends on:** PR-07, PR-10. Unblocks PR-13
> **Design:** [DR-2](../reviews/DR-2-design-freeze.md) §2.2, §2.9; [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md) UC6

## Mandate

Implement **mode 2, console off**: standalone operation with the native console
switched off or unplugged (UC6).

1. **Pot-reference replica** — Reproduce the console's Green reference (1.66 V
   from a 2.4 kΩ source; Yellow is ground), e.g. a ≈ 4.7 kΩ / 4.7 kΩ divider from
   3.3 V, connected only when the console is absent. Green is never tied to GND.
2. **Standalone cadence and encoder supplies** — switched in only when the console
   is absent; never back-feed a console rail.
3. **Console-absent detection and mode switching**, and the standalone pot
   calibration.

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
