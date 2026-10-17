---
schema_version: 1
wo_id: WO-03.3
pr_id: PR-03
title: App Pairing Gate (GO / NO-GO)
complexity: interactive
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- framework/
- platformio.ini
- tools/
verification_command: echo legacy-migrated
---
# WO3 — App Pairing Gate (GO / NO-GO)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo3-app-pairing-gate` (base: `hr-relay/wo2-hr-repeater`)
> **Worktree:** `<worktree>`

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
   cd <worktree>
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
**PASS** — committed `762af2f` on `hr-relay/wo3-app-pairing-gate`

### Test Results
- Test 2.1 (advertising): **PASS** — `Echenon-7226` visible in nRF Connect
  (Android) and bleak (macOS). Advertises `0x180D`, GATT table correct (0x2A37
  Notify, 0x2A38 Read, 0x180A Device Info). 20/20 HR notifications received
  via bleak at 1 Hz, sine wave 60–120 BPM confirmed.
- Test 2.3 (app pairing): **PASS** — Echelon Fit app on Lenovo Tab P12
  discovered `Echenon-7226`, paired successfully, displayed synthetic HR
  (observed 99 BPM during sine cycle). Serial log confirmed clean connection
  (48.75ms interval, 5.0s supervision timeout, MTU 185), stable 1 Hz
  notifications for duration of session.

### Verdict
**GO** — PR-03 may proceed. Register row #12 closed.

### Out-of-Scope Findings

1. **Android phone BLE connection failure (0x3e) — BLOCKING for generic
   HR/FTMS relay.** The owner's Samsung Android phone can discover `Echenon-7226`
   but cannot complete connection establishment. Serial logs show
   `BLE_ERR_CONN_ESTABL_FAIL` (reason 0x3e) at the link-layer controller — the
   NimBLE host never sees the connection. The same phone connects to the owner's
   eBeat HR armband via Strava without issues, proving this is a defect in our
   device's BLE behaviour, not a phone limitation. macOS (bleak) and the Echelon
   P12 tablet both connect cleanly. Root cause is likely WiFi/BLE radio
   coexistence under `PREFER_WIFI` arbiter policy starving BLE connection
   request handling. The 50ms advertising interval is the safest lever to
   investigate (100–160ms); coex preference changes are off-limits per the
   dev.73–dev.80 hardening cycle findings. Full evidence in
   `docs/measurements/phase2/A12-20261012-esp32-hr-pairing.md`.

2. **Multi-connection radio coex architecture concern.** The full Phase 2/3
   topology requires 1 BLE central (HR strap inbound) + 2 BLE peripheral
   (HR re-broadcast + FTMS) + WiFi STA — all concurrent on one 2.4 GHz radio.
   We already cannot reliably connect an Android phone with just 1 peripheral +
   WiFi. The dev.73–dev.80 cycle proved the coex knobs are razor-thin: any tilt
   toward BLE breaks WiFi auth, any tilt toward WiFi drops BLE connections.
   This raises a legitimate question about whether the single-radio ESP32-S3 can
   support this topology at all, or whether the architecture needs to adapt
   (e.g. WiFi-off during rides, dedicated BLE-only mode, reduced peripheral
   count, or dual-chip). **Recommend escalation to Chief Architect for a radio
   coex architecture review before Phase 3 work orders are written.**

