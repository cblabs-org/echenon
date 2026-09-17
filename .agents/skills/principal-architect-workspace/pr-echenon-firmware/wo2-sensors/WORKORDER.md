---
name: echenon-firmware-wo2
description: Handoff instructions for Work Order 2 — Sensor Acquisition
complexity: standard
---

# WO2 — Sensor Acquisition (Cadence + Resistance)

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `CadenceSensor` and `ResistanceADC` — the two hardware input classes that read raw electrical signals from the Echelon EX-3's internal sensors and produce clean RPM and resistance-level values.

### Strict Boundary Box
- **DO NOT** implement power calculation — that is WO3
- **DO NOT** implement BLE, session recording, or web portal code
- **DO NOT** modify `platformio.ini` or `board.h` (those are set in WO1)
- Output is two clean, self-contained classes with no dependencies on other Echenon modules

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §5 (Sensor Acquisition), §4.3 (Pin Assignments), §4.4 (EX-3 Internal Sensors)

### Line-Item Instructions

- [ ] Create `src/echenon/CadenceSensor.h` / `.cpp`:
  - ISR-driven: `attachInterrupt(PIN_CADENCE, FALLING)` in `begin()`
  - ISR stores `micros()` timestamp in `volatile uint32_t`
  - `update()` called from `EchenonModule::tick()`:
    - Compute RPM from inter-pulse interval: `RPM = 60,000,000 / intervalMicros`
    - Clamp to 0–200 RPM range
    - 3-second timeout: if no pulse for 3s, RPM = 0
    - Debounce: ignore pulses <100ms apart (>600 RPM — physically impossible)
  - Getter: `uint16_t getRPM() const`
  - Getter: `bool isActive() const` (true if RPM > 0)

- [ ] Create `src/echenon/ResistanceADC.h` / `.cpp`:
  - `update()` called from `EchenonModule::tick()`:
    - Read 16× `analogReadMilliVolts(PIN_RESISTANCE)` and average (multisampling)
    - Apply EMA filter: `filtered = α × raw + (1-α) × filtered` where α = 0.15
    - Dead zone guard: clamp readings <80mV to 80, >2850mV to 2850
  - `uint16_t getFilteredMV() const` — filtered millivolt reading
  - `uint8_t getResistanceLevel() const` — mapped to 1–32 via calibration range
  - Level mapping uses NVS-stored `cal_mv_min` / `cal_mv_max` with linear interpolation
  - Must use `analogReadMilliVolts()` NOT raw `analogRead()` (ESP32 ADC nonlinearity)

- [ ] Wire both into `EchenonModule`:
  - Add `CadenceSensor` and `ResistanceADC` as member variables
  - Call `.begin()` in `EchenonModule::begin()`
  - Call `.update()` in `EchenonModule::tick()`
  - Add serial debug output: log RPM and resistance level every 2 seconds

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. Serial output should show sensible debug logging format. Before committing, ask the user if they are happy with the changes.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
