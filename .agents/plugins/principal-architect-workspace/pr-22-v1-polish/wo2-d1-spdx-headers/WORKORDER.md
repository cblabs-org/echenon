---
schema_version: 1
wo_id: WO-22.2
pr_id: PR-22
title: D1 Apache-2.0 SPDX License Headers
complexity: standard
status: COMPLETED
allowed_files:
- test/test_native/
- tools/ble-test/control-peripheral/src/main.cpp
forbidden_files:
- src/**
- web/**
- docs/**
- framework/**
- platformio.ini
- hardware/**
- SPEC.md
- README.md
- CHANGELOG.md
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 04dd711ea1b82ada51cee1cde2e3f0a1c8fa2b1bd990dbc4d84b329a14993cd8
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_8b5e000fb145
  commit_sha: 1fa754c06e5b6b6698ae2e9d1b3deb23c1b1c3a6
  files_modified:
  - test/test_native/test_cadence_sensor.cpp
  - test/test_native/test_diagnostics_module.cpp
  - test/test_native/test_diagnostics_routes.cpp
  - test/test_native/test_ftms_server.cpp
  - test/test_native/test_hr_encoding.cpp
  - test/test_native/test_hr_parsing.cpp
  - test/test_native/test_intervals_uploader.cpp
  - test/test_native/test_level_mapping.cpp
  - test/test_native/test_power_engine.cpp
  - test/test_native/test_quadrature_generator.cpp
  - test/test_native/test_recalibration.cpp
  - test/test_native/test_ride_persistence.cpp
  - test/test_native/test_runaway_detector.cpp
  - test/test_native/test_session_manager.cpp
  - test/test_native/test_signal_logger.cpp
  - test/test_native/test_signal_logic.cpp
  - test/test_native/test_strava_uploader.cpp
  - test/test_native/test_tcx_generator.cpp
  - test/test_native/test_upload_queue.cpp
  - test/test_native/test_wahoo_uploader.cpp
  - tools/ble-test/control-peripheral/src/main.cpp
  verification_summary: 'pio test -e native --verbose: ================ 271 test cases:
    271 succeeded in 00:00:00.478 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-22.2 — D1 Apache-2.0 SPDX License Headers

## 1. Role & Objective

Close the Dimension 1 (`D1`) advisory in `docs/release-audit-v0.3.0-beta.1.md` by prepending the canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` header block to all 20 native unit test files in `test/test_native/*.cpp` and `tools/ble-test/control-peripheral/src/main.cpp`.

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only modify `test/test_native/*.cpp`, `tools/ble-test/control-peripheral/src/main.cpp`, and Part II (`## Completion Report` + `completion:` frontmatter) of this `WORKORDER.md`.
2. **Header Format (`release-audit-checks.sh` `check_missing_file_header`):** Each file must contain within its first 30 lines:
   ```cpp
   /*
    * Echenon (c) 2026 CB Labs
    * SPDX-License-Identifier: Apache-2.0
    * Module: <repo-relative-path>
    * Description: <concise file-specific description>
    */
   ```
3. **Zero Functional Changes:** Preserve all existing `/// @file`, `/// @brief`, and `/** ... */` docblocks, `#include` order, and test/firmware logic verbatim.

**Forbidden:** Do NOT edit `src/**`, `web/**`, `docs/**`, `framework/**`, `platformio.ini`, `hardware/**`, `SPEC.md`, `README.md`, or `CHANGELOG.md`.

## 3. Line-Item Instructions

- [ ] Prepend the canonical Apache-2.0 header block (`Echenon (c) 2026 CB Labs`, `SPDX-License-Identifier: Apache-2.0`, `Module:`, `Description:`) to all 20 `.cpp` files in `test/test_native/`:
  - `test_cadence_sensor.cpp`, `test_diagnostics_module.cpp`, `test_diagnostics_routes.cpp`, `test_ftms_server.cpp`, `test_hr_encoding.cpp`, `test_hr_parsing.cpp`, `test_intervals_uploader.cpp`, `test_level_mapping.cpp`, `test_power_engine.cpp`, `test_quadrature_generator.cpp`, `test_recalibration.cpp`, `test_ride_persistence.cpp`, `test_runaway_detector.cpp`, `test_session_manager.cpp`, `test_signal_logger.cpp`, `test_signal_logic.cpp`, `test_strava_uploader.cpp`, `test_tcx_generator.cpp`, `test_upload_queue.cpp`, `test_wahoo_uploader.cpp`.
- [ ] Prepend/integrate the canonical Apache-2.0 header block into `tools/ble-test/control-peripheral/src/main.cpp` while preserving its existing `/** ... */` description block.
- [ ] Run mechanical checks:
  - `grep -rnL "SPDX-License-Identifier: Apache-2.0" test/test_native/ tools/ble-test/control-peripheral/src/main.cpp` returns 0 files.
  - Verify `head -n 30` on all 21 files contains both `Module:` and `Description:` (matching `check_missing_file_header` in `framework/.agents/skills/framework-expert/scripts/release-audit-checks.sh`).

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pa wo verify WO-22.2` (executes `pio test -e native --verbose`, stamps receipt, updates `## Completion Report`, and commits `WO-22.2: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
