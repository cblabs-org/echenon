# PR-04: Intercept Board + Motor Control (Phase 3 — MITM)

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (`calibration-power` branch tip)
> **Dev Phase:** Phase 3 — Motor Interception
> **Spec sections:** §4.5–4.7, §5.3–5.6
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md)

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

- [Old PR-01 scope](../archive-pr-v1/pr-01-scaffold-sensors/PROJECT.md) (motor was in scaffold)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
