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
   auto-sync preference (`auto_sync_strava`, `auto_sync_intervals`)
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
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
     ┌──────────────┐ ┌───────────────┐ ┌─────────────┐
     │StravaUploader│ │IntervalsIcu   │ │ [Future]    │
     │(OAuth 2.0)   │ │Uploader       │ │ Wahoo /     │
     │              │ │(API Key)      │ │ Webhook     │
     └──────────────┘ └───────────────┘ └─────────────┘
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

### Tier C — Future / Manual Export

| Service | Status | Reason |
|---|---|---|
| Garmin Connect | Manual TCX export | Garmin Health API requires enterprise partner approval |
| Wahoo Cloud | Future PR | OAuth 2.0 available; deferred |
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

---

## 5. Portal Pill Registration Pattern

Each data sink contributes a configuration pill to the **Apps** portal tab,
registered via `ITabRegistry::addTab()` with an ascending order value:

```
Apps (top-level tab, order 60)
├── Strava          (order 10)  — web/upload_settings.js
├── Intervals.icu   (order 20)  — web/intervals_settings.js
├── [Future sinks at order 30+]
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
  → cleanup .tcx.gz (all sinks done)
```
