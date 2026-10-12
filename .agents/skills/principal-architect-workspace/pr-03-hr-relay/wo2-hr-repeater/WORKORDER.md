---
name: hr-relay-wo2
description: Handoff instructions for Work Order 2 — HRMRepeater (HR peripheral)
complexity: standard
---

# WO2 — HRMRepeater (HR Peripheral)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo2-hr-repeater` (base: `hr-relay/wo1-ble-enable`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Role Definition

> You are an expert senior software engineer and execution-focused worker
> agent. You are NOT the Principal Architect. Do not make high-level
> architectural decisions, do not plan, and do not create new branches beyond
> what this WORKORDER specifies. Your task is strictly limited to executing the
> checklist below. All work happens on the branch `hr-relay/wo2-hr-repeater` in
> the worktree at `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`.

## Objective

Implement `HRMRepeater` (an `IBLEServiceProvider` for BLE Heart Rate Service
`0x180D`), an EventBus HR event with a typed payload, a synthetic HR source for
testing under `WITH_HR_SYNTHETIC`, and native GoogleTest tests for `0x2A37`
encoding. After this WO, the device advertises `Echenon-XXXX` with a Heart Rate
Service and re-broadcasts HR from the EventBus (which WO4's `HRMClient` will
publish to).

## Strict Boundary Box

- **DO NOT** edit `PROGRAM.md`, `PROGRAM-PLAN.md`, or any `reviews/` files.
- **DO NOT** edit other PRs' `PROJECT.md` files.
- **DO NOT** implement `HRMClient` or `IBLEClientProvider` — that is WO4.
- **DO NOT** implement FTMS or any service other than `0x180D`.
- **DO NOT** change GPIO 43 or GPIO 44 assignments.
- **DO NOT** file GitHub issues. Surface findings in the Completion Report.

## Rules — Include in Every Flash

- **Rule 03 (Tag → Build → Flash):** Before any flash, check the highest tag
  (`git describe --tags --abbrev=0`), increment the dev PRE_NUM, push the tag,
  rebuild, then flash. Dev tags are autonomous.
- **Rule 04 (PlatformIO MCP tools only):** Use `build_project`, `run_tests`,
  etc. from the `platformio-mcp_platformio` MCP server. Never use `pio` CLI.

## Tooling Constraints

- Use the `platformio-mcp_platformio` MCP server for all builds and tests.
- Temporary files go in `tmp/` at the worktree root.
- All work is in the worktree: `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`

## Pre-WO Setup

- [ ] Create the WO2 branch from the WO1 tip:
  ```bash
  cd /Users/mcneillm/Documents/Projects/Echenon-hr-relay
  git checkout -b hr-relay/wo2-hr-repeater hr-relay/wo1-ble-enable
  ```

## Line-Item Instructions

### A. HR Event definition

Create `src/echenon/HREvents.h`:

```cpp
#pragma once
#include <cstdint>

namespace echenon {

/// HR contact state machine (SPEC §8.2, §8.4)
enum class HRContactState : uint8_t {
    UNKNOWN = 0,     ///< No data yet
    ACTIVE,          ///< Valid HR reading
    NO_CONTACT,      ///< BPM = 0 or contact flags say off-skin
    DISCONNECTED     ///< BLE connection lost
};

/// EventBus payload for topic "echenon.hr"
struct HrSample {
    uint8_t        bpm;            ///< 0 = no contact / stale
    HRContactState contactState;
    uint32_t       receivedAtMs;   ///< millis() when received
};

/// EventBus topic name
inline constexpr const char* HR_TOPIC = "echenon.hr";

} // namespace echenon
```

### B. `HRMRepeater` — IBLEServiceProvider

Create `src/echenon/HRMRepeater.h` and `src/echenon/HRMRepeater.cpp`.

**Header** (`HRMRepeater.h`):
```cpp
#pragma once
#ifdef WITH_BLE

#include "BLEManager/IBLEServiceProvider.hpp"

class EventBus;
class NimBLECharacteristic;

class HRMRepeater : public IBLEServiceProvider {
public:
    const char* getProviderId() const override { return "hr_repeater"; }

    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                               uint16_t charUuid16, bool notify, bool indicate) override;

    void subscribe(EventBus& bus);

    // 0x2A37 encoding — public for native testing
    static size_t encode0x2A37(uint8_t bpm, uint8_t bodySensorLocation,
                               uint8_t* out, size_t outLen);

private:
    NimBLECharacteristic* _hrMeasChar  = nullptr;  // 0x2A37
    NimBLECharacteristic* _bodySensChar = nullptr; // 0x2A38
    uint8_t  _bodySensorLocation = 0x00;  // Other; updated from strap
    uint8_t  _lastBpm = 0;
    bool     _anySubscriber = false;

    static void onHrEvent(const char* topic, const void* payload, void* ctx);
    void notifyHr(uint8_t bpm);
};

#endif // WITH_BLE
```

**Implementation** (`HRMRepeater.cpp`):

- [ ] `registerServices()`: Create service `0x180D` with characteristics:
  - `0x2A37` (HR Measurement): Notify, no read, no write
  - `0x2A38` (Body Sensor Location): Read only
  Set `_bodySensChar` to `0x00` (Other) initially. Return true.

- [ ] `contributeAdvertising()`:
  ```cpp
  adv.addServiceUuid16(0x180D, BLEAdvertisingBuilder::Priority::Normal);
  ```
  Use `Normal` priority — HR UUID spills to scan response when FTMS fills the
  primary PDU (per SPEC §8.2).

- [ ] `onSubscriptionChanged()`: Track whether any Central is subscribed to
  `0x2A37`. Only notify when `_anySubscriber` is true.

- [ ] `subscribe(EventBus& bus)`:
  Subscribe to `echenon::HR_TOPIC` with `HRMRepeater::onHrEvent` and `this` as
  context.

- [ ] `onHrEvent()`: Static callback. Cast payload to `const echenon::HrSample*`.
  If `bpm > 0`, call `notifyHr(bpm)`. If `bpm == 0`, stop notifying (or notify 0).

- [ ] `notifyHr(uint8_t bpm)`: Encode the measurement using `encode0x2A37()` and
  notify via `_hrMeasChar->notify()`. Only notify if `_anySubscriber`.

- [ ] `encode0x2A37()`: Static, public for testing. Encodes a uint8 HR with no
  contact flags (flags byte `0x00`):
  ```
  byte 0: flags = 0x00 (uint8 HR, no contact support, no energy, no RR)
  byte 1: BPM value
  ```
  The Body Sensor Location is on a separate characteristic (`0x2A38`), not in
  the measurement. Return the number of bytes written.

### C. `setBodySensorLocation()` public method

Add a public method to `HRMRepeater`:
```cpp
void setBodySensorLocation(uint8_t loc);
```
This updates `_bodySensorLocation` and the `0x2A38` characteristic value.
WO4's `HRMClient` will read the strap's BSL and call this. Default is `0x00`
(Other).

### D. Synthetic HR source (`WITH_HR_SYNTHETIC`)

Create `src/echenon/HRSynthetic.h` and `src/echenon/HRSynthetic.cpp`.

This is a diagnostic-only synthetic HR publisher that generates a sine-wave HR
(60–120 BPM, 10 s period) on the `echenon.hr` EventBus topic. It's gated by
`-DWITH_HR_SYNTHETIC` (added only to the diag env).

- [ ] `HRSynthetic` implements `ITickable`.
- [ ] In `tick()`, publish an `HrSample` every ~1000 ms with a synthetic BPM.
- [ ] `setEventBus(EventBus* bus)` for wiring.

### E. Wire into `EchenonModule` and `main.cpp`

- [ ] Add `HRMRepeater` as a member of `EchenonModule` (guarded by `WITH_BLE`).
  Add an accessor: `HRMRepeater& hrmRepeater() { return _hrmRepeater; }`

- [ ] Add `HRSynthetic` as a member of `EchenonModule` (guarded by
  `WITH_HR_SYNTHETIC`). Add an accessor.

- [ ] In `main.cpp`:
  - Register the repeater with BLEManager before `app.begin()`:
    ```cpp
    bleManager.registerServiceProvider(&echenon.hrmRepeater());
    ```
  - After `bleManager.begin(app)`, subscribe the repeater to EventBus:
    ```cpp
    echenon.hrmRepeater().subscribe(app.getEventBus());
    ```
  - Wire `HRSynthetic`'s EventBus and register it as a module (if
    `WITH_HR_SYNTHETIC`):
    ```cpp
    #ifdef WITH_HR_SYNTHETIC
        echenon.hrSynthetic().setEventBus(&app.getEventBus());
        app.registerModule(&echenon.hrSynthetic());
    #endif
    ```

- [ ] Add `-DWITH_HR_SYNTHETIC` to the `ex3-waveshare-nano-diag` env in
  `platformio.ini`.

### F. Native tests for `0x2A37` encoding

Create `test/test_native/test_hr_encoding.cpp`:

- [ ] Test: `encode0x2A37` with BPM=72 → `{0x00, 72}` (2 bytes)
- [ ] Test: `encode0x2A37` with BPM=0 → `{0x00, 0}` (2 bytes)
- [ ] Test: `encode0x2A37` with BPM=255 → `{0x00, 255}` (2 bytes)
- [ ] Test: Buffer too small (outLen=1) → returns 0

The test file includes `HRMRepeater.h` (or a testable header with the static
function). Since `HRMRepeater.h` is guarded by `WITH_BLE`, the test file must
either:
- Extract `encode0x2A37` into a BLE-free header (e.g. `HREncoding.h`), or
- Define `WITH_BLE` for the native test (if compatible), or
- Put `encode0x2A37` in a standalone header that both `HRMRepeater.cpp` and the
  test include.

**Recommended:** Create `src/echenon/HREncoding.h` with the static `encode0x2A37`
function (no BLE dependency), included by both `HRMRepeater.cpp` and the test.

### G. Build verification

- [ ] `env:native` builds and all tests pass (existing 24 + new HR encoding tests).
- [ ] `env:ex3-waveshare-nano` builds cleanly (BLE on, no synthetic, no logger).
- [ ] `env:ex3-waveshare-nano-diag` builds cleanly (BLE + synthetic + logger).

### H. Commit

- [ ] Stage all changes: `git add -A`
- [ ] Commit with message:
  ```
  wo2: HRMRepeater, EventBus HR event, synthetic source, encoding tests

  - HRMRepeater: IBLEServiceProvider for 0x180D (HR Service)
  - 0x2A37 HR Measurement and 0x2A38 Body Sensor Location characteristics
  - EventBus topic "echenon.hr" with HrSample payload and HRContactState
  - HRSynthetic: ITickable sine-wave HR source under WITH_HR_SYNTHETIC
  - Native tests for 0x2A37 encoding (uint8 BPM, boundary values)
  - Wired into EchenonModule and main.cpp composition
  ```
- [ ] Do NOT push — the PA handles pushing.

## Verification Requirement

1. `env:native` — builds and all tests pass (24 existing + new encoding tests).
2. `env:ex3-waveshare-nano` — builds cleanly with BLE + HRMRepeater.
3. `env:ex3-waveshare-nano-diag` — builds cleanly with BLE + HRMRepeater + synthetic.
4. No references to GPIO 43 or GPIO 44 in new code.
5. `HRMRepeater::getProviderId()` returns `"hr_repeater"`.
6. `contributeAdvertising` uses `Priority::Normal` for `0x180D`.

## Issue Surfacing

If you discover a defect, upstream bug, or concern outside your boundary box,
surface it in your Completion Report below — do not file issues directly.

---

## Completion Report (written by the worker at task completion)

### Status
Complete. Implementation verified via host GoogleTest tests and target builds across all specified environments.

### Changes Made
- **HR Events (`src/echenon/HREvents.h`):** Created `HRContactState` enum, `HrSample` payload (`bpm`, `contactState`, `receivedAtMs`), and `HR_TOPIC` (`"echenon.hr"`).
- **0x2A37 Encoding (`src/echenon/HREncoding.h`):** Created standalone `encode0x2A37()` encoder without BLE dependencies for host unit testing. Encodes uint8 BPM with flags `0x00`.
- **HRMRepeater (`src/echenon/HRMRepeater.h`, `src/echenon/HRMRepeater.cpp`):**
  - Implemented `IBLEServiceProvider` with provider ID `"hr_repeater"`.
  - Registered GATT Heart Rate Service `0x180D` with `0x2A37` (Notify) and `0x2A38` (Read-only, initialized to `0x00` Other).
  - Contributed `0x180D` advertising with `BLEAdvertisingBuilder::Priority::Normal`.
  - Implemented subscription tracking via `onSubscriptionChanged()`, notifying Centrals only when active subscribers exist.
  - Subscribed to `echenon.hr` on EventBus; sends `0x2A37` notifications when BPM > 0.
  - Implemented `setBodySensorLocation()` updating the characteristic and local state.
- **HRSynthetic (`src/echenon/HRSynthetic.h`, `src/echenon/HRSynthetic.cpp`):**
  - Implemented `ITickable` under `WITH_HR_SYNTHETIC`. Generates a 60–120 BPM sine wave (10 s period) published to `echenon.hr` every ~1000 ms using a ring-buffer payload to satisfy EventBus async memory safety.
- **Composition & Wiring (`src/echenon/EchenonModule.h`, `src/main.cpp`, `platformio.ini`):**
  - Added `HRMRepeater` and `HRSynthetic` members and accessors to `EchenonModule`.
  - Wired `hrmRepeater` to `bleManager` before `app.begin()` and subscribed to EventBus after `bleManager.begin(app)`.
  - Wired `hrSynthetic` to `app.getEventBus()` and registered with `app` before `app.begin()`.
  - Added `-DWITH_HR_SYNTHETIC` to `[env:ex3-waveshare-nano-diag]` in `platformio.ini`.
- **Native Unit Tests (`test/test_native/test_hr_encoding.cpp`):**
  - Added test suite validating `0x2A37` encoding for nominal (72 BPM), zero (0 BPM), maximum uint8 (255 BPM), small buffer (outLen=1), null buffer, zero length, and 2-arg overload.

### Decisions Made
- **Header separation (`HREncoding.h`):** Factored `encode0x2A37()` into a standalone header included by both `HRMRepeater` and native unit tests, allowing clean host compilation without pulling in NimBLE.
- **EventBus payload memory safety:** Followed EventBus contract rule 5 by using a 4-slot ring buffer (`_sampleRing`) in `HRSynthetic` to ensure payload pointers remain valid during asynchronous event dispatch on ESP32/FreeRTOS targets.
- **Module registration order:** Registered `hrSynthetic` as an `ITickable` module prior to `app.begin(config)` to avoid runtime rejection caused by Application hub post-begin locking.

### Dead Ends & Warnings
- `<cstddef>` was required in `HRSynthetic.h` to declare `size_t` for the ring buffer index in ESP-IDF C++17 compilation.

### Out-of-Scope Findings
- None.

### Blockers
- None. Flashing was intentionally skipped per owner instruction (parallel testing on device).
