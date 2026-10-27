---
schema_version: 1
wo_id: WO-02.4
pr_id: PR-02
title: Live WebSocket Feed
complexity: standard
status: IN_PROGRESS
allowed_files:
- src/echenon/LiveFeed.h
- src/echenon/LiveFeed.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- web/echenon-dashboard.js
- platformio.ini
forbidden_files: []
required_skills:
- /Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md
verification_command: pio run -e ex3-waveshare-nano
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-02.4 — Live WebSocket Feed

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo4-ws-feed` from `calibration-power/wo3-calibration-ui`
> **Charter sections:** §7 of the PR-02 Work Order Ledger

## 1. Objective

Add a real-time WebSocket feed pushing live cadence, resistance level, power,
and diagnostics at 2–4 Hz, and extend the existing dashboard to display these.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/LiveFeed.h` — new
- `src/echenon/LiveFeed.cpp` — new
- `src/echenon/EchenonModule.h` — modify (add LiveFeed member)
- `src/echenon/EchenonModule.cpp` — modify (register WS handler, tick LiveFeed)
- `web/echenon-dashboard.js` — modify (add WS-powered live display)
- `platformio.ini` — modify only if WS lib needed (PsychicHttp already includes WS)

### Out of scope
- Native tests (WS is runtime-only, no native test file needed)
- CalibrationRig (WO-02.5)
- Runaway detection (WO-02.8)

## 3. Specification

### 3.1 LiveFeed Class

```cpp
namespace echenon {

class LiveFeed {
public:
    LiveFeed() = default;

    // Set the WS handler (called during registerRoutes)
    void setHandler(PsychicWebSocketHandler* handler);

    // Called every tick from EchenonModule
    // Broadcasts JSON to all connected WS clients at the configured rate
    void tick(uint32_t currentMillis,
              float cadenceRPM,
              uint8_t echelonLevel,
              uint8_t internalLevel,
              float powerWatts,
              uint16_t potAdc,
              int8_t motorDir,     // -1, 0, +1 from motor sense (0 for v1)
              int32_t encoderDelta // from EncoderModule events (0 if not tracked)
              );

    // Configuration
    void setIntervalMs(uint32_t ms);  // default 333 ms (~3 Hz)
    uint32_t getIntervalMs() const;

private:
    PsychicWebSocketHandler* _handler = nullptr;
    uint32_t _intervalMs = 333;      // ~3 Hz
    uint32_t _lastBroadcast = 0;
};

} // namespace echenon
```

### 3.2 WebSocket JSON Payload

Broadcast at 2–4 Hz (default 3 Hz = 333 ms) to all connected clients:

```json
{
    "cadence_rpm": 85.3,
    "level_echelon": 19,
    "level_i": 148,
    "power_watts": 167.5,
    "pot_adc": 842,
    "motor_dir": 0,
    "encoder_delta": 0,
    "timestamp_ms": 123456789
}
```

### 3.3 WebSocket Handler Registration

In `EchenonModule::registerRoutes()`:

```cpp
// Create WS handler on the endpoint
static PsychicWebSocketHandler wsHandler;
server.on("/ws/echenon/live", &wsHandler);
_liveFeed.setHandler(&wsHandler);
```

The `PsychicWebSocketHandler` is part of PsychicHttp v3 (already a dependency).
Key API:
- `server.on(path, &handler)` — registers the WS endpoint
- `handler.sendAll(const char* buf)` — broadcasts to all connected clients
- `handler.onOpen(cb)` / `handler.onClose(cb)` — connection lifecycle callbacks

### 3.4 Dashboard Extension (web/echenon-dashboard.js)

Extend the existing dashboard to show a live display section powered by the
WebSocket feed. The existing dashboard shows SignalLogger data — keep that
code intact and add a new section above or below it.

**Add:**
- A "Live Metrics" section showing:
  - Cadence (RPM) — large number
  - Level (Echelon) — large number
  - Power (Watts) — large number
  - Pot ADC — smaller diagnostic value
- A simple time-series strip chart (optional — a rolling table of recent
  values is also acceptable for v1)
- WebSocket connection management: connect on tab activate, disconnect on
  tab deactivate, reconnect on error with exponential backoff
- Connection status indicator (connected/disconnected/reconnecting)

**Pattern:**
```javascript
var ws = null;
var wsUrl = 'ws://' + window.location.host + '/ws/echenon/live';

function connectWS() {
    ws = new WebSocket(wsUrl);
    ws.onmessage = function(e) {
        var data = JSON.parse(e.data);
        // Update UI elements
    };
    ws.onclose = function() { /* reconnect logic */ };
}
```

### 3.5 EchenonModule Integration

- Add `#include "echenon/LiveFeed.h"` (guarded with `#ifndef UNIT_TEST` since
  PsychicWebSocketHandler is not available on native)
- Add `LiveFeed _liveFeed;` member (guarded)
- In `tick()`: call `_liveFeed.tick(...)` with current sensor values (guarded)
- In `registerRoutes()`: create the WS handler and register it

**Important:** Since `PsychicWebSocketHandler` is a target-only type, all
LiveFeed code must be guarded behind `#ifndef UNIT_TEST` or `#ifdef ARDUINO`
to avoid breaking native tests.

## 4. Verification

1. `pio run -e ex3-waveshare-nano` — target compiles with zero errors
2. `pio test -e native` — existing native tests still pass (LiveFeed is
   guarded out of native builds)

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- Framework submodule drift — NEVER commit
- Known gotchas: `<cstddef>` before BOARD_HEADER; Arduino `#define DEFAULT 1`
- The existing `echenon-dashboard.js` has ~14KB of SignalLogger code — preserve
  it all, add to it
## Completion Report

_No completion report recorded._
