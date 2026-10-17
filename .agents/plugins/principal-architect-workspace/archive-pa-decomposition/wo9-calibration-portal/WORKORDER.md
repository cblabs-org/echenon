---
name: echenon-firmware-wo9
description: Handoff instructions for Work Order 9 — Calibration Portal Tab
complexity: standard
---

> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-02](../../pr-02-calibration-power/PROJECT.md) (passive calibration) and [PR-07](../../pr-07-mitm-motor/PROJECT.md) (motor-driven calibration). **Superseded by:** SPEC §6.3 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# WO9 — Calibration Portal Tab

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement the Calibration portal tab — a web UI for potentiometer calibration, power tier selection, community matrix dropdown, custom CSV upload, and reference power meter calibration. Uses the Foundation's IWebContributor + gzip pipeline.

### Strict Boundary Box
- **DO NOT** modify sensor, power engine, BLE, or session code (WO2–WO6)
- **DO NOT** modify the overlay or upload code (WO7–WO8)
- This WO only creates the web UI and REST API endpoints for calibration
- PowerEngine's NVS keys are read/written via ConfigManager — this WO does not change PowerEngine's calculation logic

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §6.3 (Calibration), §6.4 (NVS Storage)
- Read the framework design system skill for UI component patterns
- Read `systems/web-portal/html-js-fragments.md` for gzip+PROGMEM pipeline
- Read `systems/web-portal/save-policy.md` for the four-tier save policy

### Line-Item Instructions

- [ ] Create `web/calibration.js`:
  - Portal tab (standard `tab-changed` contract, IIFE wrapper, `calibration-*` ID prefix)
  - **Section 1 — Potentiometer Calibration:**
    - "Turn knob to minimum → click Set Min" button
    - "Turn knob to maximum → click Set Max" button
    - Live ADC readout (poll `GET /api/calibration/adc` at 2 Hz while tab active)
    - Confirmation: shows min/max mV values after calibration
  - **Section 2 — Power Source Selection:**
    - Dropdown: "Physics Defaults" / "Echelon EX-3 Community Table" / "Custom CSV" / "Reference Power Meter"
    - Selecting changes `cal_power_tier` NVS key
  - **Section 3 — Community Matrix (visible when Tier 2 selected):**
    - Dropdown of available pre-built matrices (fetched from `GET /api/calibration/matrices`)
    - "Upload Custom CSV" file picker (POST multipart to `/api/calibration/matrix/upload`)
  - **Section 4 — Reference Power Meter (visible when Tier 3 selected):**
    - Instructions: "Ride with a reference power meter and record pairs"
    - Table input: 5+ rows of (resistance, cadence, reference watts)
    - "Calculate Coefficients" button → POST data → server fits polynomial → shows result
  - Follow the framework save policy: toast on save, reboot overlay if needed

- [ ] Create `src/echenon/Calibration.h` / `.cpp`:
  - REST API endpoints (registered via EchenonModule's `registerRoutes()`):
    - `GET /api/calibration/adc` — returns `{ "mv": 1234, "level": 16, "rpm": 82 }` (live sensor values)
    - `POST /api/calibration/pot` — body: `{ "action": "set_min" | "set_max" }` → saves `cal_mv_min`/`cal_mv_max` to NVS
    - `GET /api/calibration/matrices` — returns array of available pre-built matrix IDs/names
    - `POST /api/calibration/matrix/select` — body: `{ "id": "echelon-ex3-v1" }` → saves `cal_matrix_id`
    - `POST /api/calibration/matrix/upload` — multipart CSV → saves to `cal_matrix_custom` NVS blob
    - `POST /api/calibration/tier` — body: `{ "tier": 1|2|3 }` → saves `cal_power_tier`
    - `POST /api/calibration/reference` — body: `{ "samples": [...] }` → fits polynomial, saves coefficients
  - `getMaxPostBody()` override: 16384 (CSV uploads can be larger than 4KB default)

- [ ] Wire into `EchenonModule`:
  - Register calibration routes in `registerRoutes()`
  - Register "Calibration" tab in `registerTabs()`: `registry.addTab("Calibration", nullptr, 40, kCalIcon, "/calibration.js")`
  - After calibration changes, call `powerEngine.reload()` to pick up new NVS values

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. The calibration REST API must follow the framework's ADR-15 body size limits. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
