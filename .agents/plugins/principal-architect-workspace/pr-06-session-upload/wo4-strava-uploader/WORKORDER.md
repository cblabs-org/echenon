---
schema_version: 1
wo_id: WO-06.4
pr_id: PR-06
title: StravaUploader BYOA OAuth 2.0, KeyManager NVS, Rotated Token Persistence &
  Redactor
complexity: standard
status: PENDING
allowed_files:
- src/echenon/StravaUploader.h
- src/echenon/StravaUploader.cpp
- test/test_native/test_strava_uploader.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: f840a0ab844e77d5cc5084a1b56542081704ea13a6aadc4d7d106fe754443580
completion: null
---

# WO-06.4: StravaUploader BYOA OAuth 2.0, KeyManager NVS, Rotated Token Persistence & Redactor

## 1. Objective & Scope

Implement `StravaUploader` (`src/echenon/StravaUploader.h` / `.cpp`) implementing `IUploader` with Bring-Your-Own-App (BYOA) Strava OAuth 2.0, single-use CSRF `state` parameter verification, encrypted NVS secret storage via `KeyManager`, rotated `refresh_token`-first persistence, and `Redactor::registerSecret()` integration with return-value verification (`SPEC.md` §10.3–§10.5).

## 2. Detailed Requirements

### 2.1 BYOA Credentials & Secret Management (`SPEC.md` §10.3–§10.5)
1. **Storage Strategy:**
   - Non-secret metadata (`client_id`, `token_expires_at`, `athlete_name`, `enabled`) stored via `ConfigManager` / injectable persistence interface.
   - Secrets (`client_secret`, `access_token`, `refresh_token`) stored in encrypted NVS via `KeyManager` (`"strava"` namespace on ESP32; injectable `ISecretStore` adapter so native unit tests can verify exact write ordering and secret registration).
2. **Mandatory Log Redaction (`Redactor::registerSecret`, `SPEC.md` §10.4):**
   - Register `client_secret`, `access_token`, and `refresh_token` with `Redactor::registerSecret()` (via injectable `SecretRegistrarFn` returning `bool`, wired to `Redactor::registerSecret()` in firmware) whenever credentials are loaded at boot, saved via settings, or updated via OAuth token exchange / token refresh.
   - Check the `bool` return value of `registerSecret()`; if it returns `false` (indicating the Redactor slot table is full), log a warning (without logging the secret value!).
3. **Single-Use OAuth `state` Nonce (`SPEC.md` §10.4):**
   - `std::string buildAuthorizeUrl(const std::string& redirect_uri, uint32_t now_ms = 0)`: generates a 32-hex-character random `state` nonce (`esp_random()` on ESP32), stores it with a `10-minute` (`600,000 ms`) TTL using wraparound-safe signed `(int32_t)(now_ms - state_issued_ms)` math, and returns `https://www.strava.com/oauth/authorize?client_id=...&response_type=code&redirect_uri=...&approval_prompt=auto&scope=activity:write&state=...`.
   - `bool validateAndConsumeState(const std::string& state, uint32_t now_ms)`: returns `true` **only** if `state` is non-empty, matches the active nonce, and `(int32_t)(now_ms - state_issued_ms) <= 600000`, and immediately invalidates the nonce so replay attempts return `false` (`HTTP 403`).
4. **Rotated `refresh_token`-First Persistence (`SPEC.md` §10.4):**
   - When handling the JSON response from `POST https://www.strava.com/oauth/token` (both authorization code exchange and `grant_type=refresh_token`):
     1. Parse `refresh_token`, `access_token`, and `expires_at`.
     2. **Persist `refresh_token` to `ISecretStore` / `KeyManager` FIRST.** If saving `refresh_token` fails, abort immediately so the old refresh token is never overwritten with a mismatched state.
     3. Persist `access_token` second and `expires_at` third.
     4. Register both new tokens with the `Redactor` and check the `bool` return value.
5. **Multipart `tcx.gz` Upload & Status Polling (`SPEC.md` §10.2–§10.5):**
   - `postUpload()`: builds `multipart/form-data` request to `https://www.strava.com/api/v3/uploads` with `data_type="tcx.gz"`, `external_id="echenon_<session_id>.tcx.gz"`, `trainer="1"`, and streams `/rides/ride_<id>.tcx.gz` via the injected `IHttpClient` / `IHttpTransport` (using `esp_crt_bundle_attach` on ESP32, never `setInsecure()`).
   - `pollUploadStatus()`: calls `GET https://www.strava.com/api/v3/uploads/<upload_id>` and parses `activity_id` / `error` / `status`.
   - Injectable `IHttpClient` interface so native unit tests can test OAuth code exchange, token rotation ordering, HTTP `401`/`429`/`500` error mapping, multipart framing, and status polling deterministically without live sockets.

### 2.2 Native Unit Tests (`test/test_native/test_strava_uploader.cpp`)
- Test `buildAuthorizeUrl()` and `validateAndConsumeState()` (valid match, single-use replay rejection, mismatch rejection, TTL expiry rejection).
- Test rotated `refresh_token` persistence order using an ordered write-log spy (`verify write("refresh_token") occurs at index 0 before write("access_token") at index 1`), and verify abort if `refresh_token` write fails.
- Test secret registration spy verifying `client_secret`, `access_token`, and `refresh_token` are registered on load, exchange, and refresh, and verify handling when `registerSecret()` returns `false`.
- Test `postUpload()` and `pollUploadStatus()` response parsing (`201 Created`, `200 OK` still processing -> `IN_PROGRESS`, `200 OK` with `activity_id` -> `SUCCESS`, `429 Too Many Requests` -> `RETRYABLE_ERROR`, duplicate upload error -> `PERMANENT_ERROR`).

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus `test_strava_uploader.cpp`.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
