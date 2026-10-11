---
name: echenon-firmware-wo5
description: Handoff instructions for Work Order 5 — HRM Client + Repeater
complexity: standard
---

> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-03](../../pr-03-hr-relay/PROJECT.md). **Superseded by:** SPEC §3.5, §8.1–§8.5 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# WO5 — Heart Rate Monitor Client + Repeater

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `HRMClient` (BLE Central that connects to the user's HR strap) and `HRMRepeater` (re-broadcasts HR data as a standard BLE Heart Rate Service). Together they relay HR from the Echelon Beat armband through all three output paths.

### Strict Boundary Box
- **DO NOT** modify `FTMSServer` — just provide HR data to it via shared state
- **DO NOT** implement session recording, web portal, or upload logic
- **DO NOT** implement BLE pairing/scan UI — the Foundation BLEManager provides that
- The HRMClient connects to one device; HRMRepeater re-broadcasts to the Echelon tablet

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §8 (Heart Rate Monitor Relay), §8.1–8.4
- Read the framework-expert skill for `IBLEClientProvider` interface details

### Line-Item Instructions

- [ ] Create `src/echenon/HRMClient.h` / `.cpp`:
  - Implement `IBLEClientProvider` interface:
    - `getTargetServiceUUID()` — return HR Service UUID (`0x180D`)
    - `onDeviceConnected(NimBLEClient*)` — subscribe to HR Measurement characteristic (`0x2A37`)
    - `onDeviceDisconnected()` — log and trigger reconnect
    - `getScanFilter()` — filter for devices advertising `0x180D`
  - Parse HR Measurement notification:
    - Bit 0 of flags byte: 0 = uint8 HR, 1 = uint16 HR
    - Extract BPM value
  - Store latest HR: `volatile uint8_t _lastBPM`
  - Stale data protection: if no notification for 10 seconds, set HR to 0
  - Getter: `uint8_t getBPM() const`
  - Getter: `bool isConnected() const`

- [ ] Create `src/echenon/HRMRepeater.h` / `.cpp`:
  - Implement `IBLEServiceProvider` interface:
    - `onBLEServerReady(NimBLEServer*)` — create Heart Rate Service
    - `getAdvertisedServiceUUIDs()` — return `{0x180D}`
  - Heart Rate Service (`0x180D`):
    - HR Measurement characteristic (`0x2A37`) — Notify
    - Body Sensor Location (`0x2A38`) — Read (value: "Wrist")
  - `update(uint8_t bpm)` — send HR notification (follows strap's native rate, ~1 Hz)
  - This allows the Echelon tablet to see an HR device even though it can't connect to the armband directly (the ESP32 is the Central)

- [ ] Wire into `EchenonModule`:
  - Add `HRMClient` and `HRMRepeater` as members
  - Register `HRMClient` with BLEManager via `IBLEClientProvider`
  - Register `HRMRepeater` with BLEManager via `IBLEServiceProvider`
  - In `tick()`: `hrmRepeater.update(hrmClient.getBPM())`
  - Pass HR to `FTMSServer` via the shared update call: `ftmsServer.update(watts, rpm, hrmClient.getBPM())`

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. Both `IBLEServiceProvider` and `IBLEClientProvider` contracts must be satisfied. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
