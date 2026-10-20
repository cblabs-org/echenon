# Test 2.5 / 2.6 Re-run Evidence — Auto-Reconnect (F4 Fix Verification)

**Date:** 2026-10-18
**Firmware:** `v0.3.0-dev.1` (commit `7aabaea` on `hr-relay/wo7-doc-sync`)
**Framework:** `v0.0.12-beta.3` (`726dce2d`) — includes F4 fix (PR #301: `pair()` calls `stopDiscovery()`)
**Device:** Waveshare ESP32-S3-Nano, WiFi SSID `<ssid>`, IP `<ip>`
**Strap:** Echelon Beat HW901 (`E7:A7:DC:1C:4C:AD`, RandomStatic)
**Tester:** Owner (interactive, API-monitored by PA)

---

## Test 2.5 — Auto-reconnect after strap auto-sleep

**Procedure:** Beat paired and connected. Removed from arm. Waited for auto-sleep disconnect. Put back on arm. Timed reconnect via `/api/ble/status` polling.

| Event | Time | API State |
|---|---|---|
| Beat connected, HR flowing | 16:02:52 | `connected: true, phase: Idle` |
| Beat removed from arm | 16:05:00 | `connected: true` (BPM=0 relay) |
| Polls at +30s, +60s, +90s | 16:05:32–16:07:10 | `connected: true, phase: Idle` |
| **Beat auto-sleep disconnect** | ~16:08:02 | `connected: false, phase: Fast, scanning: true` |
| Beat put back on arm | 16:08:25 | — |
| **Reconnected** | 16:08:32 | `connected: true, phase: Idle` |

**Reconnect time: ~7 seconds**

**Key observation:** `discovery.active: false` throughout — the F4 fix (`pair()` calls `stopDiscovery()`) prevents the lingering discovery scan that blocked the reconnect scheduler in the original WO5 run.

### Result: **PASS** (≤ 20s criterion; measured ~7s)

---

## Test 2.6 — Factory reset + re-pair + reconnect

**Procedure:** Factory reset via `/api/system/factory-reset`. Reconfigured WiFi. Re-paired Beat via portal BLE tab + API. Repeated auto-sleep/reconnect test.

| Event | Time | API State |
|---|---|---|
| Factory reset triggered | 16:12:34 | `status: factory-reset-complete` |
| Device rebooted to AP mode | 16:12:35 | — |
| WiFi reconfigured (STA mode) | ~16:14:30 | Serial: `State → RUNNING` |
| BLE scan found Beat | 16:15:11 | `EBEAT-0014941, rssi: -86` |
| Pair initiated | 16:15:19 | `status: pairing` |
| **Beat connected** | 16:15:50 | `connected: true, phase: Idle, discovery.active: false` |
| Beat removed from arm | ~16:15:30 | — |
| Polls at +1m, +2m, +3m | 16:16–16:18 | `connected: true, phase: Idle` |
| **Beat auto-sleep disconnect** | ~16:19:05 | `connected: false, phase: Fast, scanning: true` |
| Beat put back on arm | 16:19:19 | — |
| **Reconnected** | 16:19:34 | `connected: true, phase: Idle` |

**Reconnect time: ~15 seconds** (consistent with `slowRetryMs = 15000` surviving factory reset)

**Key observations:**
1. `discovery.active: false` after `pair()` — F4 fix confirmed post-factory-reset
2. `slowRetryMs = 15000` applied at boot (unconditional, SPEC §8.4 mandated) — factory reset does not revert to framework default (60s)
3. Portal UI showed stale state during pairing (F5, tracked as framework issue #304) but all API operations succeeded

### Result: **PASS** (≤ 20s criterion; measured ~15s; slowRetryMs survived reset)

---

## Summary

| Test | Previous Result (WO5) | This Run | Reconnect Time |
|---|---|---|---|
| 2.5 Auto-reconnect | ❌ FAIL (F4 blocked) | ✅ **PASS** | ~7s |
| 2.6 Factory reset + reconnect | ⛔ BLOCKED (by F4) | ✅ **PASS** | ~15s |

**F4 root cause confirmed fixed.** Framework PR #301 (`pair()` calls `stopDiscovery()`) resolves the lingering discovery scan that prevented the reconnect scheduler from operating.

---

## B3 Two-Subscriber Test

**Date:** 2026-10-20
**Firmware:** `v0.3.0-dev.3` (commit `8a4a072` on `hr-relay/wo7-doc-sync`)
**Framework:** `v0.0.12-beta.4` (`337bd4b1`) — includes #309 fix (BLE re-advertise after peripheral connect)
**Device:** Waveshare ESP32-S3-Nano, WiFi SSID `<REDACTED_SSID>`, IP `192.168.1.100`
**Strap:** Echelon Beat HW901 (`E7:A7:DC:1C:4C:AD`, RandomStatic)
**Tester:** Owner (interactive bench test, serial-monitored by agent)

**Procedure:**

1. Flashed `v0.3.0-dev.3` with `WITH_HR_RELAY` enabled
2. ESP32 booted, connected to HR strap, HR relay service (0x180D) advertising
3. **Pixel 12** connected to Echenon HR service, subscribed to 0x2A37 — HR values confirmed flowing
4. **nRF Connect** (second device) connected, subscribed to 0x2A37 — HR values confirmed on **both** devices simultaneously
5. **nRF Connect disconnected**
6. **Pixel 12 continued receiving HR** without interruption

**Serial evidence:**

| Event | Timestamp (uptime) | Log |
|---|---|---|
| HR service registered | 7143 ms | `HRMRepeater: Heart Rate Service (0x180D) registered successfully` |
| BLE advertising started | 7150 ms | `GAP procedure initiated: advertise; disc_mode=2` |
| HR strap connected | 27827 ms | `HRMClient: Connected to HR device` |
| Notifications flowing (both subscribers) | 128326–222058 ms | `GATT procedure initiated: notify; att_handle=16` (~1s intervals) |
| nRF unsubscribed | 222058 ms | `HRMRepeater: HR Measurement subscription changed: UNSUBSCRIBED` |
| Notifications continued (Pixel 12) | 222451+ ms | `GATT procedure initiated: notify; att_handle=16` (uninterrupted) |

**Key observations:**
1. Two simultaneous BLE peripheral connections handled correctly — both received HR notifications
2. nRF disconnect did not disrupt Pixel 12's notification stream
3. No crash, no advertising loss, no stack errors after disconnect
4. Framework #309 fix (re-advertise after peripheral connect) confirmed working

### Result: **PASS**
