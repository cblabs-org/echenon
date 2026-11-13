---
schema_version: 1
wo_id: WO-09.3
pr_id: PR-09
title: File Headers, GRAVE-05 Justification Comments & FTMS Guard — BLE & Data Sinks
complexity: mechanical
status: COMPLETED
allowed_files:
- src/echenon/FTMSData.h
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
- src/echenon/HREncoding.h
- src/echenon/HREvents.h
- src/echenon/HRParsing.h
- src/echenon/HRMClient.h
- src/echenon/HRMClient.cpp
- src/echenon/HRMRepeater.h
- src/echenon/HRMRepeater.cpp
- src/echenon/HRSynthetic.h
- src/echenon/HRSynthetic.cpp
- src/echenon/IUploader.h
- src/echenon/IntervalsIcuUploader.h
- src/echenon/IntervalsIcuUploader.cpp
- src/echenon/RidePersistence.h
- src/echenon/RidePersistence.cpp
- src/echenon/RideStore.h
- src/echenon/RideStore.cpp
- src/echenon/SessionManager.h
- src/echenon/SessionManager.cpp
- src/echenon/SessionOverlay.h
- src/echenon/SessionOverlay.cpp
- src/echenon/StravaUploader.h
- src/echenon/StravaUploader.cpp
- src/echenon/TCXGenerator.h
- src/echenon/TCXGenerator.cpp
- src/echenon/UploadQueue.h
- src/echenon/UploadQueue.cpp
- src/echenon/WahooUploader.h
- src/echenon/WahooUploader.cpp
forbidden_files:
- framework/**
- web/**
- docs/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 12cf92cf5fe88f92393a0abe49ccea7a2f4354c77ba378fb6039968760484ede
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_6d2094de781c
  commit_sha: 6b63b9cca95d1abe078f69e8307ad1f8f3734b3a
  files_modified:
  - src/echenon/FTMSData.h
  - src/echenon/FTMSServer.cpp
  - src/echenon/FTMSServer.h
  - src/echenon/HREncoding.h
  - src/echenon/HREvents.h
  - src/echenon/HRMClient.cpp
  - src/echenon/HRMClient.h
  - src/echenon/HRMRepeater.cpp
  - src/echenon/HRMRepeater.h
  - src/echenon/HRParsing.h
  - src/echenon/HRSynthetic.cpp
  - src/echenon/HRSynthetic.h
  - src/echenon/IUploader.h
  - src/echenon/IntervalsIcuUploader.cpp
  - src/echenon/IntervalsIcuUploader.h
  - src/echenon/RidePersistence.cpp
  - src/echenon/RidePersistence.h
  - src/echenon/RideStore.cpp
  - src/echenon/RideStore.h
  - src/echenon/SessionManager.cpp
  - src/echenon/SessionManager.h
  - src/echenon/SessionOverlay.cpp
  - src/echenon/SessionOverlay.h
  - src/echenon/StravaUploader.cpp
  - src/echenon/StravaUploader.h
  - src/echenon/TCXGenerator.cpp
  - src/echenon/TCXGenerator.h
  - src/echenon/UploadQueue.cpp
  - src/echenon/UploadQueue.h
  - src/echenon/WahooUploader.cpp
  - src/echenon/WahooUploader.h
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:01.234 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-09.3 — File Headers, GRAVE-05 Justification Comments & FTMS Guard — BLE & Data Sinks

## 1. Objective

Bring all 31 BLE, session-management, persistence, and cloud-uploader source files in `allowed_files` into full compliance with Dimension 1 (House Style) and Dimension 2 (Encapsulation & GRAVE-05) of the Framework Release Audit:

1. Ensure every `.h` and `.cpp` file in `allowed_files` has the canonical Apache-2.0 + `Module:` + `Description:` header block (preserving existing MIT reference attribution notices in `FTMSData.h` / `FTMSServer.*` per `SPEC.md` §2.3).
2. Add missing Doxygen `/** @brief ... */` / `///` docstrings and `///<` constant comments across public headers.
3. Wrap `extern uint32_t millis();` in `src/echenon/FTMSServer.cpp` (`~L27`) inside `#ifdef UNIT_TEST ... #endif` (Arduino `<Arduino.h>` already declares `millis()` on ESP32).
4. Add explicit `// GRAVE-05:` justification comments at every `std::function` alias or parameter in `UploadQueue.h`, `StravaUploader.h`, `IntervalsIcuUploader.h`, `WahooUploader.h`, `TCXGenerator.h`, and `SessionManager.h` noting boot-only binding / SBO safety (<=16B capture, zero tick-path allocation, GoogleTest spy support on `env:native`).

## 2. Strict Boundary Box

- **Allowed files (31 exact files):** Listed in frontmatter `allowed_files`.
- **Forbidden files:** `framework/**`, `web/**`, `docs/**`, and core/sensor/rig files owned by `WO-09.2`.
- **Code edits:** Only `src/echenon/FTMSServer.cpp` wraps `extern uint32_t millis();` in `#ifdef UNIT_TEST ... #endif`; all other changes are strictly headers/comments/docstrings. Preserve existing functionality and comments.
- **Shell hygiene:** Never use bash `$VAR` loops or `/tmp` in `run_command`. Use file tools (`read_file`/`edit_file`).

## 3. Technical Specification

1. Canonical file header format:
   ```cpp
   /*
    * Echenon (c) 2026 CB Labs
    * SPDX-License-Identifier: Apache-2.0
    * Module: src/echenon/<FileName>
    * Description: <Concise 1-2 line description.>
    */
   ```
   Preserve MIT reference attribution notices (`jamesjmtaylor/esp32-ftms-server`, `dbsqp/bluetooth-reebok-57e`, `lefty01/ESP32_TTGO_FTMS`) in `FTMSData.h`, `FTMSServer.h`, and `FTMSServer.cpp` per `SPEC.md` §2.3.
2. In `src/echenon/FTMSServer.cpp`, locate `extern uint32_t millis();` and wrap it:
   ```cpp
   #ifdef UNIT_TEST
   extern uint32_t millis();
   #endif
   ```
3. Search all `allowed_files` for `std::function` and verify every occurrence is accompanied by a `// GRAVE-05:` comment explaining why it is safe on ESP32-S3 (bound once at boot with stateless/SBO-small lambdas or used strictly on native/test/async-upload cold paths, never in `tick()`).

## 4. Verification

1. Verify every file in `allowed_files` contains `SPDX-License-Identifier: Apache-2.0` and `Module:`.
2. Via `call_mcp_tool` (`platformio-mcp_platformio`), run `build_project` on `ex3-waveshare-nano` and `ex3-waveshare-nano-diag` (`HRSynthetic.*`) and `run_tests` (`native`).
3. Run `~/.local/bin/pa wo verify WO-09.3` and post completion summary to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
