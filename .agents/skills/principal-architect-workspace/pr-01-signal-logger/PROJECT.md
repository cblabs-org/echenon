# PR-01: Signal Logger (Phase 0 — Passive Observation)

> **Programme:** Echenon Firmware
> **PR target base:** `main`
> **Dev Phase:** Phase 0 — Observe
> **Spec sections:** §3.5, §4.3–4.7, §12.1–12.3
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 0

## Mandate

Set up the PlatformIO project, board configuration, and `EchenonModule` skeleton.
Implement `SignalLogger` that reads ALL 12 wires from the bike's looms (9-pin +
3-pin) at high frequency with microsecond timestamps. Build a live web dashboard
and CSV data export.

**Hardware:** Fly wires only — alligator clips to the existing loom. No wires cut,
no intercept board built. Motor sense pins via 30kΩ/10kΩ voltage dividers for
bidirectional protection.

**GATE:** Before connecting motor sense wires (GPIO 10/11), the oscilloscope motor
voltage verification must confirm 3.3V DC (not 9V PWM).

After this PR merges, the firmware boots, reads all bike signals, displays them
live in a browser dashboard, and exports ride data as CSV.

## Prior Art

- [wo1-scaffold/WORKORDER.md](../archive-pa-decomposition/wo1-scaffold/WORKORDER.md)
- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md)
- [Old PR-01 scope](../archive-pr-v1/pr-01-scaffold-sensors/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
