---
schema_version: 1
wo_id: WO-21.6
pr_id: PR-21
title: Hardware Verification & Soak
complexity: interactive
status: COMPLETED
allowed_files:
- docs/
forbidden_files:
- src/**
- web/**
- test/**
- SPEC.md
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: ca8e425c78184d6b37f73f8d2ba1a5d6055fe9c244137ae415c75e93bd30804e
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_d584d1fb4dc9
  commit_sha: cd4c6deaa4ec38aceea264ed89982bfae7efbd2d
  files_modified:
  - docs/measurements/phase3/evidence/pr21-wahoo-serial.log
  - docs/measurements/phase3/evidence/pr21-wahoo-verification.md
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.495 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.6 — Hardware Verification & Soak

## 1. Role & Objective

Verify `WahooUploader` and the multi-sink system on real hardware (`ex3-waveshare-nano` on `/dev/cu.usbmodem1101`).
Confirm clean boot, module registration, portal asset serving, and heap health.

**Deliverables:**
- Tag SemVer dev increment per Rule 03 (`v0.X.Y-dev.N`).
- Build firmware for `ex3-waveshare-nano`.
- Flash to connected device via PlatformIO MCP tools.
- Capture serial log and verify:
  - Clean boot with version banner.
  - `WahooUploader` initialized and registered with `UploadQueue` (Sink #3).
  - Web portal serves `/wahoo_settings.js` without errors.
  - Free heap and PSRAM baselines remain healthy.
- Record verification evidence in `docs/measurements/`.

## 2. Strict Boundary Box & Constraints

1. **Rule 03 Versioning Policy:** Tagdev increment BEFORE flashing (`git tag v0.X.Y-dev.N && git push origin v0.X.Y-dev.N`).
2. **Never Call `upload_filesystem`:** Web assets are compiled PROGMEM gzip.
3. **MCP Tooling Priority (Rule 04):** Use PlatformIO MCP tools (`build_project`, `upload_firmware`, `start_monitor`) instead of direct CLI commands where possible.

**Forbidden:** No code modifications to `src/**`, `web/**`, `test/**`.

## 3. Line-Item Instructions

- [ ] Check latest git tag (`git describe --tags --abbrev=0`).
- [ ] Create and push next dev tag (`git tag v0.X.Y-dev.(N+1)`).
- [ ] Build firmware for `ex3-waveshare-nano` via `build_project` MCP tool.
- [ ] Flash firmware via `upload_firmware` MCP tool.
- [ ] Capture boot logs:
  - Confirm `EchenonModule` and `UploadQueue` report 3 sinks registered.
  - Confirm `/api/wahoo/*` routes registered.
  - Check heap stability.
- [ ] Document test findings in completion report.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose`.
- Run `pa wo verify WO-21.6`.

## Completion Report

### Summary of Completed Work
Successfully completed end-to-end hardware verification, flashing, and soak on real ESP32-S3 hardware (`ex3-waveshare-nano` on `/dev/cu.usbmodem101`) for PR-21 (Wahoo Cloud Data Sink).

### Hardware Verification & Validation Results
1. **Rule 03 Versioning Policy:**
   - Identified latest tag `v0.3.0-dev.44`.
   - Created and pushed tag `v0.3.0-dev.45` to origin (`git push origin v0.3.0-dev.45`).
2. **Firmware Compilation & Asset Generation:**
   - Compiled target `ex3-waveshare-nano` with embedded version `0.3.0-dev.45`.
   - Verified pre-build script `portalBuilder.py` automatically bundled `web/wahoo_settings.js` into gzip-compressed PROGMEM header `include/portal_assets/wahoo_settings_js.hpp` (4,105 bytes gzipped, ETag `"1009-851d25aa"`).
   - RAM utilization: 58,868 bytes (18.0% of 320 KB internal DRAM).
   - Flash utilization: 1,734,963 bytes (27.6% of 6 MB app partition).
3. **Flashing & Boot Verification:**
   - Flashed 1,735,360 bytes to `/dev/cu.usbmodem101` and verified SHA256 integrity via esptool.
   - Captured serial boot logs (`docs/measurements/phase3/evidence/pr21-wahoo-serial.log`):
     - Version banner: `ESP32 App Foundation v0.3.0-dev.45 — Echenon — booting`
     - 8 MB Octal PSRAM at 80 MHz initialized. Total free heap: 8,369,599 B.
     - 10 kernel and application modules registered, including slot 5: `echenon_wahoo`.
     - LittleFS storage and config partitions mounted cleanly.
     - WiFi connected to `<REDACTED_SSID>` at IP `192.168.1.100` (RSSI: -62 dBm).
     - BLE NimBLE stack initialized, advertising FTMS (`0x1826`) and HRM (`0x180D`), and central connected with active notifications.
4. **Live REST API & Multi-Sink Queue Verification:**
   - `GET /api/upload/status`: Confirmed 3 active sinks registered in `UploadQueue` (`strava`, `intervals`, `wahoo`).
   - `GET /api/wahoo/status`: Verified initial unconfigured status reporting.
   - `POST /api/wahoo/credentials`: Successfully stored credentials with NVS encrypted persistence.
   - `GET /api/wahoo/auth-url`: Successfully generated OAuth 2.0 authorization URL with 128-bit single-use nonce:
     `https://api.wahooligan.com/oauth/authorize?client_id=test_hw_client_id&response_type=code&redirect_uri=http%3A%2F%2F192.168.1.100%2Fapi%2Fwahoo%2Fcallback&scope=user_read%20workouts_write&state=...`
   - `POST /api/wahoo/disconnect`: Cleanly flushed tokens and reset authentication state.
   - `GET /api/tabs`: Confirmed Wahoo Cloud navigation tab registered under `Apps` parent (order 30, script `/wahoo_settings.js`).
   - `GET /wahoo_settings.js`: Successfully served HTTP 200 OK gzipped PROGMEM asset with matching ETag.
5. **Native Unit Tests Floor:**
   - Executed `pio test -e native --verbose`: all 269 test cases across 29 test suites passed with 0 errors.

### Evidence & Artifacts
- Serial capture: `docs/measurements/phase3/evidence/pr21-wahoo-serial.log`
- Hardware report: `docs/measurements/phase3/evidence/pr21-wahoo-verification.md`
- Cryptographic verification receipt: `rcpt_d584d1fb4dc9` (tree hash `d584d1fb4dc927ad16c78a6f8cfccad9c0dcf280c4615e2724bf0865d0d55083`) committed in `c89d1981eefa`.
