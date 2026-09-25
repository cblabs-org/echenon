# PR-03: HR Armband Relay (Phase 2 — BLE Foundation)

> **Programme:** Echenon Firmware
> **PR target base:** PR-01 (`signal-logger` branch tip)
> **Dev Phase:** Phase 2 — HR Relay
> **Spec sections:** §8.1–8.5
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 2
> **Parallel with:** PR-02 (no file overlap)

## Mandate

Add NimBLE to the build. Implement the HR relay — the simplest useful BLE
feature — proving dual-role BLE, WiFi coexistence, and multi-client connections.

**Entry Condition (Spike):** Echelon-app HR acceptance spike must pass before starting (use nRF Connect to advertise 0x180D named `Echenon-TEST` and verify Echelon app reads HR).

1. **WO1: Framework Submodule Bump** — Gate on framework `≥ v0.0.12-dev.82`. Add Echenon `sdkconfig.defaults` with `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3` and `CONFIG_BT_CTRL_BLE_MAX_ACT=5`. Apply `[ble]` mix-in.
2. **`BLEManager` integration** — NimBLE stack init, advertising, connection mgmt via static file-scope instance.
3. **`HRMClient`** (`IBLEClientProvider`) — Scan, connect, subscribe to HR armband. Update references to use `BLENotification.charUuid16`. Handle `setSlowRetryMs(15000)` for reconnect.
4. **`HRMRepeater`** (`IBLEServiceProvider`) — Re-broadcast HR as `0x180D`.
5. **Progressive coexistence test** — Ensure BLE and WiFi run simultaneously without panics (DRAM allocation mode verification).

**Hardware:** No bike hardware needed. ESP32 + HR armband only. Can be developed
and tested completely independently of the bike.

After this PR merges, the Echelon app can pair with `"Echenon"` as an HR device
and display live heart rate. First user-facing feature with zero bike risk.

## Prior Art

- [wo5-hrm/WORKORDER.md](../archive-pa-decomposition/wo5-hrm/WORKORDER.md)
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
