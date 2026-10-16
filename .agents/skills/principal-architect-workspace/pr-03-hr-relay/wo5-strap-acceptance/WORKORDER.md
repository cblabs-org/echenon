---
name: hr-relay-wo5
description: Interactive strap acceptance — Beat HW901, SPEC §13.5 tests 2.2–2.6, RFC §17.2 central-role evidence
complexity: interactive
---

# WO5 — Strap Acceptance

> You are an expert senior software engineer and testing-focused worker agent.
> **DO NOT trigger or act as the principal-architect.** Your task is to work
> interactively with the user (owner) to execute the acceptance tests below,
> record evidence, and produce the Completion Report.

## Objective

Flash the `ex3-waveshare-nano` firmware and run hardware acceptance tests
with the Echelon Beat HW901 armband and the Lenovo Tab P12 tablet. This is
the first time the full HR relay path (strap → HRMClient → EventBus →
HRMRepeater → tablet) is tested end-to-end.

Secondary objective: produce **RFC §17.2 items 4–9 consumer evidence** for
the framework's central BLE role, which the upstream ACCEPTANCE.md marks as
"N/A" because the in-tree app has no `IBLEClientProvider`.

## Context

| Item | Details |
|---|---|
| **Worktree** | `/Users/mcneillm/Documents/Projects/Echenon-hr-relay` |
| **Branch** | `hr-relay/wo5-strap-acceptance` (based on `hr-relay/wo4-hr-client`) |
| **SPEC** | `SPEC.md` §8 (HR relay), §8.4 (reconnect policy), §13.5 (tests 2.2–2.6) |
| **Prior evidence** | `docs/measurements/phase2/A12-20261012-esp32-hr-pairing.md` (WO3 — tablet pairing with synthetic HR) |
| **Beat armband** | Echelon Beat Advanced Armband, model HW901, advertises `EBEAT-NNNNNNN`, `0x180D`, uint8 BPM, no contact flags (sends BPM=0 for off-skin), battery `0x180F`, BSL `0x04` (Hand), auto-sleep ~30–60 s after no skin contact |
| **Tablet** | Lenovo Tab P12 running Echelon Fit app |
| **Framework** | `v0.0.12-dev.93` (submodule at `framework/`) |
| **WO3d finding** | H-COEX eliminated; Android 0x3e is NimBLE scheduling, not WiFi. PR-03 is unblocked for the P12 target. |

### Key files

- `src/echenon/HRMClient.h` / `.cpp` — Central HR client
- `src/echenon/HRMRepeater.h` / `.cpp` — Peripheral HR re-broadcaster
- `src/echenon/HRParsing.h` — `0x2A37` parser
- `src/echenon/HREvents.h` — `HrSample`, `HRContactState`, `HR_TOPIC`
- `src/echenon/EchenonModule.h` — composition
- `src/main.cpp` — lifecycle wiring, `slowRetryMs = 15000`
- `framework/.agents/skills/principal-architect-workspace/pr-225-ble-manager/wo6-validation/ACCEPTANCE.md` — upstream acceptance (items 4–9 to re-run with central role)

## Strict Boundary Box

**May change:**
- Files under `docs/measurements/phase2/` (evidence documents)
- Files under `docs/` (the upstream report draft)
- This file (`WORKORDER.md` — Completion Report)

**Must NOT change:**
- `src/`, `test/`, `framework/`, `platformio.ini`
- `SPEC.md`, `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`
- Other PRs' `PROJECT.md`
- GPIO 43/44 must not be referenced

## Pre-Flight

1. **Tag and flash.** This is the first flash of the full HR relay firmware
   (HRMClient + HRMRepeater). Follow Rule 03 (Tag → Build → Flash):
   ```
   git describe --tags --abbrev=0   # check current highest tag
   git tag v0.1.0-dev.N             # increment
   git push origin v0.1.0-dev.N
   ```
   Build `env:ex3-waveshare-nano` (NOT `-diag` — this is the production env
   without `WITH_HR_SYNTHETIC`). Flash. Verify `curl http://<ip>/api/system`
   shows the new tag.
2. **Confirm:** serial shows `BLEManager` boot, `hr_client` and `hr_repeater`
   providers registered, BLE advertising with `0x180D`.

## Test Plan — SPEC §13.5 Tests 2.2–2.6

Execute each test. Record PASS/FAIL with evidence. If any test FAILS,
record the failure detail and continue (do not abort the session).

### Test 2.2: BLE Portal — Scan and Pair HR Armband

1. Put the Beat armband on skin (confirm LED flashes green).
2. Open the Echenon web portal → BLE tab.
3. Start a scan for `hr_client`.
4. The Beat should appear as `EBEAT-NNNNNNN`.
5. Pair it.
6. **Verify:**
   - Portal shows the device as "Connected" with HR value updating.
   - Battery level shown (if available).
   - Serial log shows `>>> Central connected` and `0x2A37` notifications.
   - Body Sensor Location reads as `0x04` (Hand).
7. **Evidence:** screenshot or serial excerpt showing connected state with HR.

### Test 2.3: End-to-End Relay — Echelon App

1. With the Beat connected to the ESP32, open the Echelon Fit app on the P12.
2. Pair `Echenon-XXXX` as the heart-rate device (if not already paired from WO3).
3. **Verify:**
   - The app displays heart rate that matches what the serial log shows.
   - The HR is from the re-broadcast (HRMRepeater), NOT directly from the armband.
   - Confirm by checking that the armband is NOT visible to the Echelon app
     (it only allows one Central — the ESP32 holds it).
4. **Evidence:** photo or screenshot of the Echelon app showing HR, plus serial
   log excerpt showing `HRMClient` receiving and `HRMRepeater` notifying.

### Test 2.4: Contact Loss — Armband Removed

1. While HR is relaying, remove the armband from skin.
2. **Verify (criterion a — §8.4):**
   - HR marked absent (BPM=0 or notifications stop) within **12 s** of last
     valid notification.
   - The Beat sends BPM=0 (its off-skin signal); `HRMClient` should detect
     this via the `bpm == 0` path and publish NO_CONTACT immediately.
   - The Echelon app stops showing HR (or shows 0).
3. **Measure:** time from armband removal to HR=0 in serial log.
4. **Evidence:** serial log timestamps.

### Test 2.5: Auto-Sleep and Reconnect

1. Leave the armband off skin for ~60 s (until it stops advertising and the
   ESP32 disconnects — serial shows `onDisconnected()`).
2. Put the armband back on skin (LED should start flashing).
3. **Verify (criterion c — §8.4):**
   - ESP32 reconnects within **≤ 20 s** after the armband starts advertising.
   - `slowRetryMs = 15000` means the slow phase retries every 15 s.
   - Serial shows reconnection and `0x2A37` notifications resume.
   - The Echelon app resumes showing HR.
4. **Repeat 3 times** to verify the reconnect schedule never gives up.
5. **Measure:** time from armband LED-on to first HR notification in serial.
6. **Evidence:** serial log showing disconnect, retry attempts, and reconnect
   timestamps for each trial.

### Test 2.6: Factory Reset — slowRetryMs Persistence

1. Factory reset the ESP32 (portal → System tab → Factory Reset, or MCP
   `factory_reset`).
2. After reboot, re-pair the Beat armband (Test 2.2 abbreviated).
3. Remove the armband, let it auto-sleep, put it back on.
4. **Verify:** reconnect time is still ≤ 20 s (proving `slowRetryMs = 15000`
   is re-applied after factory reset, not lost to the 60 s framework default).
5. **Evidence:** serial log showing post-reset reconnect timing.

## RFC §17.2 Items 4–9 — Central-Role Consumer Evidence

These items were tested upstream for peripheral-only (no `IBLEClientProvider`).
Re-run them with the full central+peripheral configuration and a real strap.

| Item | Test | How to evidence |
|---|---|---|
| 4 | WiFi + BLE coexistence (central + peripheral active) | 10 rapid HTTP requests to portal while HR relaying; all 200 OK + HR uninterrupted |
| 5 | BLE connect/disconnect cycling (central role) | 3 cycles: unpair strap → re-pair → verify HR. No crash, no leaked connections, `droppedEvents: 0` |
| 6 | DRAM budget with central role active | `curl /api/system` → record `heap_free`, `heap_min_free`. Must be ≥ 20,480 B |
| 7 | Boot sequence with central role registered | Clean boot log showing `hr_client` and `hr_repeater` providers, no errors |
| 8 | Peripheral re-advertising after central disconnect | Disconnect the P12 (tablet), reconnect — HR service still available, HR resumes |
| 9 | Central reconnect does not disrupt peripheral connections | While P12 is connected and receiving HR, force-disconnect the strap (move out of range). Verify: P12 stays connected, HR goes to 0, strap reconnect resumes |

## Upstream Report

After all tests, draft a brief **app-neutral** upstream report for the
framework's central-role acceptance gap:

- File it at `docs/upstream-central-acceptance-report.md` (draft — the PA
  reviews before filing an actual issue).
- Format: "Framework Central-Role BLE Acceptance — Consumer Evidence from
  [a consumer app]" (no Echenon name in the upstream-facing version).
- Include: firmware tag, hardware (ESP32-S3 Waveshare Nano), strap model,
  test results for items 4–9, any anomalies.
- The PA files the actual upstream issue; the worker drafts the content.

## Evidence Directory

All evidence files go in:
`docs/measurements/phase2/wo5-strap-acceptance/`

Create this directory. Place:
- `evidence.md` — the main acceptance report with all test results
- Serial log excerpts (inline in `evidence.md`, not separate files)
- Screenshots referenced from `evidence.md`

## Verification Requirement

Work interactively with the user to execute all tests. The session is
complete when:
1. All 5 SPEC tests (2.2–2.6) have PASS/FAIL recorded with evidence.
2. All 6 RFC items (4–9) have PASS/FAIL recorded with evidence.
3. The upstream report draft exists.
4. The user confirms they are satisfied.

Before wrapping up, ask the user to confirm they are satisfied with the
evidence and results.

## Tooling Constraints

- **Rule 03:** Tag → Build → Flash. Dev tags are autonomous.
- **Rule 04:** PlatformIO MCP tools only (no `pio` CLI).
- **No GPIO 43/44** references.
- Surface out-of-scope findings in the Completion Report — do not file issues.

---

## Completion Report (written by the worker at task completion)

### Status

**PARTIAL** — 2/11 tests PASS, 9/11 BLOCKED by critical framework bug (MAC address byte-order inversion in `BLEStack.cpp`).

### Test Results Summary

| Test | Result | Notes |
|---|---|---|
| 2.2 Portal scan + pair | **BLOCKED** | Scan finds Beat (`EBEAT-0014941`, RSSI -39), but `pClient->connect()` times out after 30s with `status=13` due to reversed MAC address |
| 2.3 End-to-end relay | **BLOCKED** | Requires 2.2 |
| 2.4 Contact loss | **BLOCKED** | Requires 2.2 |
| 2.5 Auto-sleep + reconnect | **BLOCKED** | Requires 2.2 |
| 2.6 Factory reset slowRetryMs | **BLOCKED** | Requires 2.2 |
| RFC 4: WiFi+BLE coex | **BLOCKED** | Requires central connection |
| RFC 5: Connect/disconnect cycling | **BLOCKED** | Requires central connection |
| RFC 6: DRAM budget | **PASS ✅** | `heap_min_free` = 93,404 B (threshold: 20,480 B); post-testing: 86,496 B |
| RFC 7: Boot sequence | **PASS ✅** | Clean boot, `hr_client` + `hr_repeater` registered, BLE advertising `0x180D`, WiFi coex active, no errors |
| RFC 8: Peripheral re-advertising | **BLOCKED** | Requires central connection |
| RFC 9: Central reconnect vs peripheral | **BLOCKED** | Requires central connection |

### Firmware Tag

`v0.1.0-dev.14` — built and flashed via MCP. Verified via `curl /api/system`.

### Decisions Made

1. **Root cause investigation prioritized over test forcing.** Rather than working around the connection failure, we traced it to a definitive framework bug with a verified fix path.
2. **Mac bleak used as control test** per user direction. Confirmed the Beat is connectable and functioning correctly — the issue is entirely on the ESP32 side.
3. **Research subagent dispatched** to investigate SmartSpin2k/QZ connection approaches and NimBLE internals, which contributed to identifying the byte-order root cause.

### Dead Ends & Warnings

1. **Scan parameter widening (50% duty):** Changed `reconnectScan` to `{intervalMs:320, windowMs:160}` — no improvement. The issue is not scan timing; it's the wrong target address.
2. **`connectable: false` red herring:** The ESP32 scan results report `connectable: false` for the Beat, but this turned out to be a cosmetic API bug (field never populated), not the connection blocker.
3. **Multiple forget/re-pair cycles:** Attempted ~10+ connection cycles with reboots and parameter changes. All fail identically with `status=13` because the underlying address is always reversed.

### Out-of-Scope Findings

> These are framework-level bugs discovered during testing. Per WO5 boundary box constraints, they are surfaced here — not filed as issues and not fixed.

#### F1 (CRITICAL): MAC Address Byte-Order Inversion — `BLEStack.cpp:640, 741`

`BLEStack::executeCentralConnect()` passes `BLEPeerAddress.bytes` (little-endian) to `NimBLEAddress(const uint8_t[], type)` which expects big-endian (`esp_bd_addr_t`) and applies `std::reverse_copy()`. Result: **every central-role BLE connection targets a byte-reversed MAC address and will always time out**.

- **Impact:** ALL central-role connections are broken. No `IBLEClientProvider` can connect to any device.
- **Fix:** Use `NimBLEAddress(ble_addr_t)` constructor instead:
  ```cpp
  ble_addr_t rawAddr;
  rawAddr.type = static_cast<uint8_t>(addr.type);
  memcpy(rawAddr.val, addr.bytes, 6);
  NimBLEAddress bleAddr(rawAddr);
  ```
- **Verification:** Python byte-tracing confirms address `E7:A7:DC:1C:4C:AD` is inverted to `AD:4C:1C:DC:A7:E7`.
- **Evidence:** `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`

#### F2 (Minor): `connectable` Field Never Populated in Scan Results

`BLEManager.cpp:1136-1140` creates `BLEScanResult` but never sets `res.connectable`. It defaults to `false` for all devices.

- **Impact:** `/api/ble/scan/results` always reports `"connectable": false` regardless of the device's actual advertising type.
- **Fix:** Propagate `isConnectable()` from NimBLE scan callback through `BLEQueueMessage` to `BLEScanResult`.

### Upstream Report

Draft filed at `docs/upstream-central-acceptance-report.md` — ready for PA review.

### Evidence

All evidence in `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`.

### Blockers

**The byte-order fix (F1) must be applied in the framework before WO5 can be re-run to completion.** Once fixed:
1. Rebuild and re-flash
2. Re-run all 9 blocked tests (2.2–2.6, RFC 4, 5, 8, 9)
3. No other blockers anticipated — the Beat's BLE profile is standard `0x180D`, Mac control test proves connectivity works
