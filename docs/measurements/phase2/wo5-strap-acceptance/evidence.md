# WO5 — Strap Acceptance Evidence

**Firmware:** `v0.1.0-dev.14`  
**Board:** ESP32-S3 Waveshare Nano (`ex3-waveshare-nano`)  
**Strap:** Echelon Beat HW901, `EBEAT-0014941`, BLE address `E7:A7:DC:1C:4C:AD` (RandomStatic)  
**Tablet:** Lenovo Tab P12 (available but unused — blocked before tablet tests)  
**Date:** 2026-10-16  

---

## Pre-Flight

### Tag & Flash

- Tag: `v0.1.0-dev.14` created and pushed
- Build: `ex3-waveshare-nano` compiled successfully (RAM 16.2%, Flash 24.3%)
- Flash: MCP `upload_firmware` completed successfully
- Version verify: `curl http://192.168.1.100/api/system` → `"firmware_version": "0.1.0-dev.14"`

### Boot Log Verification

```
I BLEManager: BLE stack initialized
I BLEManager: Registering provider: hr_client (Central)
I BLEManager: Registering provider: hr_repeater (Peripheral)
I BLEManager: BLE advertising started: Echenon-7226
I BLEManager: Service UUID 0x180D in advertising payload
I WiFiManager: Connected to <REDACTED_SSID>, IP=192.168.1.100
```

Clean boot with both providers registered, BLE advertising with `0x180D`, WiFi coexistence active. No errors.

---

## RFC §17.2 Items — Results

### RFC Item 6: DRAM Budget — PASS ✅

```json
{
    "heap_free": 128451,
    "heap_total": 340587,
    "heap_min_free": 93404,
    "heap_max_alloc": 53248,
    "psram_free": 8385692,
    "psram_total": 8388608
}
```

`heap_min_free` = 93,404 B >> 20,480 B threshold. **PASS.**

Post-testing (after multiple scan/connect cycles):
```json
{
    "heap_free": 127423,
    "heap_min_free": 86496
}
```
Still well above threshold after extended testing.

### RFC Item 7: Boot Sequence — PASS ✅

Boot log confirms:
- `hr_client` and `hr_repeater` providers registered cleanly
- BLE advertising active with `0x180D`
- WiFi connected and coexisting
- No crash, no error logs on boot

---

## 🚨 Critical Blocker: BLE Central Connection Failure

**All remaining tests (2.2–2.6, RFC items 4, 5, 8, 9) are BLOCKED by a framework bug.**

### Symptoms

Every connection attempt from the ESP32 to the Beat HW901 fails with:
```
NimBLEClient: Connection failed; status=13 (BLE_ERR_CONN_ESTABLISHMENT)
```
after a 30-second timeout. Observed across **~10+ connection attempts** with varying parameters.

### ESP32 Scan Results

The device scans and finds the Beat correctly:
```json
{
    "address": "E7:A7:DC:1C:4C:AD",
    "addressType": "RandomStatic",
    "name": "EBEAT-0014941",
    "rssi": -39,
    "connectable": false
}
```

Note: The `"connectable": false` is a **reporting bug** (see Finding F1 below), not an actual property of the Beat's advertising.

### Mac Control Test — Beat is Connectable

A bleak/CoreBluetooth diagnostic script confirmed the Beat connects instantly from macOS:

```
Attempting BleakClient connection to Beat...
  CONNECTED! MTU=23
  Service: 0000fd00-0000-1000-8000-00805f9b34fb (Vendor specific)
    Char: 0000fd09 [notify], 0000fd0a [write-without-response]
    Char: 0000fd19 [notify], 0000fd1a [write-without-response]
  Service: 0000180d-0000-1000-8000-00805f9b34fb (Heart Rate)
    Char: 00002a37 [notify]     -- HR Measurement
    Char: 00002a38 [read]       -- Body Sensor Location
  Service: 0000181c (User Data), 0000180f (Battery), 0000180a (Device Info)

  Subscribing to HR notifications for 5 seconds...
    HR: 0 BPM (flags=0x00, contact_supported=0, contact_detected=0)
    HR: 0 BPM ... (5 notifications received at 1 Hz)
```

Connection from Mac: **instant** (~1 second). HR notifications flow immediately. The Beat advertises as `ADV_IND` (connectable undirected) — proven by the fact that CoreBluetooth, which strictly follows the BLE spec, connects successfully.

### Manufacturer Data

```
Manufacturer ID: 0x3845 (14405 decimal — GWSTAI/Echelon)
Payload: 01640600
```

---

## Root Cause Analysis

### Finding F1: `connectable` field never populated in scan results

**Location:** `framework/modules/BLEManager/BLEManager.cpp:1136-1140`

`BLEScanResult` is created with default values. The `connectable` field (defaulting to `false` per `BLETypes.hpp:148`) is **never set** from the NimBLE `isConnectable()` data. The `BLEQueueMessage::Type::ScanResult` handler builds `res` but only populates `address`, `name`, and `rssi`:

```cpp
BLEScanResult res;
res.address = msg.address;
strncpy(res.name, msg.providerId, sizeof(res.name) - 1);
res.rssi = msg.rssi;
// res.connectable is NEVER SET — remains false for ALL devices
```

**Impact:** The `/api/ble/scan/results` API reports `"connectable": false` for every scanned device, regardless of their actual advertising type. This is a cosmetic/API bug — it does not cause the connection failure.

### Finding F2 (CRITICAL): MAC Address Byte-Order Inversion in BLEStack.cpp

**Location:** `framework/modules/BLEManager/BLEStack.cpp` lines 640 and 741

**Root cause verified by Python byte-tracing:**

1. **`BLEPeerAddress::parse("E7:A7:DC:1C:4C:AD")`** stores the MAC in little-endian order:
   ```
   bytes[] = {0xAD, 0x4C, 0x1C, 0xDC, 0xA7, 0xE7}
             [0]    [1]    [2]    [3]    [4]    [5]
   ```

2. **`NimBLEAddress(addr.bytes, type)`** (BLEStack.cpp:640) assumes input is big-endian (`esp_bd_addr_t`) and calls `std::reverse_copy()`:
   ```cpp
   // NimBLEAddress.cpp:106-109
   NimBLEAddress::NimBLEAddress(const uint8_t address[6], uint8_t type) {
       std::reverse_copy(address, address + 6, this->val);
       this->type = type;
   }
   ```

3. **Result:** NimBLE internal `val[]` = `{0xE7, 0xA7, 0xDC, 0x1C, 0x4C, 0xAD}`, which NimBLE interprets as address **`AD:4C:1C:DC:A7:E7`** (reversed!).

4. **Correct:** The NimBLE string constructor for `"E7:A7:DC:1C:4C:AD"` produces `val[]` = `{0xAD, 0x4C, 0x1C, 0xDC, 0xA7, 0xE7}`, which NimBLE interprets as **`E7:A7:DC:1C:4C:AD`** (correct).

**Consequence:** The ESP32's Link Layer Controller enters Initiating State scanning for address `AD:4C:1C:DC:A7:E7`, which does not exist. After 30 seconds, `ble_gap_connect()` times out with `status=13`.

**Affected code paths:**
- `BLEStack.cpp:640` — `executeCentralConnect()` (connection attempts)
- `BLEStack.cpp:741` — `executeCentralForget()` (bond deletion)

**Unaffected code path:**
- `BLEStack.cpp:548` — scan callback uses `addr.toString().c_str()` → `BLEPeerAddress::parse()`, which round-trips correctly through the string representation.

**Fix (outside WO5 boundary box):**
```cpp
// Replace:
NimBLEAddress bleAddr(addr.bytes, static_cast<uint8_t>(addr.type));

// With:
ble_addr_t rawAddr;
rawAddr.type = static_cast<uint8_t>(addr.type);
memcpy(rawAddr.val, addr.bytes, 6);
NimBLEAddress bleAddr(rawAddr);
```

The `NimBLEAddress(ble_addr_t)` constructor (line 64) does a direct copy without reversal, so `addr.bytes` (which is already in the same byte order as `ble_addr_t.val`) is passed through correctly.

### SmartSpin2k Comparison

SmartSpin2k connects to HR monitors using `pClient->connect(advertisedDevice)` which passes the `NimBLEAdvertisedDevice` directly — never going through a separate address storage/parsing layer. This avoids the byte-ordering issue entirely.

---

## Diagnostic Scripts

- `tmp/beat_adv_diag.py` — bleak advertisement scanner and HR diagnostic
- `tmp/beat_adv_diag.json` — captured advertisement data
- `tools/ble-test/mac_runner.py` — existing BLE test harness (pre-existing)

---

## System State After Testing

```json
{
    "heap_free": 127423,
    "heap_min_free": 86496,
    "uptime": 730,
    "firmware_version": "0.1.0-dev.14",
    "wifi_rssi": -58,
    "status": "ok"
}
```

BLE config was modified during testing:
- `reconnectScan` widened to `{intervalMs:320, windowMs:160}` (50% duty)
- Beat stored and then forgotten via `/api/ble/forget`
- Multiple scan/pair/forget cycles performed
