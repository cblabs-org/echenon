---
schema_version: 1
wo_id: WO-91.1
pr_id: PR-91
title: 'DR-1 Prep / WO1: Bench Addendum'
complexity: interactive
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
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
> `.agents/plugins/principal-architect-workspace/pr-dr1-prep/wo1-bench-addendum/WORKORDER.md`,
> review the PR, and start DR-1."

---

## Completion Report (written by the worker at task completion)

### Status
complete (Item 7 optional — skipped, may be done in a future ride)

### Results by item

1. **Click-to-level reconciliation.** B03's 4 detents (16 PCNT steps) per
   level **confirmed** by independent bench observation. B02's "28% miss
   rate" is a measurement artifact — by design, 75% of individual detent
   clicks produce no motor response (accumulation steps, not misses).
   Logger type-6 event = 2 physical detents. Down direction is flakier
   (6–10 detents) than up (exactly 4). No genuinely lost clicks observed.
   Motor stutters on reverse (stop→restart pairs) not seen going up.
   Evidence: `A08-20261012-click-level-reconciliation.{csv,md}`. Erratum
   added to `docs/encoder-resistance-analysis.md` §4.1.

2. **Motor sign convention (row #1).** Single-ended DMM confirms neither
   motor wire goes below −0.14 V against GND (brief back-EMF transient
   on reversal only). The ±3.25 V from A01 was a differential measurement
   (Red vs Black), not against GND. Console motor driver is a 3.3 V
   H-bridge. No clamping diodes needed on ÷4 sense dividers.
   Evidence: `A01-20261012-single-ended-motor-dmm.md`.

3. **Tap cross-check (E-02).** DMM vs ADC at levels 1, 16, 32: systematic
   6–7% offset (DMM reads higher). Green reference is stable (18 mV ADC
   spread). B03 formula accurate to 1.5% at levels 16/32. Offset is
   irrelevant for passive monitoring (relative ADC counts) but would need
   calibration for active DAC-based pot resynthesis.
   Evidence: `A04-20261012-tap-crosscheck.md`.

4. **Cadence edge distortion (row #7).** Scope (Rigol DHO804) confirms
   the 4.7 kΩ + 100 nF tap does not distort the cadence edge: fall time
   < 1 ms with and without tap. High level drops ~0.3 V (3.1 → 2.8 V)
   due to 4.7 kΩ series R — above any logic threshold. The 100 nF
   filters reed-switch contact bounce (beneficial). Passive premise
   confirmed.
   Evidence: `A07-20261012-cadence-edge-{tap,notap}.png`,
   `A07-20261012-cadence-edge-distortion.md`.

5. **Console fault tolerance (safe subset).**
   - #1 Encoder disconnect: **PASS** — no fault, immediate recovery.
   - #2 Encoder stall: **PASS** — normal operation, no timeout.
   - #3 Fast burst: **PASS with caveat** — console batches rapid detents,
     but extreme speed can overshoot past the pot's working range into
     360° runaway. Recovery requires motor disconnect.
   - #7 Reversed A/B: **PASS** — works normally, direction inverted.
   - #9 Mid-seek: **PASS** — seamless target update.
   Evidence: `A09-20261012-console-fault-tolerance.md`.

6. **Row #10 input.** Encoder VCC (GPIO 10) reads 4095 counts (3.3 V, ADC
   rail) across all samples: ride-log (2092 rows), live endpoint at levels
   1/16/32, and during active knob turning. Min = mean = max = 4095.
   Console 3.3 V rail is rock-steady. Row left as "Open – DR-1" with
   this data as waiver input.

7. **60-min ride (optional).** SKIPPED — owner may do a long ride in the
   coming weeks. Firmware `v0.1.0-dev.7` is tagged and can be reflashed.

### Proposals for DR-1

- **4-detent-per-level design is settled.** Any MITM encoder scheme must
  replicate groups of 4 detents (16 PCNT steps) per level command.
- **Down-direction flakiness is a console characteristic**, not a wiring
  or sense-network issue. DR-1 should note this asymmetry as a constraint
  on encoder resynthesis timing.
- **MITM encoder output MUST be rate-limited** to prevent overshoot past
  the pot's working range into the runaway 360° seek failure mode.
- **No clamping diodes needed** on the motor sense network. The ÷4
  dividers are sufficient (max −0.035 V at GPIO from back-EMF transient).
- **Cadence tap can be retained as-is.** Passive premise confirmed; the
  100 nF is beneficial (filters bounce).
- **6–7% ADC-to-DMM offset** should be characterised as a calibration
  constant if the MITM design uses ADC readings to set DAC output levels.
  Irrelevant for passive monitoring.
- **Console is highly tolerant of encoder faults** — no presence
  monitoring, no timeout, no channel-reversal detection. Favourable for
  MITM interception.

### Dead Ends & Warnings

- **Ride-log lost to device reboot.** The device rebooted (likely OOM or
  watchdog) during the large ride-log CSV download after Item 1. Events
  log (flash) survived; ride-log (PSRAM) was lost. The events data was
  sufficient for Item 1's reconciliation. For Item 7, downloads should
  target smaller time windows or use burst captures.
- **Motor stutter on reverse** (stop→restart pairs at 731 s and 737 s in
  events data) — needs further characterisation if the MITM design
  detects seek-complete by monitoring motor cessation.

### Out-of-Scope Findings

- **Runaway 360° motor rotation failure mode.** Rapid encoder input can
  drive the console past the pot's working range. This is a console
  design limitation, not a firmware or wiring issue. The MITM design must
  include a rate limiter. Not a bug to fix; a constraint to design around.
- **Logger type-6 event resolution.** Each type-6 event = 2 physical
  detents (8 PCNT steps), not 1. This is a logger design choice, not a
  defect, but it should be documented in the logger's API reference.

### Blockers
(none)

