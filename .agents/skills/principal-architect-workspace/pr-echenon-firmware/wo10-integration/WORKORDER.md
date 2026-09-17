---
name: echenon-firmware-wo10
description: Handoff instructions for Work Order 10 — End-to-End Integration
complexity: architect
---

# WO10 — End-to-End Integration & Verification

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Wire all components together in `EchenonModule`, verify the complete firmware compiles and conforms to all framework architectural rules, run the coupling audit, and prepare the project for desk testing.

### Strict Boundary Box
- **DO NOT** add new features or modules
- **DO NOT** modify the algorithm or logic of individual components
- This WO is integration-only: wiring, ordering, compliance, and documentation
- You MAY refactor component interfaces if needed to make them compose cleanly

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Use the framework-expert skill to verify compliance:
  - `references/architecture.md` — ADR-16 (static scope), boot phases
  - `coding-patterns/service-module-pattern.md` — coupling audit
  - `references/registration.md` — registration lifecycle
- Reference: SPEC.md §3.4 (Module Decomposition), §3.5 (Foundation Integration), §13 (Testing)

### Line-Item Instructions

- [ ] Finalize `src/echenon/EchenonModule.h` / `.cpp`:
  - All 11 sub-components as member variables (static scope — no `new`)
  - `begin(Application&)`:
    1. Extract narrow refs per ADR-39 (EventBus*, ConfigManager*, DataManager*)
    2. Initialize all sub-components in dependency order:
       - CadenceSensor, ResistanceADC → PowerEngine → FTMSServer, HRMClient, HRMRepeater → SessionManager → SessionOverlay, UploadManager, StravaUploader
    3. Register BLE providers: `bleManager.registerServiceProvider(ftmsServer)`, etc.
    4. Register IDataSource: `dataManager.registerSource(stravaUploader)`
    5. Register web contributors for calibration, session, upload tabs
    6. Subscribe to EventBus topics: `session/ended`, `ble/device/*`
  - `tick()`:
    1. CadenceSensor::update() + ResistanceADC::update() — <2ms total
    2. PowerEngine::calculate() — <1ms
    3. FTMSServer::update() at 2 Hz (500ms throttle)
    4. HRMRepeater::update() (follows strap rate)
    5. SessionManager::tick() (state machine + buffer write at 2 Hz)
    6. SessionOverlay::pushData() (WebSocket at 2 Hz, only if clients connected)
    7. Total tick budget: verify <10ms (ADR-17)
  - `getConfigSchema()` + `applyConfig()`: aggregate all sub-component config
  - `registerRoutes()` + `registerTabs()`: aggregate all web contributors

- [ ] Run coupling audit — verify against §3.4 table:
  - [ ] No stored `Application*` member
  - [ ] No polling of application state in `tick()`
  - [ ] No sibling module concrete references
  - [ ] Orchestration stays in EchenonModule (composition root)
  - [ ] EventBus for all inter-module notifications

- [ ] Verify ADR-16 compliance:
  - All module instances in static/global scope in `main.cpp`
  - No heap-allocated modules (`new`/`make_unique` for modules)

- [ ] Verify secret registration:
  - Strava OAuth tokens registered with `Redactor::instance().registerSecret()`
  - No tokens appear in serial log output

- [ ] Create/update `README.md`:
  - Project description, architecture overview
  - Build instructions: `pio run -e echenon`
  - Flash instructions: `pio run -e echenon -t upload`
  - First-time setup instructions (matching Journey 5)
  - Wiring diagram reference (link to SPEC.md §4)
  - License + attribution

- [ ] Full build verification:
  - `pio run -e echenon` — zero errors, zero warnings
  - Check flash size fits within 16MB partition scheme
  - Check DRAM/IRAM usage (must have >40KB free DRAM for WiFi+BLE stack)

- [ ] Document known limitations (§13.4):
  - Power accuracy depends on calibration quality
  - Speed is synthetic
  - No ERG mode / resistance control (v1)
  - Single HR device at a time

### Verification Requirement
Complete firmware must compile cleanly. All coupling audit checks must pass. README must be comprehensive enough for a new developer to clone, build, and flash. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
