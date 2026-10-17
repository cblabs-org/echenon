---
name: echenon-firmware-wo8
description: Handoff instructions for Work Order 8 — Strava Upload Pipeline
complexity: architect
---

> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-06](../../pr-06-session-upload/PROJECT.md). **Superseded by:** SPEC §10.1–§10.6 (split `UploadQueue` pipeline, BYOA OAuth) in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# WO8 — Strava Upload Pipeline

### Role Definition
> You are an expert senior software engineer and execution-focused worker agent. You are NOT the Principal Architect. Do not make high-level architectural decisions, do not branch, and do not plan. Your task is strictly limited to executing the checklist below.

### Objective
Implement `UploadManager`, `StravaUploader`, and `IUploader` — the cloud upload pipeline that handles OAuth 2.0 authentication, TCX file upload to Strava, and an extensible interface for future platforms. Uses the Foundation's `DataManager`/`IDataSource` for TWDT-safe background execution.

### Strict Boundary Box
- **DO NOT** modify sensor, power, BLE, or session engine code (WO2–WO6)
- **DO NOT** modify the overlay or session portal tab (WO7)
- **DO NOT** implement calibration UI — that is WO9
- The StravaUploader consumes TCX files produced by WO6's TCXGenerator

### Tooling Constraints
- Use PlatformIO MCP tools for build verification
- Reference: SPEC.md §10 (Upload Services), §10.1–10.6
- Read the framework-expert skill's DataManager / IDataSource reference docs
- Read the framework-expert skill for `Redactor` secret registration

### Line-Item Instructions

- [ ] Create `src/echenon/IUploader.h`:
  - Interface: `virtual bool upload(const char* filepath, const RideSummary& summary) = 0`
  - `virtual const char* getName() const = 0`
  - `virtual bool isConfigured() const = 0`

- [ ] Create `src/echenon/StravaUploader.h` / `.cpp`:
  - Implement `IUploader` and `IDataSource`
  - **OAuth 2.0 flow:**
    - `GET /api/strava/auth` → redirect to Strava authorization URL
    - `GET /api/strava/callback` → exchange authorization code for access/refresh tokens
    - Store tokens via Foundation `KeyManager` (encrypted NVS)
    - Register tokens with `Redactor::instance().registerSecret()` for log safety
    - Token refresh: auto-refresh expired access tokens using refresh token
  - **Upload via DataManager pipeline:**
    - `fetchUpdate(FetchContext& ctx)`:
      - Phase 1: Generate TCX (via TCXGenerator) — check `ctx.isExpired()` between chunks
      - Phase 2: Upload via `POST https://www.strava.com/api/v3/uploads` — multipart form with TCX file
    - Use `ctx.deadline` for bounded transport (15s total)
    - Health state machine with exponential backoff on failure
  - **Offline resilience:**
    - If upload fails, file stays in LittleFS retry queue
    - NVS stores queue of pending session IDs (up to 5)
    - Retry on next DataManager cycle when WiFi available

- [ ] Create `src/echenon/UploadManager.h` / `.cpp`:
  - Implements `ITickable`, `IConfigurable`, `IWebContributor`
  - Owns a `std::vector<IUploader*>` for extensibility
  - `registerUploader(IUploader*)` — adds an uploader to the registry
  - `registerRoutes()`: serves `upload_settings.js` portal tab via gzip pipeline
  - `registerTabs()`: registers "Upload" tab

- [ ] Create `web/upload_settings.js`:
  - Portal tab (standard `tab-changed` contract, IIFE wrapper)
  - Per-uploader card showing: name, connection status, last upload result
  - "Connect Strava" button → initiates OAuth flow
  - "Disconnect" button → clears tokens
  - Auto-upload toggle (enable/disable per uploader)

- [ ] Wire into `EchenonModule`:
  - Add `UploadManager` and `StravaUploader` as members
  - Register `StravaUploader` with `UploadManager`
  - Register `StravaUploader` as `IDataSource` with `DataManager`
  - Listen to `session/ended` EventBus event → queue upload

- [ ] Build — must compile with zero errors

### Verification Requirement
Build must compile cleanly. OAuth flow endpoints must be registered correctly. `IDataSource` lifecycle (health state machine, backoff) must be properly wired. Before committing, ask the user if they are happy.

### Issue Surfacing
If you discover a defect, upstream bug, or concern *outside your boundary box*, surface it in your Completion Report below — do not file issues directly.

---

## Completion Report
<!-- Written by the worker at task completion -->
*(To be filled by worker)*
