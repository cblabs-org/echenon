---
schema_version: 1
charter:
  pr_id: PR-91
  slug: pr-dr1-prep
  title: 'DR-1 Prep: Phase 0 Evidence Addendum'
  branch: dr1-prep
  base_branch: main
  allowed_modules:
  - docs/measurements/
  - docs/
  charter_locked: false
status: GRADUATED
work_orders:
- id: WO-91.1
  slug: wo1-bench-addendum
  title: 'DR-1 Prep / WO1: Bench Addendum'
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
---
## 2. Scope

_See legacy content below._


# DR-1 Prep: Phase 0 Evidence Addendum

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **Owner:** Chief Architect (Tier 1). This is evidence gathering for DR-1, not a phase PR.
> **PR target base:** `main`
> **Feeds:** [DR-1 record](../reviews/DR-1-design-freeze.md), mainly §2.1 (sense network), §2.2 / §2.9 (supplies), §2.10 (interception architecture), §2.11 (scale mapping)
> **Evidence home:** [`docs/measurements/phase0/`](../../../../docs/measurements/phase0/README.md), using the register's naming convention
> **Reviews:** [2026-10-04 PR-01 progress review](../reviews/2026-10-04-pr01-progress-review.md); PR-01 wrap-up in [`pr-01-signal-logger/PROJECT.md`](../pr-01-signal-logger/PROJECT.md)

## 1. Mandate

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

## 3. Work Order Ledger

| | WO | Branch | Complexity | Deps | Items | Status |
|---|---|---|---|---|---|---|
| **[x] [wo1-bench-addendum](wo1-bench-addendum/WORKORDER.md):** | Interactive bench session, items 1–7 | `dr1-prep/wo1-bench-addendum` (base: `main`) | `interactive` | Bench kit; flashed `main` firmware | 1–7 | ✅ Complete (PR #2, `5168b1e`) |
**File overlaps:**
- New evidence files only, plus edits to `docs/measurements/phase0/README.md`
  (rows #1, #7, #8, #10) and an erratum note in `docs/encoder-resistance-analysis.md`.
- No overlap with PR-03, which touches `src/`, `platformio.ini`, `hardware/` and `framework`.

## 4. Session Log

### 2026-10-11: Project created (Chief Architect)
- **Owner approval:** *"proceed to prep the bench session for next time. Add in
  project or workorders as per /principal-architect"* (2026-10-11, in chat).
  Recorded here as the provenance anchor per `.agents/rules/00-operating-model.md`.
- Items carried from the PR-01 wrap-up and the 2026-10-04 CA addendum, which
  WO5 did not complete.
- Firmware: `main` changed `src/` after tag `v0.1.0-dev.6` (WO6, `3888a83`).
  Rule 03 requires tagging `v0.1.0-dev.7`, then rebuilding, before flashing.

### 2026-10-12: WO1 complete; PR #2 merged; Chief Architect review
- **Merged** as PR #2 (`5168b1e`) on 2026-10-12 with no pre-merge review: the
  second time the checkpoint has been skipped (see PROGRAM.md). This review is
  post-hoc. The evidence is sound and nothing would have blocked the merge.
- **Results** (Completion Report):
  - "28% missed clicks" was an artefact of 4 detents per level. The
    down direction needs 6–10 detents.
  - Motor wires stay ≥ −0.14 V against GND, so no clamps are needed.
  - The ADC reads 6–7% below the DMM through the 22 kΩ taps (systematic).
  - The cadence tap does not distort the edge.
  - Console fault tests #1, #2, #3, #7 and #9 pass. **New stock failure
    mode:** a fast spin to the top drives the motor past the working range
    into continuous rotation; recovery needs the motor unplugged.
- **Review notes:**
  1. Item 2 used a DMM, not the scope the WO asked for, so short spikes could
     be missed. That is acceptable because the 30 kΩ series resistor limits
     current; noted in DR-1 §2.1.
  2. Item 6: 4095 counts means the ADC **saturates** (≥ 3.1 V at 12 dB). The
     result shows the rail is present, not that it is stable; noted in the
     DR-1 waiver.
  3. In the runaway, the wiper read about 3.3 V, above the 1.66 V reference.
     The wiper likely floats or is pulled up in the dead zone. Confirm this in
     PR-04b / PR-07 before relying on it for runaway detection.
  4. **The logger rebooted during a large ride-log CSV download**, losing the
     PSRAM ride log. This is a logger defect (export path; see L-06), assigned
     to PR-03 WO1, which owns the diagnostic env.
  5. Type-6 events cover 2 detents. Document this in the logger's API notes.
- Feeds the [DR-1 §2A draft recommendations](../reviews/DR-1-design-freeze.md).

## Status
**Plan Status:** ✅ COMPLETE: evidence merged (PR #2). DR-1 in progress.
