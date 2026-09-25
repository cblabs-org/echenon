> ⚠️ **SUPERSEDED.** This work order was part of the v1/initial programme plan. The programme was restructured on 2026-09-25 per the Chief Architect Design Audit. See the current programme at `../PROGRAM.md` and `../PROGRAM-PLAN.md`.

---
name: echenon-firmware-wo6
description: Handoff instructions for Work Order 6 — Session Recording
complexity: standard
---

# WO6 — Session Recording Engine

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `SessionManager`, `RideStore`, and `TCXGenerator` — the session recording pipeline that auto-detects rides, buffers data points at 2 Hz, and generates TCX files on session end.

### Strict Boundary Box
- **DO NOT** implement cloud upload (Strava) — that is WO8
- **DO NOT** implement web portal UI — that is WO7 (overlay) and WO9 (calibration)
- **DO NOT** modify sensor, power, or BLE code (WO2–WO5)
- Session data stays local — this WO writes TCX to LittleFS only

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §9 (Session Recording), §9.1–9.6

### Line-Item Instructions

- [ ] Create `src/echenon/RideStore.h` / `.cpp`:
  - `RideDataPoint` struct (§9.3): timestamp_ms, cadence_rpm, resistance, power_watts, heart_rate — 10 bytes
  - PSRAM ring buffer: `ps_malloc()` for ~27,000 points (90 min at 2 Hz)
  - `void addPoint(const RideDataPoint& point)`
  - `size_t getCount() const`
  - `const RideDataPoint& getPoint(size_t index) const`
  - `void clear()`
  - `RideSummary getSummary() const` — compute avg/max power, cadence, HR, duration

- [ ] Create `src/echenon/SessionManager.h` / `.cpp`:
  - State machine: Idle → Recording → Paused → Ending → Uploading (§9.2)
  - **Auto-start:** First cadence pulse transitions Idle → Recording
  - **Auto-end:** No cadence for 180 seconds transitions Recording/Paused → Ending
  - **Manual override:** `startSession()` / `stopSession()` methods for the web UI
  - **Pause:** Cadence = 0 during recording → Paused (zero-power points still recorded)
  - `tick()`:
    - In Recording/Paused state: add `RideDataPoint` to `RideStore` at 2 Hz
    - Check auto-end timeout
  - EventBus integration: publish `session/started`, `session/ended`, `session/paused` events
  - On session end: trigger `DataManager::requestPriorityFetch()` for the upload pipeline

- [ ] Create `src/echenon/TCXGenerator.h` / `.cpp`:
  - `bool generate(const RideStore& store, const char* filepath)` — streams XML to LittleFS file
  - **1 Hz trackpoints** — interpolate 2 Hz buffer to 1-per-second output (Xert MPA compatibility)
  - Include zero-watt seconds (rider stopped but session active)
  - XML structure: `<TrainingCenterDatabase>` → `<Activities>` → `<Activity Sport="Biking">` → `<Lap>` → `<Track>` → `<Trackpoint>`
  - Each trackpoint: `<Time>`, `<Cadence>`, `<HeartRateBpm>`, `<Extensions>` → `<TPX>` → `<Watts>`
  - Streaming write — no DOM tree in memory (§9.4)
  - File path: `/sessions/<timestamp>.tcx`

- [ ] Wire into `EchenonModule`:
  - Add `SessionManager`, `RideStore` as members
  - Call `sessionManager.tick()` from `EchenonModule::tick()`
  - Pass current power/cadence/HR/resistance to SessionManager each tick

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. TCX output structure must be valid XML that Strava and Garmin Connect would accept. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
