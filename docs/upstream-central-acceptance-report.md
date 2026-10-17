# Framework Central-Role BLE Acceptance — Consumer Evidence

> **Status:** DRAFT — awaiting PA review before upstream filing.

## Overview

A consumer application using the framework's central BLE role (via `IBLEClientProvider`)
ran the RFC §17.2 items 4–9 central-role acceptance tests. Initial testing (v0.1.0-dev.14)
discovered a **critical address byte-ordering bug** in `BLEStack.cpp` that prevented all
central connections. The bug was fixed in **PR #296 (commit `1e90ec68`)**, and re-testing
(v0.1.0-dev.15) confirms all pass except auto-reconnect (F4) and item 9 is partial.

## Test Environment

| Item | Value |
|---|---|
| **Firmware Tags** | `v0.1.0-dev.14` (first run), `v0.1.0-dev.15` (re-run) |
| **Board** | ESP32-S3 Waveshare Nano (16 MB flash, 8 MB PSRAM) |
| **Framework** | First run: `v0.0.12-dev.93`; Re-run: commit `1e90ec68` (PR #296) |
| **SDK** | ESP-IDF 5.5.5 |
| **NimBLE** | esp-nimble-cpp (via PlatformIO, `BLE_EXT_ADV` disabled) |
| **BLE Strap** | Echelon Beat HW901 (`EBEAT-0014941`), standard `0x180D` Heart Rate profile |
| **Tablet** | Lenovo Tab P12 (peripheral-role test client) |
| **WiFi** | Active on `<ssid>`, IP `<ip>` |

## RFC §17.2 Results

| Item | Test | First Run | Re-Run | Notes |
|---|---|---|---|---|
| 4 | WiFi + BLE coexistence (central + peripheral) | BLOCKED | **PASS** | 10/10 HTTP 200 OK, HR uninterrupted, `droppedEvents: 0` |
| 5 | BLE connect/disconnect cycling (central role) | BLOCKED | **PASS** | 3 forget/re-pair cycles, no crash, no leaked connections |
| 6 | DRAM budget with central role active | **PASS** | **PASS** | `heap_min_free` = 77,376 B (threshold: 20,480 B) |
| 7 | Boot sequence with central role registered | **PASS** | **PASS** | Clean boot, both providers registered, no errors |
| 8 | Peripheral re-advertising after central disconnect | BLOCKED | **PASS** | P12 BT toggled off/on, reconnected, HR resumed |
| 9 | Central reconnect vs peripheral connections | BLOCKED | **PARTIAL** | Strap removed while P12 connected; P12 survived, `droppedEvents: 0`. Only strap disconnection was tested; auto-reconnect was not exercised due to F4 |

## Bug Fix Confirmation: MAC Address Byte-Order Inversion

### Original Bug (Fixed in PR #296)

`BLEStack::executeCentralConnect()` constructed a `NimBLEAddress` from `BLEPeerAddress.bytes`
using the wrong constructor, causing a **byte-order reversal** of the target MAC address.
The ESP32's Link Layer scanned for a reversed address that didn't exist and timed out after 30 s.

### Fix Verification

Serial log confirms correct address after fix:
```
I NimBLE: GAP procedure initiated: connect;
I NimBLE: peer_addr_type=1 peer_addr=
I NimBLE: e7:a7:dc:1c:4c:ad
```
Address matches the stored address — no byte reversal. Connection succeeds immediately.

### Affected Code Paths (both fixed)

- `executeCentralConnect()` (line 640) — all central connection attempts
- `executeCentralForget()` (line 741) — NVS bond deletion by address

## Remaining Finding: Discovery Scan Blocks Auto-Reconnect (F4)

The portal's discovery scan (triggered by a manual Pair action) persists and blocks the
reconnect scheduler from executing its own scan-and-connect cycle. The BLE stack supports
only one concurrent scan. The reconnect scheduler reports `phase: Fast, suspended: false`
but cannot act while discovery holds the scan resource.

- **Impact:** Auto-reconnect does not work after any discovery scan has been triggered.
  Manual Pair via portal/API still works.
- **Consumer workaround:** None available — this is a framework-level scan resource contention issue.

## Secondary Finding: `connectable` Field Never Populated

`BLEManager.cpp:1136-1140` creates `BLEScanResult` but never sets `res.connectable`
from the NimBLE scan data. It defaults to `false` (per `BLETypes.hpp:148`), causing
`/api/ble/scan/results` to report all devices as non-connectable. This is a cosmetic
bug in the API but does not affect connection behavior.

## Verification Evidence

- First run: Mac bleak/CoreBluetooth control test proved strap connectivity; Python byte-tracing confirmed double-reversal
- Re-run: 9/11 acceptance tests pass with the fix; serial logs confirm correct address throughout
- Full evidence: `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`

## Recommendation

1. ~~Fix the byte-ordering bug~~ **Done — PR #296.**
2. Fix F4 (discovery scan / reconnect scan contention) to enable auto-reconnect.
3. Fix `connectable` field population (cosmetic).

