> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-03](../../pr-03-hr-relay/PROJECT.md) (HR relay) and [PR-05](../../pr-05-ftms-broadcast/PROJECT.md) (FTMS). **Superseded by:** SPEC §3.5, §7, §8 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# PR-03: BLE Stack (FTMS Server + HRM Relay)

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (`power-engine` branch tip)
> **Spec sections:** §7.1–7.7, §8.1–8.4

## Mandate

Add NimBLE to the build. Implement three BLE provider modules using the
Foundation's BLEManager ([RFC #225 v2](https://github.com/matthewmcneill/esp32-app-foundation/issues/225)):

1. **`FTMSServer`** — Implements `IBLEServiceProvider` + `ITickable`. Creates the
   FTMS GATT service in `registerServices()`. Contributes service data (flags +
   Indoor Bike type) and appearance via `contributeAdvertising(BLEAdvertisingBuilder&)`.
   Tracks subscriber count via `onSubscriptionChanged()` — only notifies at 2 Hz
   when ≥1 subscriber exists. Pauses/resumes via `onStackPaused()`/`onStackResumed()`.
2. **`HRMClient`** — Implements `IBLEClientProvider` (declarative). Declares target
   UUID `0x180D` and subscription `{0x180D, 0x2A37, notify, required}`. Receives
   parsed `BLENotification` structs on the main loop — no raw NimBLE types.
   Relays HR to `HRMRepeater::publishHr()`. Uses `BLENotification.receivedAtMs`
   for stale detection (§8.4) and relay latency measurement (<50ms budget, p99).
3. **`HRMRepeater`** — Implements `IBLEServiceProvider`. Creates `0x180D` HR
   service in `registerServices()` (always present — static GATT per Q7).
   Contributes `addServiceUuid16(0x180D)` to advertising. Exposes `publishHr(bpm)`.

After this PR merges, the device advertises as `"Echenon-XXXX"` (BT MAC suffix)
with FTMS + HR services. Zwift, Xert EBC, TrainerRoad, and all FTMS-compatible
apps can pair and receive live data. HR from the armband is relayed through.

**BLEManager dependency:** This PR depends on Foundation BLEManager (RFC #225 v2).
If not yet merged upstream, implement against the RFC'd `IBLEServiceProvider` /
`IBLEClientProvider` contracts and stub registration calls — the integration PR
(PR-07) will wire to the real BLEManager.

**`main.cpp` wiring:**
```cpp
#ifdef WITH_BLE
    bleManager.registerServiceProvider(&ftms);
    bleManager.registerServiceProvider(&hrmRepeater);
    bleManager.registerClientProvider(&hrmClient);
    app.registerModule(&bleManager);
#endif
```

**Radio coexistence (SPEC §8.5):** WiFi and BLE share one radio on the ESP32-S3.
This PR must implement:
- Scanning discipline: discovery scans user-initiated only, bounded 30s,
  window ≤ 50% of interval. Reconnect via direct connect with exponential backoff.
- Phase-aware behaviour: no BLE at all in CONFIGURING/AP mode
- Connection parameters: Peripheral sets *preferred* params (15–30ms) — Central
  decides actual. Central role (HR strap) applies 30–75ms directly.
- `ScanInhibit` RAII token: StravaUploader acquires before uploads
- `sdkconfig.defaults` coex flags: `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`
- Opportunistic bonding: bond if peer supports SMP, never require it

## Prior Art

- [wo4-ftms/WORKORDER.md](../../archive-pa-decomposition/wo4-ftms/WORKORDER.md)
- [wo5-hrm/WORKORDER.md](../../archive-pa-decomposition/wo5-hrm/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
