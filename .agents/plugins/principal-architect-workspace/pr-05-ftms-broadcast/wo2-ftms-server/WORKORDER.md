---
schema_version: 1
wo_id: WO-05.2
pr_id: PR-05
title: FTMSServer Read-Only BLE Service Provider (0x1826, 0x2ACC, 0x2AD2)
complexity: standard
status: PENDING
allowed_files:
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
- src/echenon/FTMSData.h
- test/test_native/test_ftms_server.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: 0108b6c2d99d62143b20f8804a12dd1229e51e75a341c5db6f2d2fc4eb084a13
completion: null
---

# WO-05.2: FTMSServer Read-Only BLE Service Provider (0x1826, 0x2ACC, 0x2AD2)

## 1. Objective & Scope

Implement `FTMSServer` (`src/echenon/FTMSServer.h` and `src/echenon/FTMSServer.cpp`) as a Service Module implementing `IBLEServiceProvider` (under `#ifdef WITH_BLE`) and `ITickable`, exposing the read-only FTMS GATT service (`0x1826`), advertising UUID `0x1826`, subscribing to `echenon.hr` on the `EventBus`, and pushing 11-byte `Indoor Bike Data (0x2AD2)` notifications at 2 Hz (`500 ms`).

## 2. Detailed Requirements

### 2.1 `FTMSServer` Class Contract (`src/echenon/FTMSServer.h` & `.cpp`)
Follow the exact structural and thread-safety patterns established in `src/echenon/HRMRepeater.h` / `.cpp`:
1. **Interfaces:**
   - Inherits from `ITickable` and (when `#ifdef WITH_BLE` is active) `IBLEServiceProvider`.
   - `const char* getName() const override { return "FTMSServer"; }`
2. **Dependencies & Telemetry Provider:**
   - Constructor accepts `EventBus& eventBus` (plus optional telemetry callback or setter so `EchenonModule` can push or supply live `(speed_kph, cadence_rpm, internal_resistance, power_watts)`).
   - Provide thread-safe `updateTelemetry(float speed_kph, float cadence_rpm, uint8_t internal_resistance, int16_t power_watts)` and/or a callback getter `TelemetryProvider` so `FTMSServer` always encodes the freshest telemetry on its 2 Hz tick.
3. **Heart Rate Subscription & Stale Timeout (`SPEC.md` §7.3 / §8.2):**
   - In `begin()`, subscribe to `"echenon.hr"` on `_eventBus`:
     - When `HRMeasurementPayload` arrives with `connected == true` and `heart_rate_bpm > 0`, store `_lastHrBpm` and `_lastHrMs = millis()`.
     - When `connected == false`, clear `_lastHrBpm = 0`.
     - If `millis() - _lastHrMs > 5000` (`5 s` stale threshold), report `heart_rate_bpm = 0` in `0x2AD2`.
   - Unsubscribe cleanly in `end()` / destructor (`_eventBus.unsubscribe(_hrSubId)`).
4. **NimBLE Service Setup (`#ifdef WITH_BLE`):**
   - `void onBLEServerReady(NimBLEServer* server) override`:
     - Create service `0x1826` (`FTMS_SERVICE_UUID`).
     - Create `0x2ACC` (`Fitness Machine Feature`, `NIMBLE_PROPERTY::READ`) and set initial 8-byte value via `encodeFitnessMachineFeature()`.
     - Create `0x2AD2` (`Indoor Bike Data`, `NIMBLE_PROPERTY::NOTIFY`) with initial 11-byte zeroed packet.
     - Optional compile-time fallback `#if defined(FTMS_STUB_CONTROL_POINT) && FTMS_STUB_CONTROL_POINT`:
       - Create `0x2AD9` (`Fitness Machine Control Point`, `NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE`).
       - On write: if opcode `0x00` (`Request Control`), indicate `[0x80, 0x00, 0x01]` (`Success`); for any other opcode `req`, indicate `[0x80, req, 0x02]` (`Op Code Not Supported`).
     - Do **NOT** call `service->start()` inside `onBLEServerReady()` — `BLEManager::begin()` starts all registered services before advertising.
   - `void onAdvertisingBuild(BLEAdvertisingBuilder& builder) override`:
     - Call `builder.addServiceUUID(NimBLEUUID(static_cast<uint16_t>(0x1826)))`.
5. **2 Hz Notification Tick (`tick()`):**
   - Every `500 ms` (`NOTIFY_INTERVAL_MS = 500`), encode the 11-byte `Indoor Bike Data` packet via `encodeIndoorBikeData()`.
   - Under `#ifdef WITH_BLE`, if `_bikeDataChar != nullptr` and `_server != nullptr && _server->getConnectedCount() > 0`, call `_bikeDataChar->setValue(buf, 11)` and `_bikeDataChar->notify()`.
   - Expose `getLastPacket(uint8_t out[11]) const`, `getNotifyCount() const`, and `getEffectiveHrBpm(uint32_t now_ms) const` for unit testing and diagnostics.

### 2.2 Native Unit Tests (`test/test_native/test_ftms_server.cpp`)
- Test `FTMSServer` lifecycle (`begin()`, `updateTelemetry()`, `"echenon.hr"` `EventBus` delivery, `5000 ms` HR staleness timeout, `500 ms` notification cadence, and `end()` unsubscription) in the native test environment.

## 3. Acceptance Criteria
- `pio test -e native` passes all existing and new `FTMSServer` unit tests with zero warnings or memory leaks.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
