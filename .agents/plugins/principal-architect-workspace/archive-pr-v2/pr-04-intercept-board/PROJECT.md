> ⚠️ **SUPERSEDED — prior art only.** This was part of the v2 (7-PR) programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-04a](../../pr-04a-passive-tap/PROJECT.md) (passive tap board) and [PR-07](../../pr-07-mitm-motor/PROJECT.md) (MITM board, motor control). **Superseded by:** SPEC §4.6–§4.8, §5.3–§5.6 and hardware spec §7–§8, all as revised at DR-1 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# PR-04: Intercept Board + Motor Control (Phase 3 — MITM)

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (`calibration-power` branch tip)
> **Dev Phase:** Phase 3 — Motor Interception
> **Spec sections:** §4.6–4.8, §5.3–5.6
> **Strategy:** [`docs/implementation-strategy.md`](../../../../../docs/implementation-strategy.md) Phase 3
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../../docs/hardware_specification.md)

## Mandate

Build the intercept board and implement motor control firmware. **This is the
highest-risk PR** — it's where we physically modify signal paths and drive a
motor for the first time.

### Phase 3a — Intercept (Transparent Relay)
1. **`InterceptController`** — Read console motor sense (GPIO 10/11 via voltage
   dividers), relay to L9110S (GPIO 6/7). User should not notice any difference.

### Phase 3b — Override (Direct Control)
2. **`ResistanceEncoder`** — Quadrature decoder for the red knob.
3. **`ResistanceMotor`** — L9110S driver with bang-bang position controller using
   pot ADC feedback. 100 resistance levels.
4. **`ConfigManager`** — Mode switching (Intercept/Override) + NVS persistence.

**Hardware:** Build the intercept board per `docs/hardware_specification.md`.
Perfboard with ESP32 carrier, L9110S, MP1584EN, JST connectors, R1–R5, C1–C2.
Split motor wires. Install between bike frame and console.

After this PR merges, Intercept Mode works identically to stock, and Override
Mode provides 100-level direct resistance control.

## Prior Art

- [Old PR-01 scope](../../archive-pr-v1/pr-01-scaffold-sensors/PROJECT.md) (motor was in scaffold)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
