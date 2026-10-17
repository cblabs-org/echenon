---
name: hr-relay-wo7
description: Doc sync, F3 fix, and PR-03 wrap-up
complexity: standard
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
| **Worktree** | `/Users/mcneillm/Documents/Projects/Echenon-hr-relay` |
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
_To be written._

### F3 Fix
_To be written._

### SPEC Verification
_To be written._

### Build Results
_To be written._

### Upstream Report Review
_To be written._
