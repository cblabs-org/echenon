---
schema_version: 1
wo_id: WO-05.4
pr_id: PR-05
title: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak
complexity: interactive
status: COMPLETED
allowed_files:
- docs/measurements/phase3/C08-20261024-ftms-verification.md
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
- src/echenon/EchenonModule.cpp
- test/test_native/test_ftms_server.cpp
forbidden_files: []
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: 0bc01c5e104d5d0c50a820be5ad572793507bc750e351f7c781af89109f18e4d
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_31e6fb476dd9
  commit_sha: aa2d1f57859dcb703e0ee8d1d8d64fe159b616bc
  files_modified:
  - docs/measurements/phase3/C08-20261024-ftms-verification.md
  verification_summary: 'pio test -e native: ================ 155 test cases: 155
    succeeded in 00:00:01.168 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-05.4: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak

## 1. Objective & Scope

Flash `ex3-waveshare-nano` onto the connected ESP32-S3-Nano bench unit, verify the live FTMS `0x1826` GATT profile and 2 Hz `0x2AD2` notifications, validate client pairing and telemetry display on the **Lenovo Tab P12** (and macOS/iOS), and record a 5-minute multi-client coexistence soak (`SPEC.md` §13.6 tests `1.4–1.7`, `3.1–3.11`) in `docs/measurements/phase3/C08-20261024-ftms-verification.md`.

## 2. Detailed Requirements

1. **Flash & Boot Verification:**
   - Flash `ex3-waveshare-nano` via `pio-manager` MCP tools and verify clean serial boot with both `HRMRepeater` (`0x180D`) and `FTMSServer` (`0x1826`) registered on `BLEManager`.
2. **GATT Structure & Payload Inspection (`SPEC.md` §13.6 `3.1–3.5`):**
   - Verify BLE advertisement includes `0x1826` (`FTMS`) and `0x180D` (`Heart Rate`).
   - Read `0x2ACC` (`Fitness Machine Feature`) and confirm 8-byte payload `82 44 00 00 00 00 00 00` (`Target Setting Features == 0`).
   - Subscribe to `0x2AD2` (`Indoor Bike Data`) and confirm 11-byte notifications at `2 Hz` (`500 ms` interval) with `Flags = 0x0264`, live Virtual Speed (`0.01 km/h`), Cadence (`0.5 RPM`), Resistance (`0–254`), Power (`W`), and Heart Rate (`BPM`).
   - Verify `/ws/echenon/live` WebSocket stream includes `peloton_resistance` (`0–100`) and `speed_kph`.
3. **Client Compatibility & Lenovo Tab P12 Coexistence Soak (`SPEC.md` §13.6 `3.7–3.11`):**
   - Test pairing and live telemetry reception on the **Lenovo Tab P12** (plus macOS/iOS client apps as available).
   - If any target client app refuses to pair without `0x2AD9`, enable `FTMS_STUB_CONTROL_POINT=1` in `FTMSServer.h`, re-verify, and document the finding.
   - Run a 5-minute coexistence soak (FTMS client + HR relay + `/ws/echenon/live` WebSocket open simultaneously), verifying zero disconnects, zero watchdog resets, and stable free internal SRAM.
4. **Evidence Report:**
   - Record all captures, GATT hex dumps, latency measurements, and coexistence metrics in `docs/measurements/phase3/C08-20261024-ftms-verification.md`.
5. **Audit Round 1 Remediation (`AUDIT-round-1.md`):**
   - **BLOCKING-1 (`src/echenon/EchenonModule.cpp` L109–L113):** Replace `getInternalLevel()` in `_ftmsServer->updateTelemetry(...)` with the unified `0..254` mapping gated by `_calibrationWizard.isPosDone()`:
     ```cpp
     uint8_t ftmsInternalRes = _calibrationWizard.isPosDone()
         ? LevelMapping::normalizedToInternal(
               LevelMapping::echelonContinuousToNormalized(continuousLevel))
         : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
     ```
     Add unit tests in `test/test_native/test_ftms_server.cpp` asserting that Echelon Level 32 maps to `internal_resistance = 254` (encoding `resistance = 254` in `0x2AD2`, not `0`) when calibrated, and `INTERNAL_RESISTANCE_UNCALIBRATED` (`255` -> `0`) when uncalibrated.
   - **Non-Blocking Polish (`FTMSServer.h` / `FTMSServer.cpp`):**
     - `NON-BLOCKING-1` & `NON-BLOCKING-2`: Change `getProviderId()` to `"ftms"`, update `contributeAdvertising()` to use `Priority::Required` for `0x1826`, `setAppearance(0x0485)` (`kIndoorBikeAppearance`), and `addServiceData16(0x1826, svcData, 3, Priority::Optional)`.
     - `NON-BLOCKING-3`: Clarify `updateTelemetry()` doc comment in `FTMSServer.h` ("must be called from main loop").
     - `NON-BLOCKING-4`: Add `onStackPaused()` / `onStackResumed()` overrides (pausing/resuming notifications during OTA) and add a `NimBLECharacteristicCallbacks` write handler inside `#if FTMS_STUB_CONTROL_POINT` on `0x2AD9`.
6. **Live BLE (`bleak`) + Calibration Rig Verification:**
   - Tag `v0.3.0-dev.18`, build & flash `ex3-waveshare-nano-rig` (to drive synthetic cadence + resistance via `/api/echenon/rig/command`), run a Python script in `tmp/` using `bleak` + `websockets` to:
     - Read `0x2ACC` (`82 44 00 00 00 00 00 00`) over real BLE
     - Subscribe to `0x2AD2` (`Indoor Bike Data`) and capture 2 Hz (`500 ms`) 11-byte notifications across multiple rig states (e.g. 0 RPM idle, 80 RPM at Levels 1, 16, 32) confirming Level 32 reports `resistance = 254`!
     - Connect to `ws://192.168.1.100/ws/echenon/live` and capture live JSON frames containing `peloton_resistance` and `speed_kph`.
   - Then build & flash production `ex3-waveshare-nano` (`v0.3.0-dev.18`), re-verify BLE `0x2ACC` and `0x2AD2` via `bleak`, update `docs/measurements/phase3/C08-20261024-ftms-verification.md`, and run `pa wo verify WO-05.4`.

## 3. Acceptance Criteria
- `BLOCKING-1` and non-blocking findings remediated with unit tests passing (`pio test -e native`).
- Hardware verification report `docs/measurements/phase3/C08-20261024-ftms-verification.md` updated with real `bleak` GATT `0x2ACC` & `0x2AD2` captures (including Level 32 = 254), interval timing, and `/ws/echenon/live` WebSocket payloads.
- `pio test -e native` passes cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
