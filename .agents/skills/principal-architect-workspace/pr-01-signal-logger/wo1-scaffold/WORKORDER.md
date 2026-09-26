---
name: signal-logger-wo1
description: Handoff instructions for Work Order 1 — Project scaffold + framework bump
complexity: standard
---

# WO1 — Project Scaffold + Framework Bump

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Replace the WO0 spike code with the canonical Echenon PlatformIO project
scaffold: `platformio.ini`, `board.h`, partitions, sdkconfig, `EchenonModule`
skeleton, and `main.cpp` — all following the ESP32 App Foundation's consumer
pattern. Bump the framework submodule from `v0.0.12-dev.57` to
`v0.0.12-dev.82` with BLE **off**. Prove the build compiles.

After this WO, the project is a clean, running App Foundation consumer app on
the Waveshare ESP32-S3-Nano with all ten tap pins declared, ready for the
SignalLogger module in WO2.

## Context

- **Mandate:** [PROJECT.md](../PROJECT.md) §§1–3
- **SPEC §4.3:** Hardware configuration architecture (one `board.h` per target)
- **SPEC §4.4.1:** Phase 0/1 pin map (GPIO 1–10, all ADC1)
- **SPEC §12.1:** Project structure
- **SPEC §12.2:** `platformio.ini` — the full canonical content is in the SPEC
- **SPEC §12.3:** `board.h` and build flags — the full canonical content is in the SPEC
- **Framework submodule:** `framework/` — currently at `0268974a` (`v0.0.12-dev.57-19`);
  bump to tag `v0.0.12-dev.82`
- **Template app:** `framework/template-app/` — the consumer pattern to follow
- **Board contract:** `framework/hardware/_common/board_contract.h`
- **Board template:** `framework/hardware/_TEMPLATE/board.h`
- **WO0 spike findings (informational only):** ADC-continuous at 8.3 kHz/ch
  works; ISR + ADC coexist with `gpio_input_enable()`; Ch B is ~1,444 mV
  (below V_IH). These don't change the scaffold, but WO2 will use them.

## Strict Boundary Box

- **IN SCOPE:** `platformio.ini`, `sdkconfig.defaults` (project root),
  `hardware/ex3-waveshare-nano/` (create: `board.h`, `partitions.csv`,
  `sdkconfig.defaults`), `src/main.cpp`, `src/echenon/EchenonModule.h`,
  `src/echenon/EchenonModule.cpp`, `.gitignore`, `.gitmodules` / framework
  submodule pointer, `CMakeLists.txt`, `src/CMakeLists.txt`.
- **OUT OF SCOPE:** `SignalLogger`, dashboard, web assets, `test/`, any file
  under `docs/` (except the evidence already committed by WO0), any file
  under `.agents/` (except reading the mandate). Do not touch `SPEC.md`.
- **Do not add BLE.** No `${ble.*}` interpolation, no `sdkconfig.ble.defaults`
  in the chain, no `WITH_BLE`, no NimBLE dependency.
- **Do not add application `WITH_*` flags** (`WITH_HR_RELAY`, `WITH_FTMS`,
  `WITH_SESSION`, `WITH_STRAVA`, `WITH_OTA`, `WITH_MOTOR_CONTROL`, `WITH_ERG`).
  Those are added by later PRs.
- **Do not set `WITH_DISPLAY_U8G2` or `WITH_BUTTONS`.** GPIO 8, 9, 10 are
  tap pins (SPEC §12.3).
- **Do not create additional branches.** All work stays on
  `signal-logger/wo1-scaffold`.

## Tooling Constraints

- Use the `framework-expert` skill for any framework integration questions
  (Service Module Pattern, `registerModule`, `ITickable`, `IWebContributor`).
- Use the `pio-manager` skill and PlatformIO MCP tools for building. Read
  `pio-manager/SKILL.md` before your first build call.
- Build with `pio run -e ex3-waveshare-nano`. The build must succeed cleanly.
- Also verify that `pio run -e native` compiles (it will have no tests yet,
  but the environment must parse without error).

## Line-Item Instructions

### A. Framework submodule bump

- [ ] Bump the framework submodule to `v0.0.12-dev.82`:
  ```bash
  cd framework && git fetch --tags && git checkout v0.0.12-dev.82 && cd ..
  ```
- [ ] Verify the checkout: `cd framework && git describe --tags --exact-match`
  should output `v0.0.12-dev.82`.
- [ ] Stage the submodule pointer: `git add framework`.

### B. `platformio.ini` (replace the spike version)

- [ ] Replace the existing spike `platformio.ini` with the canonical version
  from **SPEC §12.2**. The SPEC contains the complete, authoritative
  `platformio.ini` content — copy it exactly, with these Phase 0 modifications:
  - The `[env:ex3-waveshare-nano]` section must **NOT** include
    `${ble.build_flags}`, `${ble.lib_deps}`, or `sdkconfig.ble.defaults` in
    the cmake args chain. Comment them out with a note saying "PR-03 turns
    BLE on".
  - The `build_flags` for the target env must **NOT** include any of:
    `-D WITH_OTA`, `-D WITH_HR_RELAY`, `-D WITH_FTMS`, `-D WITH_SESSION`,
    `-D WITH_STRAVA`. Comment them out with the PR that adds each.
  - The `lib_deps` for the target env must **NOT** include `${ble.lib_deps}`.
  - Keep the `[env:native]` section exactly as in the SPEC.
  - Keep the `[ble]` mix-in section — it is a declaration, not a usage. It
    will be referenced by `[env:ex3-waveshare-nano]` in PR-03.

### C. `hardware/ex3-waveshare-nano/board.h`

- [ ] Create `hardware/ex3-waveshare-nano/board.h` exactly as specified in
  **SPEC §12.3** (the complete canonical `board.h` content is there). It
  includes:
  - `#pragma once` and `#include "board_pins.h"`
  - `BOARD_NAME`, `BOARD_ID_STRING`, `HAS_PSRAM`
  - All `HAS_*` flags and pin constants from SPEC §4.4.1
  - `HAS_MOTOR_CONTROL 0`
  - No display pin block, no button block
  - The comment noting `WITH_DISPLAY_U8G2` and `WITH_BUTTONS` are forbidden

### D. `hardware/ex3-waveshare-nano/partitions.csv`

- [ ] Create a partition table for 16 MB flash. Use the framework template's
  partition table as a reference (`framework/template-app/` or
  `framework/hardware/esp32s3nano/partitions.csv` if it exists), but sized
  for 16 MB flash with adequate LittleFS space (~2.75 MiB). A typical layout:
  ```csv
  # Name,   Type, SubType, Offset,  Size,     Flags
  nvs,      data, nvs,     0x9000,  0x5000,
  otadata,  data, ota,     0xe000,  0x2000,
  app0,     app,  ota_0,   0x10000, 0x640000,
  app1,     app,  ota_1,   0x650000,0x640000,
  spiffs,   data, spiffs,  0xC90000,0x2C0000,
  coredump, data, coredump,0xF50000,0x10000,
  ```
  Adjust if needed — the key is: two OTA slots, LittleFS ≥ 2.75 MiB,
  NVS, and coredump. The total must not exceed 16 MB (0x1000000).

### E. `hardware/ex3-waveshare-nano/sdkconfig.defaults`

- [ ] Create target-specific SDK config overrides. Merge the project-root
  `sdkconfig.defaults` (from WO0 — PSRAM, flash, USB console) into this
  target file. Add any target-specific settings. The project-root
  `sdkconfig.defaults` will be updated to hold only project-level defaults.
- [ ] The cmake args in `platformio.ini` chain:
  `sdkconfig.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults`
  (no BLE defaults in Phase 0).

### F. `sdkconfig.defaults` (project root)

- [ ] Update the project-root `sdkconfig.defaults` to hold project-level
  defaults only (things common to all possible targets). Target-specific
  settings (PSRAM type, flash size, USB console) move to the target's file.
  Keep `CONFIG_AUTOSTART_ARDUINO=y` and `CONFIG_ESP_MAIN_TASK_STACK_SIZE`.

### G. `src/main.cpp`

- [ ] Replace the spike `src/main.cpp` with a clean App Foundation consumer
  entry point following `framework/template-app/src/main.cpp`. Specifically:
  ```cpp
  #include "AppFoundation.h"
  #include <Arduino.h>
  #include "echenon/EchenonModule.h"

  Application app;
  EchenonModule echenonModule;

  void setup() {
      Serial.begin(115200);
      app.setAppName("Echenon");

      // Wire dependencies
      echenonModule.setConfigManager(&app.getConfigManager());

      // Register — compile-time introspection detects implemented interfaces
      app.registerModule(&echenonModule);

      FrameworkConfig config;
      app.begin(config);
  }

  void loop() {
      app.tick();
      delay(1);  // Yield to RTOS idle task — feeds the TWDT
  }
  ```
  - No `#ifdef WITH_OTA`, `WITH_BLE`, `WITH_DISPLAY_*`, `WITH_BUTTONS`,
    `WITH_MCP`, `WITH_TIME` guards — those modules are not compiled in PR-01.
  - No display, no HA, no MCP.

### H. `src/echenon/EchenonModule.h` and `EchenonModule.cpp`

- [ ] Create the `EchenonModule` class following the **Service Module Pattern**
  from the `framework-expert` skill. It should implement at minimum:
  - `ITickable` (for the main loop tick)
  - `IConfigurable` (for configuration persistence)
  - `IWebContributor` (for portal tab registration — the dashboard comes in WO3)
  The skeleton should compile but do minimal work. Leave TODO comments where
  WO2 (SignalLogger) and WO3 (dashboard) will plug in.
- [ ] Include `setConfigManager(ConfigManager*)` for dependency injection.
- [ ] The module name string should be `"echenon"`.

### I. `CMakeLists.txt` and `src/CMakeLists.txt`

- [ ] Update the root `CMakeLists.txt` project name from `adc-spike` to
  `echenon`.
- [ ] Update `src/CMakeLists.txt` — keep `GLOB_RECURSE` for sources, ensure
  `REQUIRES` includes the IDF components the framework needs (at minimum
  `esp_adc driver`; check the framework's template-app or sample-app for
  the full list).

### J. `.gitignore`

- [ ] Ensure `tmp/` is in `.gitignore` (per rule 02).
- [ ] Ensure standard PlatformIO and build artifacts are ignored.

### K. Build verification

- [ ] Run `pio run -e ex3-waveshare-nano` — must compile cleanly with 0
  errors. Warnings from the framework are acceptable; warnings from Echenon
  source code are not.
- [ ] Flash to the board and confirm it boots to the framework's setup AP
  (or connects to known WiFi). This proves the scaffold, the bumped
  submodule, and the board.h all work together.

## Verification Requirement

- `pio run -e ex3-waveshare-nano` compiles with 0 errors.
- The firmware boots on the Waveshare ESP32-S3-Nano and reaches the
  framework's WiFi configuration portal.
- Before committing, ask the user if they are happy with the changes.

## Issue Surfacing

If you discover a defect, upstream bug, or concern *outside your boundary box*,
surface it in your Completion Report below — do not file issues directly. The
Principal Architect will triage and file them under the correct author identity.

---

## Completion Report (written by the worker at task completion)

<!-- Append the completion report here when done. Use this template: -->

<!--
### Status
complete | partial | blocked

### Changes Made
- `path/to/file` — what changed and why

### Build Output
- `pio run -e ex3-waveshare-nano`: PASS/FAIL (errors/warnings)
- Boot test: PASS/FAIL (description)

### Decisions Made
(list any non-obvious decisions)

### Dead Ends & Warnings
(things that didn't work)

### Out-of-Scope Findings
(defects or concerns outside the boundary box)

### Blockers
(none if status is 'complete')
-->
