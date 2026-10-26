---
schema_version: 1
wo_id: WO-06.5
pr_id: PR-06
title: SessionOverlay REST API, Authenticated TCX Download, Portal Tabs & Module Wiring
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/SessionOverlay.h
- src/echenon/SessionOverlay.cpp
- src/echenon/LiveFeed.h
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/main.cpp
- platformio.ini
- web/session.js
- web/upload_settings.js
- include/portal_assets/echenon_session_js.h
- include/portal_assets/echenon_upload_js.h
- test/test_native/test_session_manager.cpp
forbidden_files: []
required_skills:
- framework-expert
- esp32-app-foundation-design
verification_command: pio test -e native && pio run -e ex3-waveshare-nano && pio run
  -e ex3-waveshare-nano-diag
max_attempts: 5
max_structural_failures: 2
spec_hash: 6d0f1807438323851997731c54dd84966e2454d0637ee97ed0875a033c48bcd3
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_0de415574724
  commit_sha: 08ee2f6b358f1b03e9bd62b93ef879e77e1ba3b0
  files_modified:
  - src/echenon/EchenonModule.cpp
  - src/echenon/SessionOverlay.cpp
  - test/test_native/test_session_manager.cpp
  - web/session.js
  verification_summary: 'pio test -e native && pio run -e ex3-waveshare-nano && pio
    run -e ex3-waveshare-nano-diag: ========================= 1 succeeded in 00:00:26.714
    ========================='
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-06.5: SessionOverlay REST API, Authenticated TCX Download, Portal Tabs & Module Wiring

## 1. Objective & Scope

Implement `SessionOverlay` (`src/echenon/SessionOverlay.h` / `.cpp`) exposing the `/api/session/*` and `/api/strava/*` REST endpoints (including authenticated `/api/session/download` and `POST /api/session/discard`), build the Session History (`web/session.js`) and Strava/Upload Settings (`web/upload_settings.js`) portal tabs (registered at tab orders `80` and `90`), and wire `SessionManager` (with 2 Hz fundamental-first telemetry sampling), `RideStore`, `TCXGenerator`, `UploadQueue`, `StravaUploader`, and `SessionOverlay` into `EchenonModule` and `src/main.cpp` (`SPEC.md` §11.3–§11.5).

## 2. Detailed Requirements

### 2.1 REST Endpoints & Authenticated TCX Download (`src/echenon/SessionOverlay.h` & `.cpp`, `SPEC.md` §11.3–§11.5)
Implement `IWebContributor` and `ITabRegistry` contributor in `SessionOverlay`:
1. **Tab Ordering (`PR-05` Coexistence):**
   - Register `Session` tab at order **`80`** and `Upload Services` tab at order **`90`** (after `Echenon = 50`, `Calibration = 60`, and `Recalibration = 70` in `EchenonModule.cpp`).
2. **Session Lifecycle & History Endpoints (Authenticated via `AuthManager`):**
   - `GET /api/session/status`: returns current session state (`IDLE`, `ACTIVE`, `PAUSED`), elapsed duration, active ride metrics (`avg_power_w`, `max_power_w`, `total_work_kj`), `auto_start_enabled`, `auto_start_delay_sec`, and `inactivity_timeout_sec`.
   - `POST /api/session/start`, `POST /api/session/pause`, `POST /api/session/resume`, `POST /api/session/end` (and alias `POST /api/session/stop`): manual UI session controls.
   - `POST /api/session/discard` (optional `?id=<session_id>`): discards the active/paused session or deletes a stored/queued session and cancels any pending upload (`SPEC.md` §11.3 & §11.5).
   - `POST /api/session/config`: updates `auto_start_enabled`, `auto_start_delay_sec`, and `inactivity_timeout_sec`.
   - `GET /api/session/history`: returns JSON array of stored completed rides from `RideStore` plus upload queue status for each ride.
   - `GET /api/session/download?id=<session_id>&format=<tcx|tcx.gz>`: **authenticated** endpoint (`AuthManager` guard) that streams the ride's `.tcx` or `.tcx.gz` file with `Content-Disposition: attachment; filename="echenon_<id>.tcx"`.
   - `POST /api/session/upload?id=<session_id>`: manually enqueues a stored session into `UploadQueue`.
3. **Strava BYOA Endpoints (Authenticated via `AuthManager`):**
   - `GET /api/strava/status`: returns `{ configured, client_id, athlete_name, token_valid, queue_size }` (never returns `client_secret`, `access_token`, or `refresh_token`).
   - `POST /api/strava/credentials`: saves `client_id` and `client_secret` (to `KeyManager` + `Redactor`).
   - `GET /api/strava/auth-url`: generates single-use `state` nonce and returns `{ authorize_url }`.
   - `GET /api/strava/callback`: validates `state` (returns `HTTP 403` on mismatch/replay), exchanges `code` for tokens, and redirects to the portal Upload Settings tab.
4. **`LiveFeed` Session State Integration (`src/echenon/LiveFeed.h`):**
   - Include `"session_state": "IDLE" | "ACTIVE" | "PAUSED"` and `"session_elapsed_s": <uint32>` in `/ws/echenon/live` JSON so the dashboard shows live ride timer and manual Start / Pause / End / Discard controls in real time.

### 2.2 Portal UI Assets (`web/session.js`, `web/upload_settings.js`, `include/portal_assets/`)
1. **`web/session.js` (`include/portal_assets/echenon_session_js.h`):**
   - **Active Ride Controls Card:** Start / Pause / Resume / End / Discard Session buttons, live elapsed duration, work (`kJ`), average/max power, and **Auto-Start Configuration Toggle** (`autoStartEnabled` checkbox + `autoStartDelaySec` input defaulting to `5 s` + `inactivityTimeoutSec` defaulting to `180 s`).
   - **Ride History Table:** Lists stored rides with date/time, duration, avg/max watts, kJ, Strava upload badge (`Uploaded`, `Queued`, `Failed`, `Not Uploaded`), **Download `.tcx`** button, **Upload to Strava** button, and **Discard** button.
2. **`web/upload_settings.js` (`include/portal_assets/echenon_upload_js.h`):**
   - Strava BYOA setup card (`Client ID`, `Client Secret`, **Connect with Strava** OAuth button, connection status badge, and upload queue status table).

### 2.3 `EchenonModule` & `src/main.cpp` Wiring
- Feed 2 Hz (`500 ms`) telemetry from `EchenonModule` (`power_watts`, `cadence_rpm`, `heart_rate_bpm`, `hr_connected`, `internal_resistance = LevelMapping::potNormalizedToInternal(pTilde)`, `resistance_calibrated`) into `SessionManager` (which averages each 2-sample pair into 1 Hz `RideRecord` entries).
- Register `SessionManager`, `UploadQueue`, and `SessionOverlay` with the framework in `src/main.cpp`.
- Verify both `ex3-waveshare-nano` (with `WITH_BLE` and `WITH_FTMS`) and `ex3-waveshare-nano-diag` (without `WITH_BLE`) compile cleanly (`SPEC.md` §10.6 `ScanInhibit` `#ifdef WITH_BLE` verification).

## 3. Acceptance Criteria
- `pio test -e native && pio run -e ex3-waveshare-nano && pio run -e ex3-waveshare-nano-diag` exits `0` with all native unit tests passing and both firmware environments compiling cleanly.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
