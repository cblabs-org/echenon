> ⚠️ **SUPERSEDED.** This work order was part of the v1/initial programme plan. The programme was restructured on 2026-09-25 per the Chief Architect Design Audit. See the current programme at `../PROGRAM.md` and `../PROGRAM-PLAN.md`.

# PR-01: Project Scaffold + Sensor Acquisition

> **Programme:** Echenon Firmware
> **PR target base:** `main`
> **Spec sections:** §3.5, §4.3–4.7, §5.1–5.2, §12.1–12.3

## Mandate

Set up the PlatformIO project, board configuration, and `EchenonModule` skeleton.
Implement `CadenceSensor` (ISR-driven reed switch with RPM calculation) and
`ResistanceADC` (multisampled ADC with EMA filter and 1–32 level mapping).

After this PR merges, the firmware boots the Foundation, reads both sensors, and
logs clean RPM + resistance level values to serial.

## Prior Art

The Chief Architect's initial decomposition contains detailed line-item
instructions for this scope:
- [wo1-scaffold/WORKORDER.md](../archive-pa-decomposition/wo1-scaffold/WORKORDER.md)
- [wo2-sensors/WORKORDER.md](../archive-pa-decomposition/wo2-sensors/WORKORDER.md)

These are reference material — the PA should use them as input when creating
their own WO decomposition.

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
