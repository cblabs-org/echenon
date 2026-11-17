---
schema_version: 1
wo_id: WO-09.2
pr_id: PR-09
title: File Headers & Doxygen House Style — Core, Sensors, Power & Rig
complexity: mechanical
status: COMPLETED
allowed_files:
- src/main.cpp
- src/echenon/CadenceSensor.h
- src/echenon/CadenceSensor.cpp
- src/echenon/CalibrationRig.h
- src/echenon/CalibrationRig.cpp
- src/echenon/CalibrationWizard.h
- src/echenon/CalibrationWizard.cpp
- src/echenon/DiagnosticsModule.h
- src/echenon/DiagnosticsModule.cpp
- src/echenon/DiagnosticsRoutes.h
- src/echenon/DiagnosticsRoutes.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/echenon/LevelMapping.h
- src/echenon/LevelMapping.cpp
- src/echenon/LiveFeed.h
- src/echenon/LiveFeed.cpp
- src/echenon/PowerEngine.h
- src/echenon/PowerEngine.cpp
- src/echenon/PowerTable.h
- src/echenon/PowerTableDefault.h
- src/echenon/QuadratureGenerator.h
- src/echenon/QuadratureGenerator.cpp
- src/echenon/RecalibrationUI.h
- src/echenon/RecalibrationUI.cpp
- src/echenon/ResistanceSensor.h
- src/echenon/ResistanceSensor.cpp
- src/echenon/RunawayDetector.h
- src/echenon/RunawayDetector.cpp
- src/echenon/SignalLogger.h
- src/echenon/SignalLogger.cpp
- src/echenon/SignalLogic.h
forbidden_files:
- framework/**
- web/**
- docs/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: b5b2dd064ac442f1e6f33e61de4fe2177e2eb915994d8a7359c29d5cf569647b
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_6184086abe12
  commit_sha: 450fb751dd979bf0855d16ac1bffb692ca6c567f
  files_modified:
  - src/echenon/CadenceSensor.cpp
  - src/echenon/CadenceSensor.h
  - src/echenon/CalibrationRig.cpp
  - src/echenon/CalibrationRig.h
  - src/echenon/CalibrationWizard.cpp
  - src/echenon/CalibrationWizard.h
  - src/echenon/DiagnosticsModule.cpp
  - src/echenon/DiagnosticsModule.h
  - src/echenon/DiagnosticsRoutes.cpp
  - src/echenon/DiagnosticsRoutes.h
  - src/echenon/EchenonModule.cpp
  - src/echenon/EchenonModule.h
  - src/echenon/LevelMapping.cpp
  - src/echenon/LevelMapping.h
  - src/echenon/LiveFeed.cpp
  - src/echenon/LiveFeed.h
  - src/echenon/PowerEngine.cpp
  - src/echenon/PowerEngine.h
  - src/echenon/PowerTable.h
  - src/echenon/PowerTableDefault.h
  - src/echenon/QuadratureGenerator.cpp
  - src/echenon/QuadratureGenerator.h
  - src/echenon/RecalibrationUI.cpp
  - src/echenon/RecalibrationUI.h
  - src/echenon/ResistanceSensor.cpp
  - src/echenon/ResistanceSensor.h
  - src/echenon/RunawayDetector.cpp
  - src/echenon/RunawayDetector.h
  - src/echenon/SignalLogger.cpp
  - src/echenon/SignalLogger.h
  - src/echenon/SignalLogic.h
  - src/main.cpp
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:02.796 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-09.2 — File Headers & Doxygen House Style — Core, Sensors, Power & Rig

## 1. Objective

Bring all 32 core, sensor, power, calibration, diagnostics, and signal-logger source files in `allowed_files` into full compliance with Dimension 1 (House Style & File Headers) of the Framework Release Audit:

1. Ensure every `.h` and `.cpp` file in `allowed_files` has a canonical Apache-2.0 file header block (`Echenon (c) 2026 CB Labs`, `SPDX-License-Identifier: Apache-2.0`, `Module: <repo-relative-path>`, `Description: ...`, and MIT reference attribution preserved on `LevelMapping.*`, `PowerEngine.*`, `PowerTable*` per `SPEC.md` §2.3).
2. Add missing Doxygen `/** @brief ... */` or `///` docstrings on undocumented public classes/methods and inline `///<` comments on undocumented `constexpr` / `static const` module constants.
3. Zero functional or runtime behaviour changes.

## 2. Strict Boundary Box

- **Allowed files (32 exact files):** `src/main.cpp` and the 31 files in `src/echenon/` listed in frontmatter `allowed_files`.
- **Forbidden files:** `framework/**`, `web/**`, `docs/**`, and all BLE/session/sink files reserved for `WO-09.3`.
- **Comment-only discipline:** Do NOT alter C++ logic, types, signatures, control flow, or existing technical comments.
- **Shell hygiene:** Never use bash `$VAR` loops or `/tmp` in `run_command`. Use file tools (`read_file`/`edit_file`) or short Python scripts without `$` variables if batch-checking headers.
- **PlatformIO MCP:** Do not run `pio` directly in `run_command`; use `call_mcp_tool` (`platformio-mcp_platformio`) if testing before `~/.local/bin/pa wo verify WO-09.2`.

## 3. Technical Specification

1. Canonical file header format (placed below `#pragma once` or at line 1 of `.cpp` / `src/main.cpp` — preserve any existing `@file`/`@brief` info by folding into `Description:`):
   ```cpp
   /*
    * Echenon (c) 2026 CB Labs
    * SPDX-License-Identifier: Apache-2.0
    * Module: src/echenon/<FileName>
    * Description: <Concise 1-2 line description of the module's role.>
    */
   ```
   For `PowerEngine.*`, `LevelMapping.*`, `PowerTable.h`, and `PowerTableDefault.h`, preserve or append the `SPEC.md` §2.3 third-party MIT reference notice (acknowledging `ptx2/gymnasticon`, `cagnulein/qdomyos-zwift`, `lefty01/ESP32_TTGO_FTMS` where applicable) inside the header block.
2. In headers (`.h`), add `/** @brief ... */` / `///` comments for any undocumented public class or method, and `///<` inline comments on any undocumented numeric constants (`constexpr`, `static constexpr`, enum values).
3. Keep formatting clean and idiomatic; do not disturb `#pragma once` or `#include` ordering.

## 4. Verification

1. Verify every file in `allowed_files` contains `SPDX-License-Identifier: Apache-2.0` and `Module:`.
2. Verify `git diff --unified=0` shows only comment/header additions (no code logic edits).
3. Via `call_mcp_tool` (`platformio-mcp_platformio`), optionally build `ex3-waveshare-nano` to ensure zero accidental header syntax typos (for instance if editing files not compiled under `env:native` such as `src/main.cpp`, `EchenonModule.*`, `CalibrationRig.*`, `LiveFeed.*`, `DiagnosticsRoutes.*`, `SignalLogger.*`, `QuadratureGenerator.*` — **run `build_project` on `ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, and `ex3-waveshare-nano-rig` before `pa wo verify`**!).
4. Run `~/.local/bin/pa wo verify WO-09.2` and post completion summary to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
