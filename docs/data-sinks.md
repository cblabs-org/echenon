# Data Sinks — Multi-Sink Architecture Design Document

> **SPEC References:** §10 (Upload Services), §10.5 (Downstream Platform Sync),
> §11.2 (Portal Tabs), §14 (Ride Persistence)
>
> **PR:** PR-19 — Pluggable Data Sinks & Ride History Management

---

## 1. Multi-Sink Architecture Overview

Echenon's upload path supports multiple concurrent cloud destinations
("sinks") through a pluggable `IUploader` interface. The `UploadQueue`
manages up to `MAX_SINKS = 4` registered sinks and dispatches upload
jobs across all configured and enabled sinks when a session completes.

### Key Design Principles

1. **Fan-out on session end.** `enqueueSession(session_id)` creates one
   `UploadJob` per enabled sink, each identified by the composite key
   `(session_id, service_id)`.

2. **Sequential execution.** At most one sink performs TLS I/O at any
   time. Jobs drain in FIFO order, gated by `BLEManager::ScanInhibit`,
   so BLE FTMS / HRM streams never experience radio starvation.

3. **Shared `.tcx.gz` lifecycle.** TCX generation is expensive (PSRAM
   streaming gzip). The file is generated **once** per session. All
   sinks reference the same file path. Cleanup occurs only when every
   sink job for that session reaches `STAGE_COMPLETE` or `STAGE_FAILED`.

4. **Per-sink auto-sync toggles.** Each sink has an independent
   auto-sync preference (`auto_sync_strava`, `auto_sync_intervals`, `auto_sync_wahoo`)
   stored via `ConfigManager`. Disabled sinks are skipped during fan-out
   but can receive manually triggered uploads.

### Architecture Diagram

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
              ┌────────────────┼────────────────┬────────────────┐
              ▼                ▼                ▼                ▼
     ┌──────────────┐ ┌───────────────┐ ┌──────────────┐ ┌─────────────┐
     │StravaUploader│ │IntervalsIcu   │ │WahooUploader │ │ [Future]    │
     │(OAuth 2.0)   │ │Uploader       │ │(OAuth 2.0)   │ │ Webhook     │
     │              │ │(API Key)      │ │              │ │             │
     └──────────────┘ └───────────────┘ └──────────────┘ └─────────────┘
```

---

## 2. Provider Sync Strategy (Tier A/B/C/D)

Not every external platform requires a direct cloud uploader on the
ESP32. The design distinguishes four integration tiers:

### Tier A — Downstream Platform Sync (No ESP32 Code Required)

Platforms that ingest data from Strava automatically:

| Platform | Sync Method | Setup |
|---|---|---|
| Xert | Automatic Strava webhook | Connect Xert to Strava in Xert Online |
| Relive | Automatic Strava sync | Connect in Relive settings |
| VeloViewer | Automatic Strava sync | Connect in VeloViewer settings |

Xert additionally captures rides live via BLE FTMS on the tablet (Xert
EBC app), giving it two independent ingest paths.

### Tier B — Direct ESP32 Cloud Uploaders

Services with on-device `IUploader` implementations:

| Service | Auth Type | Implementation |
|---|---|---|
| Strava | OAuth 2.0 (BYOA) | `StravaUploader` — reference OAuth sink |
| Intervals.icu | REST API key | `IntervalsIcuUploader` — reference API-key sink |
| Wahoo Cloud | OAuth 2.0 (BYOA) | `WahooUploader` — Wahoo Cloud OAuth sink |

### Tier C — Future / Manual Export

| Service | Status | Reason |
|---|---|---|
| Garmin Connect | Manual TCX export | Garmin Health API requires enterprise partner approval |
| Generic Webhook | Future PR | HTTP POST for Home Assistant / NAS / InfluxDB |

### Tier D — BLE FTMS Clients (Not Upload Targets)

Zwift, Xert EBC, TrainerRoad, and SYSTM connect as real-time BLE FTMS
clients. They originate their own activity records. Echenon does not
upload to them.

---

## 3. Adding a New Sink: Implementation Checklist

To add a new data sink (e.g. "Wahoo Cloud"), follow these steps:

### 3.1 Implement `IUploader`

Create `src/echenon/WahooUploader.h` and `.cpp`:

```cpp
class WahooUploader : public IUploader, public IConfigurable {
public:
    // Identity
    const char* serviceId() const override { return "wahoo"; }
    const char* name() const override { return "Wahoo"; }
    const char* getConfigNamespace() const override { return "echenon_wahoo"; }

    // State
    bool isConfigured() const override;
    bool isAuthenticated() const override;
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;
    bool isEnabled() const override;

    // Stage operations
    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override;
    UploadStageResult postUpload(uint32_t session_id, const std::string& tcx_gz_path,
                                  uint32_t timeout_ms, std::string& out_upload_id,
                                  std::string& err_msg) override;
    UploadStageResult pollUploadStatus(const std::string& upload_id,
                                       uint32_t timeout_ms,
                                       std::string& out_activity_id,
                                       std::string& err_msg) override;
};
```

### 3.2 Register the Sink

In `src/main.cpp` `setup()`, inside the `#ifdef WITH_SESSION` block:

```cpp
uploadQueue.registerSink(&echenonModule.wahooUploader());
```

### 3.3 Add Portal Pill

Create `web/wahoo_settings.js` following the same card/form layout as
`web/upload_settings.js` (Strava) or `web/intervals_settings.js`
(Intervals.icu). Register it in `registerTabs()`:

```cpp
registry.addTab("Wahoo", nullptr, 30, kWahooIcon, "/wahoo_settings.js");
```

### 3.4 Add REST API Routes

Add configuration routes in `SessionOverlay::registerRoutes()`:

```
/api/wahoo/status       GET   — connection status
/api/wahoo/credentials  POST  — save credentials
/api/wahoo/test         POST  — test connection
/api/wahoo/disconnect   POST  — clear credentials
```

### 3.5 Add Native Tests

Create `test/test_wahoo_uploader/` with tests for:

- Request building and response parsing (success, 401, 429, 5xx)
- Credential storage and retrieval
- `isConfigured()` / `isAuthenticated()` state transitions

### 3.6 Update Documentation

- Add the sink to SPEC.md §10.3, §10.5 Tier B table
- Add the portal pill to SPEC.md §11.2 Apps tab table
- Update this document's Tier B table

---

## 4. Per-Sink Configuration and Secret Storage

### Secret Storage Architecture

API keys and OAuth tokens are stored encrypted via `KeyManager` NVS and
registered with `Redactor::registerSecret()`. Each sink's secrets use a
namespaced key prefix:

| Sink | NVS Namespace | Keys |
|---|---|---|
| Strava | `echenon_strava` | `strava.client_id`, `strava.client_secret`, `strava.access_token`, `strava.refresh_token`, `strava.token_expiry` |
| Intervals.icu | `echenon_intervals` | `intervals.api_key`, `intervals.athlete_id` |
| Wahoo Cloud | `echenon_wahoo` | `wahoo.client_secret`, `wahoo.refresh_token`, `wahoo.access_token` |

### ISecretStore Abstraction

The `ISecretStore` interface abstracts secret storage, allowing native
test injection via `InMemorySecretStore`. Production code uses
`KeyManagerSecretStore` backed by the framework's `KeyManager`.

### Redactor Integration

Every secret registered with `Redactor::registerSecret()` is
automatically scrubbed from all log output. Token refreshes must
re-register new tokens and check the returned `RegisterResult`.

### Auto-Sync Toggle Storage

Per-sink auto-sync preferences are stored via `ConfigManager` under the
sink's `IConfigurable` namespace:

| Key | Default | Description |
|---|---|---|
| `auto_sync_strava` | `true` | Auto-upload to Strava on session end |
| `auto_sync_intervals` | `true` | Auto-upload to Intervals.icu on session end |
| `auto_sync_wahoo` | `true` | Auto-upload to Wahoo Cloud on session end |

---

## 5. Portal Pill Registration Pattern

Each data sink contributes a configuration pill to the **Apps** portal tab,
registered via `ITabRegistry::addTab()` with an ascending order value:

```
Apps (top-level tab, order 60)
├── Strava          (order 10)  — web/upload_settings.js
├── Intervals.icu   (order 20)  — web/intervals_settings.js
├── Wahoo Cloud     (order 30)  — web/wahoo_settings.js
├── [Future sinks at order 35+]
├── Level Mapping   (order 25)  — calibration wizard
├── Power Tuning    (order 30)  — recalibration
└── Diagnostics     (order 35)  — event log viewer
```

### Pill Layout Convention

Each sink pill follows a consistent card layout:

1. **Header:** Service name and connection status badge
2. **Authentication section:**
   - OAuth sinks: `[Connect]` / `[Disconnect]` buttons
   - API-key sinks: Inline form with `[Test Connection]` / `[Remove]`
3. **Auto-sync toggle:** `☑ Auto-upload enabled`
4. **Queue status:** Pending count, last upload result, last error

### Stored Rides Pill (Echenon Tab)

The **Stored Rides** card lives in the main **Echenon** tab (not the
Apps tab) and shows:

- Recent rides with date, duration, average power, and distance
- Per-sink sync badges: `✅ Synced`, `⏳ Pending`, `❌ Failed (429)`
- One-click actions: `[Download TCX]`, `[Delete]`, `[Force Push ▾]`

Data sources: `GET /api/session/history` and `GET /api/echenon/runs`.

---

## 6. REST API Conventions

### Multi-Sink Upload API

| Route | Method | Auth | Description |
|---|---|---|---|
| `/api/upload/sinks` | `GET` | ✓ | List all registered sinks |
| `/api/upload/status` | `GET` | ✓ | Aggregate upload health |
| `/api/session/upload` | `POST` | ✓ | Force-push / retry |
| `/api/session/history` | `GET` | ✓ | Session list with per-sink sync status |
| `/api/session/download` | `GET` | ✓ | Download TCX file |

### Per-Sink Configuration API Pattern

Each sink follows the same REST route pattern:

```
/api/{service_id}/status       GET   — connection status
/api/{service_id}/credentials  POST  — save/update credentials
/api/{service_id}/test         POST  — test connection with saved credentials
/api/{service_id}/disconnect   POST  — clear credentials and revoke tokens
```

### Response Format: `/api/upload/sinks`

```json
{
  "sinks": [
    {
      "service_id": "strava",
      "name": "Strava",
      "configured": true,
      "authenticated": true,
      "enabled": true,
      "pending_jobs": 0,
      "last_error": null
    },
    {
      "service_id": "intervals",
      "name": "Intervals.icu",
      "configured": true,
      "authenticated": true,
      "enabled": true,
      "pending_jobs": 1,
      "last_error": null
    },
    {
      "service_id": "wahoo",
      "name": "Wahoo Cloud",
      "configured": true,
      "authenticated": true,
      "enabled": true,
      "pending_jobs": 0,
      "last_error": null
    }
  ]
}
```

### Response Format: `/api/session/history` (per-sink sync status)

```json
{
  "sessions": [
    {
      "session_id": 1730140320,
      "start_time": 1730140320,
      "duration_s": 2520,
      "avg_power_w": 178,
      "sync_status": [
        {
          "service_id": "strava",
          "status": "completed",
          "remote_id": "20757005151",
          "remote_url": "https://www.strava.com/activities/20757005151",
          "error": null
        },
        {
          "service_id": "intervals",
          "status": "pending",
          "remote_id": null,
          "remote_url": null,
          "error": null
        },
        {
          "service_id": "wahoo",
          "status": "completed",
          "remote_id": "987654321",
          "remote_url": "https://api.wahooligan.com/v1/workouts/987654321",
          "error": null
        }
      ]
    }
  ]
}
```

### Force-Push / Retry: `POST /api/session/upload`

Single-sink retry:
```json
{ "session_id": 1730140320, "service_id": "intervals" }
```

All-sink retry (omit `service_id`):
```json
{ "session_id": 1730140320 }
```

---

## 7. BLE Coexistence Invariant (ADR-11)

Every sink upload executes inside a `BLEManager::ScanInhibit` token
context. Simultaneous uploads across multiple sinks are **prohibited**;
jobs execute sequentially with inter-stage yielding to ensure zero
packet drop on FTMS and HRM BLE characteristics.

The `UploadQueue` drains sinks in registration order for deterministic
behaviour:

```
Session End → enqueueSession()
  → Job(session_1, strava, GENERATE)      ← shared .tcx.gz
  → Job(session_1, strava, REFRESH)       ← ScanInhibit
  → Job(session_1, strava, UPLOAD)        ← ScanInhibit
  → Job(session_1, strava, POLL)          ← ScanInhibit
  → Job(session_1, intervals, REFRESH)    ← ScanInhibit (no-op)
  → Job(session_1, intervals, UPLOAD)     ← ScanInhibit
  → Job(session_1, intervals, POLL)       ← ScanInhibit (no-op)
  → Job(session_1, wahoo, REFRESH)        ← ScanInhibit
  → Job(session_1, wahoo, UPLOAD)         ← ScanInhibit
  → Job(session_1, wahoo, POLL)           ← ScanInhibit (synchronous completion)
  → cleanup .tcx.gz (all sinks done)
```

---

## 8. Wahoo Cloud Sink (OAuth 2.0 Sink #3)

### 8.1 Architectural Role & Registry Placement

Wahoo Cloud is integrated as **Sink #3** in Echenon's 4-sink `UploadQueue`
registry (`MAX_SINKS = 4`). It provides direct cloud synchronization to
the Wahoo ecosystem (Wahoo SYSTM, ELEMNT companion apps, and Wahoo Cloud
training history) without requiring an intermediate phone app or secondary
export.

Like Strava, Wahoo Cloud implements the full `IUploader` and `IConfigurable`
(`"echenon_wahoo"`, schema v1) interfaces. It participates in the standard
multi-sink lifecycle:
- Fan-out on session completion creates `UploadJob(session_id, "wahoo")`.
- Execution is strictly sequential and guarded by `BLEManager::ScanInhibit`.
- TCX generation is shared: it consumes the canonical `.tcx.gz` generated in
  `STAGE_GENERATE_TCX_GZ`.

### 8.2 Bring-Your-Own-App (BYOA) Developer Setup

Echenon adopts a Bring-Your-Own-App (BYOA) model for Wahoo Cloud. No shared
client secrets are baked into the open-source firmware. Riders obtain their
own free API credentials directly from Wahoo:

1. **Navigate to the Developer Portal:**
   Visit [Wahoo Developer Portal](https://developers.wahooligan.com) and sign in
   with your Wahoo account credentials.
2. **Create New Application:**
   Click **Create Application** and fill in:
   - **Application Name:** `Echenon` (or your preferred identifier)
   - **Redirect URI:** `http://echenon.local/api/wahoo/callback` (or your device's
     fixed IP, e.g. `http://192.168.1.150/api/wahoo/callback`)
   - **Scopes:** Select `user_read` and `workouts_write`
3. **Copy Credentials:**
   Retrieve the generated **Client ID** and **Client Secret**.
4. **Save in Echenon Portal:**
   Open the **Apps** tab → **Wahoo Cloud** pill (`/wahoo_settings.js`, order 30).
   Paste the Client ID and Client Secret, then click **Save Credentials**.
   The Client Secret is stored encrypted in NVS and is treated as write-only.

### 8.3 Required Scopes & Permissions

Wahoo Cloud requires two OAuth scopes:

| Scope | Purpose |
|---|---|
| `user_read` | Read athlete profile information (first name, last name, username) for display in the portal pill badge (`Connected: John Doe`) |
| `workouts_write` | Permission to upload and record new workouts via `POST https://api.wahooligan.com/v1/workouts` |

During the OAuth authorization redirect, Echenon requests `scope=user_read+workouts_write`.
Omitting either scope prevents profile identification or workout creation.

### 8.4 OAuth 2.0 Flow Architecture

```
Rider Browser                    Echenon ESP32                 Wahoo Cloud API
     │                                 │                              │
     │ 1. GET /api/wahoo/auth-url      │                              │
     │────────────────────────────────>│                              │
     │ 2. Return authorize URL + state │ (generates 128-bit nonce)   │
     │<────────────────────────────────│                              │
     │                                 │                              │
     │ 3. Redirect to https://api.wahooligan.com/oauth/authorize       │
     │───────────────────────────────────────────────────────────────>│
     │                                 │                              │
     │ 4. Rider consents & Wahoo redirects to /api/wahoo/callback     │
     │<───────────────────────────────────────────────────────────────│
     │ 5. GET /api/wahoo/callback?code=...&state=...                  │
     │────────────────────────────────>│                              │
     │                                 │ 6. validateAndConsumeState() │
     │                                 │    (single-use, 10 min TTL)  │
     │                                 │ 7. POST /oauth/token         │
     │                                 │─────────────────────────────>│
     │                                 │ 8. Return tokens + user JSON │
     │                                 │<─────────────────────────────│
     │                                 │ 9. Rotated refresh token 1st │
     │                                 │    Access token 2nd          │
     │                                 │    Redactor registration 3rd │
     │ 10. Redirect /#tab-apps         │                              │
     │<────────────────────────────────│                              │
```

#### State Nonce Security (CSRF Defense)
- `WahooUploader::buildAuthorizeUrl()` generates a 128-bit cryptographically
  secure random nonce formatted as a 32-character hexadecimal string.
- The nonce has a 10-minute time-to-live (`STATE_TTL_MS = 600000u`) and is
  checked with wraparound-safe unsigned/signed 32-bit `millis()` arithmetic.
- When `GET /api/wahoo/callback` is invoked, `validateAndConsumeState()` validates
  the matching state and immediately zeroes/deactivates the nonce. Replay attacks
  are impossible.

#### Rotated Refresh-Token-First Persistence
Wahoo rotates OAuth refresh tokens on each token exchange. A crash or power loss
during refresh must never corrupt or lose the credentials:
1. When `/oauth/token` returns HTTP 200/201 with `refresh_token`, `access_token`,
   and `expires_in`:
2. **Step 1:** Write the new rotated `refresh_token` to `ISecretStore` (`wahoo.refresh_token`).
   If this write fails, abort immediately with `RETRYABLE_ERROR` without altering
   in-memory state.
3. **Step 2:** Write the new `access_token` to `ISecretStore` (`wahoo.access_token`).
4. **Step 3:** Persist non-secret metadata (`expires_at`, `user_name`) via `ConfigManager`.
5. **Step 4:** Register both new tokens with `Redactor::instance().registerSecret()`.

#### Redactor Secret Protection
Every secret (`wahoo.client_secret`, `wahoo.refresh_token`, `wahoo.access_token`) is
registered with the framework's `Redactor`. Any occurrence of these token strings
is scrubbed from serial console logs, crash dumps, and diagnostics before output.

### 8.5 TCX Upload Framing & Payload

Workouts are created by sending a multipart/form-data POST request to Wahoo's
workouts endpoint:

```
POST https://api.wahooligan.com/v1/workouts
Host: api.wahooligan.com
Authorization: Bearer <wahoo.access_token>
Content-Type: multipart/form-data; boundary=----EchenonWahooBoundary<session_id>
```

#### Framing Specification:
- **`workout[workout_type_id]`:** String `"1"` (Wahoo workout type ID 1 = Cycling).
- **`workout[file]`:** The compressed TCX binary file:
  - Header: `Content-Disposition: form-data; name="workout[file]"; filename="echenon_<session_id>.tcx.gz"`
  - Content-Type: `application/gzip`
  - Body: Binary stream read directly from the shared `.tcx.gz` file on LittleFS/PSRAM.
- **Boundary:** Fixed per session: `----EchenonWahooBoundary<session_id>`.

#### Synchronous Status Handling
In contrast to Strava (which returns an asynchronous upload processing ticket
that must be repeatedly polled via `GET /api/v3/uploads/{id}`), Wahoo Cloud
creates the activity synchronously. When `POST /v1/workouts` returns HTTP 200/201,
the response body already contains the completed workout JSON:

```json
{
  "id": 987654321,
  "id_str": "987654321",
  "name": "Echenon Indoor Ride",
  "starts": "2026-10-28T18:32:00.000Z",
  "workout_type_id": 1
}
```

Because processing is synchronous:
- `postUpload()` extracts `id` / `id_str` and assigns it to `out_upload_id`.
- `pollUploadStatus()` executes as a zero-latency stage: it immediately returns
  `UploadStageResult::SUCCESS` and assigns `out_activity_id = upload_id`.
  No secondary network round-trips or backoff polling loops are needed.

### 8.6 Status Codes & Error Handling Taxonomy

The `WahooUploader` maps HTTP status codes into `UploadStageResult`:

| HTTP Status | Stage Result | Action & Retry Policy |
|---|---|---|
| **200 / 201** | `SUCCESS` | Stage succeeded. In `postUpload()`, workout ID is parsed; in `refreshToken()`, tokens are persisted. |
| **401** | `PERMANENT_ERROR` | Unauthorized. Bad client secret, expired authorization code, or revoked refresh token. Job halts permanently; user must re-authenticate. |
| **429** | `RETRYABLE_ERROR` | Rate limited by Wahoo API. `UploadQueue` backs off exponentially (2 s, 4 s, 8 s...) up to 5 attempts before terminal failure. |
| **5xx / <= 0** | `RETRYABLE_ERROR` | Server error (500, 502, 503) or network transport failure (DNS error, timeout). Backoff and retry. |
| **400 / 403 / 404 / 422** | `PERMANENT_ERROR` | Malformed multipart request, invalid workout type ID, or corrupt file. Not retryable. |

### 8.7 Multi-Sink Interaction & Shared Lifecycle

When a ride ends:
1. `UploadQueue::enqueueSession(session_id)` inspects all registered sinks.
2. If `auto_sync_wahoo` is `true` (and the sink is configured/authenticated), an
   `UploadJob(session_id, "wahoo")` is enqueued alongside Strava and Intervals.icu jobs.
3. The queue first generates the canonical `.tcx.gz` file once in `STAGE_GENERATE_TCX_GZ`.
4. Jobs drain sequentially across sinks in registration order:
   - Strava: Refresh → Upload → Poll (async)
   - Intervals.icu: Refresh (no-op) → Upload (synchronous) → Poll (no-op)
   - Wahoo Cloud: Refresh (conditional) → Upload (synchronous) → Poll (immediate)
5. Each sink job executes under an isolated `BLEManager::ScanInhibit` lock, ensuring
   active BLE FTMS and HRM sensor streaming is never disrupted by TLS handshakes.
6. Only when **all** sink jobs for that session reach a terminal state
   (`STAGE_COMPLETE` or `STAGE_FAILED`) is the shared `.tcx.gz` file deleted from storage.

### 8.8 Configuration & Secret Storage

Configuration is partitioned between non-volatile secret storage (`ISecretStore` / `KeyManager`)
and structured settings (`ConfigManager` under namespace `"echenon_wahoo"`):

| Storage | Key | Type | Description |
|---|---|---|---|
| `ISecretStore` (Encrypted NVS) | `wahoo.client_secret` | string | BYOA OAuth Client Secret |
| `ISecretStore` (Encrypted NVS) | `wahoo.refresh_token` | string | Rotated OAuth 2.0 refresh token |
| `ISecretStore` (Encrypted NVS) | `wahoo.access_token` | string | Active OAuth 2.0 bearer access token |
| `ConfigManager` (`echenon_wahoo`) | `client_id` | string | BYOA OAuth Client ID |
| `ConfigManager` (`echenon_wahoo`) | `enabled` | bool | Master sink toggle (`auto_sync_wahoo`) |
| `ConfigManager` (`echenon_wahoo`) | `user_name` / `athlete_name` | string | Display name of authenticated Wahoo rider |
| `ConfigManager` (`echenon_wahoo`) | `expires_at` | uint32 | Epoch timestamp of access token expiry |
