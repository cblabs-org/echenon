---
name: echenon-firmware-wo1
description: Handoff instructions for Work Order 1 — Project Scaffold
complexity: mechanical
---

> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-01](../../pr-01-signal-logger/PROJECT.md). **Superseded by:** SPEC §4.3, §4.4.1 (Phase 0/1 pin map), §12.1–§12.3 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).
>
> 🚫 **Do not follow this layout.** This WO prescribes `[env:echenon]` extending `env:esp32s3nano`, `hardware/echenon/board.h` and `src/echenon/Config.h`. The current layout is env `ex3-waveshare-nano`, an Echenon-declared base env following `framework/template-app/platformio.ini`, and one target directory `hardware/ex3-waveshare-nano/` (`board.h` built from `board_contract.h` and `_TEMPLATE`, `partitions.csv`, `sdkconfig.defaults`) with no `config.h`, no `WITH_DISPLAY_U8G2` and no `WITH_BUTTONS`. See PR-01 §3 and SPEC §12.

# WO1 — Project Scaffold & Clean Build

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Set up the PlatformIO project structure, board configuration, partition table, and a minimal `main.cpp` that boots the ESP32 App Foundation with a stub `EchenonModule`. The build must compile clean with zero warnings.

### Strict Boundary Box
- **DO NOT** implement any sensor reading, BLE, session, or upload logic
- **DO NOT** modify the `framework/` submodule
- **DO NOT** create web portal JS files — those come in later WOs
- **DO NOT** add NimBLE library yet — BLE comes in WO4
- The `EchenonModule` in this WO is a skeleton: `begin()`, `tick()`, config stubs — no real logic

### Tooling Constraints
- Use the PlatformIO MCP server tools for build verification (`build_project`)
- Use `check_project` to validate the platformio.ini
- Read the framework's `platformio.ini` (`framework/platformio.ini`) to understand the `env:esp32s3nano` base environment

### Line-Item Instructions

- [ ] Copy `SPEC.md` from the Antigravity artifacts directory to `Echenon/SPEC.md`
  - Source: [`SPEC.md`](../../../../../SPEC.md) (originally drafted outside the repo)
- [ ] Create `platformio.ini` per §12.2 of the spec:
  - `[env:echenon]` extending `env:esp32s3nano`
  - Board partitions, sdkconfig, build flags as specified
  - **Do NOT add `h2zero/NimBLE-Arduino` yet** — leave BLE deps for WO4
- [ ] Create `hardware/echenon/board.h` per §12.3:
  - Pin definitions: `PIN_CADENCE` (GPIO 5), `PIN_RESISTANCE` (GPIO 1)
  - ADC config: `ADC_SAMPLES 16`, `ADC_MV_MIN 80`, `ADC_MV_MAX 2850`
  - Board identity macros
- [ ] Create `hardware/echenon/partitions.csv`:
  - Must include: nvs (24KB), otadata (8KB), app0 (3MB), app1 (3MB), spiffs/littlefs (4MB+)
  - Total must fit in 16MB flash
- [ ] Create `hardware/echenon/sdkconfig.defaults`:
  - PSRAM enable, WiFi/BLE coexistence, task watchdog config
- [ ] Create `src/echenon/Config.h`:
  - Central constants: firmware version, sensor sample rates, session timeouts, FTMS UUIDs
  - All values from the spec (2 Hz FTMS, 20 Hz ADC, 3-min auto-end, etc.)
- [ ] Create `src/echenon/EchenonModule.h` / `.cpp`:
  - Skeleton module implementing `ITickable`, `IConfigurable`
  - `begin(Application&)` — extract narrow refs per ADR-39, log startup
  - `tick()` — empty (just returns)
  - `getConfigSchema()` / `applyConfig()` — return empty JSON for now
- [ ] Create `src/main.cpp`:
  - `#include "AppFoundation.h"` and `"echenon/EchenonModule.h"`
  - Static `EchenonModule echenonModule;` (ADR-16)
  - `setup()`: `app.registerModule(echenonModule); app.begin();`
  - `loop()`: `app.tick();`
- [ ] Build with PlatformIO — must compile with zero errors
- [ ] Create `README.md` with project name, description, build instructions, and license badge

### Verification Requirement
The project MUST compile cleanly via `pio run -e echenon`. Before committing, ask the user if they are happy with the changes.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly. The Principal Architect will triage and file them under the correct author identity.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
