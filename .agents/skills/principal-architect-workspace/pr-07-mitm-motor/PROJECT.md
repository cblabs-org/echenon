# PR-07: Active Intercept Board + Motor Control + Safety (Phase 5) — v2

> **Release:** **v2 "Active"** (provisional; decomposed after DR-2). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after DR-2 approved and the v1 release (PR-09)
> **Dev Phase:** Phase 5 — Active board + motor control + safety
> **Depends on:** **DR-2** ([decision record](../reviews/DR-2-design-freeze.md)), v1 release (PR-09). Unblocks PR-10 and PR-12
> **Spec sections:** [SPEC](../../../../SPEC.md) §4.4.2 (Phase 3+ pin map), §4.6–§4.8 (intercept board, voltage safety, parts), §5.3–§5.6 (level mapping, encoder, motor control, operating modes), §6.3 (motor-driven calibration) — all as revised at DR-2; §13.8 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 as revised at DR-2 (§7.8 pin map)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 5

## Mandate

Build the v2 **active intercept board** and implement cam-aware motor control and
safety, to the design frozen at DR-2. Encoder emulation and relay mode are PR-10;
ERG is PR-08; console-off standalone is PR-12.

1. **Hardware scope** — Active board with motor split, **H-bridge with PWM**
   (DRV8833 class, DR-2 §2.3), **normally-closed fail-safe bypasses** on every
   intercepted path (DR-2 §2.4; audit H-05), and **runaway protection** (DR-2
   §2.4). Standalone supplies are PR-12. GPIO 6/7 are never both driver outputs
   and sense inputs. **UART0 (GPIO 43/44) stays reserved.**
2. **Motor control** — Cam-aware position controller (audit S-01, M-02): full-
   revolution angle tracking, working half-revolution and approach direction,
   angle-based level table, run-time bound of one revolution, reset-safe driver
   inputs, IRAM-safe ISR paths (audit M-01).
3. **Motor-driven calibration** — A full-revolution sweep, driven by the ESP32, that
   finds the cam extremes and click-off points and refines the passive calibration
   table from PR-02. This PR owns motor-driven calibration.
4. **`EncoderModule`** — Reuse the framework's PCNT-based decoder via a
   `BoardEncoder` descriptor (audit H-06); no Ch B conditioning is needed (B04).
5. **OTA regression** — Manual OTA (enabled in PR-04a) still works with the
   intercept board fitted and `autoCheck` stays forced to `false`.

Board build, motor tests and fail-safe tests are `complexity: interactive` WOs with
evidence committed.

**Verification:**
- Intercept mode is indistinguishable from stock, including the console's power-on
  calibration.
- **Override level resolution is measured and recorded; 100 levels is a goal to
  validate, not an acceptance criterion.**
- Fail-safe activates on reset, crash, brownout and an unseated ESP32.
- The stock runaway cannot occur with the board fitted (or is stopped within the
  DR-2 bound).
- The motor stops within one revolution in every fault case.
- Motor-driven calibration reproduces the cam extremes across repeated sweeps.
- OTA works with the intercept board fitted.

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md) — pre-audit intercept design
- [`hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 are PROVISIONAL until DR-2

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** PROVISIONAL — decomposed after DR-2
