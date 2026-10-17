---
name: echenon-firmware-wo3
description: Handoff instructions for Work Order 3 — Power Calculation Engine
complexity: standard
---

> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-02](../../pr-02-calibration-power/PROJECT.md). **Superseded by:** SPEC §6.1–§6.4 (angle-indexed power model, as revised at DR-1) in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# WO3 — Power Calculation Engine

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `PowerEngine` — the module that takes resistance level + cadence RPM and calculates instantaneous power in watts. Supports three calibration tiers: physics defaults, community power matrix tables with bilinear interpolation, and reference power meter coefficients.

### Strict Boundary Box
- **DO NOT** implement BLE, session recording, web portal, or upload logic
- **DO NOT** modify CadenceSensor or ResistanceADC (those are done in WO2)
- **DO NOT** implement the calibration UI — that is WO9
- PowerEngine takes input values and produces watts — it does not read sensors directly

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §6 (Power Calculation Engine), §6.3 (Calibration)

### Line-Item Instructions

- [ ] Create `src/echenon/PowerEngine.h` / `.cpp`:
  - `void begin()` — load active tier and coefficients from NVS
  - `int16_t calculate(uint8_t resistanceLevel, uint16_t cadenceRPM)` — main entry point
  - **Tier 1 (physics defaults):**
    - Bivariate polynomial: `P = A(R) × RPM² + B(R) × RPM`
    - Where `A(R) = a0 + a1×R` and `B(R) = b0 + b1×R`
    - Default coefficients: `a0=0.0005, a1=0.00015, b0=0.01, b1=0.005`
    - Stored in NVS keys `cal_a0` through `cal_b1`
  - **Tier 2 (community power matrix):**
    - Bilinear interpolation over a 32×31 lookup table
    - Resistance 1–32 × cadence 0–150 RPM in 5 RPM bins
    - Pre-built tables stored as `const PROGMEM` arrays
    - Custom CSV tables stored as NVS blob
  - **Tier 3 (reference power meter):**
    - Same polynomial as Tier 1 but with user-derived coefficients
  - Active tier selected by NVS key `cal_power_tier` (1, 2, or 3)
  - Zero guard: if cadence = 0, return 0 watts regardless of tier
  - Negative guard: clamp output to ≥ 0

- [ ] Create `src/echenon/PowerMatrix.h`:
  - Define the `PowerMatrixTable` struct: `{ const char* id; const char* name; const int16_t data[32][31]; }`
  - Create one placeholder EX-3 matrix with id `"echelon-ex3-v1"` and name `"Echelon EX-3 Community Table"`
  - Populate with physics-model-derived values (these will be replaced with real community data later)
  - Bilinear interpolation function: `int16_t interpolate(const PowerMatrixTable& table, uint8_t resistance, uint16_t rpm)`

- [ ] Wire into `EchenonModule::tick()`:
  - Call `powerEngine.calculate(resistance.getResistanceLevel(), cadence.getRPM())`
  - Store result for downstream consumers (FTMS, session, overlay)
  - Add serial debug: log calculated watts alongside RPM and resistance

- [ ] NVS integration:
  - Load/save calibration tier and coefficients via ConfigManager
  - Default to Tier 1 if no NVS data exists

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. Verify that `calculate(16, 80)` produces a reasonable wattage (~150W) with default Tier 1 coefficients. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
