---
name: hr-relay-wo3b
description: Interactive CA-directed investigation of Android BLE 0x3e connection failure
complexity: interactive
---

# WO3b — Debug Android BLE Connection Failure

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo3b-debug-android-connect` (base: `hr-relay/wo3-app-pairing-gate`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`
> **Directive:** Chief Architect — owner-approved 2026-10-12

## Role Definition

> You are an expert senior software engineer working interactively with the
> owner on a diagnostic bench session. You are NOT the Principal Architect.
> Your task is to execute the investigation plan below in order, record
> evidence, and file one upstream issue.

## Background

During the WO3 app-pairing gate session, the owner's Samsung Android phone
could discover `Echenon-7226` but failed to connect (`BLE_ERR_CONN_ESTABL_FAIL`,
reason `0x3e`). The P12 tablet and macOS connected cleanly. The same phone
connects to the eBeat HR armband via Strava without issues.

**Chief Architect analysis — prime suspect:**

The framework's `WifiManager::applyPowerSave()` forces `WIFI_PS_NONE` when BT
is enabled (`WifiManager.cpp` L230–231, framework `v0.0.12-dev.93`). ESP-IDF
explicitly warns: *"Should enable WiFi modem sleep when both WiFi and Bluetooth
are enabled."* With `WIFI_PS_NONE`, the WiFi radio never yields — the coex
arbiter under `PREFER_WIFI` starves BLE connection event servicing. This causes
the ESP32's controller to miss the phone's `CONNECT_IND` packets.

**History:** WO-10 originally set `WIFI_PS_MIN_MODEM` on BLE builds, but this
caused `AUTH_EXPIRE` on congested 2.4 GHz networks (dev.81 regression), so it
was reverted to `WIFI_PS_NONE`. The question is whether `AUTH_EXPIRE` recurs on
the owner's network.

**Secondary factor:** The Android phone's initial connection interval may be
tighter than the P12 tablet's, making it more sensitive to missed events.

## Investigation Plan (execute in order)

### Test A: WiFi STA disconnected

1. Build `env:ex3-waveshare-nano-diag` (current code, `WITH_HR_SYNTHETIC`).
2. Flash — **ask the owner before flashing**.
3. Disconnect WiFi STA (either disable WiFi in firmware or disconnect the AP).
   The simplest method: temporarily comment out WiFi connection in code, or
   use the portal to forget the network, or just ensure no AP is available.
4. Attempt Android phone connection.
5. **Record result:** If Android connects with WiFi off, this confirms the
   coex arbiter (not the advertising stack) is the problem.

### Test B: `WIFI_PS_MIN_MODEM` override

1. Add a **local-only** override in `main.cpp` (after `app.begin(config)`):
   ```cpp
   #include <WiFi.h>
   WiFi.setSleep(WIFI_PS_MIN_MODEM);
   ```
   This is a **diagnostic override**, not a framework change.
2. Rebuild `env:ex3-waveshare-nano-diag` and flash.
3. Connect WiFi STA to the owner's network.
4. Attempt Android phone connection.
5. **If Android connects:**
   - Run for 5+ minutes — check serial logs for `AUTH_EXPIRE` or WiFi
     disconnections.
   - If WiFi stays stable: `WIFI_PS_MIN_MODEM` is the fix. Record this.
   - If `AUTH_EXPIRE` occurs: Record the timing and conditions. The framework
     will need a more nuanced solution (e.g. modem sleep only when BLE
     connections are active, or network-dependent config).

### Test C: Android HCI snoop log

1. On the Android phone, enable **Bluetooth HCI snoop log** (Developer Options
   → Enable Bluetooth HCI Snoop Log).
2. Reproduce the connection failure (or success if test B fixed it).
3. Pull the snoop log: `adb pull /data/misc/bluetooth/logs/btsnoop_hci.log`
   (path varies by Android version; may need `adb bugreport`).
4. Analyse with Wireshark — look for:
   - `CONNECT_IND` packet and the connection parameters the phone requests
   - Whether the ESP32 responds with `LL_CONNECTION_UPDATE_IND`
   - Any `LL_REJECT` or timeout
5. Record the phone's requested connection interval, supervision timeout, and
   latency.

### Test D: 100 ms advertising interval (only if A–C inconclusive)

1. **Only run this test if A–C did not identify the root cause.**
2. Patch `BLEStack.cpp` L390–391 locally:
   ```cpp
   pAdv->setMinInterval(160);   // 160 × 0.625 ms = 100 ms
   pAdv->setMaxInterval(160);
   ```
3. Rebuild, flash, test Android connection.
4. Record result.

## Strict Boundary Box

- **DO NOT** change the framework permanently. All overrides are local
  diagnostics, reverted before commit.
- **DO NOT** edit `PROGRAM.md`, `PROGRAM-PLAN.md`, or `reviews/` files.
- **DO NOT** pick an architecture option or make coex policy decisions.
- **Flash only with owner permission.**

## Rules

- **Rule 03 (Tag → Build → Flash):** Tag before flash.
- **Rule 04 (PlatformIO MCP tools only):** No `pio` CLI.

## Evidence

Create `docs/measurements/phase2/BLE-0x3e-debug-<date>.md` with:
- Test A result (WiFi off)
- Test B result (`WIFI_PS_MIN_MODEM`)
- Test C result (HCI snoop analysis)
- Test D result (if run)
- Connection parameters from serial logs and HCI snoop
- Conclusion: which factor is primary, which is secondary

## Upstream Issue

After the investigation, file **one** app-neutral upstream issue on the
framework repo. The issue should cover:
- The `WIFI_PS_NONE` / coex finding (primary, if confirmed)
- Configurable advertising interval (secondary, if data supports)
- Evidence references (relative paths within the repo, or inline)
- No Echenon-specific details in the issue — it's a framework concern

## Commit

- [ ] Revert all local diagnostic overrides (`WIFI_PS_MIN_MODEM`, adv
  interval patches) — the commit must match the WO3 codebase.
- [ ] `git add docs/measurements/phase2/BLE-0x3e-debug-*.md`
- [ ] `git commit -m "wo3b: Android BLE 0x3e debug — <result summary>"`
- [ ] Do NOT push.

---

## Completion Report (written by the worker at task completion)

### Status
**PAUSED — escalated to Chief Architect for revised test plan.**

Research phase complete. Hardware test plan revised by CA mid-session.
Baseline data and two research threads (desk search + SmartSpin2k codebase
analysis) surfaced a critical untested combination that changes the
investigation's direction. Evidence doc passed to CA for review before
continuing hardware tests.

### Test Results (partial)
- Test A (WiFi off): **BLOCKED** — BLE requires `AppState::Running` which
  requires WiFi STA precondition. AP-only mode never reaches RUNNING.
  This is itself a finding (see v1 finding below).
- Baseline (WiFi STA connected, `WIFI_PS_NONE`): **EXECUTED**
  - Mac (bleak): ✅ PASS (4.07s, itvl=30ms, timeout=720ms)
  - Lenovo Tab P12: ✅ PASS (itvl=48.75ms, timeout=5000ms)
  - Google Pixel 10 Pro Fold: ❌ FAIL × 2 (reason `0x3e`)
- Test B (WIFI_PS_MIN_MODEM): NOT STARTED (pending CA review of revised plan)
- Test C (HCI snoop): NOT STARTED
- Test D (100ms adv): DEPRIORITISED by CA

### CA-Revised Test Order (2026-10-13)
Original test order superseded. New order per CA evaluation:
0. Desk search (DONE — 13 ESP-IDF issues curated)
1. H2: disconnect phone's other BLE devices, retry ×10 (pending)
2. CONTROL (decisive): BLE with WiFi idle — bare NimBLE sketch or
   `advertiseInConfiguring=true`, retry ×10 (pending)
3. Test C: Pixel HCI snoop log (pending)
4. Test B: `WIFI_PS_MIN_MODEM` override, retry ×10 (pending, only if
   Step 2 passes)
5. Reproduce on DEFAULT env (no SignalLogger) (pending)
6. Client matrix: non-Pixel Android + iPhone (pending)

### Research Findings (new evidence, not in original workorder)

**1. `WIFI_PS_NONE` is explicitly prohibited by ESP-IDF when BLE is active.**
Multiple esp-idf issues confirm this (esp-idf#9595 crashes, esp-idf#2467
causes 10–15% 0x3e failures). Our framework runs this configuration.

**2. `PREFER_BALANCE` + `WIFI_PS_NONE` was never tested together.**
The dev.78 test of `PREFER_BALANCE` (which caused `AUTH_EXPIRE`) likely ran
with modem sleep active, not `WIFI_PS_NONE`. The combination of balanced
arbitration + always-on radio was never evaluated.

**3. SmartSpin2k (shipping product) proves `PREFER_BALANCE` + `WIFI_PS_NONE`
is viable.** SmartSpin2k runs BLE FTMS server + HR client + WiFi web UI on
the original ESP32 (less capable than our S3) using exactly this
combination, plus: `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT=y` (max 3),
BT controller modem sleep, disabled WiFi auto-reconnect with paced retry,
and wider advertising intervals (100–156ms).

**4. The framework has two compounding factors, not one:**
- `WIFI_PS_NONE` — WiFi radio never sleeps
- `PREFER_WIFI` — WiFi always wins arbitration
Together, BLE is completely starved. The Pixel's connection establishment
window expires because the controller cannot service it.

### Root Cause (preliminary)
`PREFER_WIFI` + `WIFI_PS_NONE` creates a coex dead zone where the BLE
controller is starved of radio time during connection establishment.
Devices with conservative connection parameters (P12 tablet) survive;
devices with tighter parameters (Pixel) fail. The fix likely involves
`PREFER_BALANCE` + `WIFI_PS_NONE` (untested in the dev.73–dev.80 hardening
cycle but proven in SmartSpin2k), plus NimBLE connection reattempt and
paced WiFi reconnect mitigations.

**This is NOT a final root cause determination.** Hardware tests (Steps 1–6)
are required. The evidence and suggested test matrix have been escalated to
the Chief Architect.

### Upstream Issue
NOT YET FILED — pending CA review of the complete evidence. The issue scope
has expanded beyond the original `WIFI_PS_NONE` finding to include:
- `PREFER_BALANCE` + `WIFI_PS_NONE` as a candidate fix
- `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT` as a framework default
- WiFi auto-reconnect pacing
- BLE-depends-on-WiFi-STA architectural defect (separate issue)

### Out-of-Scope Findings
1. **BLE depends on WiFi STA** — BLE cannot operate without WiFi STA
   connected. At boot: no STA → no RUNNING → no BLE. During operation:
   STA loss regresses to CONFIGURING, killing BLE relay. Hardware test
   (router off 3 min) and upstream issue pending.
2. **softAP SSID uses generic name** — Filed as
   [esp32-app-foundation#294](https://github.com/matthewmcneill/esp32-app-foundation/issues/294)
   (neutralised to use app-generic wording).

### Evidence
- [`docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`](../../../../docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md) — 742 lines,
  complete investigation report with serial evidence, desk search (13
  issues), SmartSpin2k comparison, framework coex history (dev.67–dev.83),
  architectural tension matrix, and escalation recommendations.
- Serial logs: `tmp/serial-testA-*.log` (captured, not committed)

### Diagnostic Overrides (to revert before commit)
- `sdkconfig.diag.defaults` — NimBLE log level override
- `platformio.ini` — `board_build.cmake_extra_args` in `[env:ex3-waveshare-nano-diag]`

