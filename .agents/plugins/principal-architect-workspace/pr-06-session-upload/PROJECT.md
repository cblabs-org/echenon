---
schema_version: 1
charter:
  pr_id: PR-06
  programme_id: null
  slug: pr-06-session-upload
  title: Session Recording, TCX Export & Strava Upload
  branch: feat/pr-06-session-upload
  base_branch: main
  depends_on: []
  allowed_modules:
  - src/echenon/
  - src/main.cpp
  - web/
  - include/portal_assets/
  - test/
  - docs/
  - platformio.ini
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native
  charter_locked: false
  charter_hash: null
status: APPROVED
approved_at: '2026-10-24T20:58:10.078370Z'
github_pr: null
work_orders:
- id: WO-06.1
  slug: wo1-session-ridestore
  title: RideStore PSRAM/LittleFS Storage & SessionManager Lifecycle (Auto-Start Toggle
    + 5 s Default)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 1d171df13958918d2395414d06a3e9c2a4bddcc7
- id: WO-06.2
  slug: wo2-tcx-generator
  title: Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: ab4525e582475cf19912a14164c3129a3f4289f1
- id: WO-06.3
  slug: wo3-upload-queue
  title: IUploader & Split-Stage UploadQueue IDataSource State Machine
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 4e988fe56fa2ad6aa18699427ebbd70ffa36a990
- id: WO-06.4
  slug: wo4-strava-uploader
  title: StravaUploader BYOA OAuth 2.0, KeyManager NVS, Rotated Token Persistence
    & Redactor
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 172aa9a81f30fb8c3fdb0f3b9fcf19ca55f52d79
- id: WO-06.5
  slug: wo5-portal-wiring
  title: SessionOverlay REST API, Authenticated TCX Download, Portal Tabs & Module
    Wiring
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 08ee2f6b358f1b03e9bd62b93ef879e77e1ba3b0
- id: WO-06.6
  slug: wo6-hw-verification
  title: Hardware Session Lifecycle, Crash Recovery, TCX Download & Live Strava Upload
    Verification
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
---

# PR-06: Session Recording, TCX Export & Strava Upload (Phase 4, v1 Passive)

> **Parent:** [`PROGRAM.md`](../PROGRAM.md)
> **Phase:** 4 (`docs/implementation-strategy.md`) — **v1 "Passive" release**
> **Depends on:** PR-02 (`8ccccb6`), PR-05 (`a6c7a88`)
> **Branch:** `feat/pr-06-session-upload` (based on `main` at `a6c7a88`)
> **Spec sections:** §9.1–§9.6 (`SessionManager`, `RideStore`, `TCXGenerator`), §10.1–§10.6 (`IUploader`, `UploadQueue`, `StravaUploader`), §11.3–§11.5 (`/api/session/*` + `/api/strava/*`, Session History & Upload Settings portal tabs), §13.7 (`4.1–4.14`)

---

# Part I — PR Charter (Chief Architect Locked)

## 1. Mandate & Architectural Invariants

Implement full ride session recording, crash-resilient PSRAM/LittleFS storage, streaming `1 Hz` compressed `TCX.gz` export, and background Strava v3 API upload so every ride on the Echelon EX-3 can be started/stopped from the portal UI (or auto-started after a configurable cadence threshold), recovered across power loss, downloaded as a `.tcx` / `.tcx.gz` file, or automatically uploaded to Strava.

### 1.1 Session Lifecycle, Fundamental-First Storage & Owner Decisions (`SessionManager` + `RideStore`)
- **State Machine:** `IDLE` $\to$ `ACTIVE` $\leftrightarrow$ `PAUSED` $\to$ `ENDED`.
- **Owner Decision (2026-10-24) — Configurable Auto-Start & Manual UI Mode:**
  - `session_auto_start` (`bool`, default `true` in `EchenonModule` `IConfigurable` schema): when `false`, pedalling does **not** auto-start a session — sessions start and end strictly via the portal UI buttons (`POST /api/session/start`, `POST /api/session/pause`, `POST /api/session/resume`, `POST /api/session/end` or `/api/session/stop`).
  - `session_auto_start_s` (`uint16_t`, default **`5`** seconds, configurable `1–60 s`): when `session_auto_start == true` and state is `IDLE`, continuous `cadence > 0 RPM` for `session_auto_start_s` transitions `IDLE -> ACTIVE`.
  - `session_inactivity_s` (`uint16_t`, default **`180`** seconds / 3 minutes, configurable `30–3600 s`): continuous `cadence == 0 RPM` while `ACTIVE` ends the session and **trims the trailing zero-cadence records** after the last active pedal stroke from `RideStore` (`SPEC.md` §9.2).
- **`2 Hz` Anti-Aliased Sampling $\to$ `1 Hz` Fundamental-First Record (`RideRecord`, 8 bytes packed, `SPEC.md` §5.3 & §9.3–§9.4):**
  - `SessionManager::tick()` samples live telemetry at **2 Hz (`500 ms` intervals)** in lockstep with `PowerEngine` and `FTMSServer`, and averages each consecutive pair of `500 ms` samples into a `1 Hz` 8-byte `RideRecord`:
    - `uint16_t elapsed_sec`, `uint16_t power_watts` (2-sample average), `uint8_t cadence_rpm` (2-sample average), `uint8_t heart_rate_bpm`, `uint8_t internal_resistance` (`0..254 = round(254 * pTilde)`, `255 = UNCALIBRATED`), `uint8_t flags` (`bit 0 = hr_valid`, `bit 1 = resistance_calibrated`, `bit 2 = paused_marker`).
  - Storing `internal_resistance` (`0..254`) rather than discrete `1..32` preserves the fundamental normalized cam coordinate $\tilde{p} = i / 254.0$ at $8\times$ finer resolution, allowing lossless reconstruction of Echelon (`1..32`), Peloton (`0..100`), or $\tilde{p}$ downstream (`LevelMapping::internalToEchelon` / `internalToPeloton`).
  - Allocated in `8 MB` OPI PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` on ESP32, `malloc` fallback in native tests) for up to `14,400` records (`4 hours` = `115.2 KB`).
- **Wraparound-Safe Signed `int32_t` Timers & Mid-Ride NTP Back-Computation (`PR-05` `WO-05.4` Lesson & `SPEC.md` §9.2):**
  - All `millis()` interval checks (`500 ms` sample tick, `5 s` auto-start, `180 s` inactivity, `30 s` checkpoint flush) MUST use signed `(int32_t)(now_ms - last_ms) >= threshold_ms` arithmetic.
  - If a session starts before NTP sync (`start_epoch_s < 1700000000`) and NTP syncs mid-ride (`now_epoch > 1700000000`), `SessionManager` dynamically back-computes `start_epoch_s = now_epoch - elapsed_s` so the exported TCX carries real-world UTC timestamps.
- **30-Second LittleFS Crash Recovery Checkpoints (`SPEC.md` §9.2):**
  - Every `30 s` (`30` records), append a delta chunk to `/ride_active.bin` on LittleFS.
  - On `begin()`, if `/ride_active.bin` exists with $\ge 60$ active records, recover the interrupted session into the completed history ring (`/rides/ride_<idx>.bin`, max `5` queued/completed sessions) so no ride data is lost on power-off.

### 1.2 Streaming 1 Hz `TCXGenerator` with Virtual Speed Integration (`SPEC.md` §7.5 & §9.4)
- Generates valid Garmin Training Center Database v2 XML (`Activity Sport="Biking"`) with `<Creator><Name>Echenon EX-3</Name></Creator>`, `<Author>`, `<Lap>`, `<Extensions><ns3:LX><ns3:AvgWatts>`, `<ns3:MaxWatts>`, and `1 Hz` `<Trackpoint>` elements (`<Time>`, `<DistanceMeters>`, `<HeartRateBpm>` when `hr_valid`, `<Cadence>`, `<Extensions><ns3:TPX><ns3:Speed>` and `<ns3:Watts>`).
- **First-Principles Virtual Speed & Distance Integration (`PR-05` Continuity):**
  - Each 1 Hz `<Trackpoint>` computes virtual road speed $v_{\text{m/s}} = \text{PowerEngine::computeVirtualSpeedKph}(P) / 3.6\text{f}$ (`0.245 v^3 + 3.53 v = P`), accumulates `cumulative_distance_m += v_mps * 1.0f`, and writes both `<DistanceMeters>` and `<ns3:Speed>` so Strava, Garmin, and Xert receive physically consistent speed and distance.
- **Coasting vs. Pause Semantics & Xert MPA Continuity (`SPEC.md` §9.4):**
  - Active coasting (`cadence == 0` while `ACTIVE`) **always emits** 1 Hz `<Trackpoint>` elements with `<Cadence>0</Cadence>`, `<ns3:Watts>0</ns3:Watts>`, and `<ns3:Speed>0.00</ns3:Speed>` with zero timestamp gaps so Xert MPA continuous differential equations are never corrupted.
  - Manual UI Pause intervals (`PAUSED` state between `pauseSession()` and `resumeSession()`) suspend point emission until resumed.
- **Streaming Gzip Compression:** Streams through a bounded **`4 KB` scratch buffer** into `/rides/ride_<id>.tcx.gz` (using `uzlib` / `miniz` / `zlib` deflate with RFC 1952 gzip header + CRC32 + ISIZE trailer) and checks `DeadlineToken` so arbitrary 4-hour rides never allocate full XML strings in heap or blow the 15 s DataManager budget.

### 1.3 Split-Stage `UploadQueue` (`IDataSource`) & Pure Capability `StravaUploader` (`SPEC.md` §10.1–§10.6)
- **Capability-Only `IUploader` & Transport-Owning `UploadQueue` (`SPEC.md` §10.2):**
  - `IUploader` describes service capabilities, configuration state, and request-building / response-parsing (`buildRequest` / `handleResponse` or injected `IHttpTransport`) without owning raw sockets.
  - `UploadQueue` implements `IDataSource`, `IConfigurable`, and `IPreconditionContributor`, owns the HTTP transport and `BLEManager::ScanInhibit` token, and executes **at most one non-blocking stage per `fetchUpdate(const FetchContext& ctx)` call** (`STAGE_GENERATE_TCX_GZ` $\to$ `STAGE_REFRESH_TOKEN` $\to$ `STAGE_POST_UPLOAD` $\to$ `STAGE_POLL_STATUS`) within the 15 s `DeadlineToken`.
  - Persistent queue of up to `5` pending sessions in LittleFS (`/upload_queue.json`) with exponential backoff (`30 s`, `60 s`, `120 s`, `300 s`, max 5 attempts per ride) and `POST /api/session/discard` support (`SPEC.md` §11.3/§11.5).
- **BLE ScanInhibit (`#ifdef WITH_BLE`):**
  - Acquires `BLEManager::ScanInhibit` RAII guard during TLS stages (`STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`) when `#ifdef WITH_BLE` is defined, and compiles cleanly when `WITH_BLE` is absent (`SPEC.md` §10.3).
- **Strava BYOA OAuth 2.0 & Security (`SPEC.md` §10.4–§10.5):**
  - Stores `client_secret`, `access_token`, and `refresh_token` in encrypted NVS via `KeyManager` (`"strava"` namespace).
  - Generates a single-use random `state` nonce on `/api/strava/auth-url` and validates it on `/api/strava/callback` (rejecting mismatches with HTTP `403`).
  - **Rotated Refresh Token First:** On token refresh, persists the new `refresh_token` to NVS **before** `access_token` (`SPEC.md` §10.4).
  - Registers `client_secret`, `access_token`, and `refresh_token` with `Redactor::registerSecret()` immediately upon load and upon every OAuth exchange/refresh, checking the boolean return value and logging a warning if the Redactor slot table is full (`SPEC.md` §10.4).
- **Portal Tab Ordering (`WO-06.5`):**
  - Registers `Session` at tab order `80` and `Upload Services` at tab order `90` (after `Echenon = 50`, `Calibration = 60`, and `Recalibration = 70`).

## 2. Scope

### Allowed Files & Modules
- `src/echenon/RideStore.h`, `src/echenon/RideStore.cpp`
- `src/echenon/SessionManager.h`, `src/echenon/SessionManager.cpp`
- `src/echenon/TCXGenerator.h`, `src/echenon/TCXGenerator.cpp`
- `src/echenon/IUploader.h`
- `src/echenon/UploadQueue.h`, `src/echenon/UploadQueue.cpp`
- `src/echenon/StravaUploader.h`, `src/echenon/StravaUploader.cpp`
- `src/echenon/SessionOverlay.h`, `src/echenon/SessionOverlay.cpp`
- `src/echenon/LiveFeed.h`
- `src/echenon/EchenonModule.h`, `src/echenon/EchenonModule.cpp`
- `src/main.cpp`, `platformio.ini`
- `web/session.js`, `web/upload_settings.js`, `include/portal_assets/`
- `test/test_native/test_session_manager.cpp`, `test/test_native/test_tcx_generator.cpp`, `test/test_native/test_upload_queue.cpp`, `test/test_native/test_strava_uploader.cpp`
- `docs/measurements/phase4/`

### Out of Scope
- OBS `/overlay` transparent browser source page (`PR-09`).
- Bounded production signal-capture run persistence (`PR-14`).

---

# Part II — Principal Architect Decomposition & Execution

## 3. Work Order Ledger

| WO ID | Slug | Title | Complexity | Status | Attempts | Commit SHA |
|---|---|---|---|---|---|---|
| `WO-06.1` | `wo1-session-ridestore` | RideStore PSRAM/LittleFS Storage & SessionManager Lifecycle (Auto-Start Toggle + 5 s Default) | `standard` | `PLANNED` | 0 | — |
| `WO-06.2` | `wo2-tcx-generator` | Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps) | `standard` | `PLANNED` | 0 | — |
| `WO-06.3` | `wo3-upload-queue` | IUploader & Split-Stage UploadQueue IDataSource State Machine | `standard` | `PLANNED` | 0 | — |
| `WO-06.4` | `wo4-strava-uploader` | StravaUploader BYOA OAuth 2.0, KeyManager NVS, Rotated Token Persistence & Redactor | `standard` | `PLANNED` | 0 | — |
| `WO-06.5` | `wo5-portal-wiring` | SessionOverlay REST API, Authenticated TCX Download, Portal Tabs & Module Wiring | `standard` | `PLANNED` | 0 | — |
| `WO-06.6` | `wo6-hw-verification` | Hardware Session Lifecycle, Crash Recovery, TCX Download & Live Strava Upload Verification | `interactive` | `PLANNED` | 0 | — |

## 4. Session Log

- **2026-10-24:** Chief Architect drafted and locked `PR-06` Charter and 6-WO decomposition (`WO-06.1` through `WO-06.6`) incorporating owner decisions (`autoStartDelaySec = 5` default, `autoStartEnabled` toggle for manual UI start/stop, sequential execution after `PR-05`).
- **2026-10-26:** Chief Architect conducted a critical post-`PR-05` review of `PR-06` and updated `docs/power-physics-model.md`, `SPEC.md`, `PROGRAM.md`, `PROJECT.md`, and `WO-06.1`–`WO-06.6`: (1) replaced discrete `1..32` `resistance_level` in `RideRecord` with canonical `0..254` `internal_resistance` (`round(254 * pTilde)`, `255 = UNCALIBRATED`), (2) specified `2 Hz` (`500 ms`) sampling averaged into `1 Hz` `RideRecord` entries in lockstep with `PowerEngine` and `FTMSServer`, (3) mandated wraparound-safe signed `int32_t` `millis()` deltas and mid-ride NTP wall-clock back-computation, (4) wired `PowerEngine::computeVirtualSpeedKph(P)/3.6` (`0.245 v^3 + 3.53 v = P`) and Xert MPA 1 Hz coasting continuity into `TCXGenerator`, (5) aligned `inactivityTimeoutSec = 180 s`, 5-ride storage cap, `Redactor::registerSecret()` return-value check, `POST /api/session/discard`, and portal tab orders `80`/`90`.
