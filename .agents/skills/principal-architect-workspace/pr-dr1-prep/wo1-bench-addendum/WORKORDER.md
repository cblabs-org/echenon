---
name: dr1-prep-wo1
description: Interactive bench session that gathers the remaining Phase 0 evidence DR-1 needs
complexity: interactive
---

# DR-1 Prep / WO1: Bench Addendum

## Role Definition

You are an expert embedded engineer and bench assistant, working
interactively with the owner, who is at the bench. You are NOT the
Principal Architect or the Chief Architect. Do not make design decisions, do
not change firmware, and do not plan beyond this checklist. Walk the owner
through each item, interpret the readings with them, and commit the evidence.

## Objective

Close the evidence gaps DR-1 needs (items 1–7 in
[`../PROJECT.md`](../PROJECT.md)) using the merged PR-01 logger and the
existing bench kit. Record the results in the Phase 0 register.

## Strict Boundary Box

- **No firmware changes.** Build and flash `main` as it is, with the logger
  enabled. If PR-03 WO1 has merged, the logger lives in the
  `ex3-waveshare-nano-diag` env, so flash that env. If the logger cannot capture something, record that
  as a finding. Do not patch the logger.
- **No hardware modification.** Use the passive Y-harness and taps only.
  Item 5 #7 (reversed A/B) is allowed only if the owner has a plug-in swap
  adapter; nothing may be cut or soldered.
- **Never** run the design-notes tests #4 or #6 (pot or motor disconnected
  while seeking) unless the owner explicitly says to in this session *and* a
  kill switch is within reach. Otherwise, skip them.
- Edit only:
  - new evidence files in `docs/measurements/phase0/`;
  - `docs/measurements/phase0/README.md` (rows #1, #7, #8, #10 and the
    bench-report mapping);
  - an erratum note in `docs/encoder-resistance-analysis.md`;
  - this WORKORDER's Completion Report;
  - `../PROJECT.md` (ledger status only).
- Do NOT edit `SPEC.md`, `PROGRAM*.md`, the DR-1 record or `reviews/*`. They
  belong to the Chief Architect.
- Write design conclusions as **"Proposal for DR-1"** in the Completion
  Report, never as decisions.

## Tooling Constraints

- **Rule 04:** use the PlatformIO MCP tools (`build_project`,
  `upload_firmware`, `start_monitor` / `capture_serial_window`), never the
  `pio` CLI. Hooks enforce this.
- **Rule 03 (flash gate):** `src/` changed after `v0.1.0-dev.6` (WO6,
  `3888a83`). Before flashing:
  1. tag `v0.1.0-dev.7` and push the tag;
  2. rebuild;
  3. flash;
  4. confirm that `firmware_version` from `/api/system` matches the tag.
- **Logger endpoints** on `echenon.local` (or the device IP):
  - `GET /api/echenon/status`;
  - `GET /api/echenon/live`;
  - `POST /api/echenon/burst/start` with body `{"duration_ms": N}`;
  - CSV downloads: `GET /api/echenon/ride-log.csv`, `/events.csv`, `/burst.csv`.
- **Event types** (`src/echenon/SignalLogic.h`): `0` cadence, `5` motor
  direction, `6` encoder (PCNT). Types 1–4 are legacy and no longer emitted.
- Scratch analysis scripts go in `tmp/`.

## Context

| | |
|---|---|
| **Pins** | SPEC §4.4.1: GPIO 1 wiper, 2 Yellow (0 V), 3 Green (≈ 1.66 V), 4 Grey, 5 Orange (4.7 kΩ + 100 nF), 6/7 motor ± (÷4 divider), 8/9 encoder A/B (PCNT), 10 encoder VCC |
| **Level formula (B03)** | `wiper = 39.1 × level + 97` counts. 4 detents (16 encoder steps) per level |
| **Already tried** | B02 measured "28% of clicks produce no motor response" (ADC-sampled encoder, before the wire-colour correction and before the 4-detent finding). A01 probed the motor wires "directly" without recording the probe reference |

## Line-Item Instructions

### Setup
- [ ] Branch `dr1-prep/wo1-bench-addendum` from the live tip of `main`.
- [ ] Tag, build and flash per Rule 03. Confirm the logger is capturing via `/api/echenon/status`.

### Item 1: click-to-level reconciliation
- [ ] With a class running (so the console seeks), turn the knob **one detent at a time** for 3+ full levels up and 3+ down. Wait for the motor to settle after each detent. Then do a set of rapid single clicks.
- [ ] Download `events.csv` and `ride-log.csv`. For each detent, record: encoder event (type 6), whether a motor-direction event (type 5) followed, and the settled level from the wiper.
- [ ] Report detents per level change, the % of detents that move the motor, and whether any detent was genuinely **lost** (encoder registered, console never acted, level wrong afterwards).
- [ ] Evidence: `A08-<yyyymmdd>-click-level-reconciliation.{csv,md}`. Add an erratum to `docs/encoder-resistance-analysis.md` §(28%) that points to it.

### Item 2: motor-wire sign convention (row #1)
- [ ] Scope CH1 on Red, CH2 on Black, **both probe grounds on Brown (GND)**. Capture forward and reverse at 10 ms/div.
- [ ] Report the min and max of each wire against GND. State clearly whether either wire goes below 0 V. Do not connect the ÷4 dividers to anything new.
- [ ] Evidence: `A01-<yyyymmdd>-single-ended-fwd.png`, `…-rev.png`. Append the result to row #1.

### Item 3: tap cross-check (E-02)
- [ ] At levels 1, 16 and 32, measure with the DMM: wiper (Blue) to GND, Green to GND, Yellow to GND, **at the bike side of the 22 kΩ resistors**. Record the matching ADC means from `/api/echenon/live`, converted to mV.
- [ ] Report each offset (mV and %). Flag any |offset| > 2% as a finding.
- [ ] Evidence: `A04-<yyyymmdd>-tap-crosscheck.md`.

### Item 4: cadence edge distortion (row #7)
- [ ] Scope Orange at the console side while pedalling slowly. Capture once with the 4.7 kΩ + 100 nF tap connected and once disconnected, and compare the edge rise/fall times and low level.
- [ ] Evidence: `A07-<yyyymmdd>-cadence-edge-{tap,notap}.png`. Append the result to row #7.

### Item 5: console fault tolerance (safe subset)
For each test, record the console display, the app, the motor (type-5 events) and the recovery. Log a burst around each one.
- [ ] #1 Encoder disconnect: unplug the encoder at the Y-harness during a class, wait 30 s, reconnect.
- [ ] #2 Encoder stall: hold the knob still for 60 s while the app requests changes (if the app can).
- [ ] #3 Fast burst: spin the knob more than 10 detents per second, up then down.
- [ ] #7 Reversed A/B: **only with a plug-in swap adapter**; otherwise mark SKIPPED.
- [ ] #9 Mid-seek: click 2–3 detents while the motor is still seeking from a previous change.
- [ ] Evidence: `A09-<yyyymmdd>-console-fault-tolerance.md` plus the burst CSVs.

### Item 6: row #10 input
- [ ] From the ride log, report the encoder VCC (GPIO 10) mean, min and max at idle and during a class. Append to row #10 as "Input for DR-1 waiver". Leave the row's result as `Open – DR-1`.

### Item 7 (optional): SPEC test 0.9
- [ ] If the owner rides at least 60 minutes, download the ride log and confirm it was not truncated (wrap flag false). Evidence: `A00-<yyyymmdd>-ride-60min.csv` (gzip it if larger than 5 MB).

### Wrap-up
- [ ] Update the README register rows and the bench-report mapping.
- [ ] Commit the evidence (docs only), and ask the owner to confirm before committing.
- [ ] Fill in the Completion Report below.

## Verification Requirement (adapted for interactive work)

Work interactively with the owner until every item is captured or explicitly
skipped with a reason. Before committing, ask the owner to confirm they are
happy with the evidence and the register text.

## Issue Surfacing

If you find a logger defect, a framework bug or anything else outside this
boundary, write it under Out-of-Scope Findings. Do not fix it and do not file
issues. The Chief Architect triages them.

## Return Protocol

When done:
1. Append the Completion Report.
2. Push the branch and open a docs-only PR titled
   "DR-1 prep: Phase 0 evidence addendum". **Do not merge it.**
3. Give the owner this return prompt:

> **Return to Chief Architect:** "DR-1 prep WO1 is complete. Read the
> Completion Report at
> `.agents/skills/principal-architect-workspace/pr-dr1-prep/wo1-bench-addendum/WORKORDER.md`,
> review the PR, and start DR-1."

---

## Completion Report (written by the worker at task completion)

<!-- Append when complete:
### Status
complete | partial | blocked

### Results by item
1. Click-to-level: …
2. Motor sign convention: …
3. Tap cross-check: …
4. Cadence edge: …
5. Fault tolerance (#1, #2, #3, #7, #9): …
6. Row #10 input: …
7. 60-min ride (optional): …

### Proposals for DR-1
- …

### Dead Ends & Warnings
- …

### Out-of-Scope Findings
- …

### Blockers
(none)
-->
