# PR-03: BLE Stack (FTMS Server + HRM Relay)

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (`power-engine` branch tip)
> **Spec sections:** §7.1–7.7, §8.1–8.4

## Mandate

Add NimBLE to the build. Implement three BLE modules:

1. **`FTMSServer`** — GATT Fitness Machine Service broadcasting Indoor Bike Data
   (power, cadence, speed, resistance, HR) at 2 Hz via `IBLEServiceProvider`.
2. **`HRMClient`** — BLE Central that scans for, connects to, and reads HR
   Measurement notifications from the Echelon Beat armband via `IBLEClientProvider`.
3. **`HRMRepeater`** — re-broadcasts HR data as a standard BLE Heart Rate
   Service via `IBLEServiceProvider`, so the Echelon tablet sees an HR source.

After this PR merges, the device advertises as `"Echenon"` with FTMS + HR
services. Zwift, Xert EBC, TrainerRoad, and all other FTMS-compatible apps can
pair and receive live data. HR from the armband is relayed through.

**BLEManager dependency:** This PR depends on Foundation BLEManager (RFC #225).
If not yet merged upstream, implement against the RFC'd `IBLEServiceProvider` /
`IBLEClientProvider` interface contracts and stub registration calls — the
integration PR (PR-07) will wire to the real BLEManager.

**Radio coexistence (SPEC §8.5):** WiFi and BLE share one radio on the ESP32-S3.
This PR must implement:
- Scanning discipline: window ≤ 50% of interval, bounded 30s, user-initiated only
- Phase-aware behaviour: no scanning in CONFIGURING/AP mode
- Connection parameter tuning: tight intervals for FTMS Peripheral (15–30ms),
  relaxed for HR Central (30–75ms)
- `sdkconfig.defaults` coex flags: `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`

## Prior Art

- [wo4-ftms/WORKORDER.md](../archive-pa-decomposition/wo4-ftms/WORKORDER.md)
- [wo5-hrm/WORKORDER.md](../archive-pa-decomposition/wo5-hrm/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
