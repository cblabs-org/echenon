---
schema_version: 1
wo_id: WO-05.4
pr_id: PR-05
title: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak
complexity: interactive
status: PENDING
allowed_files:
- docs/measurements/phase3/C08-20261024-ftms-verification.md
- src/echenon/FTMSServer.h
- src/echenon/FTMSServer.cpp
forbidden_files: []
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native && pio run -e ex3-waveshare-nano
max_attempts: 5
max_structural_failures: 2
spec_hash: d887f5741c43f94233747e19a26ab7d3d2c2ecf73f194eae9c6a1096cc8e8d85
completion: null
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

## 3. Acceptance Criteria
- Hardware verification report `docs/measurements/phase3/C08-20261024-ftms-verification.md` committed with passing results for GATT structure, 2 Hz `0x2AD2` payload decoding, client app pairing, and 5-minute coexistence soak on the Lenovo Tab P12.
- `pio test -e native && pio run -e ex3-waveshare-nano` passes cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
