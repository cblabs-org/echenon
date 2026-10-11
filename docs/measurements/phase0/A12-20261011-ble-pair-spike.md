# A12 — BLE HRM Pairing Spike

**Date:** 2026-10-11
**Register row:** #12
**Status:** INCONCLUSIVE

## Objective

Determine whether the Echelon companion app discovers and pairs with a standard
Bluetooth Low Energy Heart Rate Monitor (`0x180D` Heart Rate Service).

## Method

1. Used nRF Connect on an Android phone (Samsung) to create a mock HRM
   peripheral advertising `0x180D` with a full GATT server.
2. Configured: Connectable, Discoverable, Legacy advertising, 250 ms interval.
3. GATT server included:
   - Heart Rate Measurement (`0x2A37`) — Notify
   - Body Sensor Location (`0x2A38`) — Read
   - Heart Rate Control Point (`0x2A39`) — Write
4. Attempted discovery from:
   - Echelon Fit app on Lenovo Tab P12 (Android)
   - Android tablet system Bluetooth settings
   - A second Android phone system Bluetooth settings
   - macOS host using Python `bleak` library

## Results

### Mac (bleak) — SUCCESS
```
Found: (no name) addr=CF4D7438-C8B0-70B4-703E-5AC75EC576C0 RSSI=-64
Connected!
  Service: 0000180d (Heart Rate)
    Char: 00002a37 (Heart Rate Measurement) [notify]
    Char: 00002a38 (Body Sensor Location) [read]
    Char: 00002a39 (Heart Rate Control Point) [write]
```
Full GATT service enumerated. All three Heart Rate characteristics present.

### Android tablet (Lenovo Tab P12) — FAIL
- System Bluetooth settings: MockHRM not visible
- Other BLE devices visible: BYD BLE3, SolarAssistant
- Location services: enabled

### Second Android phone — FAIL
- System Bluetooth settings: MockHRM not visible
- Same other devices visible

### Echelon Fit app — COULD NOT TEST
- App UI explicitly says "scanning for HRM" and prompts to "turn on HRM
  and put in pairing mode"
- Could not test pairing because MockHRM was not visible to Android

## Observations

1. The nRF Connect mock peripheral was fully functional — the Mac connected
   and enumerated the complete Heart Rate GATT service.
2. The mock was not discoverable by any Android device tested, despite other
   BLE devices being visible. Root cause unknown.
3. The Echelon app confirms it scans for Heart Rate Monitors — the UI
   explicitly prompts for HRM pairing.

## Conclusion

**Inconclusive.** The mock peripheral's invisibility to Android devices
prevented testing whether the Echelon app pairs with standard `0x180D`
services. The app does scan for HRM devices.

## Recommended Follow-Up

Build a mock HRM BLE server on the ESP32 using NimBLE. An ESP32 peripheral
uses a public Bluetooth address and standard BLE advertising, which should
be universally discoverable. This allows:

1. Definitive test of Echelon app HRM pairing
2. If successful: prototype the HRM shim that will feed real HR data from
   a chest strap through the ESP32 into the Echelon app
3. Foundation for the Phase 3 BLE MITM architecture

**This is the scope of PR-02 (inserted before the current PR-03 plan).**
