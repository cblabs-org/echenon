---
name: hr-relay-wo6
description: P12 v1 topology coexistence — programme gate
complexity: interactive
---

# WO6 — P12 v1 Topology Coexistence

> You are an expert senior firmware test engineer and testing-focused worker
> agent. **DO NOT trigger or act as the principal-architect.** Your task is
> to work interactively with the user (owner) to execute the coexistence
> gate test below, record evidence, and produce the Completion Report.

## Objective

Run the **v1 topology coexistence gate** on the Lenovo Tab P12: the full HR
relay path (Beat strap → HRMClient → EventBus → HRMRepeater → P12) running
simultaneously with the web portal open and a large HTTPS POST, sustained
for 5 minutes.

This is a **programme gate**: "v1 BLE topology proven on the P12". If this
test fails, **STOP and escalate to the Chief Architect** — do not attempt
fixes or pick architecture alternatives.

## Context

| Item | Details |
|---|---|
| **Worktree** | `<worktree>` |
| **Branch** | `hr-relay/wo6-coexistence` (based on `hr-relay/wo5-strap-acceptance`) |
| **SPEC** | §8.5 (Radio Coexistence Policy), §13.5 test 2.7 |
| **Prior evidence** | WO5 re-run: 9/11 PASS (2.2–2.4, RFC 4–9). HR relay proven point-to-point. |
| **Beat armband** | Echelon Beat HW901, `EBEAT-0014941`, BLE addr `E7:A7:DC:1C:4C:AD` (RandomStatic) |
| **Tablet** | Lenovo Tab P12 running Echelon Fit app |
| **Framework** | submodule at `1e90ec68` (PR #296 — MAC byte-order fix) |
| **Firmware** | Currently `v0.1.0-dev.15` — if no code changes, no re-tag needed |
| **WO5 findings** | F3: BPM=0 not relayed (cosmetic, does not affect this test). F4: discovery scan blocks auto-reconnect (manual pair used — no impact). |

### Pass criterion (SPEC §8.4 criterion b)

> No HR gap > **2.5 s** while connected and on-skin.

### Topology under test

```
Beat HW901 ──BLE central──▶ ESP32-S3 ──BLE peripheral──▶ P12 Echelon Fit
                                │
                            WiFi STA
                                │
                    ┌───────────┴───────────┐
                    │                       │
              Portal (WebSocket          HTTPS POST
               open, ~2 Hz WS)          (large payload)
```

## Strict Boundary Box

**May change:**
- Files under `docs/measurements/phase2/` (evidence documents)
- This file (`WORKORDER.md` — Completion Report)

**Must NOT change:**
- `src/`, `test/`, `framework/`, `platformio.ini`
- `SPEC.md`, `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`
- Other PRs' `PROJECT.md`
- GPIO 43/44 must not be referenced

## Pre-Flight

1. **Branch.** Create `hr-relay/wo6-coexistence` from `hr-relay/wo5-strap-acceptance`.
2. **No re-tag needed** if the firmware is still `v0.1.0-dev.15` with no
   code changes since WO5. If there are code changes, tag `v0.1.0-dev.16`
   first (Rule 03). The current firmware on the device should be correct —
   check with `curl http://<ip>/api/system`.
3. **Confirm equipment:**
   - Beat armband on skin (green LED flashing)
   - P12 tablet with Echelon Fit app ready
   - Portal accessible in a browser (laptop/phone)
   - Serial monitor running for timestamps

## Test Plan — SPEC §13.5 Test 2.7

### Phase 1: Establish the topology (setup, not timed)

1. **Pair the Beat** via portal or API (`/api/ble/pair`). Verify serial shows
   `0x2A37` notifications at ~1 Hz.
2. **Connect P12** via Echelon Fit app. Verify HR displaying on the tablet.
3. **Open the portal** in a browser tab. Navigate to any page that maintains
   a WebSocket connection (Home, BLE tab, etc.).
4. **Verify all three paths active:**
   - Serial: `0x2A37` notifications from Beat (central)
   - Serial: `GATT notify att_handle=16` to P12 (peripheral)
   - Portal: page loads, WebSocket connected
5. Record baseline: `curl http://<ip>/api/system` — note `heap_free`,
   `heap_min_free`, `uptime`.

### Phase 2: Coexistence stress (5-minute sustained test)

Start a **5-minute timer** from the serial log. During this period:

1. **HR relay running continuously** — Beat on skin, P12 Echelon Fit displaying.
2. **Portal open** — keep the browser tab open. Periodically interact
   (refresh, navigate between tabs) to ensure the WebSocket stays alive.
3. **HTTPS POST stress** — at least 3 large HTTPS POSTs during the 5 minutes.
   Use the ride-log export, firmware update check, or a manual `curl` POST:
   ```bash
   # Example: 10 rapid API hits to /api/system (each is a full HTTP request)
   for i in $(seq 1 10); do curl -s http://<ip>/api/system > /dev/null && echo "OK $i"; done
   ```
   Or if there's a ride-log CSV export endpoint, trigger that (it's the
   heaviest WiFi operation available).
4. **Watch serial** for:
   - Any gap in `0x2A37` notifications > 2.5 s
   - Any `0x3e` or disconnect events
   - Any `droppedEvents` increments
   - Any heap warnings or watchdog triggers

### Phase 3: Record results

1. **Post-test snapshot:** `curl http://<ip>/api/system` — record `heap_free`,
   `heap_min_free`, `uptime`, `droppedEvents`.
2. **HR gap analysis:** From the serial log, calculate the maximum gap between
   consecutive `0x2A37` notifications during the 5-minute window. The pass
   criterion is: **no gap > 2.5 s**.
3. **Connection stability:** Record whether:
   - The P12 peripheral connection survived the full 5 minutes
   - The Beat central connection survived the full 5 minutes
   - The portal WebSocket stayed connected
4. **Heap trajectory:** Compare `heap_min_free` pre vs post. Any significant
   drop (> 10 KB) suggests a slow leak under load.

## Evidence Directory

All evidence files go in:
`docs/measurements/phase2/wo6-coexistence/`

Create this directory. Place:
- `evidence.md` — the main coexistence report with timestamps and results
- Serial log excerpts (inline or attached)

## Gate Decision

| Outcome | Action |
|---|---|
| **PASS** — no HR gap > 2.5 s, all connections stable, heap stable | Record PASS. PR-03 programme gate cleared. |
| **FAIL** — HR gap > 2.5 s, or connection drop, or heap exhaustion | **STOP. Do not attempt fixes.** Record the failure evidence and escalate to the Chief Architect. |

## Verification Requirement

The session is complete when:
1. The 5-minute coexistence test has PASS/FAIL recorded with evidence.
2. The evidence document is committed.
3. The user confirms they are satisfied.

## Tooling Constraints

- **Rule 03:** Tag → Build → Flash if code changed. No re-tag if unchanged.
- **Rule 04:** PlatformIO MCP tools only (no `pio` CLI).
- **No GPIO 43/44** references.
- Surface out-of-scope findings in the Completion Report — do not file issues.

---

## Completion Report (written by the worker at task completion)

### Status
**COMPLETE** — 2026-10-17. Programme gate **PASS**.

### Test Results Summary

Test 2.7 (SPEC §13.5) — P12 v1 topology coexistence, executed 2026-10-17
22:58–23:04 BST on firmware `v0.1.0-dev.15`.

| Criterion | Result | Detail |
|---|---|---|
| **No HR gap > 2.5 s** | ✅ **PASS** | Max gap 1,200 ms (326 notifications over 325.3 s) |
| Beat central survived 5 min | ✅ PASS | Connected throughout |
| P12 peripheral survived 5 min | ✅ PASS | Connected throughout (handle 1, 48.75 ms interval) |
| Portal WebSocket survived 5 min | ✅ PASS | Maintained with periodic interaction |
| HTTP stress (30 requests) | ✅ PASS | 30/30 OK across 3 bursts |
| `droppedEvents` | ✅ 0 | No BLE events lost |
| Heap stable (no leak) | ✅ PASS | `heap_free` Δ = -712 B; `heap_min_free` = 81,080 B >> 20,480 B |

HR gap distribution: 90.8% of gaps within 900–1,100 ms (expected ~1 Hz).
Zero gaps > 1,500 ms. Maximum gap 1,200 ms — well within the 2,500 ms criterion.

### Gate Decision

**PASS** — PR-03 programme gate cleared. The v1 BLE topology (Beat HW901
central + P12 Echelon Fit peripheral + WiFi portal WebSocket + HTTPS POST
stress) is proven on the P12, sustained >5 minutes with zero HR gaps
exceeding 2.5 s, zero dropped events, zero connection drops, and stable heap.

### Evidence
- [`docs/measurements/phase2/wo6-coexistence/evidence.md`](../../../../docs/measurements/phase2/wo6-coexistence/evidence.md) — full test report with timestamps, HR gap analysis, heap trajectory, and connection stability.
- Serial log captured during test (1,131 lines, `att_handle=16` notifications at ~1 Hz).
