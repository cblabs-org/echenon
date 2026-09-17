---
name: echenon-firmware-wo4
description: Handoff instructions for Work Order 4 — BLE FTMS Server
complexity: standard
---

# WO4 — BLE FTMS Server

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `FTMSServer` — the BLE Fitness Machine Service that broadcasts Indoor Bike Data notifications at 2 Hz. This is the first BLE module, so this WO also adds NimBLE to the build and configures the BLE build flags.

### Strict Boundary Box
- **DO NOT** implement HRM client/repeater — that is WO5
- **DO NOT** implement session recording, web portal, or upload logic
- **DO NOT** modify sensor or power calculation code (WO2/WO3)
- FTMSServer receives power/cadence/HR values — it does not read sensors

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §7 (BLE FTMS Server), §7.2–7.7
- Read the framework-expert skill references for `IBLEServiceProvider` interface
- Reference MIT-licensed FTMS GATT scaffolding: [esp32-ftms-server](https://github.com/jamesjmtaylor/esp32-ftms-server)

### Line-Item Instructions

- [ ] Update `platformio.ini`:
  - Add `h2zero/NimBLE-Arduino@^2.1.0` to `lib_deps`
  - Add `-DWITH_BLE` to build flags (if not already present)
  - Add NimBLE configuration flags: `-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=4`

- [ ] Create `src/echenon/FTMSServer.h` / `.cpp`:
  - Implement `IBLEServiceProvider` interface:
    - `onBLEServerReady(NimBLEServer*)` — create FTMS service and characteristics
    - `getAdvertisedServiceUUIDs()` — return `{0x1826}` (FTMS UUID)
    - `onClientConnected(uint16_t connHandle)` — log connection
    - `onClientDisconnected(uint16_t connHandle)` — log disconnection
  - GATT service structure per §7.3:
    - Fitness Machine Service (`0x1826`)
    - Fitness Machine Feature (`0x2ACC`) — Read
    - Indoor Bike Data (`0x2AD2`) — Notify @ 2 Hz
    - Fitness Machine Status (`0x2ADA`) — Notify
    - Device Information Service (`0x180A`) — Manufacturer, Model, Firmware
  - `update(int16_t watts, uint16_t rpm, uint8_t hr)`:
    - Assemble Indoor Bike Data packet per §7.4 (11-byte structure with flags, speed, cadence, resistance, power, HR)
    - Speed derived from cadence: `speed = rpm × 10` (0.01 km/h units)
    - Send notification if any client is subscribed
  - Notification timer: only notify at 2 Hz max (500ms minimum interval)
  - Must use legacy BLE 4.x advertising (fitness apps don't support extended)

- [ ] Wire into `EchenonModule`:
  - Add `FTMSServer` as member
  - Register with BLEManager via `IBLEServiceProvider`
  - Call `ftmsServer.update(watts, rpm, hr)` from `tick()` at 2 Hz

- [ ] Build — must compile with zero errors (first BLE compilation)

### Verification Requirement
Build must compile cleanly with NimBLE. The FTMS GATT service structure must match the Bluetooth SIG FTMS v1.0 specification. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
