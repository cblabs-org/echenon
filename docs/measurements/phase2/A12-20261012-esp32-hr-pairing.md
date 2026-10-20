# A12 — Echelon App HR Pairing Evidence

**Date:** 2026-10-12
**Firmware:** v0.1.0-dev.8
**Environment:** ex3-waveshare-nano-diag (WITH_HR_SYNTHETIC)
**Device:** Waveshare ESP32-S3-Nano (EX-3)
**App:** Echelon Fit on Lenovo Tab P12

## Test 2.1 — BLE Advertising

- [x] `Echenon-7226` visible in BLE scan (nRF Connect Android + bleak macOS)
- [x] Advertises Heart Rate Service (0x180D)
- [x] HR Measurement (0x2A37) sends notifications
- [x] Body Sensor Location (0x2A38) readable
- [x] Device Information Service (0x180A) present

### Bleak Diagnostic (macOS) — Full GATT Table

```
Service: 0000180d (Heart Rate)
  ├─ 00002a37 [HR Measurement]    Props: notify    Handle: 0x000F
  │  └─ 00002902 [CCCD]           Val: 0000
  ├─ 00002a38 [Body Sensor Loc]   Props: read      Handle: 0x0012
Service: 0000180a (Device Information)
  ├─ 00002a29 [Manufacturer Name] Props: read      Handle: 0x0015
  ├─ 00002a24 [Model Number]      Props: read      Handle: 0x0017
  ├─ 00002a25 [Serial Number]     Props: read      Handle: 0x0019
  ├─ 00002a26 [Firmware Revision] Props: read      Handle: 0x001B
  ├─ 00002a27 [Hardware Revision] Props: read      Handle: 0x001D
  ├─ 00002a28 [Software Revision] Props: read      Handle: 0x001F
```

### Bleak HR Notification Stream (20 notifications, 20 seconds)

```
HR = 114, 99, 80, 65, 60, 66, 81, 100, 115, 120,
     114, 99, 80, 65, 60, 66, 81, 100, 115, 120 BPM
Range: 60–120 BPM — confirmed sine-wave synthetic HR
```

## Test 2.3 — App Pairing Gate

- [x] Echelon app lists `Echenon-7226` as HR device
- [x] Pairing succeeds
- [x] App displays synthetic HR (observed 99 BPM during sine cycle)

### Serial Log — Echelon Tablet Connection

```
21:54:37  >>> Central connected: handle=1, addr=5D:2A:EA:8B:B3:6A,
              itvl=39 (48.75ms), latency=0, timeout=500 (5000ms), MTU=23
21:54:37  >>> Central MTU changed: handle=1, MTU=185
21:54:38  HR Measurement subscription changed: SUBSCRIBED
21:54:38+ GATT notify at 1 Hz — continuous, stable
```

Connection parameters: tablet requested 5.0s supervision timeout (no framework
adjustment needed). MTU negotiated to 185. Connection stable for duration of
test session with 1 Hz HR notifications.

## Verdict

**PASS**

## Notes

- **Android phone (Strava, nRF Connect):** Could see `Echenon-7226` in scan but
  connection establishment failed (`BLE_ERR_CONN_ESTABL_FAIL`, reason 0x3e).
  Serial logs showed `Reattempt advertising; reason: 0x3e` — the phone's
  connection requests failed at the link-layer controller before the NimBLE host
  processed them. **This is NOT a phone-specific issue:** the owner's eBeat HR
  armband connects to the same phone via Strava without problems. Something
  about our device's advertising or link-layer connection handling is
  incompatible with certain Android BLE stacks. The Echelon P12 tablet (the
  target device) connected cleanly on first attempt. **This is a real bug
  requiring follow-up investigation** — potential causes include the 50ms
  advertising interval under WiFi coex load, advertising packet format
  differences vs commercial HR monitors, or a NimBLE link-layer timing issue.
- **macOS (bleak):** Connected successfully twice during the session. Full GATT
  enumeration, HR subscription, and 20+ notifications received with zero drops.
  Supervision timeout updated from 720ms → 4.0s by framework (expected).
- **nRF Connect screenshot:** Device type shown as "UNKNOWN", advertising type
  "Legacy". 0x2A38 appeared as a top-level service in nRF Connect UI — this is
  a display artefact; bleak confirmed it is correctly nested under 0x180D.
- **WiFi/BLE coexistence:** WiFi connected on channel 1 (BW20). Coex config per
  framework bluetooth.md §6 — PREFER_WIFI default, SW coexist enabled, +20 dBm
  TX power. No coex changes were made or needed for this test.

## Out-of-Scope Finding — BLOCKING for Generic HR/FTMS Relay

**Android phone BLE connection establishment failure (0x3e)**

The Echenon device cannot maintain a BLE connection with the owner's Android
phone (Samsung). Connection establishment fails at the link-layer controller
(`BLE_ERR_CONN_ESTABL_FAIL`, reason 0x3e) before the NimBLE host processes it.
The same phone connects to the owner's eBeat HR armband via Strava without
issues — proving the fault is in our device's BLE behaviour, not the phone.

**Impact:** The Echelon P12 tablet (WO3 target) is unaffected, but future work
requires the device to function as a generic BLE HR/FTMS peripheral for phone
apps (Strava, Zwift, Wahoo, etc.). This defect blocks that use case.

**Suspected causes (require investigation):**
1. 50ms advertising interval under WiFi coex load — may not give the radio
   arbiter enough time to service connection request packets from phones
   (which may use shorter scan windows than tablets)
2. Advertising packet format differences vs commercial HR monitors (e.g. eBeat)
3. NimBLE link-layer timing under ESP32-S3 WiFi coexistence

**Constraint:** The dev.73–dev.80 hardening cycle (documented in bluetooth.md §6)
established that `esp_coex_preference_set()` MUST NOT be called —
`PREFER_BT` starved WiFi, `PREFER_BALANCE` caused `AUTH_EXPIRE`. Any fix must
work within the current `PREFER_WIFI` coex default.

**Recommendation:** Escalate to Chief Architect for a dedicated investigation
work order in a future PR. The advertising interval (currently 50ms, could test
100–160ms) is the safest lever to explore first.

