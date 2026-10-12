# PR-04a: Passive Tap Board (Phase 3 — Passive tap board) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 merged and DR-1 approved
> **Dev Phase:** Phase 3 — Passive tap board
> **Depends on:** PR-02, DR-1 ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §4.4.2 (v1 rows of the Phase 3+ pin map, as frozen at DR-1; UART0 reserved), §4.6–§4.8 (board wiring, voltage safety, parts), §8.5 (manual OTA, `autoCheck`), §12 (build), §13.6 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §6.2 (passive tap board; a placeholder finalised at DR-1), §3 (power)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3

## Mandate

Build and install the **passive tap board**: the v1 permanent install and the
product's daily-use configuration (UC1–UC3). The console keeps full control of the
motor and the knob; the board only observes. **No resistance control of any kind,
no encoder or motor interception** (owner decision 2026-10-12).

1. **Hardware scope** — J1–J4 pass-through with high-impedance series-resistor taps,
   no motor split, per the passive-tap-board design frozen at DR-1 (sense network,
   v1 pin map, power model). No R3/R4/R5 or standalone supplies (DR-1 §2.9).
2. **Power: VIN + switch** (DR-1 §2.3) — The ESP32 is powered from the bike's 9 V
   (White/Brown) via the Waveshare **VIN** pin (6–21 V, register row #11) through
   its onboard regulator; the external MP1584EN buck is **dropped**. A **power
   switch on the VIN line** lets USB-C power the board instead.
   **Back-feed / power-path check (deliverable):** with USB and VIN both
   connected, measure and document whether VBUS back-feeds the 9 V line or VIN
   back-feeds the USB host, and record the safe procedure (default until proven:
   switch VIN off before plugging USB). Evidence committed.
3. **UART0 reserved** — GPIO 43 (TX, D1) and GPIO 44 (RX, D0) are left free,
   unconnected and marked reserved in `board.h` and the board layout, for a
   future nRF51422 ANT+ add-on. Console and logging use native USB-CDC.
4. **Firmware** — Target the v1 pin map from DR-1 in
   `hardware/ex3-waveshare-nano/board.h`, keeping `HAS_*` flags accurate for the
   passive configuration (no motor drive, no outputs).
5. **Manual OTA (before the board is sealed)** — Manual OTA is enabled and tested on
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
- The board runs from VIN with the switch on, and from USB-C with it off; the
  USB + VIN back-feed result is documented.
- GPIO 43/44 are unconnected and reserved.

**Known limitation:** the stock console runaway (A09-20261012 #3) cannot be
prevented by this board; the firmware may detect and report it.

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md) — intercept board, not passive
- [`hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 are the v2 active board (DR-2), not this board

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
