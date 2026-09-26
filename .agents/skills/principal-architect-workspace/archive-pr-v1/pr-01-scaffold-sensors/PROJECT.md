> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-01](../../pr-01-signal-logger/PROJECT.md) and [PR-02](../../pr-02-calibration-power/PROJECT.md). **Superseded by:** SPEC §4.3–§4.4, §5.1–§5.3, §12.1–§12.3 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

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
- [wo1-scaffold/WORKORDER.md](../../archive-pa-decomposition/wo1-scaffold/WORKORDER.md)
- [wo2-sensors/WORKORDER.md](../../archive-pa-decomposition/wo2-sensors/WORKORDER.md)

These are reference material — the PA should use them as input when creating
their own WO decomposition.

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
