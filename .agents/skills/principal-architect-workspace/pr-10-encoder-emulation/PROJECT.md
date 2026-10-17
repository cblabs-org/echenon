# PR-10: Encoder Emulation + Relay Mode (mode 1) — v2

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-07 merged
> **Dev Phase:** Phase 5 — Encoder emulation + relay mode
> **Depends on:** PR-07 (and through it DR-2, v1 release). Unblocks PR-08, PR-12
> **Design:** [DR-2](../reviews/DR-2-design-freeze.md) §2.10, §2.12; [`docs/mitm-encoder-design-notes.md`](../../../../docs/mitm-encoder-design-notes.md) §3, §11, §12

## Mandate

Implement **mode 1, relay/active**: the knob and the console's motor drive are
taken in as signals and re-emitted.

1. **Encoder ingest and emulation** — PCNT ingest of the knob; rate-limited
   quadrature resynthesised toward the console (timing from DR-2 prep), with the
   NC encoder bypass under ESP32 control.
2. **Console as signal source** — The console's motor drive (GPIO 6/7 sense) is a
   *request*; the ESP32 drives the motor through the PR-07 H-bridge (design notes §11).
3. **Wiper-verified level tracking** and runaway prevention in the relay path.
4. **Fail-safe** — Any fault returns the encoder and motor paths to stock via the
   NC bypasses.

## Work Order Ledger
<!-- PA fills this in after DR-2 -->

## Session Log
<!-- PA enriches this progressively -->

### 2026-10-12: Stub created (Chief Architect)
- Created when the owner restructured the programme into v1 Passive / v2 Active
  releases (2026-10-12, in chat; recorded in [PROGRAM.md](../PROGRAM.md)). This
  stub is **not** an approved decomposition.

## Status
**Plan Status:** PROVISIONAL — decomposed after DR-2
