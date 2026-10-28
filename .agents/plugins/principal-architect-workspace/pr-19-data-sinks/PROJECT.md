---
schema_version: 1
charter:
  pr_id: PR-19
  programme_id: prog-default
  slug: pr-19-data-sinks
  title: Pluggable Data Sinks & Ride History Management
  branch: feat/pr-19-data-sinks
  base_branch: main
  depends_on:
  - PR-14
  - PR-06
  allowed_modules:
  - src/echenon/
  - src/main.cpp
  - web/
  - test/
  - docs/
  - SPEC.md
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: 53b172290b1e083d9c229f456eb68bbf34fe552befc3505ed2539edda2aedb36
status: STAGED_FOR_AUDIT
approved_at: '2026-10-28T18:21:42.991677Z'
github_pr: 17
work_orders:
- id: WO-19.1
  slug: wo1-multi-sink-engine
  title: Multi-Sink UploadQueue Engine
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 488469a61d17d4ed709d59fbe2ecc1287d10a6a5
- id: WO-19.2
  slug: wo2-intervals-icu-sink
  title: Intervals.icu API-Key Sink
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: c28c5f9f0301337cb43a490e945ebc168a99eb77
- id: WO-19.3
  slug: wo3-rest-api-extensions
  title: REST API Extensions
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: fcd437d373fb11a503afa64530ff5f359241e06f
- id: WO-19.4
  slug: wo4-stored-rides-pill
  title: Stored Rides Portal Pill
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: a4a065913f8667de01d264546e7193bb954b8c5e
- id: WO-19.5
  slug: wo5-strava-pill-refactor
  title: Strava Pill Refactor & Connected Apps Polish
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 2f64605a748818444d8fa521ea0bbe014d8192e5
- id: WO-19.9
  slug: wo9-main-cpp-wiring
  title: IntervalsIcuUploader main.cpp Wiring
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 6ccc63811fcb08a762b4b34c62794301f3209a87
- id: WO-19.6
  slug: wo6-native-tests
  title: Native Unit Tests
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: ff8982ec0eb68444ec70158b0ac428720e0104a1
- id: WO-19.7
  slug: wo7-spec-doc-sync
  title: SPEC & Doc Sync
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: d869a2b2d6d06b7b25b7fc701d942a49c4d87319
- id: WO-19.8
  slug: wo8-hardware-verification
  title: Hardware Verification & Soak
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: 6525cc62bf10bcbe9326fa56c01c8093dbeae5ac
- id: WO-19.10
  slug: wo10-build-fixes
  title: Firmware Build Fixes
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 41978fba3551cb8b4ae697c76857a5202c5c9252
---

# PR-19 — Pluggable Data Sinks & Ride History Management

> **Programme:** Echenon Firmware
> **Target Base:** `main` (commit `c95391a`)
> **Depends On:** PR-14 ✅, PR-06 ✅
> **Spec References:** SPEC.md §10 (Upload Services), §11.2–§11.4 (Portal Surfaces), §14 (Ride Persistence)

---

## 1. PR Charter: Scope, Goals, & Non-Goals

### 1.1 Mandate & Problem Statement

In PR-06, cloud activity upload was introduced with `StravaUploader` as the
sole destination. While `IUploader` was designed as an abstract interface with
`serviceId()`, `isConfigured()`, `isAuthenticated()`, `refreshToken()`,
`postUpload()`, and `pollUploadStatus()`, the concrete `UploadQueue` is wired
to a single `IUploader* _uploader` pointer, `UploadJob` tracks one upload per
session (no `service_id` dimension), and the portal UI has a single
Strava-specific pill under the **Apps** tab.

Users frequently train across multiple ecosystems. The Xert screenshot from
the owner's live training setup shows seven sync/connect providers:

| Provider | Category | API Type |
|---|---|---|
| Strava | Cloud sync | OAuth 2.0 — **live on Echenon (PR-06)** |
| Garmin Connect | Cloud sync | OAuth 2.0 — partner-restricted |
| Intervals.icu | Analytics | REST API key |
| Wahoo Cloud | Ecosystem sync | OAuth 2.0 |
| Xert | Training platform | Strava webhook + BLE FTMS live capture |
| Zwift | Virtual cycling | BLE FTMS client (not an upload target) |
| Segments / WhatsApp | Social / sharing | N/A |

Furthermore, while rides are safely recorded in LittleFS and PSRAM
(`RidePersistence` and `RideStore`), the web portal lacks a dedicated interface
to browse historic rides, view per-service synchronisation status, trigger
manual force-pushes/retries, or manage flash storage.

### 1.2 Provider Sync Strategy

Not every external platform requires a direct cloud uploader on the ESP32.
The design distinguishes three integration tiers:

#### Tier A — Downstream Platform Sync (No ESP32 Code Required)

Some platforms ingest data from Strava automatically and need no direct
Echenon integration (SPEC.md §10.5):

| Platform | Sync Method | Setup |
|---|---|---|
| **Xert** | Automatic Strava webhook | Connect Xert to Strava once in Xert Online → Sync settings |
| **Relive** | Automatic Strava sync | Connect in Relive settings |
| **VeloViewer** | Automatic Strava sync | Connect in VeloViewer settings |

When Echenon uploads a ride to Strava, Xert pulls the full 1 Hz power stream,
runs its MPA model, checks for breakthroughs and updates the athlete's fitness
signature — with zero Xert-specific code on the ESP32.

Additionally, Xert captures rides live when the rider uses the Xert EBC app on
the tablet: the app connects to Echenon's BLE FTMS service (`0x1826`,
`0x2ACC`, `0x2AD2` at 2 Hz) and records the session directly, syncing to Xert
Online on completion. This means Xert has **two independent ingest paths** —
live BLE FTMS capture and Strava webhook sync — neither requiring a dedicated
`IUploader` implementation.

#### Tier B — Direct ESP32 Cloud Uploaders (On-Device `IUploader`)

These services benefit from direct upload because:
- The rider may not always have the tablet open (console-off standalone rides,
  quick free rides).
- The session recording in `RideStore` captures the full 1 Hz data stream
  with internal resistance, HR, and calibrated power, which may differ from
  what a FTMS client captures.
- Auto-upload on session completion provides a "ride and forget" experience.

| Service | Auth Type | PR-19 Tier | Notes |
|---|---|---|---|
| **Strava** | OAuth 2.0 (BYOA) | **Core (live)** | Reference OAuth sink; verified Activity `20757005151` |
| **Intervals.icu** | REST API key | **Core** | Simple `POST /api/v1/athlete/{id}/activities` — no OAuth redirect dance |

#### Tier C — Future / Manual Export

| Service | Status | Reason |
|---|---|---|
| **Garmin Connect** | Manual TCX export | Garmin Health API requires corporate enterprise partner approval; manual upload via Stored Rides pill works seamlessly |
| **Wahoo Cloud** | Future PR | OAuth 2.0 developer registration available; deferred to post-v1 |
| **Generic Webhook** | Future PR | HTTP POST multipart/TCX for Home Assistant / NAS / InfluxDB integrations |

#### Tier D — BLE FTMS Clients (Not Upload Targets)

Zwift, Xert EBC, TrainerRoad, and SYSTM connect to Echenon's BLE FTMS service
as real-time clients. They originate their own activity records. Echenon does
not upload to them.

### 1.3 Core Scope & Goals

1. **Multi-Sink `UploadQueue` Architecture:**
   - Evolve `UploadQueue` from holding a single `_uploader` pointer to a
     multi-sink registry: `registerSink(IUploader*)` with `MAX_SINKS = 4`.
   - Extend `UploadJob` with a `service_id` field, creating a
     `(session_id, service_id)` composite key.
   - Upon session completion, automatically queue upload jobs for **all
     configured and enabled** sinks.
   - Maintain strict sequential execution: at most one sink performs TLS I/O
     at any time, gated by `BLEManager::ScanInhibit` so BLE FTMS / HRM
     streams never experience radio starvation during mid-ride uploads or
     post-ride queue draining.

2. **Intervals.icu Sink (API-Key Reference):**
   - Implement `IntervalsIcuUploader` conforming to `IUploader`.
   - Authentication via personal API key (no OAuth redirect; rider pastes
     athlete ID and API key into the portal pill).
   - `postUpload()`: `POST https://intervals.icu/api/v1/athlete/{id}/activities`
     with `Content-Type: application/octet-stream` body containing `.tcx.gz`.
   - `pollUploadStatus()`: Intervals.icu processes synchronously, so this
     stage returns `SUCCESS` immediately with the activity URL.
   - `refreshToken()`: No-op (API keys do not expire).

3. **Portal "Stored Rides" Management Pill (Echenon Tab):**
   - In the main **Echenon** portal tab, add a **Stored Rides** card/pill
     alongside the existing Live Metrics and Status cards.
   - Data source: `GET /api/session/history` (extended to include per-sink
     sync status) and `GET /api/echenon/runs`.
   - Render recent rides with:
     - Date & time (UTC epoch → browser local time).
     - Duration, average power, distance (km), average HR.
     - Per-sink sync badges:
       - `Strava: ✅ Synced` with direct clickable link
         `https://www.strava.com/activities/<id>`.
       - `Intervals: ⏳ Pending` or `❌ Failed (429)`.
     - One-click `[Download TCX]` action (`GET /api/session/download?id=<n>`).
     - `[Delete]` action (`DELETE /api/echenon/runs/<id>`).
     - `[Force Push / Retry]` per-sink action
       (`POST /api/session/upload` with `{session_id, service_id}`).

4. **Per-Service Pills in the Apps Tab:**
   - Each data sink gets its own sub-pill under the **Apps** top-level tab,
     registered via `ITabRegistry::addTab()`:

     ```
     Apps (order 60)
     ├── Strava         (order 10, /upload_settings.js)   — existing, refactored
     ├── Intervals.icu  (order 20, /intervals_settings.js) — new
     └── [Future sinks at order 30+]
     ```

   - Each pill displays:
     - Service name, icon, and connection status
       (`Connected: <Athlete>`, `Not Configured`, `Token Expired`).
     - Auto-sync toggle per service
       (`auto_sync_strava`, `auto_sync_intervals`).
     - Authentication controls:
       - **OAuth sinks (Strava):** `[Connect]` redirect → consent → callback,
         `[Disconnect]` to revoke and clear tokens.
       - **API-key sinks (Intervals.icu):** Inline form for athlete ID and
         API key, `[Test Connection]` button, `[Remove]` to clear.
     - Upload queue status: pending count, last upload result, last error.

### 1.4 Non-Goals

- **Real-time cloud telemetry streaming.** Live data flows via BLE FTMS
  (to apps on the tablet) and local WebSocket (to the ride page). Cloud
  uploading is a post-session concern.
- **In-browser TCX decompression or editing.** The portal displays ride
  metadata; TCX files are downloaded as-is.
- **Closed / proprietary vendor SDKs.** All communication is standard HTTPS
  REST over ESP32 `WiFiClientSecure` with certificate bundle validation.
- **Garmin Connect direct upload.** Garmin's Health API requires corporate
  enterprise partner approval. Riders use the Stored Rides pill to download
  TCX and upload manually to `connect.garmin.com`.
- **Xert direct upload.** Xert syncs from Strava automatically (Tier A) and
  captures rides live via BLE FTMS on the tablet. A dedicated `IUploader`
  would be redundant.

---

## 2. Architectural Requirements & Invariants

1. **BLE Coexistence & Radio Invariant (ADR-11):**
   Every sink upload must execute inside a `BLEManager::ScanInhibit` token
   context. Simultaneous uploads across multiple sinks are **prohibited**;
   jobs execute sequentially with inter-stage yielding to ensure zero packet
   drop on FTMS and HRM BLE characteristics. The `UploadQueue` must drain
   sinks in registration order for deterministic behaviour.

2. **Zero Heap `std::function` (GRAVE-05):**
   All EventBus subscriptions, timer callbacks, and route handlers must use
   static free functions or non-capturing lambdas with explicit `user_ctx`.
   The existing `NowEpochFn`, `NtpSyncedFn`, and `TcxGenerateFn` on
   `UploadQueue` are already documented as GRAVE-05 deviations (set once at
   boot, never reallocated); new sink registration must not introduce
   additional `std::function` heap allocations.

3. **PSRAM Memory Safety:**
   TCX generation and gzip compression must remain in PSRAM
   (`StreamingGzipEncoder`), maintaining the < 4 KB stack frame budget for
   `httpd` tasks. Each sink upload shares the same generated `.tcx.gz` file;
   the file is deleted only after **all** sinks have completed or permanently
   failed for that session.

4. **Design System & Responsive UI:**
   UI components must follow framework design tokens (`--card-bg`,
   `--text-muted`, `--primary`, `--success`, `--warning`, `--error`) and
   display cleanly on mobile, desktop, and Lenovo Tab P12 viewports. Each
   Apps pill must follow the same card/form layout conventions as the existing
   Strava pill (`web/upload_settings.js`).

5. **Secret Storage (SPEC.md §10.4):**
   API keys and OAuth tokens are stored encrypted via `KeyManager` NVS and
   registered with `Redactor::registerSecret()`. Each sink's secrets use a
   namespaced key prefix (e.g. `strava.client_secret`,
   `intervals.api_key`). The `ISecretStore` abstraction allows native test
   injection via `InMemorySecretStore`.

6. **Single `.tcx.gz` Per Session, Shared Across Sinks:**
   TCX generation is expensive (PSRAM streaming gzip). The `UploadQueue`
   generates the `.tcx.gz` file **once** per session in `STAGE_GENERATE_TCX_GZ`.
   All registered sinks reference the same file. Artifact cleanup
   (`removeFileEverywhere()`) occurs only when the last sink job for that
   session reaches `STAGE_COMPLETE` or `STAGE_FAILED`.

---

## 3. Implementation Plan & Work Order Breakdown

### WO-19.1 — Multi-Sink UploadQueue Engine
Refactor `UploadQueue` from single-`_uploader` to a multi-sink registry.
Extend `UploadJob` with `service_id`. Implement composite-key job lookup,
per-sink state serialization in `upload_queue.json`, and shared `.tcx.gz`
lifecycle (generate once, delete when all sinks complete). Update
`enqueueSession()` to fan out jobs across all registered and enabled sinks.

### WO-19.2 — Intervals.icu API-Key Sink
Implement `IntervalsIcuUploader` conforming to `IUploader` and
`IConfigurable` (`"echenon_intervals"`). API-key authentication (no OAuth
redirect flow). `KeyManager` NVS storage with `Redactor` integration.
Portal pill JS (`web/intervals_settings.js`) with athlete ID, API key form,
connection test, and auto-sync toggle.

### WO-19.3 — REST API Extensions
Extend `/api/session/history` response to include per-sink sync status
array (`[{service_id, status, remote_id, remote_url, error}]`).
Add `POST /api/session/upload` with `{session_id, service_id}` for
single-sink force-push/retry. Add `GET /api/upload/status` for aggregate
sink health. Extend `GET /api/strava/status` pattern to generic
`GET /api/upload/sinks` listing all registered sinks and their states.

### WO-19.4 — Stored Rides Portal Pill
Build the **Stored Rides** card in the Echenon dashboard tab. JS module
(`web/stored_rides.js`) fetching `/api/session/history` with per-sink sync
badges, one-click TCX download, delete, and per-sink force-push/retry
buttons. Framework design tokens for responsive card layout.

### WO-19.5 — Strava Pill Refactor & Connected Apps Polish
Refactor `web/upload_settings.js` from a standalone Strava-only panel to
a modular per-sink pill pattern. Register Strava and Intervals.icu pills
under the **Apps** tab via `ITabRegistry::addTab()`. Ensure consistent
card layout, status badges, and auto-sync toggle across all sink pills.

### WO-19.9 — IntervalsIcuUploader main.cpp Wiring
Add DI wiring in `src/main.cpp` `setup()` to connect `IntervalsIcuUploader`
to `KeyManager`, `Redactor`, `UploadQueue`, and `SessionOverlay`. Six lines
inside the existing `#ifdef WITH_SESSION` block.

### WO-19.6 — Native Unit Tests
Native test suite covering:
- Multi-sink `UploadQueue` dispatch (fan-out, sequential execution, shared
  `.tcx.gz` lifecycle).
- `IntervalsIcuUploader` request building and response parsing (success,
  401, 429, 5xx).
- Per-sink job state serialization and deserialization.
- Composite-key job lookup and per-sink retry/backoff.
- Precondition evaluation for multi-sink configuration.

### WO-19.7 — SPEC & Doc Sync
Update SPEC.md §10 (Upload Services) to reflect multi-sink architecture,
per-sink pills, and Intervals.icu integration. Update §10.5 (Downstream
Platform Sync) with Xert BLE FTMS live capture insight. Update §11.2
(Portal Tabs) with Stored Rides pill and per-sink Apps pills. Update
`docs/` with `data-sinks.md` design document.

### WO-19.8 — Hardware Verification & Soak
Bench test ride with concurrent dual-sink sync (Strava + Intervals.icu):
- Verify auto-upload fans out to both sinks on session end.
- Verify ScanInhibit sequential execution (no BLE drops during upload).
- Verify Stored Rides pill shows per-sink sync badges with direct links.
- Verify force-push/retry from Stored Rides pill.
- Verify Strava and Intervals.icu pills display correct connection state.

### WO-19.10 — Firmware Build Fixes
Fix two compilation errors in the hardware target (`ex3-waveshare-nano`):
1. `src/main.cpp`: Add `setKeyManager(KeyManager*)` to `IntervalsIcuUploader`
   (following `StravaUploader`'s `KeyManagerSecretStore` adapter pattern) and
   update main.cpp wiring to call `setKeyManager()` instead of `setSecretStore()`.
2. `src/echenon/SessionOverlay.cpp`: Fix PROGMEM asset lambda capture for
   `intervals_settings_js.h` — move `#include` before the lambda or add
   explicit captures.

---

## 4. Provider Sync Architecture Diagram

```
                    ┌─────────────────────────────────┐
                    │       SessionManager             │
                    │  (1 Hz RideRecord → RideStore)   │
                    └──────────┬──────────────────────┘
                               │ session.end EventBus
                               ▼
                    ┌─────────────────────────────────┐
                    │         UploadQueue              │
                    │   (IDataSource, sequential)      │
                    │                                  │
                    │  ┌─ STAGE_GENERATE_TCX_GZ ──┐   │
                    │  │  (shared .tcx.gz, once)   │   │
                    │  └───────────┬───────────────┘   │
                    │              │ fan-out per sink   │
                    │  ┌───────────┴───────────────┐   │
                    │  │   Per-Sink Job Pipeline    │   │
                    │  │  REFRESH → POST → POLL    │   │
                    │  │  (ScanInhibit guarded)     │   │
                    │  └───────────────────────────┘   │
                    └──────────┬──────────────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
     ┌──────────────┐ ┌───────────────┐ ┌─────────────┐
     │ StravaUploader│ │IntervalsIcu   │ │ [Future]    │
     │ (OAuth 2.0)  │ │Uploader       │ │ Wahoo /     │
     │              │ │(API Key)      │ │ Webhook     │
     └──────────────┘ └───────────────┘ └─────────────┘

  Tier A (No ESP32 Code):
  ─────────────────────
  Xert ← Strava webhook (automatic)
  Xert ← BLE FTMS live capture (tablet app)
  Relive, VeloViewer ← Strava sync

  Tier D (BLE FTMS Clients, not upload targets):
  ───────────────────────────────────────────────
  Zwift, Xert EBC, TrainerRoad, SYSTM
```

---

## 5. Portal Tab Structure (Post PR-19)

```
┌─────────┬──────────┬──────────┬──────────┐
│ Echenon │   Apps   │  System  │   BLE    │  ← Top-level tabs
└────┬────┴────┬─────┴────┬─────┴──────────┘
     │         │          │
     │         │          ├── Level Mapping (order 25)
     │         │          ├── Power Tuning  (order 30)
     │         │          └── Diagnostics   (order 35)
     │         │
     │         ├── Strava         (order 10) — OAuth 2.0 config pill
     │         ├── Intervals.icu  (order 20) — API key config pill
     │         └── [Future: Wahoo (order 30), Webhook (order 40)]
     │
     ├── Live Metrics      (existing dashboard card)
     ├── Echenon Status    (existing status card)
     ├── Stored Rides      (NEW — ride history, per-sink sync, download/delete)
     └── Open Ride Page    (existing link card)
```

---

## 6. Outcome & Session Log

- **2026-10-27 21:43:** Charter authored and locked (initial draft).
- **2026-10-27 21:52:** Charter revised with provider sync strategy (Tier A/B/C/D
  taxonomy), Xert BLE FTMS live capture insight, per-sink Apps pills, Stored
  Rides management pill, and polished work order breakdown. Pending re-lock.
