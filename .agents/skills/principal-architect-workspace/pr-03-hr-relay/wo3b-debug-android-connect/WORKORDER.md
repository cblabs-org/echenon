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

<!-- Written by the worker at task completion -->

### Status
(pending)

### Test Results
- Test A (WiFi off): (pending)
- Test B (WIFI_PS_MIN_MODEM): (pending)
- Test C (HCI snoop): (pending)
- Test D (100ms adv, if run): (pending)

### Root Cause
(pending)

### Upstream Issue
(pending — issue number and URL)

### Out-of-Scope Findings
- (pending)
