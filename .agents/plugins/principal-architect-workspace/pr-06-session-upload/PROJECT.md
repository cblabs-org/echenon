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
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-06.2
  slug: wo2-tcx-generator
  title: Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps)
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-06.3
  slug: wo3-upload-queue
  title: IUploader & Split-Stage UploadQueue IDataSource State Machine
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-06.4
  slug: wo4-strava-uploader
  title: StravaUploader BYOA OAuth 2.0, KeyManager NVS, Rotated Token Persistence
    & Redactor
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-06.5
  slug: wo5-portal-wiring
  title: SessionOverlay REST API, Authenticated TCX Download, Portal Tabs & Module
    Wiring
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
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
> **Depends on:** PR-02 (`8ccccb6`); executed sequentially after `PR-05` merges to `main` (owner decision 2026-10-24)
> **Branch:** `feat/pr-06-session-upload` (stacked off `main` after `PR-05`)
> **Spec sections:** §9.1–§9.6 (`SessionManager`, `RideStore`, `TCXGenerator`), §10.1–§10.6 (`IUploader`, `UploadQueue`, `StravaUploader`), §11.4 (`/api/session/*` + `/api/strava/*`), §11.5 (Session History & Upload Settings portal tabs), §13.7 (`4.1–4.14`)

---

# Part I — PR Charter (Chief Architect Locked)

## 1. Mandate & Architectural Invariants

Implement full ride session recording, crash-resilient PSRAM/LittleFS storage, streaming `1 Hz` compressed `TCX.gz` export, and background Strava v3 API upload so every ride on the Echelon EX-3 can be started/stopped from the portal UI (or auto-started after a configurable cadence threshold), recovered across power loss, downloaded as a `.tcx` / `.tcx.gz` file, or automatically uploaded to Strava.

### 1.1 Session Lifecycle & Owner Decisions (`SessionManager` + `RideStore`)
- **State Machine:** `IDLE` $\to$ `ACTIVE` $\leftrightarrow$ `PAUSED` $\to$ `ENDED`.
- **Owner Decision (2026-10-24) — Configurable Auto-Start & Manual UI Mode:**
  - `autoStartEnabled` (`bool`, default `true` in `IConfigurable` schema `"echenon_session"`): when `false`, pedalling does **not** auto-start a session — sessions start and end strictly via the portal UI buttons (`POST /api/session/start`, `POST /api/session/pause`, `POST /api/session/resume`, `POST /api/session/end`).
  - `autoStartDelaySec` (`uint16_t`, default **`5`** seconds, configurable `1–60 s`): when `autoStartEnabled == true` and state is `IDLE`, continuous `cadence > 0 RPM` for `autoStartDelaySec` transitions `IDLE -> ACTIVE`.
  - `inactivityTimeoutSec` (`uint16_t`, default **`300`** seconds / 5 minutes, configurable `30–3600 s`): continuous `cadence == 0 RPM` while `ACTIVE` ends the session and **trims the trailing 300 s of zero-cadence records** from `RideStore` (`SPEC.md` §9.1 / §9.3).
- **1 Hz Record Format (`RideRecord`, 8 bytes packed, `SPEC.md` §9.2):**
  - `uint16_t timestamp_offset_s`, `uint16_t power_watts`, `uint8_t cadence_rpm`, `uint8_t heart_rate_bpm`, `uint8_t resistance_level` (`1–32`), `uint8_t flags` (`bit 0 = hr_valid`, `bit 1 = pause_boundary`).
  - Allocated in `8 MB` OPI PSRAM (`ps_malloc` on ESP32, `malloc` fallback in native tests) for up to `14,400` records (`4 hours` = `115.2 KB`).
- **30-Second LittleFS Crash Recovery Checkpoints (`SPEC.md` §9.3):**
  - Every `30 s` (`30` records), append a delta chunk to `/rides/active.bin` on LittleFS.
  - On `begin()`, if `/rides/active.bin` exists, recover the interrupted session into the completed history ring (`/rides/ride_<idx>.bin`, max `10` stored rides) so no ride data is lost on power-off.

### 1.2 Streaming 1 Hz `TCXGenerator` (`SPEC.md` §9.4–§9.5)
- Generates valid Garmin Training Center Database v2 XML (`Activity Sport="Biking"`) with `<Creator><Name>Echenon EX-3</Name></Creator>`, `<Author>`, `<Lap>`, `<Extensions><LX xmlns=".../ActivityExtension/v2"><AvgWatts>`, `<MaxWatts>`, and `1 Hz` `<Trackpoint>` elements (`<Time>`, `<HeartRateBpm>` when `hr_valid`, `<Cadence>`, `<Extensions><TPX><Watts>`).
- **Coasting vs. Pause Semantics:**
  - Active coasting (`cadence == 0` while `ACTIVE`) **emits** `<Trackpoint>` elements with `Cadence=0, Watts=0`.
  - Pause intervals (`PAUSED` state between `pause()` and `resume()`) **omit** `<Trackpoint>` elements across the paused time gap (`SPEC.md` §9.5).
- **Streaming Gzip Compression:** Streams through a bounded **`4 KB` scratch buffer** into `/rides/ride_<id>.tcx.gz` (using `miniz` / `zlib` deflate with gzip wrapper) so arbitrary 4-hour rides never allocate full XML strings in heap.

### 1.3 Split-Stage `UploadQueue` (`IDataSource`) & `StravaUploader` (`SPEC.md` §10.1–§10.6)
- **Split-Stage `fetchUpdate()` within 15 s `DeadlineToken`:**
  - `UploadQueue` implements `IDataSource`, `IConfigurable`, and `IPreconditionContributor`.
  - Each `fetchUpdate(const FetchContext& ctx)` invocation executes **at most one non-blocking stage** (`STAGE_GENERATE_TCX_GZ` $\to$ `STAGE_REFRESH_TOKEN` $\to$ `STAGE_POST_UPLOAD` $\to$ `STAGE_POLL_STATUS`) and checks `ctx.deadline.remainingMs()` before starting network I/O.
  - Persistent queue of up to `5` pending sessions in LittleFS (`/rides/upload_queue.json`) with exponential backoff (`30 s`, `60 s`, `120 s`, `300 s`, max 5 attempts per ride).
- **BLE ScanInhibit (`#ifdef WITH_BLE`):**
  - Acquires `BLEManager::ScanInhibit` RAII guard during TLS stages (`STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`) when `#ifdef WITH_BLE` is defined, and compiles cleanly when `WITH_BLE` is absent (`SPEC.md` §10.6).
- **Strava BYOA OAuth 2.0 & Security (`SPEC.md` §10.3–§10.5):**
  - Stores `client_secret`, `access_token`, and `refresh_token` in encrypted NVS via `KeyManager` (`"strava"` namespace).
  - Generates a single-use random `state` nonce on `/api/strava/auth-url` and validates it on `/api/strava/callback` (rejecting mismatches with HTTP `403`).
  - **Rotated Refresh Token First:** On token refresh, persists the new `refresh_token` to NVS **before** `access_token` (`SPEC.md` §10.5).
  - Registers `client_secret`, `access_token`, and `refresh_token` with `Redactor::registerSecret()` immediately upon load and upon every OAuth exchange/refresh.

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
