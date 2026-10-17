# PR-04a: Passive Tap Board (Phase 3 — Passive tap board) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 merged and DR-1 approved (DR-1 approved with conditions 2026-10-12: **unblocked by DR-1**; PR-02 still pending)
> **Dev Phase:** Phase 3 — Passive tap board
> **Depends on:** PR-02, DR-1 ([decision record](../reviews/DR-1-design-freeze.md))
> **Spec sections:** [SPEC](../../../../SPEC.md) §4.4.2 (v1 rows of the Phase 3+ pin map, frozen at DR-1; UART0 reserved), §4.6–§4.8 (board wiring, power selection, voltage safety, parts), §8.5 (manual OTA, `autoCheck`), §12.3 (build flags), §13.6 (tests)
> **Hardware spec:** [`docs/hardware_specification.md`](../../../../docs/hardware_specification.md) §6.2 (passive tap board build specification, frozen at DR-1), §3 (power)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 3

## Mandate

Build and install the **passive tap board**: the v1 permanent install and the
product's daily-use configuration (UC1–UC3). The console keeps full control of the
motor and the knob; the board only observes. **No resistance control of any kind,
no encoder or motor interception** (owner decision 2026-10-12).

1. **Hardware scope: the PR-01 detection circuit made permanent** (DR-1 §4,
   condition 2.8) — J1/J2 (9-pin) and J3/J4 (3-pin) Y-harness pass-through,
   no motor split, and **all 10 PR-01 taps** (condition 2.7) with their exact
   values: 22 kΩ on the pot taps (GPIO 1–3), 4.7 kΩ + 100 nF on cadence
   (GPIO 4/5) and encoder VCC (GPIO 10), 4.7 kΩ on the PCNT encoder inputs
   (GPIO 8/9), 30 kΩ / 10 kΩ ÷4 dividers on the motor wires (GPIO 6/7). No
   R3/R4/R5, no Green–GND link, no outputs (DR-1 §2.9). Build spec:
   hardware spec §6.2.
2. **WO1 — power-path check and selector design, before any soldering**
   (DR-1 §4, condition 2.3). The board is powered from **9 V (VIN) or USB-C,
   never both at once**; the external MP1584EN buck is **removed**. WO1 reads
   the Waveshare ESP32-S3-Nano **VBUS/VIN power path from the schematic**,
   records whether the board isolates the two, and designs the **selector**
   (9 V disconnected in the USB position), adding a **diode on the VIN path**
   if the board does not isolate them. Evidence (schematic extract, finding,
   selector part and wiring) is committed **before soldering**; behaviour in
   both selector positions is measured on the built board (SPEC §13.6 3.1a).
3. **UART0 reserved** — GPIO 43 (TX, D1) and GPIO 44 (RX, D0) are left free,
   unconnected and marked reserved in `board.h` and the board layout, for a
   future nRF51422 ANT+ add-on. Console and logging use native USB-CDC.
4. **Firmware** — The v1 pin map equals the Phase 0/1 map, so
   `hardware/ex3-waveshare-nano/board.h` keeps all ten pins and its `HAS_*`
   flags (`HAS_MOTOR_CONTROL 0`, no outputs). Update only the comments that
   call the monitor pins "Phase 0/1 only" (SPEC §12.3).
5. **OTA (before the board is sealed)** — Manual OTA is enabled and tested on
   the installed board; OTA and rollback are demonstrated before the console
   housing is closed (audit P-07). Framework **#226 (OTA check-on-boot
   deferral) is closed upstream**: use the framework's check-on-boot option
   instead of Echenon's `autoCheck = false` enforcement, **after verifying the
   option on the framework version brought in by the PR-03 framework bump**.
   Until that verification passes, keep forcing `autoCheck` to `false` after
   config load and again after factory reset (the framework default is `true`
   and a factory reset restores it).
6. **Housing fit** — checked before the board is sealed (hardware spec §6.2.5).

Board build, installation and ride tests are `complexity: interactive` WOs with
evidence committed.

**Verification:**
- A stock ride is indistinguishable from no board (console behaviour, power-on
  calibration, levels, app).
- All ten taps read correctly against the PR-02 calibration.
- The board fits in the console housing.
- Manual OTA succeeds on the installed board; no automatic update check runs
  at boot, after a reboot or after a factory reset (framework option, or
  `autoCheck = false` until it is verified).
- The WO1 power-path finding and selector design were committed before
  soldering; the board runs from VIN in the VIN position and from USB-C in the
  USB position with 9 V disconnected; VIN and USB are never connected together.
- GPIO 43/44 are unconnected and reserved.

**Known limitation:** the stock console runaway (A09-20261012 #3) cannot be
prevented by this board; the firmware may detect and report it.

## DR-1 outcome (2026-10-12): unblocked by DR-1

DR-1 was approved with conditions on 2026-10-12
([record §4](../reviews/DR-1-design-freeze.md)); soldering is allowed under
the condition 2.3 power-path check (item 2). The PR still waits for PR-02 to
merge (its base).

## Prior Art

- [Archived PR-04](../archive-pr-v2/pr-04-intercept-board/PROJECT.md) — intercept board, not passive
- [`hardware_specification.md`](../../../../docs/hardware_specification.md) §7–§8 are the v2 active board (DR-2), not this board

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
