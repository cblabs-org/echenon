---
schema_version: 1
wo_id: WO-06.6
pr_id: PR-06
title: Hardware Session Lifecycle, Crash Recovery, TCX Download & Live Strava Upload
  Verification
complexity: interactive
status: PENDING
allowed_files:
- docs/measurements/phase4/C09-20261026-session-strava-verification.md
- docs/measurements/phase4/C09-20261024-session-strava-verification.md
- src/echenon/SessionManager.cpp
- src/echenon/SessionOverlay.cpp
- src/echenon/StravaUploader.cpp
forbidden_files: []
required_skills:
- framework-expert
- pio-manager
verification_command: pio test -e native && pio run -e ex3-waveshare-nano
max_attempts: 5
max_structural_failures: 2
spec_hash: fc0ae592db7d90e6f4a36d0b4d012c253056fc2a6081db104c6f684c18579c60
completion: null
---

# WO-06.6: Hardware Session Lifecycle, Crash Recovery, TCX Download & Live Strava Upload Verification

## 1. Objective & Scope

Flash `ex3-waveshare-nano` onto the bench ESP32-S3-Nano, verify end-to-end session recording (both 5-second auto-start and manual UI start/stop/discard with auto-start disabled, 2 Hz sampling averaged into 1 Hz `RideRecord` with canonical `internal_resistance` `0..254`), 30-second LittleFS crash recovery across a hard reset, authenticated `.tcx` download (with virtual speed `<ns3:Speed>` and `<DistanceMeters>`), and Strava BYOA OAuth + upload / log redaction (`SPEC.md` §13.7 tests `4.1–4.14`). Record all evidence in `docs/measurements/phase4/C09-20261026-session-strava-verification.md`.

## 2. Detailed Requirements

1. **Manual UI Start/Pause/Resume/End/Discard & Auto-Start Toggle (`SPEC.md` §13.7 `4.1–4.4`):**
   - Verify default `autoStartDelaySec = 5 s` auto-starts a session after 5 seconds of pedalling (`cadence > 0`).
   - Disable `autoStartEnabled` via the portal UI (`POST /api/session/config`), confirm pedalling does NOT auto-start a session, and verify manual **Start $\to$ Pause $\to$ Resume $\to$ End** and **Start $\to$ Discard** from the portal UI.
2. **Crash Recovery Across Power-Cycle (`SPEC.md` §13.7 `4.6`):**
   - Start a session, record $> 60\text{ s}$ so at least two 30-second checkpoints are flushed to `/ride_active.bin`, trigger a hard reset before ending the session, and verify that on reboot `RideStore::recoverInterruptedSession()` recovers the ride into `/api/session/history`.
3. **Authenticated TCX Download, Virtual Speed & Schema Verification (`SPEC.md` §13.7 `4.7–4.9`):**
   - Verify unauthenticated `GET /api/session/download` is rejected (`401`).
   - Download a completed ride's `.tcx` and `.tcx.gz` with authentication and verify valid Garmin Training Center Database v2 XML (`<Creator><Name>Echenon EX-3</Name></Creator>`, 1 Hz trackpoints, `<DistanceMeters>` and `<ns3:Speed>` matching `PowerEngine::computeVirtualSpeedKph(W)/3.6`, coasting zeros with continuous 1 Hz timestamps, pause gap omission).
4. **Strava BYOA OAuth, Upload Queue & Log Redaction (`SPEC.md` §13.7 `4.10–4.14`):**
   - Verify `/api/strava/callback` rejects mismatched/replayed `state` with `HTTP 403`.
   - Verify `UploadQueue` processes `.tcx.gz` generation and upload stages without blocking the main loop or dropping BLE links (`ScanInhibit` active during TLS).
   - Inspect serial and web diagnostic logs to confirm `client_secret`, `access_token`, and `refresh_token` are 100% redacted (`[REDACTED]`).

## 3. Acceptance Criteria
- Hardware verification report `docs/measurements/phase4/C09-20261026-session-strava-verification.md` committed with all test results.
- `pio test -e native && pio run -e ex3-waveshare-nano` passes cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
