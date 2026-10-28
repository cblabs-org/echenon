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
status: PLANNED
approved_at: null
github_pr: null
work_orders: []
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
Implement `WahooUploader` conforming to `IUploader` and `IConfigurable`. Include
OAuth 2.0 URL construction, token exchange, refresh logic, multipart TCX upload framing,
and `KeyManagerSecretStore` adapter.

### WO-21.2 — System Wiring & REST Endpoints
Wire `WahooUploader` into `EchenonModule`, `src/main.cpp`, and `UploadQueue`.
Implement `/api/wahoo/*` REST endpoints in `SessionOverlay` and register route handlers.

### WO-21.3 — Portal UI: Apps Tab Pill & Stored Rides Badges
Create `web/wahoo_settings.js` (order 30 under Apps tab). Update `web/stored_rides.js`
to render per-sink badges and retry actions for Wahoo. PROGMEM asset compilation.

### WO-21.4 — Native Unit Tests
Author `test/test_native/test_wahoo_uploader.cpp`. Test authorization, refresh,
multipart framing, status parsing, and 3-sink queue progression. Verify verification floor.

### WO-21.5 — SPEC & Documentation Sync
Update SPEC.md §10 and `docs/data-sinks.md` with Wahoo Cloud sink documentation.

### WO-21.6 — Hardware Verification & Soak
Build firmware for `ex3-waveshare-nano`, flash to test device, verify boot,
portal pills, and multi-sink queue readiness.

---

## 4. Outcome & Session Log
- **2026-10-28:** PR-21 chartered and locked by Chief Architect following PR-19 graduation.
