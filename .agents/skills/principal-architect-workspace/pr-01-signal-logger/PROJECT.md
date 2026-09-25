# PR-01: Signal Logger (Phase 0 — Passive Observation)

> **Programme:** Echenon Firmware
> **PR target base:** `main`
> **Dev Phase:** Phase 0 — Observe
> **Spec sections:** §3.5, §4.3–4.8, §12.1–12.3
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 0

## Mandate

Set up the PlatformIO project (`env:esp32s3nano` base, `um_nanos3` board, unified `-DBOARD_HEADER=...board.h`), target hardware config directory (`hardware/ex3-waveshare-nano/` with `board.h`, `partitions.csv`, and `sdkconfig.defaults`), and `EchenonModule` skeleton. Implement `SignalLogger` that reads ALL 12 wires from the bike's looms (9-pin + 3-pin) at high frequency with microsecond timestamps. Build a live web dashboard and CSV data export. Verify mDNS fallback (`echenon.local`).

**Corrections (A1-A6):** Ensure correct pin map (D8=GPIO 17, D9=GPIO 18, ADC1 for motor sense GPIO 6/7). Read motor sense as **analog** (ADC1, ≥1 kHz). Establish evidence convention in `docs/measurements/phase0/`.

**Hardware (`complexity: interactive`):** Fly wires only. No wires cut, no intercept board built. Scope captures must be recorded.

**GATE:** Oscilloscope motor voltage verification must confirm 3.3V DC (not 9V PWM) before connecting wires.
**GATE:** Appendix A exit checklist required for DR-1.

After this PR merges, the firmware boots, reads all bike signals, displays them live in a browser dashboard, and exports ride data as CSV for DR-1.

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
