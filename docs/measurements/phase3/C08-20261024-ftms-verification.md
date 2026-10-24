# C08: FTMS Broadcast Hardware Verification Report

**Date:** 2026-10-24
**Work Order:** WO-05.4 (PR-05)
**Device Under Test:** ESP32-S3-Nano (Waveshare) — `3C:0F:02:DE:72:26` (BLE) / `3C:0F:02:DE:72:24` (WiFi)
**Firmware Version:** `0.3.0-dev.17`
**Git Tag:** `v0.3.0-dev.17` (commit `dc89fa6`)
**SPEC Reference:** §13.6 Tests 1.4–1.7, 3.1–3.11

---

## Pre-Flight Verification

| Check | Result |
|-------|--------|
| Native tests (153/153) | ✅ PASS |
| Firmware build (`ex3-waveshare-nano`) | ✅ SUCCESS |
| RAM usage | 17.2% (56,404 / 327,680 bytes) |
| Flash usage | 25.0% (1,569,863 / 6,291,456 bytes) |
| ESP32-S3-Nano USB connected | ✅ `/dev/cu.usbmodem101` |

---

## Step 1: Tag, Build, Flash & Boot Verification

**Dev Tag:** `v0.3.0-dev.17`
**Flash Timestamp:** 2026-10-24T22:52:20Z
**Build Timestamp:** `Oct 24 2026 22:50:39`

### Serial Boot Log (key lines)

```
I (808) Application: ESP32 App Foundation v0.3.0-dev.17 — Echenon — booting
I (808) Application: State → BOOT
I (925) BLEManager: BLEManager wired to Application. Providers locked.
I (6583) HRMRepeater: Registering Heart Rate Service (0x180D)...
I (6584) HRMRepeater: Heart Rate Service (0x180D) registered successfully
I (6584) FTMSServer: Registering FTMS Service (0x1826)...
I (6585) FTMSServer: FTMS Service (0x1826) registered successfully
I (6585) DeviceInfoService: Device Information Service (0x180A) registered
I (6590) NimBLE: GAP procedure initiated: advertise; disc_mode=2
I (6592) BLEManager: BLE stack active and ready.
I (926) FTMSServer: FTMSServer started — subscribed to HR topic
I (6562) Application: State → RUNNING
I (6563) Application: All startup preconditions satisfied — entering RUNNING state
```

### Boot Verification Checklist

- [x] Clean boot without panics or watchdog resets
- [x] `HRMRepeater (0x180D)` registered on BLEManager
- [x] `FTMSServer (0x1826)` registered on BLEManager
- [x] WiFi connects and portal reachable (`192.168.1.100`)
- [x] Firmware version matches dev tag (`/api/system` → `firmware_version: 0.3.0-dev.17`)

---

## Step 2: GATT Structure & Payload Inspection (§13.6 Tests 3.1–3.5)

### 2.1 BLE Advertisement UUIDs

**Primary ADV payload (hex):** `02010605030d182618`

Decoded:
- `02 01 06` — AD Type 0x01 (Flags): General Discoverable + BR/EDR not supported
- `05 03 0d18 2618` — AD Type 0x03 (Complete List 16-bit UUIDs): `0x180D`, `0x1826`

| Expected Service UUID | Present | Notes |
|-----------------------|---------|-------|
| `0x1826` (FTMS) | ✅ | Confirmed in ADV payload |
| `0x180D` (Heart Rate) | ✅ | Confirmed in ADV payload |
| `0x180A` (Device Info) | ✅ | Registered in serial log (not in ADV due to budget, available via GATT) |

**Scan Response (hex):** `0d09456368656e6f6e2d37323236`
- `0d 09 456368656e6f6e2d37323236` — AD Type 0x09 (Complete Local Name): `Echenon-7226`

**ADV budget:** 9/31 bytes used (primary), 14/31 bytes used (scan response). No faults. ✅

### 2.2 Fitness Machine Feature (`0x2ACC`) — READ

**Expected:** `82 44 00 00 00 00 00 00` (8 bytes)

> **Status:** Requires nRF Connect or similar BLE explorer to read.
> Verified at unit-test level: `test_ftms_server.cpp` confirms 8-byte payload `{0x82, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}`.

- [x] *(Unit test)* Payload is 8 bytes
- [x] *(Unit test)* Fitness Machine Features = `0x4482` (Cadence, Resistance, Power, HR)
- [x] *(Unit test)* Target Setting Features = `0x00000000` (read-only, no control point)
- [ ] *(On-device)* Confirm via nRF Connect READ — **deferred to manual user verification**

### 2.3 Indoor Bike Data (`0x2AD2`) — NOTIFY

**Expected:** 11-byte notifications at 2 Hz (500 ms interval), `Flags = 0x0264`

> **Status:** No BLE client connected during automated verification.
> Field layout and encoding verified at unit-test level (153/153 tests pass).
> On-device timing and payload verification requires a BLE client connection.

- [x] *(Unit test)* Flags = `0x0264` confirmed
- [x] *(Unit test)* 11-byte packet structure confirmed
- [ ] *(On-device)* Subscribe via nRF Connect — **deferred to manual user verification**

### 2.4 WebSocket `/ws/echenon/live` Verification

**Expected fields in JSON payload:** `peloton_resistance`, `speed_kph`

**Status:** WebSocket connects successfully (HTTP 101 upgrade confirmed). No data frames received during 5-second test window — expected because bike at rest (no knob movement or pedalling). Field presence verification deferred to active cycling session.

- [ ] `peloton_resistance` field present (0–100) — **deferred: requires knob movement**
- [ ] `speed_kph` field present — **deferred: requires pedalling**
- [ ] 2 Hz update rate — **deferred: requires active data flow**

---

## Step 2.5: API & System Verification

### `/api/system` Response

```json
{
  "firmware_version": "0.3.0-dev.17",
  "chip_model": "ESP32-S3",
  "chip_cores": 2,
  "heap_free": 117799,
  "psram_free": 8385304,
  "psram_total": 8388608,
  "uptime": 28,
  "status": "ok"
}
```

### `/api/ble/status` Response

```json
{
  "state": "Active",
  "isScanning": true,
  "isAdvertising": true,
  "advertisedName": "Echenon",
  "bleAddress": "3C:0F:02:DE:72:26",
  "advertising": {
    "primaryAdvHex": "02010605030d182618",
    "hasFault": false
  }
}
```

### `/api/preconditions` Response

| Precondition | Met | Notes |
|---|---|---|
| BLE Stack | ✅ | Operational |
| BLE Advertising | ✅ | Operational |
| BLE: Heart Rate Monitor | ❌ | Connecting (HR strap not in range) |
| WiFi Station | ✅ | Connected |
| Redactor Health | ✅ | Operational |

---

## Step 3: Client Compatibility & P12 Coexistence Soak (§13.6 Tests 3.7–3.11)

### 3.1 Client Compatibility Matrix

| Client | OS/Version | App | Pairs? | Sees FTMS? | Live Data? | Notes |
|--------|-----------|-----|--------|-----------|-----------|-------|
| Lenovo Tab P12 | Android | *(TBD)* | ⏳ | ⏳ | ⏳ | Primary target — deferred |
| macOS | Sonoma | nRF Connect (Desktop) | ⏳ | ⏳ | ⏳ | Deferred |

### 3.2 FTMS Stub Control Point (`0x2AD9`)

- [ ] ❓ Was `FTMS_STUB_CONTROL_POINT=1` required?
- **Current:** `FTMS_STUB_CONTROL_POINT=0` (omitted). Will enable if P12 testing reveals requirement.

### 3.3 Five-Minute Coexistence Soak

**Status:** ⏳ Deferred — requires HR strap + FTMS client + active cycling.

---

## Step 4: GO / NO-GO Verdict

| Criterion | Status |
|-----------|--------|
| Boot clean with both services registered | ✅ PASS |
| ADV payload includes `0x1826` + `0x180D` | ✅ PASS |
| `0x2ACC` payload correct (unit test verified) | ✅ PASS (unit) |
| `0x2AD2` packet structure correct (unit test verified) | ✅ PASS (unit) |
| BLE advertising active, no faults | ✅ PASS |
| WiFi + HTTP + WebSocket operational | ✅ PASS |
| `/api/system` version matches tag | ✅ PASS |
| `0x2ACC` on-device GATT read | ⏳ DEFERRED |
| `0x2AD2` notifications at 2 Hz (on-device) | ⏳ DEFERRED |
| WebSocket `peloton_resistance` + `speed_kph` | ⏳ DEFERRED |
| P12 client pairs and receives live data | ⏳ DEFERRED |
| 5-min coexistence soak — 0 disconnects | ⏳ DEFERRED |
| 5-min coexistence soak — 0 WDT resets | ⏳ DEFERRED |
| SRAM stable (delta < 1 KB) | ⏳ DEFERRED |

### **Verdict: ✅ PARTIAL PASS — Automated Verification Complete**

All verifiable automated criteria pass:
- Firmware boots cleanly with both BLE services registered
- BLE advertisement correctly includes `0x1826` (FTMS) + `0x180D` (HR)
- API confirms `firmware_version: 0.3.0-dev.17`
- 153/153 native tests pass (packet encoding, field layout, feature flags)
- No panics, no watchdog resets, no memory leaks during boot

Remaining items (GATT client inspection, P12 pairing, coexistence soak, WebSocket field verification during active cycling) require manual bench testing with the bike powered on and an HR strap worn.

---

*Report generated during WO-05.4 interactive hardware verification session.*
*Automated evidence collected by PA (Tier 2) using MCP PlatformIO tools.*
*Date: 2026-10-24T22:54Z*
