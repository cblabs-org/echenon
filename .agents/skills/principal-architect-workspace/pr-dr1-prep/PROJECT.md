# DR-1 Prep: Phase 0 Evidence Addendum

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Owner:** Chief Architect (Tier 1). This is evidence gathering for DR-1, not a phase PR.
> **PR target base:** `main`
> **Feeds:** [DR-1 record](../reviews/DR-1-design-freeze.md), mainly §2.1 (sense network), §2.2 / §2.9 (supplies), §2.10 (interception architecture), §2.11 (scale mapping)
> **Evidence home:** [`docs/measurements/phase0/`](../../../../docs/measurements/phase0/README.md), using the register's naming convention
> **Reviews:** [2026-10-04 PR-01 progress review](../reviews/2026-10-04-pr01-progress-review.md); PR-01 wrap-up in [`pr-01-signal-logger/PROJECT.md`](../pr-01-signal-logger/PROJECT.md)

## Mandate

PR-01 closed 11 of the 13 register rows. Some items DR-1 needs are still missing
or ambiguous. Gather them in **one interactive bench session**, then submit the
evidence as a small docs-only PR. **No firmware changes and no hardware
modification**: use the merged PR-01 logger (`WITH_SIGNAL_LOGGER`) and the
existing Y-harness.

| # | Item | Why DR-1 needs it |
|---|---|---|
| 1 | **Click-to-level reconciliation.** Re-derive B02's "28% of clicks produce no motor response" using the WO5 finding of 4 detents (16 steps) per level | DR-1 §2.10. The encoder-only MITM case rests on the stock console *missing* clicks. If 3 in 4 clicks never move the motor by design, the case changes |
| 2 | **Row #1 sign convention.** Measure each motor wire single-ended against Brown (GND), both directions | DR-1 §2.1. A01 records −3.25 V on reverse and says the GPIO is "clamped". Any wire going negative against GND breaks the ESP32's −0.3 V absolute minimum, so the sense network needs clamping |
| 3 | **E-02 tap cross-check.** DMM against ADC on the wiper (levels 1, 16, 32) and Green, through the existing 22 kΩ taps | Validates the absolute values behind `wiper = 39.1 × level + 97` and the 1.66 V reference |
| 4 | **Cadence edge distortion.** Scope the Orange line at the console side with and without the 4.7 kΩ + 100 nF tap | Confirms the passive premise (row #7) |
| 5 | **Console fault tolerance**, the safe subset from `docs/mitm-encoder-design-notes.md` §12: #1 encoder disconnect, #2 encoder stall, #3 fast burst, #7 reversed A/B (only with a swap adapter; skip otherwise), #9 clicks mid-seek | DR-1 §2.10 (whether the console tolerates an intercepted or resynthesised encoder) |
| 6 | **Row #10 decision input.** Note the encoder-VCC (console 3.3 V rail) reading at idle and in a class, from the GPIO 10 log | Lets DR-1 waive row #10, which is expected because E-01 removes parallel supplies |
| 7 | **Optional:** SPEC test 0.9, a 60-minute ride log, if you ride during the session | Closes PR-01's only unevidenced SPEC test |

**Explicitly out of scope:**
- Design-notes tests #4 and #6 (pot or motor disconnected mid-seek). These need the owner's go-ahead *in the session* and a kill switch within reach.
- Measuring the minimum quadrature pulse width. That needs a signal source driving the console encoder input, which means a split encoder loom. It is deferred to the encoder-relay work if DR-1 adopts it.
- Any design decision. Record conclusions as **proposals for DR-1**.

## Work Order Ledger

| | WO | Branch | Complexity | Deps | Items | Status |
|---|---|---|---|---|---|---|
| **[x] [wo1-bench-addendum](wo1-bench-addendum/WORKORDER.md):** | Interactive bench session, items 1–7 | `dr1-prep/wo1-bench-addendum` (base: `main`) | `interactive` | Bench kit; flashed `main` firmware | 1–7 | ✅ Complete (2026-10-12). Items 1–6 done, Item 7 skipped (optional). |

**File overlaps:**
- New evidence files only, plus edits to `docs/measurements/phase0/README.md`
  (rows #1, #7, #8, #10) and an erratum note in `docs/encoder-resistance-analysis.md`.
- No overlap with PR-03, which touches `src/`, `platformio.ini`, `hardware/` and `framework`.

## Session Log

### 2026-10-11: Project created (Chief Architect)
- **Owner approval:** *"proceed to prep the bench session for next time. Add in
  project or workorders as per /principal-architect"* (2026-10-11, in chat).
  Recorded here as the provenance anchor per `.agents/rules/00-operating-model.md`.
- Items carried from the PR-01 wrap-up and the 2026-10-04 CA addendum, which
  WO5 did not complete.
- Firmware: `main` changed `src/` after tag `v0.1.0-dev.6` (WO6, `3888a83`).
  Rule 03 requires tagging `v0.1.0-dev.7`, then rebuilding, before flashing.

## Status
**Plan Status:** READY — WO1 runs interactively with the owner. Then a docs-only
PR, a Chief Architect review, and DR-1.
