---
schema_version: 1
charter:
  pr_id: "PR-08"
  slug: "pr-08-erg-control"
  title: "ERG + FTMS Control Point Targets + Resistance Setting"
  branch: "feat/pr-08-erg-control"
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


# PR-08: ERG + FTMS Control Point Targets + Resistance Setting (Phase 6) — v2

> **Release:** **v2 "Active"** (provisional; decomposed after DR-2). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-10 and PR-05 merged
> **Dev Phase:** Phase 6 — ERG + Control Point
> **Depends on:** PR-10 (and through it PR-07, DR-2), PR-05. Unblocks PR-11
> **Spec sections:** [SPEC](../../../../SPEC.md) §5.6 (operating modes, ERG), §7.3–§7.4 (Feature bits, Fitness Machine Status, Control Point `0x2AD9`), §11.5 (trust boundaries), §13.9 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 6

## 1. Mandate

Implement closed-loop ERG mode and the FTMS Control Point targets. v1's PR-05 is
**read-only** (Target Setting Features = 0; the Control Point is omitted if FTMS
v1.0 allows, otherwise it rejects every target opcode). This PR adds or extends
`0x2AD9` (Request Control plus target opcodes), sets the Target Setting Features
bits, and drives resistance through the PR-10 relay / PR-07 controller.

1. **`ERGController`** — State machine **ACTIVE / SUSPENDED_BY_RIDER / RELEASED**
   (audit B-02). Cadence smoothing, setpoint deadband and rate limit.
2. **Control Point target opcodes** — Set Target Power and Set Target Resistance
   Level (plus Reset), driving the PR-07 position controller via the PR-10 relay path.
3. **Fitness Machine Status (`0x2ADA`)** — 0x08 Target Power Changed and 0x07 Target
   Resistance Level Changed on target changes; 0xFF Control Permission Lost on release
   (SPEC §5.6). The first WO confirms the opcode values against FTMS v1.0.
4. **Knob override** — A knob turn moves ERG to SUSPENDED_BY_RIDER. The state latches
   until the client sends a new Request Control or Reset, or until
   `[DR-2/PR-08: hold time]` expires. Target writes that arrive while suspended are
   stored, not applied.
5. **Web endpoint** — `POST /api/resistance`, authenticated, Override mode only.

ERG ride tests are `complexity: interactive` WOs with evidence committed.

**Verification:** ERG setpoint reached within `[DR-2/PR-08: settle time]` without
oscillation, validated against an independent power reference rather than
Echenon's own estimate (audit P-08); position error within the level resolution
measured in PR-07. Knob override suspends ERG and latches as specified. Status
notifications are sent on every target change and on release.

## Prior Art

- [Archived PR-06 (ERG + Strava)](../archive-pr-v2/pr-06-erg-strava/PROJECT.md)

## 3. Work Order Ledger
<!-- PA fills this in during planning -->

## 4. Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** PROVISIONAL — decomposed after DR-2
