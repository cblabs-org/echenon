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

1. **`BLEManager` integration** — NimBLE stack init, advertising, connection mgmt.
   Foundation for all future BLE work.
2. **`HRMClient`** (`IBLEClientProvider`) — Scan, connect, subscribe to HR armband.
3. **`HRMRepeater`** (`IBLEServiceProvider`) — Re-broadcast HR as `0x180D`.
4. **Portal BLE page** — Scan for HR devices, select, pair, store MAC in NVS.

**Hardware:** No bike hardware needed. ESP32 + HR armband only. Can be developed
and tested completely independently of the bike.

**BLEManager dependency:** Requires Foundation BLEManager (RFC #225 v2). If not
yet merged, implement against RFC interface contract and stub calls.

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
