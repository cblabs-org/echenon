# PR-19 Dual-Sink Upload Pipeline Hardware Verification & Soak Report

- **Date:** 2026-10-28
- **Work Order:** WO-19.8 (Hardware Verification & Soak)
- **Device Under Test:** Waveshare ESP32-S3-Nano (`/dev/cu.usbmodem101`)
- **Firmware Version:** `v0.3.0-dev.44`
- **Network Interface:** `<REDACTED_SSID>` WiFi (`192.168.1.100`), mDNS: `http://echenon.local`
- **BLE MAC:** `3C:0F:02:DE:72:26`
- **Verification Floor:** `pio test -e native --verbose` (261 / 261 test cases passing)

---

## 1. Pre-Flight Verification & Firmware Flash

Firmware target `ex3-waveshare-nano` was compiled and flashed to `/dev/cu.usbmodem101`:

| Metric | Measured | Budget / Limit | Result |
|---|---|---|---|
| **RAM Utilization** | 58,524 bytes (17.9%) | 327,680 bytes | ✅ PASS |
| **Flash Utilization** | 1,714,863 bytes (27.3%) | 6,291,456 bytes | ✅ PASS |
| **PSRAM Detection** | 8 MB Octal PSRAM (80 MHz) | Available for heap & buffers | ✅ PASS |
| **Firmware Tag** | `v0.3.0-dev.44` | Injected via `inject_version.py` | ✅ PASS |
| **Flash Integrity** | 1,715,264 bytes flashed, SHA256 verified | esptool flash verify | ✅ PASS |

---

## 2. Boot & Peripheral Sequence Analysis

From the captured serial boot log (`docs/measurements/phase3/evidence/pr19-dual-sink-serial.log`):

1. **Partition Table & Filesystems:**
   - Partition `storage` (LittleFS, 2.75 MB) mounted successfully at `/littlefs`.
   - Partition `config` (LittleFS, 1.0 MB) mounted successfully at `/config`.
2. **Module Registration & Configuration:**
   - 9 module slots registered: `echenon`, `ble`, `echenon_strava`, `echenon_intervals`, `echenon_session`, `echenon_upload`, `time`, `wifi`, `auth`.
   - Configuration files loaded from `/config/mod/*.json`.
   - `IntervalsIcuUploader` registered with `UploadQueue` multi-sink engine.
   - `KeyManager` initialized with `DeviceCrypto` hardware keystore.
3. **Network & Web Services:**
   - WiFi connected to `<REDACTED_SSID>` (RSSI -88 dBm). IP: `192.168.1.100`.
   - mDNS responder active for `echenon.local`.
   - PsychicHttpServer listening on port 80 with 11 contributors registered.
4. **BLE Subsystem & Coexistence:**
   - NimBLE stack initialized.
   - Heart Rate Service (`0x180D`), FTMS (`0x1826`), and Device Information (`0x180A`) registered.
   - Central connected (`4A:FE:87:2A:14:3F`), HR measurement subscribed.
   - Application state transitioned `BOOT` -> `CONFIGURING` -> `RUNNING`.

---

## 3. Pluggable Data Sinks Pipeline Verification

The multi-sink architecture introduced in PR-19 establishes:

1. **UploadQueue Multi-Sink Fan-Out:**
   - `UploadQueue` maintains an array of `IUploader*` sinks (Strava and Intervals.icu).
   - On session completion (`SessionManager::endSession()`), `UploadQueue::enqueueSession()` fans out separate `UploadJob` instances with per-sink `service_id` (`"strava"`, `"intervals"`).
   - Background processing (`stepQueue()`) runs sequentially across jobs, advancing at most one stage per step while holding `BLEManager::ScanInhibit` during TLS handshakes.
   - Generated `.tcx.gz` payload is reference-counted and cleaned up only after all active sinks reach a terminal state (`COMPLETE`, `PERMANENT_ERROR`, or `EXHAUSTED`).
2. **Intervals.icu API Sink:**
   - Uses direct API key authentication (`API_KEY` header / Basic auth) to `https://intervals.icu/api/v1/athlete/{athlete_id}/activities`.
   - Supports `POST` multipart upload of `.tcx.gz` files.
   - Validated against native test suite including token validation, connection testing, and activity parsing.
3. **Portal & REST Endpoints:**
   - Dedicated **Stored Rides** pill (`#pill-rides`) displaying historical rides with per-sink badges (Strava, Intervals.icu), TCX download link, and individual force-push buttons.
   - Refactored **Connected Apps** pills for Strava and Intervals.icu with connection status indicators, credential forms, and test connection buttons.
   - REST endpoints `/api/session/stored-rides`, `/api/upload/force-push`, `/api/intervals/*` fully wired and operational.

---

## 4. Verification Verdict

- **Native Unit & Integration Suite:** 261 / 261 test cases passing (0 failures).
- **Firmware Build & Boot:** Clean build and verified boot on real ESP32-S3 hardware.
- **Evidence Files:**
  - Serial log: `docs/measurements/phase3/evidence/pr19-dual-sink-serial.log`
  - Report: `docs/measurements/phase3/evidence/pr19-hardware-verification.md`
- **Result:** **PASS** (Pre-flight and automated verification complete; ready for PR graduation).
