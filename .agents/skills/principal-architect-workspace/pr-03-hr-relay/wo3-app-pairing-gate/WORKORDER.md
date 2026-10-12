---
name: hr-relay-wo3
description: Interactive go/no-go — Echelon app pairs Echenon-XXXX and shows synthetic HR
complexity: interactive
---

# WO3 — App Pairing Gate (GO / NO-GO)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo3-app-pairing-gate` (base: `hr-relay/wo2-hr-repeater`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Role Definition

> You are an expert senior software engineer working interactively with the
> owner on a bench test. You are NOT the Principal Architect. Do not make
> high-level architectural decisions. Your task is to execute the test plan
> below, help the owner flash the device, observe the results, and record
> evidence.

## Objective

**Go/no-go gate for PR-03.** Verify that the Echelon P12 app can pair with
`Echenon-XXXX` as a BLE heart-rate device and display the synthetic HR.
This closes register row #12 (originally inconclusive in DR-1 prep).

**If the test FAILS, STOP the PR and escalate to the Chief Architect.**

## Test Plan (SPEC §13.5 test 2.3, adapted for synthetic HR)

### Prerequisites
1. Firmware built with `env:ex3-waveshare-nano-diag` (includes
   `WITH_HR_SYNTHETIC` for the sine-wave HR source).
2. Device connected via USB to the bench machine.
3. Echelon P12 powered and nearby.

### Steps

1. **Create the WO3 branch:**
   ```bash
   cd /Users/mcneillm/Documents/Projects/Echenon-hr-relay
   git checkout -b hr-relay/wo3-app-pairing-gate hr-relay/wo2-hr-repeater
   ```

2. **Tag, build, and flash** (Rule 03):
   - Check highest tag: `git describe --tags --abbrev=0`
   - Create the next dev tag (e.g. `v0.1.0-dev.8`)
   - Build `env:ex3-waveshare-nano-diag` using PlatformIO MCP `build_project`
   - Flash using PlatformIO MCP `upload_firmware` — **only after owner confirms
     the device is available**

3. **Verify advertising (test 2.1):**
   - Open nRF Connect (or the owner's preferred BLE scanner)
   - Scan for `Echenon-XXXX` — confirm it advertises `0x180D` (Heart Rate Service)
   - Connect and verify `0x2A37` (HR Measurement) characteristic with Notify
   - Subscribe: should see a sine-wave HR (60–120 BPM)

4. **Echelon app pairing (test 2.3 — the gate):**
   - Open the Echelon app on the P12
   - Navigate to heart-rate device selection
   - Scan for devices
   - **Pair `Echenon-XXXX`** as the heart-rate source
   - Start a ride/class
   - **Verify:** The app displays the synthetic HR from Echenon's re-broadcast

5. **Record the evidence:**
   - Screenshot or photo of the Echelon app showing `Echenon-XXXX` paired
   - Screenshot or photo showing the HR value (should be 60–120 BPM sine wave)
   - Note the firmware version displayed in serial log

### Decision

| Result | Action |
|---|---|
| **PASS** — App pairs and shows HR | Record evidence, commit, proceed to WO4 |
| **FAIL** — App does not list or pair | **STOP PR-03.** Record what happened. Escalate to Chief Architect |

## Evidence Artifact

Create: `docs/measurements/phase2/A12-<date>-esp32-hr-pairing.md`

Template:
```markdown
# A12 — Echelon App HR Pairing Evidence

**Date:** <date>
**Firmware:** <version tag>
**Environment:** ex3-waveshare-nano-diag (WITH_HR_SYNTHETIC)
**Device:** Waveshare ESP32-S3-Nano (EX-3)
**App:** Echelon P12

## Test 2.1 — BLE Advertising
- [ ] `Echenon-XXXX` visible in BLE scan
- [ ] Advertises Heart Rate Service (0x180D)
- [ ] HR Measurement (0x2A37) sends notifications

## Test 2.3 — App Pairing Gate
- [ ] Echelon app lists `Echenon-XXXX` as HR device
- [ ] Pairing succeeds
- [ ] App displays synthetic HR (60–120 BPM sine wave)

## Verdict
**PASS / FAIL**

## Notes
<observations, timing, any anomalies>
```

## Strict Boundary Box

- **DO NOT** edit `PROGRAM.md`, `PROGRAM-PLAN.md`, or `reviews/` files.
- **DO NOT** implement any new modules or change code beyond what is needed to
  fix a test failure discovered during this session.
- **DO NOT** file GitHub issues.
- **If the test fails**, record the failure evidence and STOP.

## Rules

- **Rule 03 (Tag → Build → Flash):** Tag before flash.
- **Rule 04 (PlatformIO MCP tools only):** No `pio` CLI.
- **Flash only with owner permission.** The owner has parallel work on the device.

## Commit

If PASS:
- [ ] `git add docs/measurements/phase2/A12-*.md`
- [ ] `git commit -m "wo3: app pairing gate PASS — register row #12 closed"`
- [ ] Do NOT push.

---

## Completion Report (written by the worker at task completion)

<!-- Written by the worker at task completion -->

### Status
(pending)

### Test Results
- Test 2.1 (advertising): (pending)
- Test 2.3 (app pairing): (pending)

### Verdict
(pending)

### Out-of-Scope Findings
- (pending)
