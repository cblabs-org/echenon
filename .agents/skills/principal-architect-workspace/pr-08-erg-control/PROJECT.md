# PR-08: ERG Mode + FTMS Control Point (Phase 6 — ERG + Control Point)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-05 and PR-07 merged
> **Dev Phase:** Phase 6 — ERG + Control Point
> **Depends on:** PR-05, PR-07
> **Spec sections:** [SPEC](../../../../SPEC.md) §5.6 (operating modes, ERG), §7.3–§7.4 (Feature bits, Fitness Machine Status, Control Point `0x2AD9`), §11.5 (trust boundaries), §13.9 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 6

## Mandate

Implement closed-loop ERG mode and the target side of the FTMS Control Point. PR-05
already exposes `0x2AD9`, handles 0x00 Request Control and rejects target opcodes;
this PR replaces the rejection with real handling when the intercept board is in
control.

1. **`ERGController`** — State machine **ACTIVE / SUSPENDED_BY_RIDER / RELEASED**
   (audit B-02). Cadence smoothing, setpoint deadband and rate limit.
2. **Control Point target opcodes** — Set Target Power and Set Target Resistance
   Level (plus Reset), driving the PR-07 position controller.
3. **Fitness Machine Status (`0x2ADA`)** — 0x08 Target Power Changed and 0x07 Target
   Resistance Level Changed on target changes; 0xFF Control Permission Lost on release
   (SPEC §5.6). The first WO confirms the opcode values against FTMS v1.0.
4. **Knob override** — A knob turn moves ERG to SUSPENDED_BY_RIDER. The state latches
   until the client sends a new Request Control or Reset, or until
   `[DR-1/PR-08: hold time]` expires. Target writes that arrive while suspended are
   stored, not applied.
5. **Web endpoint** — `POST /api/resistance`, authenticated, Override mode only.

ERG ride tests are `complexity: interactive` WOs with evidence committed.

**Verification:** ERG setpoint reached within `[DR-1/PR-08: settle time]` without
oscillation, validated against an independent power reference rather than
Echenon's own estimate (audit P-08); position error within the level resolution
measured in PR-07. Knob override suspends ERG and latches as specified. Status
notifications are sent on every target change and on release.

## Prior Art

- [Archived PR-06 (ERG + Strava)](../archive-pr-v2/pr-06-erg-strava/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
