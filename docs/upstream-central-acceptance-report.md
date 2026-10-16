# Framework Central-Role BLE Acceptance — Consumer Evidence

> **Status:** DRAFT — awaiting PA review before upstream filing.

## Overview

A consumer application using the framework's central BLE role (via `IBLEClientProvider`)
attempted to run the RFC §17.2 items 4–9 central-role acceptance tests. During testing,
a **critical address byte-ordering bug** was discovered in the framework's `BLEStack.cpp`
that prevents any central-role BLE connection from succeeding.

## Test Environment

| Item | Value |
|---|---|
| **Firmware Tag** | `v0.1.0-dev.14` |
| **Board** | ESP32-S3 Waveshare Nano (16 MB flash, 8 MB PSRAM) |
| **Framework** | `v0.0.12-dev.93` submodule |
| **SDK** | ESP-IDF 5.5.5 |
| **NimBLE** | esp-nimble-cpp (via PlatformIO, `BLE_EXT_ADV` disabled) |
| **BLE Strap** | Echelon Beat HW901 (`EBEAT-0014941`), standard `0x180D` Heart Rate profile |
| **Tablet** | Lenovo Tab P12 (peripheral-role test client, not reached) |
| **WiFi** | Active on `<REDACTED_SSID>`, IP 192.168.1.100 |

## RFC §17.2 Results

| Item | Test | Result | Notes |
|---|---|---|---|
| 4 | WiFi + BLE coexistence (central + peripheral) | **BLOCKED** | Central connection never establishes |
| 5 | BLE connect/disconnect cycling (central role) | **BLOCKED** | Central connection never establishes |
| 6 | DRAM budget with central role active | **PASS** | `heap_min_free` = 93,404 B (threshold: 20,480 B) |
| 7 | Boot sequence with central role registered | **PASS** | Clean boot, both providers registered, no errors |
| 8 | Peripheral re-advertising after central disconnect | **BLOCKED** | Central connection never establishes |
| 9 | Central reconnect vs peripheral connections | **BLOCKED** | Central connection never establishes |

## Critical Finding: MAC Address Byte-Order Inversion

### Summary

`BLEStack::executeCentralConnect()` constructs a `NimBLEAddress` from `BLEPeerAddress.bytes`
using the wrong constructor, causing a **byte-order reversal** of the target MAC address.
The ESP32's Link Layer scans for a reversed address that doesn't exist and times out after 30 s.

### Location

`framework/modules/BLEManager/BLEStack.cpp`, lines 640 and 741:
```cpp
NimBLEAddress bleAddr(addr.bytes, static_cast<uint8_t>(addr.type));
```

### Mechanism

1. `BLEPeerAddress::parse("AA:BB:CC:DD:EE:FF")` stores bytes in little-endian:
   `bytes[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA}`

2. `NimBLEAddress(const uint8_t[], uint8_t type)` assumes big-endian input
   (documented as "compatibility with bluedroid esp library") and calls
   `std::reverse_copy()`, producing `val[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}`

3. NimBLE interprets `val[]` as little-endian, yielding address `FF:EE:DD:CC:BB:AA` — reversed.

4. `ble_gap_connect()` enters Initiating State, never sees `ADV_IND` from the reversed address,
   times out → `status=13` (`BLE_ERR_CONN_ESTABLISHMENT`), reason `0x3e`.

### Proposed Fix

```cpp
// Replace NimBLEAddress(addr.bytes, type) with:
ble_addr_t rawAddr;
rawAddr.type = static_cast<uint8_t>(addr.type);
memcpy(rawAddr.val, addr.bytes, 6);
NimBLEAddress bleAddr(rawAddr);
```

The `NimBLEAddress(ble_addr_t)` constructor performs a direct copy without byte reversal.

### Affected Code Paths

- `executeCentralConnect()` (line 640) — all central connection attempts
- `executeCentralForget()` (line 741) — NVS bond deletion by address

### Unaffected Code Path

- Scan callback (line 548) — uses `addr.toString()` → string parsing, which round-trips correctly.

## Secondary Finding: `connectable` Field Never Populated

`BLEManager.cpp:1136-1140` creates `BLEScanResult` but never sets `res.connectable`
from the NimBLE scan data. It defaults to `false` (per `BLETypes.hpp:148`), causing
`/api/ble/scan/results` to report all devices as non-connectable. This is a cosmetic
bug in the API but does not affect connection behavior.

## Verification Evidence

- Mac bleak/CoreBluetooth connects to the same strap instantly (proving `ADV_IND` is valid)
- Python byte-tracing confirms the double-reversal produces the wrong address
- ~10+ connection attempts with varied scan parameters all produce `status=13`

## Recommendation

Fix the byte-ordering bug (2 lines changed), rebuild, re-flash, and re-run the blocked
acceptance tests (items 4, 5, 8, 9 and SPEC tests 2.2–2.6).
