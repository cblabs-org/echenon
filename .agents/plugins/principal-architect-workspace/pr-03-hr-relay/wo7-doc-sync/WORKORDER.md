---
schema_version: 1
wo_id: WO-03.7
pr_id: PR-03
title: Doc Sync + F3 Fix + PR-03 Wrap-Up
complexity: standard
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
# WO7 — Doc Sync + F3 Fix + PR-03 Wrap-Up

> You are a senior software engineer worker agent.
> **DO NOT trigger or act as the principal-architect.** Execute the tasks
> below, produce the Completion Report, and stop.

## Objective

Final PR-03 work order: fix the F3 bug (BPM=0 not relayed), verify SPEC
sections match what was built, and prepare PROJECT.md for PR submission.

## Context

| Item | Details |
|---|---|
| **Worktree** | `<worktree>` |
| **Branch** | `hr-relay/wo7-doc-sync` (based on `hr-relay/wo6-coexistence`) |
| **SPEC** | `SPEC.md` — verify §3.5, §7.8, §8, §13.5 match implementation |
| **Prior WOs** | WO1–WO4 ✅, WO5 ⚠️ 9/11, WO6 ✅ programme gate cleared |

### Key files

- `src/echenon/HRMRepeater.cpp` — F3 fix target (line 87)
- `src/echenon/HRMRepeater.h`
- `src/echenon/HRMClient.h` / `.cpp`
- `src/echenon/HRParsing.h`
- `src/echenon/HREvents.h`
- `src/echenon/EchenonModule.h`
- `src/main.cpp`
- `SPEC.md` — read-only for verification, DO NOT EDIT
- `docs/upstream-central-acceptance-report.md` — verify content

## Strict Boundary Box

**May change:**
- `src/echenon/HRMRepeater.cpp` (F3 fix only — remove the `bpm > 0` guard)
- `test/` (if a new native test is needed for the F3 fix)
- `docs/upstream-central-acceptance-report.md` (if corrections needed)
- This file (`WORKORDER.md` — Completion Report)

**Must NOT change:**
- `SPEC.md`, `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`
- `framework/`, `platformio.ini`
- Other PRs' `PROJECT.md`
- GPIO 43/44 must not be referenced
- Other files in `src/` beyond the F3 fix

## Tasks

### Task 1: F3 Fix — BPM=0 Relay

**Problem:** `HRMRepeater::onHrEvent()` at line 87 of `src/echenon/HRMRepeater.cpp`
has `if (sample->bpm > 0)` which suppresses BPM=0 notifications. The Beat
sends BPM=0 as its "off-skin" signal. The relay should be transparent — the
downstream app (Echelon Fit) is designed to handle BPM=0.

**Fix:** Remove the `if (sample->bpm > 0)` guard so that `notifyHr()` is
called for all BPM values including 0. The `_lastBpm = sample->bpm` assignment
on line 86 already runs unconditionally, so the fix is simply removing the
conditional wrapper.

Before:
```cpp
self->_lastBpm = sample->bpm;
if (sample->bpm > 0) {
    self->notifyHr(sample->bpm);
}
```

After:
```cpp
self->_lastBpm = sample->bpm;
self->notifyHr(sample->bpm);
```

**Verify:** Run `env:native` tests — all must pass. If existing tests cover
the BPM=0 case, confirm they now exercise the notification path. If not,
add a brief test verifying that BPM=0 produces a valid `0x2A37` encoding
(it should — `encode0x2A37(0)` is `{0x00, 0x00}`).

### Task 2: SPEC Verification (read-only)

Read these SPEC sections and compare with the actual implementation. Record
any discrepancies in the Completion Report as findings — do NOT edit the SPEC.

1. **§3.5 (Composition):** Verify the module registration order in `main.cpp`
   matches what §3.5 describes. Check `BLEManager` composition, provider
   registration, and lifecycle ordering.

2. **§7.8 (Connection budget, device name):** Verify `MAX_CONNECTIONS=4`,
   `BLE_MAX_ACT=6`, device name `Echenon-XXXX` (MAC suffix) matches.

3. **§8 (HR relay):** Verify §8.1–§8.5 match the implementation:
   - §8.1: HRMRepeater re-broadcasts `0x180D`/`0x2A37`
   - §8.2: EventBus decoupling (no direct call)
   - §8.3: Entry gates closed
   - §8.4: Reconnect policy `{2,4,8,16,32}` then 15s, stale timeout 10s,
     BPM=0 handling (after F3 fix)
   - §8.5: Coexistence policy (ESP-IDF defaults, no `esp_coex_preference_set`)

4. **§13.5 (Tests 2.1–2.7):** Verify all test descriptions match what was
   actually tested. Note any test whose pass criterion changed during execution.

### Task 3: Build Verification

Run `env:native` tests via PlatformIO MCP tools. All must pass after the F3 fix.
Build `env:ex3-waveshare-nano` — verify it compiles clean. Record test count
and build metrics in the Completion Report.

### Task 4: Upstream Report Check

Read `docs/upstream-central-acceptance-report.md`. Verify:
- No Echenon-specific names or paths (must be app-neutral)
- The framework fix (PR #296) is referenced
- Test results from the WO5 re-run are included
- The document is ready for the PA to review before filing upstream

## Tooling Constraints

- **Rule 04:** PlatformIO MCP tools for builds and tests.
- Surface findings in the Completion Report — do not file issues.

---

## Completion Report (written by the worker at task completion)

### Status
**COMPLETE** — All 4 tasks executed within the Strict Boundary Box on branch `hr-relay/wo7-doc-sync`.

### F3 Fix
- **Target:** `src/echenon/HRMRepeater.cpp` line 87 (in `HRMRepeater::onHrEvent()`).
- **Change:** Removed the `if (sample->bpm > 0)` guard so that `notifyHr(sample->bpm)` is called unconditionally for all BPM values, including BPM=0.
- **Result:** When a strap sends BPM=0 (e.g. Echelon Beat off-skin), or when `HRMClient` marks contact lost and publishes BPM=0, `HRMRepeater` now immediately re-broadcasts the 0 BPM reading via GATT `0x2A37` (`{0x00, 0x00}`) to connected peripheral clients (e.g. Echelon Fit app).
- **Test Coverage:** Verified existing native tests in `test/test_native/test_hr_encoding.cpp` (`HREncodingTest.EncodeBpm0` encoding BPM=0 to `{0x00, 0x00}`) and `test/test_native/test_hr_parsing.cpp` (`HRParsingTest.BpmZeroWithNoContactFlags`).

### SPEC Verification
Read SPEC §3.5, §7.8, §8, and §13.5 and compared with implementation:
1. **§3.5 (Composition):**
   - Registration order matches: `app.setAppName()`, `app.getWifiManager().setDefaultHostname()`, `echenonModule.setConfigManager()`, `app.registerModule(&echenonModule)`.
   - Under `WITH_BLE`: `bleManager.setDefaultAdvertisedName("Echenon")`, `bleManager.registerServiceProvider(&echenonModule.hrmRepeater())`, `bleManager.registerClientProvider(&echenonModule.hrmClient())`, `app.registerModule(&bleManager)`, `app.registerModule(&echenonModule.hrmClient())` (registered as `ITickable` for stale timeout).
   - Lifecycle: `app.begin(config)` precedes `bleManager.begin(app)` (locks registration).
   - Post-begin wiring: `bleCfg["slowRetryMs"] = 15000` applied via `bleManager.applyConfig()`, `hrmClient().setEventBus()`, `hrmClient().setRepeater()`, `hrmRepeater().subscribe()`.
   - Note on future phases: §3.5 includes `ftmsServer()`, `uploadQueue()`, and `otaManager` which belong to Phases 3, 4, and OTA; their absence in PR-03 is as designed.
2. **§7.8 (Connection budget, device name):**
   - `hardware/ex3-waveshare-nano/sdkconfig.defaults` sets `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` and `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (3 peripheral + 1 central = 4 connections; 4 connections + adv + scan = 6 activity slots).
   - Device name: `bleManager.setDefaultAdvertisedName("Echenon")` with default `nameSuffix = true` yields `Echenon-XXXX` via framework MAC suffix. Matches §7.8.
3. **§8 (HR Relay):**
   - **§8.1:** `HRMRepeater` re-broadcasts `0x180D`/`0x2A37` (Notify) and `0x2A38` (Read). Matches.
   - **§8.2:** EventBus decoupling: `HRMClient` publishes `HrSample` on topic `"echenon.hr"` via a 4-slot ring buffer. `HRMRepeater` subscribes to `"echenon.hr"`. No direct HR data call between client and repeater (only `setBodySensorLocation` is forwarded on client connection ready). Matches.
   - **§8.3:** Entry gates closed:
     - Gate 1 (Echelon-app HR spike / row #12): Closed by WO3 / WO5 (Echelon Fit on Lenovo Tab P12 paired and showed live HR from Echenon). Android phone 0x3E investigated in WO3b-WO3d and deferred to PR-05 per CA/owner direction.
     - Gate 2 (Framework central-role acceptance evidence): Executed in WO5 (Beat HW901 connected, RFC §17.2 items 4-9 tested).
   - **§8.4:** Reconnect policy `{2,4,8,16,32}` fast backoff, 15s slow retry (`slowRetryMs = 15000` set via `bleManager.applyConfig()`), stale timeout 10s (`HRMClient::tick()` marks `NO_CONTACT` and publishes HR=0), BPM=0 handling (F3 fix applied: BPM=0 notified). Matches.
   - **§8.5:** Coexistence policy: Framework's `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y` retained. Application code does not call `esp_coex_preference_set()`. Matches.
4. **§13.5 (Tests 2.1–2.7):**
   - Test 2.1 (`Echenon-XXXX` advertises `0x180D`): PASS (WO1/WO3).
   - Test 2.2 (Portal scan and pair HR strap): PASS (WO5, Beat HW901, BSL=0x04).
   - Test 2.3 (Echelon app pair `Echenon-XXXX` as HR device): PASS (WO3 synthetic, WO5/WO6 live Beat on Tab P12; 82–100 BPM).
   - Test 2.4 (Armband off skin -> HR absent within 12s): PASS (WO5, absent within ~2s upon BPM=0).
   - Test 2.5 (Auto-sleep and reconnect ≤ 20s): FAIL due to framework bug F4 (discovery scan resource blocks auto-reconnect scheduler). Manual pair works; auto-reconnect blocked until framework fix.
   - Test 2.6 (Factory reset, repeat 2.5): BLOCKED by Test 2.5 / F4.
   - Test 2.7 (Progressive coexistence check: HR relay + portal open + HTTP POST bursts, 5 minutes): PASS (WO6; max HR gap 1,200 ms ≤ 2,500 ms limit; 326 notifications / 325.3s; 30/30 HTTP OK; heap min 81 KB >> 20 KB threshold; programme gate cleared).
   - Discrepancy finding: Tests 2.5/2.6 recorded as FAIL/BLOCKED due to framework limitation F4 (persistent discovery scan holds scan lock). Does not block PR-03 as programme coexistence gate (Test 2.7) passed.

### Build Results
All builds executed via PlatformIO MCP tools (`platformio-mcp_platformio`):
- `run_tests` (`env:native`): **45 / 45 tests PASSED** in 0.54s (8 HREncoding, 13 HRParsing, 10 SignalLogger, 14 SignalLogic).
- `build_project` (`env:ex3-waveshare-nano`): **PASSED** (RAM: 53,164 B / 16.2%, Flash: 1,527,583 B / 24.3%).
- `build_project` (`env:ex3-waveshare-nano-diag`): **PASSED** (RAM: 53,768 B / 16.4%, Flash: 1,677,427 B / 26.6%).
- GPIO 43/44: Confirmed not referenced anywhere in codebase.

### Upstream Report Review
- Checked `docs/upstream-central-acceptance-report.md`.
- App-neutral: Confirmed zero occurrences of "Echenon" (uses generic "consumer application").
- Framework fix reference: PR #296 (commit `1e90ec68`) explicitly cited and verified.
- WO5 re-run evidence: RFC §17.2 test table (items 4-9) included with both First Run and Re-Run columns.
- Document status: In `DRAFT` state, fully prepared for PA review and upstream filing.
- Note for PA: Pre-existing file `docs/measurements/phase0/B05-20261011-wo5-bench-evidence-report.md` contains legacy `file:///` URLs from Phase 0; outside WO7 Strict Boundary Box.
