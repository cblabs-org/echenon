# PR-08: ERG Mode + FTMS Control Point

> **Programme:** Echenon Firmware
> **PR target base:** `main` after PR-05 + PR-07 merged
> **Dev Phase:** Phase 6 — Control
> **Spec sections:** §5.6, §7.4

## Mandate

Implement closed-loop ERG mode.

1. **`ERGController`** — State machine (ACTIVE / SUSPENDED_BY_RIDER / RELEASED). Cadence smoothing + setpoint deadband.
2. **FTMS Control Point Integration** — Set Target Power, Set Target Resistance.
3. **Fitness Machine Status** — Send status notifications on each ERG state transition.
4. **Override Semantics** — Knob override immediately suspends ERG.

**Verification:** ERG setpoint reached within X s without oscillation, position error ≤ 1 level. Knob override suspends ERG. Status notifications sent correctly.

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
