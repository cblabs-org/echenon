# DiagnosticsModule — Developer Guide

> **SPEC reference:** [SPEC.md §13](../SPEC.md) (Diagnostics)
>
> **Source files:**
> [`DiagnosticsModule.h`](../src/echenon/DiagnosticsModule.h),
> [`DiagnosticsModule.cpp`](../src/echenon/DiagnosticsModule.cpp),
> [`DiagnosticsRoutes.h`](../src/echenon/DiagnosticsRoutes.h),
> [`DiagnosticsRoutes.cpp`](../src/echenon/DiagnosticsRoutes.cpp)

---

## Overview

The `DiagnosticsModule` provides a lightweight, always-on event log for
production Echenon devices. It records structured diagnostic events in a
PSRAM-backed ring buffer and exposes them through a paged REST API.

### Key Design Decisions

- **PSRAM ring buffer, not flash.** Events are stored in volatile PSRAM, not
  LittleFS. This avoids flash wear from frequent writes (events can fire on
  every boot, session start/end, etc.) while providing sufficient capacity for
  diagnostic visibility. Events are lost on power-off — this is intentional;
  the log captures _recent_ device behaviour, not permanent history.

- **Separate from SignalLogger.** `SignalLogger` is a Phase 0 development
  tool that captures raw ADC waveforms at high frequency. It is compiled only
  behind `WITH_SIGNAL_LOGGER` and is not present in production builds.
  `DiagnosticsModule` is always compiled and records high-level lifecycle
  events, not raw sensor data.

- **Separate from the framework Audit module.** The framework's
  `AuditModule` (`WITH_AUDIT`) provides a general-purpose flash-backed audit
  trail with rate limiting. `DiagnosticsModule` is Echenon-specific and
  focused on the bike's operational lifecycle.

---

## Architecture

```
                     ┌────────────────────────────────────────┐
                     │          DiagnosticsModule              │
                     │   (ITickable, PSRAM ring buffer)        │
                     │                                        │
  Boot sequence ────►│  recordBoot(epoch, uptime)             │
  SessionManager ───►│  recordSessionStart(epoch, id)         │
  SessionManager ───►│  recordSessionEnd(epoch, dur, count)   │
  Runaway detect ───►│  recordRunawayAlert(epoch, dur_ms)     │
  CalibWizard ──────►│  recordCalibrationDone(epoch, type)    │
  OTA boot ─────────►│  recordFirmwareChange(epoch, maj,min,p)│
                     │                                        │
                     │  queryEvents(query, out, cap)           │
                     └──────────────────┬─────────────────────┘
                                        │
                     ┌──────────────────▼─────────────────────┐
                     │          DiagnosticsRoutes              │
                     │   (REST API, PsychicHttpServer)         │
                     │                                        │
                     │  GET  /api/echenon/events               │
                     │  GET  /api/echenon/runs                 │
                     │  GET  /api/echenon/runs/<id>.csv.gz     │
                     │  DELETE /api/echenon/runs/<id>          │
                     └────────────────────────────────────────┘
```

---

## How to Record Custom Events

All recording is done through convenience methods on `DiagnosticsModule`.
Each method encodes its arguments into the 3-byte payload and calls
`recordEvent()`.

### Available Recorders

```cpp
// Boot event — called once during startup
diag.recordBoot(epoch_s, uptime_s);

// Session lifecycle — called by SessionManager
diag.recordSessionStart(epoch_s, session_id);
diag.recordSessionEnd(epoch_s, duration_s, record_count);

// Safety alert — called when runaway is detected
diag.recordRunawayAlert(epoch_s, duration_ms);

// Calibration — called when calibration completes
diag.recordCalibrationDone(epoch_s, 0);  // 0 = initial, 1 = recalibration

// Firmware change — called on first boot after OTA
diag.recordFirmwareChange(epoch_s, major, minor, patch);
```

### Raw Event Recording

For custom or future event types, use the low-level API directly:

```cpp
diag.recordEvent(
    DiagEventType::BOOT,    // type discriminator
    epoch_s,                // UTC epoch seconds
    payload_byte_0,         // p0
    payload_byte_1,         // p1
    payload_byte_2          // p2
);
```

### Event Data Structure

Each event is exactly 8 bytes, packed:

```
┌──────────────┬──────────┬────────────────────┐
│  epoch_s     │  type    │  payload[3]        │
│  (4 bytes)   │ (1 byte) │  (3 bytes)         │
└──────────────┴──────────┴────────────────────┘
```

---

## Event Log Query Examples

### Query from C++

```cpp
#include "echenon/DiagnosticsModule.h"

using namespace echenon;

// Query the last 10 events
EventQuery query;
query.limit = 10;

DiagnosticEvent results[10];
size_t count = diag.queryEvents(query, results, 10);

for (size_t i = 0; i < count; ++i) {
    const auto& evt = results[i];
    // evt.epoch_s, evt.type, evt.payload[0..2]
}
```

### Time-Windowed Query

```cpp
// Events from the last hour
uint32_t now = time(nullptr);
EventQuery query;
query.from_epoch = now - 3600;
query.to_epoch   = now;
query.limit      = 50;

DiagnosticEvent results[50];
size_t count = diag.queryEvents(query, results, 50);
```

### Query via REST API

All endpoints require authentication (SPEC §11.4).

```bash
# Get all events (up to 100)
curl -u admin:password http://echenon.local/api/echenon/events

# Get events from a specific time range
curl -u admin:password \
  "http://echenon.local/api/echenon/events?from=1698400000&to=1698500000&limit=20"
```

**Response format:**

```json
{
  "events": [
    {
      "epoch": 1698400000,
      "type": "BOOT",
      "type_id": 0,
      "payload": { "uptime_s": 12345 }
    },
    {
      "epoch": 1698400100,
      "type": "SESSION_START",
      "type_id": 1,
      "payload": { "session_id": 42 }
    },
    {
      "epoch": 1698401800,
      "type": "SESSION_END",
      "type_id": 2,
      "payload": { "duration_s": 1700, "record_count_approx": 1664 }
    }
  ],
  "count": 3,
  "total": 42
}
```

---

## Payload Encoding Reference

The 3-byte payload is type-specific. Decode helpers are provided in
`DiagnosticsRoutes.h` for use in both firmware and native tests:

| Event Type | Decode Function | Fields |
|---|---|---|
| `BOOT` | `decodeBootPayload()` | `uptime_s` (24-bit LE, max 16,777,215 s) |
| `SESSION_START` | `decodeSessionStartPayload()` | `session_id` (low 16-bit LE) |
| `SESSION_END` | `decodeSessionEndPayload()` | `duration_s` (16-bit LE), `record_count_approx` (×64) |
| `RUNAWAY_ALERT` | `decodeRunawayPayload()` | `duration_ms` (×100, 100 ms resolution) |
| `CALIBRATION_DONE` | `decodeCalibrationPayload()` | `calibration_type` (0=initial, 1=recal) |
| `FIRMWARE_CHANGE` | `decodeFirmwarePayload()` | `major`, `minor`, `patch` |

---

## Portal Event Viewer

The event log viewer is available under the **System** tab in the portal.
It displays recent diagnostic events in a sortable table with human-readable
timestamps and decoded payloads.

A runaway warning banner appears in the diagnostics view when a
`RUNAWAY_ALERT` event is present in the recent log, alerting the rider to
check their console (SPEC §5.2).

<!-- TODO: Add screenshot when portal UI is implemented -->

---

## Ring Buffer Capacity

The ring buffer holds **100 events** (800 bytes of PSRAM). At typical usage
rates (1 boot + 1–3 sessions per day), this provides several weeks of
history. When full, the oldest event is silently overwritten.

The buffer is allocated with `heap_caps_calloc(... MALLOC_CAP_SPIRAM)` and
falls back to internal heap if PSRAM is unavailable (e.g., during native
tests).

---

## Testing

DiagnosticsModule and its helpers are tested in the native `env:native`
googletest suite. The test file covers:

- Ring buffer wrap and oldest-first ordering
- Event recording and retrieval via index
- Time-windowed queries with from/to/limit
- All 6 payload encode/decode round-trips
- `diagEventTypeToString()` for all types and unknown values
- `formatPayloadJson()` output format validation
- Capacity clamping and edge cases (empty buffer, single event, full wrap)
