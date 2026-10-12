---
name: hr-relay-wo4
description: Handoff instructions for Work Order 4 — HRMClient (HR strap Central)
complexity: standard
---

# WO4 — HRMClient (HR Strap Central)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo4-hr-client` (base: `hr-relay/wo3-app-pairing-gate`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Role Definition

> You are an expert senior software engineer and execution-focused worker
> agent. You are NOT the Principal Architect. Do not make high-level
> architectural decisions and do not create new branches beyond what this
> WORKORDER specifies. All work happens on `hr-relay/wo4-hr-client` in the
> worktree at `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`.

## Objective

Implement `HRMClient` — an `IBLEClientProvider` + `ITickable` that connects to
a BLE HR strap (Central role), parses the full `0x2A37` specification, manages a
three-state contact machine, publishes HR via EventBus (decoupled from
HRMRepeater), reads battery level (`0x2A19`), and uses the framework's
`slowRetryMs` configuration for the 15 s reconnect schedule. Native-test the
parser thoroughly.

## Strict Boundary Box

- **DO NOT** edit `PROGRAM.md`, `PROGRAM-PLAN.md`, or `reviews/` files.
- **DO NOT** edit other PRs' `PROJECT.md` files.
- **DO NOT** modify `HRMRepeater` (WO2's code) beyond setting its BSL.
- **DO NOT** add FTMS or any service beyond `0x180D` / `0x180F`.
- **DO NOT** change GPIO 43/44.
- **DO NOT** flash firmware — build + native tests only (owner's parallel work).
- **DO NOT** file GitHub issues; surface findings in the Completion Report.

## Rules — Include in Every Flash

- **Rule 03 (Tag → Build → Flash):** Before any flash, tag first. (No flash
  this WO — build-only.)
- **Rule 04 (PlatformIO MCP tools only):** Use MCP tools; never `pio` CLI.

## Tooling Constraints

- PlatformIO MCP server for builds and tests.
- Temporary files in `tmp/` at the worktree root.
- All work in: `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Pre-WO Setup

- [ ] Create branch:
  ```bash
  cd /Users/mcneillm/Documents/Projects/Echenon-hr-relay
  git checkout -b hr-relay/wo4-hr-client hr-relay/wo3-app-pairing-gate
  ```

## Line-Item Instructions

### A. `HRMClient` — IBLEClientProvider + ITickable

Create `src/echenon/HRMClient.h` and `src/echenon/HRMClient.cpp`.

**Interface contract** (from `IBLEClientProvider.hpp` — already in the worktree
at `framework/modules/BLEManager/IBLEClientProvider.hpp`):

```cpp
class HRMClient : public IBLEClientProvider, public ITickable {
public:
    const char* getProviderId() const override { return "hr_client"; }
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }

    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override;
    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override;

    void onConnected(const BLEStoredDevice& device) override;
    void onNotification(const BLENotification& notification) override;
    void onDisconnected(BLEDisconnectReason reason) override;
    void onClientReadyWorker(NimBLEClient& client) override;

    void tick(uint32_t currentMillis) override;  // stale timeout

    void setEventBus(EventBus* bus) { _bus = bus; }
    void setRepeater(HRMRepeater* rep) { _repeater = rep; }

    // Accessors for portal / MCP
    uint8_t getBpm() const { return _lastBpm; }
    echenon::HRContactState getContactState() const { return _contactState; }
    uint8_t getBatteryPct() const { return _batteryPct; }
    uint8_t getBodySensorLocation() const { return _bodySensorLocation; }
};
```

### A.1 Target service UUIDs

```cpp
size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override {
    if (max == 0) return 0;
    out[0] = 0x180D;  // Heart Rate Service
    return 1;
}
```

### A.2 Subscriptions

```cpp
size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override {
    size_t n = 0;
    if (n < max) out[n++] = { 0x180D, 0x2A37, false, true  }; // HR Measurement — required
    if (n < max) out[n++] = { 0x180F, 0x2A19, false, false }; // Battery Level — optional
    return n;
}
```

### A.3 `onConnected()`

- Reset `_contactState` to `ACTIVE` (optimistic).
- Record `millis()` in `_lastValidHrMs`.
- Log the connected device name.

### A.4 `onClientReadyWorker()` — worker-task hook

- Read Body Sensor Location (`0x2A38`) from service `0x180D` on the connected
  client. Store in `_bodySensorLocation`.
- Call `_repeater->setBodySensorLocation(_bodySensorLocation)` to propagate
  to the re-broadcast characteristic.
- If the read fails, default to `0x00` (Other) — not an error.
- **Thread safety:** This runs on the BLEManager worker task, NOT the main loop.
  Only write to `_bodySensorLocation` (an atomic-safe uint8_t read later from
  main loop). The `_repeater->setBodySensorLocation()` call updates a NimBLE
  characteristic which is thread-safe (NimBLE uses its own mutex).

### A.5 `onNotification()` — full `0x2A37` parser

Implement the SPEC §8.2 parser. The function must handle ALL flag combinations,
not just the Beat's subset:

```cpp
void onNotification(const BLENotification& n) {
    if (n.charUuid16 == 0x2A19) {          // Battery Level
        _batteryPct = n.data[0];
        return;
    }
    if (n.charUuid16 != 0x2A37 || n.length < 2) return;

    uint8_t flags = n.data[0];
    bool hrIs16Bit        = flags & 0x01;
    bool contactSupported = flags & 0x04;
    bool contactDetected  = flags & 0x02;
    bool energyPresent    = flags & 0x08;
    bool rrPresent        = flags & 0x10;

    uint16_t bpm;
    size_t idx;
    if (hrIs16Bit) {
        if (n.length < 3) return;
        bpm = n.data[1] | (n.data[2] << 8);
        idx = 3;
    } else {
        bpm = n.data[1];
        idx = 2;
    }

    // No-contact: BPM=0 OR (contact flags supported && not detected)
    if (bpm == 0 || (contactSupported && !contactDetected)) {
        _contactState = echenon::HRContactState::NO_CONTACT;
        publishHr(0, _contactState, n.receivedAtMs);
        return;
    }

    _contactState = echenon::HRContactState::ACTIVE;
    _lastValidHrMs = n.receivedAtMs;
    publishHr(bpm > 255 ? 255 : static_cast<uint8_t>(bpm), _contactState, n.receivedAtMs);

    // Skip energy expended if present
    if (energyPresent && idx + 2 <= n.length) idx += 2;

    // RR-intervals (store last for future TCX enrichment)
    if (rrPresent) {
        while (idx + 2 <= n.length) {
            uint16_t rrRaw = n.data[idx] | (n.data[idx + 1] << 8);
            _lastRrMs = rrRaw / 1024.0f * 1000.0f;
            idx += 2;
        }
    }
}
```

### A.6 `onDisconnected()`

- Set `_contactState = DISCONNECTED`.
- Publish HR = 0 immediately.
- Log the disconnect reason.

### A.7 `tick()` — 10 s stale timeout

```cpp
void tick(uint32_t currentMillis) {
    if (_contactState != echenon::HRContactState::ACTIVE) return;
    if (currentMillis - _lastValidHrMs >= 10000) {
        _contactState = echenon::HRContactState::NO_CONTACT;
        publishHr(0, _contactState, currentMillis);
    }
}
```

### A.8 `publishHr()` — ring buffer pattern

```cpp
void publishHr(uint8_t bpm, echenon::HRContactState state, uint32_t receivedAtMs) {
    _lastBpm = bpm;
    auto& slot = _sampleRing[_ringHead];
    _ringHead = (_ringHead + 1) % kRingSize;
    slot = { bpm, state, receivedAtMs };
    if (_bus) _bus->publish(echenon::HR_TOPIC, &slot);
}
```

Use a small ring buffer (4 slots) — same pattern as `HRSynthetic`.

### B. Extract `0x2A37` parser into a testable function

Extract the core parsing logic into `src/echenon/HRParsing.h` (BLE-free,
inline) so native tests can exercise it without `WITH_BLE`:

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include "echenon/HREvents.h"

namespace echenon {

struct ParsedHrMeasurement {
    uint16_t       bpm = 0;
    HRContactState contactState = HRContactState::UNKNOWN;
    bool           hasEnergy = false;
    uint16_t       energyExpended = 0;
    bool           hasRr = false;
    float          lastRrMs = 0.0f;
    uint8_t        fieldsConsumed = 0;  // bytes parsed
};

/// Parse a 0x2A37 HR Measurement notification payload.
/// Returns true if valid, false if truncated/invalid.
inline bool parse0x2A37(const uint8_t* data, size_t length, ParsedHrMeasurement& out) {
    // ... (the full parser logic)
}

} // namespace echenon
```

`HRMClient::onNotification()` calls `parse0x2A37()` and acts on the result.

### C. Native tests for the `0x2A37` parser

Create `test/test_native/test_hr_parsing.cpp`:

- [ ] uint8 BPM, no flags (Beat armband: `{0x00, 72}`)
- [ ] uint16 BPM (`{0x01, 0x20, 0x01}` → 288 BPM, clamped to 255 in publish)
- [ ] Contact supported + detected (`{0x06, 80}`)
- [ ] Contact supported + NOT detected (`{0x04, 80}` → NO_CONTACT)
- [ ] BPM = 0 with no contact flags (`{0x00, 0}` → NO_CONTACT)
- [ ] Energy expended present (`{0x08, 72, 0x00, 0x00}`)
- [ ] RR-interval present (`{0x10, 72, 0x00, 0x04}` → RR = 1000 ms)
- [ ] All flags set: 16-bit + contact + energy + RR
- [ ] Truncated payload (length=0, length=1) → returns false
- [ ] uint16 BPM but only 2 bytes → returns false

### D. `slowRetryMs` — verify and set to 15000

- [ ] Read `framework/modules/BLEManager/BLEManager.hpp` and/or the framework
  BLE skill docs to understand how `slowRetryMs` is exposed.
- [ ] The SPEC says: *"`slowRetryMs` is a persisted field of BLEManager's
  `BLEConfig` (framework default 60000 ms)"* and Echenon sets it to `15000`
  through BLEManager's `IConfigurable` namespace `"ble"`.
- [ ] **Verify the mechanism:** How does a consumer app set `slowRetryMs`?
  Options to check:
  - Does `BLEManager::applyConfig()` accept `"slowRetryMs"` in the `"ble"`
    config namespace?
  - Is there a setter method like `bleManager.setSlowRetryMs(15000)`?
  - Is there a `BLEConfig` struct that can be set before `begin()`?
- [ ] Implement whichever mechanism the framework provides. Add the setting
  in `main.cpp` at the appropriate point in the lifecycle.
- [ ] **Record what you found** in the Completion Report — the SPEC says
  "PR-03 verifies the exact mechanism."

### E. Wire into `EchenonModule` and `main.cpp`

- [ ] Add `HRMClient` as a member of `EchenonModule` (guarded by `WITH_BLE`).
  Add accessor: `HRMClient& hrmClient() { return _hrmClient; }`

- [ ] In `main.cpp`, register the client provider before `app.begin()`:
  ```cpp
  bleManager.registerClientProvider(&echenonModule.hrmClient());
  ```

- [ ] After `bleManager.begin(app)`, wire EventBus and repeater:
  ```cpp
  echenonModule.hrmClient().setEventBus(&app.getEventBus());
  echenonModule.hrmClient().setRepeater(&echenonModule.hrmRepeater());
  ```

- [ ] Register `HRMClient` as an `ITickable` module:
  ```cpp
  app.registerModule(&echenonModule.hrmClient());
  ```
  (before `app.begin(config)` — check if this is correct for ITickable, or if
  it needs to be after. The framework's `registerModule` is safe before `begin`.)

### F. Build verification

- [ ] `env:native` builds and ALL tests pass (32 existing + new parser tests).
- [ ] `env:ex3-waveshare-nano` builds cleanly.
- [ ] `env:ex3-waveshare-nano-diag` builds cleanly.
- [ ] No GPIO 43/44 references in new code.
- [ ] **No flash** — build-only.

### G. Commit

- [ ] `git add -A`
- [ ] Commit:
  ```
  wo4: HRMClient, 0x2A37 parser, contact FSM, stale timeout, slowRetryMs

  - HRMClient: IBLEClientProvider + ITickable for Central HR strap connection
  - Full 0x2A37 parser: uint8/uint16 BPM, contact flags, energy, RR-intervals
  - Three-state contact machine: ACTIVE / NO_CONTACT / DISCONNECTED
  - 10s stale timeout publishes HR=0 on missed notifications
  - Battery level (0x2A19) and Body Sensor Location (0x2A38) read
  - EventBus publish via ring buffer (no direct HRMRepeater call)
  - slowRetryMs=15000 set via <mechanism found>
  - Native tests for parser covering all flag combinations
  ```
- [ ] Do NOT push.

## Verification Requirement

1. `env:native` — all tests pass.
2. `env:ex3-waveshare-nano` — builds cleanly.
3. `env:ex3-waveshare-nano-diag` — builds cleanly.
4. No GPIO 43/44 references.
5. `getProviderId()` returns `"hr_client"`.
6. Parser handles ALL `0x2A37` flag combinations.
7. `slowRetryMs` mechanism documented in Completion Report.

## Issue Surfacing

Surface findings in the Completion Report — do not file issues directly.

---

## Completion Report (written by the worker at task completion)

<!-- Written by the worker at task completion -->

### Status
(pending)

### Changes Made
- (pending)

### Decisions Made
- (pending)

### Dead Ends & Warnings
- (pending)

### Out-of-Scope Findings
- (pending)

### Blockers
(pending)

### slowRetryMs Mechanism
(pending — document what the framework provides and how Echenon sets it)
