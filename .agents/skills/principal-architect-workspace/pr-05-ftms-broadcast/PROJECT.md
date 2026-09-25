# PR-05: FTMS Broadcast (Phase 4 — App Visibility)

> **Programme:** Echenon Firmware
> **PR target base:** `main` after PR-02 + PR-03 merged
> **Dev Phase:** Phase 3 — FTMS
> **Spec sections:** §7.1–7.7
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 4

## Mandate

Add the FTMS GATT service to the existing BLE stack (from PR-03). The bike
becomes visible to external fitness apps.

1. **`FTMSServer`** (`IBLEServiceProvider` + `ITickable`) — FTMS GATT service
   (`0x1826`), Indoor Bike Data notifications at 2 Hz, Feature flags, Control
   Point characteristic.
2. **FTMS Corrections** — Ensure Target Setting is bit 2. Add missing chars (Supported Resistance Level Range `0x2AD6`, Supported Power Range `0x2AD8`). Implement mode-dependent features (clear target-setting bits in Intercept). Add `Request Control` requirement for the Control Point.
3. **Progressive coexistence test** — Verify 3 simultaneous BLE connections (HR central, HR peripheral, FTMS peripheral) with WiFi active.

`BLEManager`, `HRMClient`, and `HRMRepeater` are already built and proven from
PR-03. This PR adds FTMS and verifies multi-service + multi-client BLE.

**Hardware:** Passive Tap Board (from PR-04a) or Intercept Board. No new hardware required.

After this PR merges, Zwift/Xert/TrainerRoad see `"Echenon"` and display live
power, cadence, resistance, and HR data.

## Prior Art

- [wo4-ftms/WORKORDER.md](../archive-pa-decomposition/wo4-ftms/WORKORDER.md)
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
