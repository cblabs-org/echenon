# PR-07: MITM Board + Motor Control + Safety

> **Programme:** Echenon Firmware
> **PR target base:** `main` after PR-02 merged, DR-1 approved
> **Dev Phase:** Phase 5 — Intercept
> **Spec sections:** §4.6–4.8, §5.3–5.6

## Mandate

Intercept board build per revised hardware spec (post DR-1). Implement cam-aware position controller.

1. **Hardware Scope** — Full intercept board with motor split, fail-safe mechanism (NC relay or equivalent per DR-1 decision). Switchable R3/R4/R5.
2. **Motor Control** — Cam-aware position controller. Motor safety (run-time bound, reset-safe inputs, IRAM safety).
3. **`EncoderModule`** — Use PCNT hardware peripheral for quadrature decoding.
4. **OTA** — Enabled and tested.

**Verification:** Intercept mode indistinguishable from stock. Override 100-level control. Fail-safe activates on reset/crash. Motor stops within 1 revolution. OTA works.

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md)
- [`hardware_specification.md`](../../../../docs/hardware_specification.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
