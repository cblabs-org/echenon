---
schema_version: 1
wo_id: WO-21.1
pr_id: PR-21
title: WahooUploader Engine
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/WahooUploader.h
- src/echenon/WahooUploader.cpp
forbidden_files:
- src/main.cpp
- src/echenon/EchenonModule.*
- src/echenon/SessionOverlay.*
- web/**
- test/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: f9229689ad541fa93417cf04fe10337df5c3f768731965c3468bd51b2ba45268
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_aa687d11b5b8
  commit_sha: c26e2cabe3b69cdc704af8dce9404831c74bd297
  files_modified:
  - src/echenon/WahooUploader.cpp
  - src/echenon/WahooUploader.h
  verification_summary: 'pio test -e native --verbose: ================ 261 test cases:
    261 succeeded in 00:00:00.485 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.1 — WahooUploader Engine

## 1. Role & Objective

Implement `WahooUploader` conforming to `IUploader` and `IConfigurable` (`"echenon_wahoo"`, schema v1)
for OAuth 2.0-authenticated workout uploads to Wahoo Cloud (`api.wahooligan.com`).

**Deliverables:**
- `WahooUploader.h` and `WahooUploader.cpp` in `src/echenon/`.
- OAuth 2.0 authorization URL construction with 128-bit single-use state nonce (10-minute TTL, wraparound-safe).
- Authorization code token exchange (`POST https://api.wahooligan.com/oauth/token`).
- Token refresh (`POST https://api.wahooligan.com/oauth/token` with `grant_type=refresh_token`).
- Rotated refresh-token-first persistence order in `ISecretStore` (`wahoo.refresh_token` stored before `wahoo.access_token`).
- Zero heap `std::function` (GRAVE-05) via injected function pointers or user context callbacks.
- `Redactor::registerSecret()` integration on credential load, exchange, and refresh.
- Multipart TCX upload (`POST https://api.wahooligan.com/v1/workouts`) with `workout[workout_type_id]=1` (Cycling) and `.tcx.gz` payload.
- Synchronous upload completion in `pollUploadStatus()` mapping remote workout ID.
- Status handling: 200/201 -> `SUCCESS`, 401 -> `PERMANENT_ERROR`, 429/5xx -> `RETRYABLE_ERROR`.

## 2. Strict Boundary Box & Constraints

1. **Abstractions Reuse:** Include `echenon/StravaUploader.h` for `ISecretStore`, `InMemorySecretStore`, `IHttpClient`, `HttpResponse`, `MultipartUploadRequest`. Do NOT re-declare or redefine them.
2. **Zero Heap `std::function` (GRAVE-05):** Follow `StravaUploader` / `IntervalsIcuUploader` patterns for callbacks (`SecretRegistrarFn`, `MetadataSaveFn`).
3. **Secret Storage Integrity (SPEC §10.4):** NVS secret keys: `wahoo.client_secret`, `wahoo.refresh_token`, `wahoo.access_token`. Register all credentials with Redactor.
4. **Scope:** `user_read workouts_write`.
5. **Config Namespace:** `"echenon_wahoo"`, schema v1 (persists non-secret metadata: `client_id`, `enabled`, `user_name`/`athlete_name`, `expires_at`).

**Forbidden:** No edits outside `src/echenon/WahooUploader.h` and `src/echenon/WahooUploader.cpp`.

## 3. Line-Item Instructions

- [ ] Create `src/echenon/WahooUploader.h`:
  - Inherit `IUploader` and `IConfigurable`.
  - Define constants: `AUTHORIZE_URL = "https://api.wahooligan.com/oauth/authorize"`, `TOKEN_URL = "https://api.wahooligan.com/oauth/token"`, `WORKOUTS_URL = "https://api.wahooligan.com/v1/workouts"`.
  - Secret key constants: `KEY_CLIENT_SECRET = "wahoo.client_secret"`, `KEY_REFRESH_TOKEN = "wahoo.refresh_token"`, `KEY_ACCESS_TOKEN = "wahoo.access_token"`.
  - Nonce constants: `STATE_TTL_MS = 600000u`, `STATE_HEX_LEN = 32u`, `TOKEN_EXPIRY_MARGIN_S = 60u`.
  - DI setters: `setSecretStore()`, `setKeyManager()`, `setHttpClient()`, `setFileSystem()`, `setSecretRegistrar()`, `setMetadataSaveCallback()`.
  - Core methods: `begin()`, `loadCredentials()`, `setClientCredentials()`, `disconnect()`, `buildAuthorizeUrl()`, `validateAndConsumeState()`, `exchangeAuthCode()`, `refreshToken()`.
  - `IUploader` interface: `serviceId()` -> `"wahoo"`, `serviceName()` -> `"Wahoo Cloud"`, `isConfigured()`, `isAuthenticated()`, `needsTokenRefresh()`, `isEnabled()`, `setEnabled()`, `postUpload()`, `pollUploadStatus()`.
  - `IConfigurable` interface: `getConfigNamespace()` -> `"echenon_wahoo"`, `getConfigSchemaVersion()` -> `1`, `buildConfig()`, `applyConfig()`.
- [ ] Create `src/echenon/WahooUploader.cpp`:
  - Implement random state nonce generation (using `esp_random()` on ESP32 or deterministic random on host/native).
  - Implement state nonce validation with wraparound-safe `int32_t` arithmetic and single-use invalidation.
  - Implement `exchangeAuthCode()`: URL-encoded POST to `TOKEN_URL`, JSON response deserialization (`ArduinoJson`), rotated refresh token write first, access token write second, expiry calculation, Redactor secret registration, metadata save.
  - Implement `refreshToken()`: similar rotated refresh token write.
  - Implement `postUpload()`: read `.tcx.gz` via `IRideFileSystem`, construct `MultipartUploadRequest` with Bearer token, boundary, `workout[workout_type_id]=1`, and `.tcx.gz` attachment. Dispatch via `IHttpClient::postMultipart()`. Parse 200/201 response for `"id"`, return `SUCCESS`.
  - Implement `pollUploadStatus()`: return `SUCCESS` immediately with `out_activity_id = upload_id`.
  - Implement `applyConfig()` / `buildConfig()`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` to ensure all existing native tests compile and pass cleanly with the new engine in place.
- Run `pa wo verify WO-21.1` to generate cryptographic receipt.

## Completion Report

_Pending execution._
