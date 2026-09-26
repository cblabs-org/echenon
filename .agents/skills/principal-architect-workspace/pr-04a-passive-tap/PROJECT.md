# PR-04a: Passive Tap Board (Phase 3 — Passive tap board)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 merged and DR-1 approved
> **Dev Phase:** Phase 3 — Passive tap board
> **Depends on:** PR-02, DR-1 ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §4.4.2 (Phase 3+ pin map, as frozen at DR-1), §4.6–§4.8 (board wiring, voltage safety, parts), §8.5 (manual OTA, `autoCheck`), §12 (build), §13.6 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §6.2 (passive tap board; a placeholder finalised at DR-1), §3 (power)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3

## Mandate

Build and install the **passive tap board**: the default permanent install and the
product's daily-use configuration (UC1–UC3). The console keeps full control of the
motor; the board only observes.

1. **Hardware scope** — J1–J4 pass-through with high-impedance series-resistor taps,
   no motor split, per the passive-tap-board design frozen at DR-1 (sense network,
   switchable supplies, power model, Phase 3 pin map). This is the first board to
   use the on-board 9 V → 3.3 V buck; its rail design comes from DR-1.
2. **Firmware** — Target the Phase 3 pin map from DR-1 in
   `hardware/ex3-waveshare-nano/board.h`, keeping `HAS_*` flags accurate for the
   passive configuration (no motor drive).
3. **Manual OTA (before the board is sealed)** — Manual OTA is enabled and tested on
   the installed board. **`autoCheck` is forced to `false` after config load and
   again after factory reset**, because the framework default is `true` and a
   factory reset restores it. This enforcement stays until framework #226
   (OTA check-on-boot deferral) lands. OTA and rollback are demonstrated before the
   console housing is closed (audit P-07).

Board build, installation and ride tests are `complexity: interactive` WOs with
evidence committed.

**Verification:**
- A stock ride is indistinguishable from no board (console behaviour, power-on
  calibration, levels, app).
- Every sensor tap reads correctly against the PR-02 calibration.
- The board fits in the console housing.
- Manual OTA succeeds on the installed board; `autoCheck` reads `false` after a
  reboot and after a factory reset.

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md) — intercept board, not passive
- [`hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 are PROVISIONAL until DR-1

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
