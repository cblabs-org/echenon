# PR-14 Hardware Verification Report

- **Date:** 2026-10-27
- **Hardware:** ESP32-S3 Waveshare Nano (`ex3-waveshare-nano`) at `192.168.1.100` / `echenon.local`
- **Firmware Version:** `v0.3.0-dev.31` (commit `486dff9`)
- **Scope:** 5-minute smoke test & full pipeline verification (60-minute endurance ride deferred to training session per owner agreement).

---

## 1. Bench Ride Summary (Session #2)

A 5-minute interactive test ride was executed by the rider on the Echelon EX-3 bike:

| Metric | Value |
|---|---|
| **Session ID** | `2` |
| **Start Time** | `2026-10-27 18:24:36 UTC` |
| **Duration** | `305 seconds` (5m 05s) |
| **Record Count** | `305` anti-aliased 1-Hz records |
| **Average Power** | `40 W` |
| **Max Power** | `69 W` |
| **Average Cadence** | `58 RPM` |
| **Average Heart Rate** | `105 BPM` (Max: `110 BPM`) |
| **Total Work** | `12 kJ` |
| **NTP Validation** | Valid |

---

## 2. Diagnostics & Persistence Verification

### 2.1 TCX Download Integrity
- **Endpoint:** `GET /api/session/download?id=2`
- **Verification:** Downloaded the full 305-second Training Center Database XML (.tcx).
- **Result:** Successfully streamed 305 trackpoints with Distance, HR, Cadence, Speed, and Watts. Device heap and uptime remained completely stable without crashing or watchdog reset.
- **Root Cause Fixed:** Resolved stack overflow in the 8 KB `httpd` task by moving `ScratchBufferWriter` and formatting buffers to PSRAM heap.

### 2.2 Run-Data Persistence & Gzip Compression
- **Verification:** Persistence compression path updated from 270 KB ROM miniz to PSRAM-allocated `StreamingGzipEncoder`.
- **Result:** Session finalized cleanly without DRAM exhaustion reboots.

### 2.3 Diagnostic Event Log
- **Endpoint:** `GET /api/echenon/events`
- **Verification:** Captured system `BOOT` events with valid UTC timestamps (1793125272+), plus `RUNAWAY_ALERT` edge detections.

---

## 3. UI & Telemetry Verification (`/ride`)

1. **Settings Gear Icon:** Replaced emoji `⚙️` with an inline SVG gear icon conforming to the dark theme design system.
2. **Auto-Start Toggle:** Added an interactive "Auto-start" toggle switch on `/ride` connected to `GET/POST /api/session/config` (`auto_start`).
3. **Live Telemetry & Heart Rate:** Added instantaneous `hr` to the live telemetry stream (`/api/echenon/live` and `/ws/echenon/live`).
4. **Coexistence-Safe Fallback Polling:** Set HTTP fallback polling rate to 2.5 seconds (2500 ms) when WebSocket is disconnected, protecting the 2.4 GHz radio from BLE starvation.
5. **Button Logic:** Fixed button transitions so ending a session cleanly resets UI to IDLE and enables Start.

---

## 4. Verdict

**PASS** ✅ — The complete PR-14 diagnostics, persistence, and ride UI pipeline is functional and verified on real hardware.
