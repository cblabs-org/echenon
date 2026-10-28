---
schema_version: 1
charter:
  pr_id: PR-21
  programme_id: prog-default
  slug: pr-21-wahoo-sink
  title: Wahoo Cloud Data Sink
  branch: feat/pr-21-wahoo-sink
  base_branch: main
  depends_on:
  - PR-19
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
  charter_hash: 9ec62b7477a848e655aa2335bc510eaa87914b68f5f1a8009216327f1e8a2ce0
status: APPROVED
approved_at: '2026-10-28T22:58:39.349600Z'
github_pr: null
work_orders:
- id: WO-21.1
  slug: wo1-wahoo-uploader
  title: WahooUploader Engine
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: b738ed5890d3ae574cc9a9b1ebf3be0e9a497e5c
- id: WO-21.2
  slug: wo2-system-wiring
  title: System Wiring & REST Endpoints
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 3e9b495da64eeea315f389aa0fc289e092f66a58
- id: WO-21.3
  slug: wo3-portal-ui
  title: Portal UI — Wahoo Pill & Stored Rides Badges
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 2c059c3a37b8185490986182e57a43007be32676
- id: WO-21.4
  slug: wo4-native-tests
  title: Native Unit Tests
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: d079e3309a4d09c0bc12c5e477c28c936df54b28
- id: WO-21.5
  slug: wo5-spec-doc-sync
  title: SPEC & Documentation Sync
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: be5e696c35ce6e51da67d7785bf5d75c257ab49b
- id: WO-21.6
  slug: wo6-hardware-verification
  title: Hardware Verification & Soak
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
---

# PR-21 — Wahoo Cloud Data Sink

> **Programme:** Echenon Firmware
> **Target Base:** `main` (commit `e5f9d6e`)
> **Depends On:** PR-19 ✅ (Pluggable Multi-Sink Architecture)
> **Spec References:** SPEC.md §10 (Upload Services), §10.5 (Provider Sync Strategy), §11.2 (Portal Surfaces)

---

## 1. PR Charter: Scope, Goals, & Non-Goals

### 1.1 Mandate & Problem Statement

In PR-19, `UploadQueue` was refactored from a single-uploader architecture to a
pluggable multi-sink registry supporting up to `MAX_SINKS = 4` concurrent `IUploader`
implementations, with Strava (OAuth 2.0) and Intervals.icu (API-Key) operating as
the initial active sinks.

The Xert connected accounts list and user training setups frequently feature **Wahoo Cloud**
(Wahoo Fitness / Wahoo SYSTM / ELEMNT ecosystem). Unlike Garmin Connect (whose Health API
is restricted to corporate B2B partners), Wahoo provides a public developer OAuth 2.0 API
(`api.wahooligan.com/v1/workouts`) permitting direct consumer activity uploads.

Adding Wahoo Cloud as Sink #3 enables riders in the Wahoo ecosystem to automatically sync
their passive-board 1 Hz power, cadence, and heart rate sessions directly into Wahoo Cloud
without requiring intermediary third-party relay tools.

### 1.2 Core Scope & Goals

1. **`WahooUploader` Implementation:**
   - Implement `WahooUploader` conforming to `IUploader` and `IConfigurable` (`"echenon_wahoo"`).
   - OAuth 2.0 Bring-Your-Own-App (BYOA) authentication:
     - Authorization URL generation: `https://api.wahooligan.com/oauth/authorize`
     - Token exchange & refresh: `https://api.wahooligan.com/oauth/token`
     - Activity upload: `POST https://api.wahooligan.com/v1/workouts` with `multipart/form-data`
       containing `.tcx.gz` (or `.tcx`) and `workout[workout_type_id]=1` (Cycling).
     - Status handling: Map HTTP 200/201 to `SUCCESS`, 401 to `PERMANENT_ERROR`, 429/5xx to `RETRYABLE_ERROR`.
   - Credentials stored encrypted in NVS via `KeyManagerSecretStore` (`wahoo.client_secret`,
     `wahoo.refresh_token`, `wahoo.access_token`) and registered with `Redactor::registerSecret()`.

2. **UploadQueue Registration & Wiring:**
   - Register `WahooUploader` as Sink #3 in `UploadQueue` (`_sinks[2]`, within `MAX_SINKS = 4`).
   - Wire `WahooUploader` in `src/main.cpp` and `EchenonModule`.
   - Add REST API endpoints in `SessionOverlay` (`/api/wahoo/status`, `/api/wahoo/credentials`,
     `/api/wahoo/auth`, `/api/wahoo/callback`, `/api/wahoo/disconnect`).

3. **Portal UI (Apps Tab Pill & Stored Rides Badge):**
   - Register a dedicated Wahoo configuration pill under the **Apps** tab (`/wahoo_settings.js`, order 30)
     via `ITabRegistry::addTab()`.
   - Pill features: connection status badge, client ID/secret form, `[Connect with Wahoo]` button,
     auto-sync toggle (`auto_sync_wahoo`), and disconnect action.
   - Stored Rides pill (`/stored_rides.js`) automatically reflects Wahoo sync status badges:
     - `Wahoo: ✅ Synced` (with link to Wahoo workout if available)
     - `Wahoo: ⏳ Pending` / `❌ Failed` with manual retry trigger.

4. **Native Unit Test Suite:**
   - Full test coverage in `test/test_native/test_wahoo_uploader.cpp`:
     - Authorization URL generation & state nonce validation.
     - Token exchange & token refresh with NVS persistence.
     - Multipart form framing and `.tcx.gz` attachment.
     - HTTP response status code taxonomy (200, 401, 429, 500).
     - Multi-sink queue integration test with Strava, Intervals.icu, and Wahoo.

5. **Documentation & Specification Synchronization:**
   - Update SPEC.md §10.3 and §10.5 to detail the Wahoo Cloud direct integration.
   - Update `docs/data-sinks.md` with Wahoo sink specification.

### 1.3 Non-Goals

- Direct control of Wahoo smart trainers (this PR is an activity upload sink, not trainer control).
- Real-time cloud telemetry streaming (data flows via BLE FTMS locally; Wahoo Cloud receives post-ride TCX).
- Modifying `StravaUploader` or `IntervalsIcuUploader` (all existing sinks remain isolated).

---

## 2. Architectural Requirements & Invariants

1. **BLE Coexistence & Radio Invariant (ADR-11):**
   Wahoo uploads must execute strictly within a `BLEManager::ScanInhibit("upload_queue")` context.
   Uploads occur sequentially across sinks — Wahoo will never initiate TLS I/O while Strava or
   Intervals.icu is mid-upload.

2. **Zero Heap `std::function` (GRAVE-05):**
   All callbacks, EventBus subscriptions, and route handlers must use non-capturing lambdas
   or static functions with explicit `user_ctx`.

3. **Single `.tcx.gz` Lifecycle:**
   Wahoo shares the single PSRAM-compressed `.tcx.gz` generated in `STAGE_GENERATE_TCX_GZ`.
   The file is deleted only when all registered and enabled sinks (Strava, Intervals, Wahoo)
   have reached terminal state (`STAGE_COMPLETE` or `STAGE_FAILED`).

4. **Secret Storage Integrity (SPEC §10.4):**
   OAuth tokens are stored encrypted in NVS via `KeyManager` and registered with `Redactor`
   to ensure zero credential leakage in serial logs.

---

## 3. Implementation Plan & Work Order Breakdown

### WO-21.1 — WahooUploader Engine
- **Slug:** `wo1-wahoo-uploader`
- **Complexity:** Standard (`Model: "flash"`)
- **Allowed Files:** `src/echenon/WahooUploader.h`, `src/echenon/WahooUploader.cpp`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Implement `WahooUploader` conforming to `IUploader` and `IConfigurable` (`"echenon_wahoo"`, schema v1).
  - OAuth 2.0 authorization URL construction with 128-bit single-use state nonce (10-minute TTL, wraparound-safe).
  - Token exchange (`POST https://api.wahooligan.com/oauth/token`) and token refresh.
  - Rotated refresh-token-first persistence order in `ISecretStore` (`wahoo.refresh_token` stored before `wahoo.access_token`).
  - Zero heap `std::function` (GRAVE-05).
  - `Redactor::registerSecret()` integration on credential load, exchange, and refresh.
  - Multipart TCX upload (`POST https://api.wahooligan.com/v1/workouts`) with `workout[workout_type_id]=1` (Cycling) and `.tcx.gz` payload.
  - Synchronous upload completion in `pollUploadStatus()` mapping remote workout ID.
  - Status handling: 200/201 -> `SUCCESS`, 401 -> `PERMANENT_ERROR`, 429/5xx -> `RETRYABLE_ERROR`.

### WO-21.2 — System Wiring & REST Endpoints
- **Slug:** `wo2-system-wiring`
- **Complexity:** Standard (`Model: "flash"`)
- **Allowed Files:** `src/echenon/EchenonModule.h`, `src/echenon/EchenonModule.cpp`, `src/echenon/SessionOverlay.h`, `src/echenon/SessionOverlay.cpp`, `src/main.cpp`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Wire `WahooUploader` into `EchenonModule` (member & accessor).
  - Register Wahoo tab in `setupTabs()` (`registry.addTab("Wahoo Cloud", "Apps", 30, nullptr, "/wahoo_settings.js")`).
  - Wire `WahooUploader` in `src/main.cpp`: inject `KeyManager`, register with `UploadQueue` (Sink #3) and `app.registerModule()`, and inject into `SessionOverlay`.
  - Include `echenon/WahooUploader.cpp` under `#ifdef UNIT_TEST` in `SessionOverlay.cpp`.
  - Implement REST API endpoints:
    - `GET /api/wahoo/status`
    - `POST /api/wahoo/credentials`
    - `GET /api/wahoo/auth-url`
    - `GET /api/wahoo/callback` (redirecting to `/#tab-apps`)
    - `POST /api/wahoo/disconnect`
  - Serve `/wahoo_settings.js` under `#if __has_include("include/portal_assets/wahoo_settings_js.hpp")`.

### WO-21.3 — Portal UI: Apps Tab Pill & Stored Rides Badges
- **Slug:** `wo3-portal-ui`
- **Complexity:** Standard (`Model: "flash"`)
- **Allowed Files:** `web/wahoo_settings.js`, `web/stored_rides.js`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Author `web/wahoo_settings.js`: configuration pill under Apps tab (order 30) featuring connection status badge, client ID/secret form, `[Connect with Wahoo]` button, auto-sync toggle, and disconnect button.
  - Update `web/stored_rides.js`: update `buildRemoteUrl()` for `sid === 'wahoo'` and support multi-sink sync status badges (`Wahoo: ✅ Synced`, `⏳ Pending`, `❌ Failed`) with retry trigger.

### WO-21.4 — Native Unit Tests
- **Slug:** `wo4-native-tests`
- **Complexity:** Standard (`Model: "flash"`)
- **Allowed Files:** `test/test_native/test_wahoo_uploader.cpp`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Full GoogleTest coverage for `WahooUploader`:
    - `BuildAuthorizeUrlAndValidateSingleUseStateNonce`
    - `StateNonceTtlExpiryAndMillisWraparound`
    - `RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt`
    - `RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens`
    - `RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor`
    - `PostUploadMultipartFramingAndStatusResponses`
    - `PollUploadStatusReturnsSuccessImmediately`
    - `ThreeSinkQueueProgression` (UploadQueue integration testing 3 simultaneous sinks).

### WO-21.5 — SPEC & Documentation Sync
- **Slug:** `wo5-spec-doc-sync`
- **Complexity:** Mechanical (`Model: "flash_lite"`)
- **Allowed Files:** `SPEC.md`, `docs/data-sinks.md`
- **Required Skills:** None
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Update SPEC.md §10.3 and §10.5 with Wahoo Cloud sink specification and portal tab table (§11.2).
  - Update `docs/data-sinks.md` with complete documentation for Wahoo Cloud integration.

### WO-21.6 — Hardware Verification & Soak
- **Slug:** `wo6-hardware-verification`
- **Complexity:** Interactive (`Model: "inherit"`)
- **Allowed Files:** `docs/`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:**
  - Dev tag increment per Rule 03 (`v0.X.Y-dev.N`).
  - Build and flash firmware for `ex3-waveshare-nano`.
  - Serial monitor verification of 3-sink registration, route registration, portal asset serving, and heap health.

---

## 4. Outcome & Session Log
- **2026-10-28:** PR-21 chartered and locked by Chief Architect following PR-19 graduation.
- **2026-10-28:** Tier 2 Principal Architect authored Part II Work Order decomposition (WO-21.1 to WO-21.6) and initialized all specification work orders.
