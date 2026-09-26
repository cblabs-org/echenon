# PR-03: HR Armband Relay (Phase 2 — HR relay)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-01 merged
> **Dev Phase:** Phase 2 — HR relay
> **Depends on:** PR-01; entry gates below
> **Spec sections:** [SPEC](../../../../SPEC.md) §3.5 (composition), §7.8 (connection budget, device name), §8.1–§8.5 (HR relay, BLE integration, reconnect policy, coexistence), §13.5 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 2
> **Parallel with:** PR-02. **Shared files:** `EchenonModule.*`, `platformio.ini`, `main.cpp`. Whichever of PR-02 / PR-03 merges second rebases onto `main` and resolves the conflict.

## Entry gates

| Gate | Status |
|---|---|
| Echelon-app HR acceptance spike: nRF Connect advertises `0x180D` as `Echenon-TEST` and the Echelon app on the P12 lists it and shows HR (audit B-04; Phase 0 register row #12) | OPEN |
| **Framework central-role acceptance evidence**: RFC §17.2 items 4–9 run against a real `0x180D` peripheral on a framework tag that includes the WO-8 central use-after-free fix. At `v0.0.12-dev.82` the framework's `ACCEPTANCE.md` marks "Central Scan — N/A (by design)"; the only run was at `dev.67`, before the fix (audit P-01) | **OPEN** |
| Framework submodule at `v0.0.12-dev.82` or later (bumped by PR-01 WO1, BLE off) | Met when PR-01 merges |

## Mandate

Turn BLE on and implement the HR relay — the simplest useful BLE feature —
proving dual-role BLE, WiFi coexistence and multi-client connections.

1. **Enable BLE** — Declare the `[ble]` mix-in locally in Echenon's
   `platformio.ini` (`-D WITH_BLE`, `h2zero/esp-nimble-cpp@^2.1.0`, `BLEManager`)
   with `framework/hardware/_common/sdkconfig.ble.defaults` in the sdkconfig chain.
   Echenon's `hardware/ex3-waveshare-nano/sdkconfig.defaults` sets
   `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` and `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (3 peripheral
   + 1 central; SPEC §7.8), applied after `sdkconfig.ble.defaults`. If the
   central-acceptance gate requires a newer framework tag, this WO performs that bump.
2. **`BLEManager` integration** — Composition per SPEC §3.5 using the real API:
   providers are owned by `EchenonModule` and exposed through accessors, registered
   with `BLEManager` before `app.begin(config)`, then `bleManager.begin(app)`. Confirm
   the order against the framework headers and docs.
3. **`HRMClient`** (`IBLEClientProvider`) — Scan, connect and subscribe to the HR
   armband; uses `BLENotification.charUuid16`. Publishes HR on the EventBus; it
   makes no direct calls into `HRMRepeater`. Reconnect policy per SPEC §8.4: fast
   backoff {2, 4, 8, 16, 32} s, then an indefinite slow retry every 15 s while
   RUNNING. **`slowRetryMs` is set through the BLEManager configuration**
   (`BLEConfig.slowRetryMs`, a persisted IConfigurable field, default 60000), not
   through the copy returned by `getReconnectPolicy()`. The WO verifies the exact
   mechanism against the framework headers and records it.
4. **`HRMRepeater`** (`IBLEServiceProvider`) — Subscribes to HR events and
   re-broadcasts them as `0x180D`.
5. **Progressive coexistence test** — BLE and WiFi run simultaneously without panics
   or DRAM exhaustion; reconnect recovery criterion ≤ 20 s. The final coexistence
   test belongs to PR-09.

**Hardware:** No bike hardware needed: ESP32 + HR armband + P12 tablet. Tests with
the armband and the tablet are `complexity: interactive` WOs with evidence committed.

After this PR merges, the Echelon app pairs with **`Echenon-XXXX`** (base name
`Echenon`, MAC suffix added by the framework) as an HR device and displays live
heart rate. First user-facing feature with zero bike risk.

## Prior Art

- [wo5-hrm/WORKORDER.md](../archive-pa-decomposition/wo5-hrm/WORKORDER.md) — predates BLEManager's real API
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED — entry gates OPEN
