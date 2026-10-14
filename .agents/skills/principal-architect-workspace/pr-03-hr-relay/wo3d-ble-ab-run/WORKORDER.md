---
name: hr-relay-wo3d
description: Run the A/B/A′ BLE coexistence experiment
complexity: interactive
---

# WO3d — BLE A/B/A′ Run

> You are an expert senior software engineer and debugging-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not branch, and do not plan beyond the immediate experiment
> scope. Your task is to work interactively with the owner to run the A/B/A′
> experiment described below, using the test rig built in WO3c.

## Objective

Run the decisive A/B/A′ experiment to determine whether WiFi coexistence
(H-COEX) is the dominant cause of the Pixel BLE connection failures. Use the
automated rig from `tools/ble-test/` and the control peripheral from
`tools/ble-test/control-peripheral/`.

**You must NOT choose or implement a coex fix.** Escalate the results to the
Chief Architect.

## Background

Read these documents for full context:
- `reviews/2026-10-14-wo3b-ble-test-strategy.md` (on `origin/main` — the CA's
  test strategy, §5–§7 are the experiment spec)
- `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md` (the investigation
  paper)
- `docs/measurements/phase2/evidence/t6-prime-2026-10-14/ANALYSIS.md` (T6'
  HCI/nRF cross-correlation)

### Key hypotheses under test

| ID | Hypothesis | Status |
|---|---|---|
| **H-COEX** | WiFi holds the shared radio at critical BLE moments | **Leading** |
| **H-PHONE** | Pixel stack/controller mishandles the connection | **Open** |
| H-WIN | Pixel's short establishment window increases sensitivity | Contributing |
| H-UPD | Established links drop after connection-parameter update | **Unresolved** |

### The decisive question

> Does the Pixel failure rate collapse when the ESP32 has **no WiFi at all**?

## Fixed Conditions (All Batches)

- Pixel at ~1 m from the board, same position throughout
- P12 and Mac **not** connecting to the board during batches
- Portal closed in every browser; no MCP calls during a batch
- Router and home network as normal
- Owner's watch (`EF:BE:A7:3A:81:C6`) left paired as normal (reflects
  real-world use)
- Use the **default** env (`ex3-waveshare-nano`, no `WITH_SIGNAL_LOGGER`) for
  Echenon batches unless the owner says otherwise
- Record the firmware tag, build env, and all diagnostic overrides in each
  batch header

## Phone Hygiene (Before Each Batch)

- Screen stays on ("Stay awake" developer option)
- No other foreground apps
- Bluetooth on
- Log the Android build and bonded devices
  (`adb shell dumpsys bluetooth_manager` summary) into the batch header

## Batches (Run in Order)

| Batch | Firmware on the board | Purpose |
|---|---|---|
| **A** | Echenon, WiFi STA connected | Baseline rate, clustering, drops. First failure → bug report |
| **B** | **Control peripheral (no WiFi)** | **Decisive for H-COEX** |
| **A′** | Echenon again (same build as A) | Checks that conditions didn't drift between A and B |
| C (optional) | Echenon, with the watch's Bluetooth off | Phone-side factor (H-PHONE) |

### Batch procedure

1. Flash the correct firmware (Rule 03: Tag → Build → Flash for Echenon;
   control peripheral uses its own PIO project under `tools/ble-test/control-peripheral/`)
2. Wait for boot and WiFi connection (Echenon) or BLE advertising start (control)
3. Start `esp32_log.py` (serial capture)
4. Start `pixel_runner.py --batch-id <X> --attempts 50 --evidence-dir <dir>`
5. Monitor the run; capture bug reports on first failure
6. After all attempts complete, stop `esp32_log.py`
7. Review `summary.md` and `attempts.csv`
8. **Between batches:** power-cycle the ESP32, wait 2 min

### Target: ~50 attempts per batch, ~45–60 min each

## Decision Rules (Pre-Agreed with CA)

| Outcome | Conclusion | Next step |
|---|---|---|
| B ≥ 48/50, and A and A′ both ≤ 85% | **H-COEX confirmed** | WO3e: power-save experiment. Echenon with local `WIFI_PS_MIN_MODEM` override + hybrid (PS_NONE while associating, MIN_MODEM once connected), each one batch. Plus 10 WiFi association cycles (connect time, `AUTH_EXPIRE`). **Escalate to CA before WO3e** |
| B ≈ A (within ~10 points) | **Not coex.** H-PHONE / interop | Analyse failure trace (§6 of strategy); escalate to CA |
| A and A′ disagree by > 15 points | Conditions drifted; inconclusive | Find the drift; rerun A/B/A′ |
| Drops (H-UPD) in any batch | Separate finding | Report drop times relative to the parameter update |

## Trace Analysis (For Any Captured Failure)

From each bug report: extract `FS/data/misc/bluetooth/logs/btsnooz_hci.log`.
Use `tools/ble-test/snoop_parse.py` to decode. Confirm the time span contains
the failure time from `attempts.csv`. Compare the failing attempt with a passing
one:

1. The create-connection command and parameters
2. Whether the phone reports `LE Connection Complete status 0x00` followed by
   `Disconnection Complete reason 0x3E`, or something else
3. Whether the ESP32 serial shows anything at that moment
4. What the phone's controller was doing just before (background scans, cancels)

## Evidence Commit

- Commit batch evidence under `docs/measurements/phase2/evidence/wo3d-ab-run/`
- Analysis files (`.md`, `.csv`, summaries) are committed
- Raw data (bug reports, full serial logs, HCI dumps) go in gitignored `raw/`
  subdirectories
- **Relative paths only** — no `file:///` or `/Users/`

## Success Criteria for Eventual Fix (Reference)

These criteria are NOT tested in this WO. They define what the CA needs to
accept any future coex change:
- Pixel single-attempt success ≥ 95% (n ≥ 100, using this rig)
- Zero established-link drops across 30-minute holds (Pixel + P12)
- P12 + Echelon app first-time connect unchanged
- 10/10 WiFi associations on the owner's network, no `AUTH_EXPIRE`

## Rules

- **Do NOT choose or implement a coex fix or architecture option.** Escalate
  the results to the CA.
- Any fix belongs in **matthewmcneill/esp32-app-foundation** (framework PR,
  app-neutral). Echenon carries only local diagnostic overrides, never committed.
- Use relative links only in committed docs.
- **PR-03 is not blocked.** WO4 (HRMClient) may proceed in parallel.

## Strict Boundary Box

**DO NOT touch:**
- `src/` — no Echenon application code changes
- `framework/` — no framework changes
- `SPEC.md`, `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`
- Do not commit local sdkconfig overrides (e.g. `WIFI_PS_MIN_MODEM`) — those
  belong to WO3e if triggered

**You MAY:**
- Flash the control peripheral and Echenon firmware (with owner permission)
- Create evidence directories under `docs/measurements/phase2/evidence/`
- Update the WO3b paper's summary with the A/B/A′ result
- Write upstream issue drafts (but do NOT file them)

## Verification Requirement

Work interactively with the owner to run the experiment. The session is complete
when:
1. All batches (A, B, A′, and optionally C) have run
2. Evidence is committed
3. Decision rules are evaluated
4. The WO3b paper summary is updated with the A/B/A′ result

Before wrapping up, ask the owner to confirm they are satisfied.

## Completion Report Return Prompt

After WO3d (and WO3e if triggered and approved), give the owner this return
prompt:

> **WO3d complete. Chief Architect: review the A/B/A′ batch results
> (`tools/ble-test` output under `docs/measurements/phase2/evidence/`), the
> captured failure trace, and the upstream issue drafts, then decide the coex
> direction.**

---

## Completion Report (written by the worker at task completion)

```markdown
---
## Completion Report
<!-- Written by the worker at task completion -->

### Status
complete | partial | blocked

### Batch Results Summary
| Batch | Pass/Total | Consecutive FAILs | Drops | Firmware | Config |
|---|---|---|---|---|---|

### Decision Rule Outcome
<!-- Which decision rule was triggered and what it means -->

### Changes Made
- `path/to/file` — what changed and why

### Trace Analysis
<!-- Summary of any captured failure HCI traces -->

### Decisions Made
- Decision description and rationale

### Dead Ends & Warnings
- What was attempted, why it failed, and what to avoid

### Out-of-Scope Findings
- Finding description and suggested action

### Upstream Issue Drafts
<!-- Summaries of the 4 issue drafts (NOT filed) -->

### Blockers
(none)
```
