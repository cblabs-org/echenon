---
schema_version: 1
wo_id: WO-19.2
pr_id: PR-19
title: Intervals.icu API-Key Sink
complexity: standard
status: PENDING
allowed_files:
- src/echenon/IntervalsIcuUploader.h
- src/echenon/IntervalsIcuUploader.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 8f03946cccc1e18f78968bf59f3093a3927e0047f337e629f9475c242ce49750
completion: null
---

# WO-19.2 — Intervals.icu API-Key Sink

## 1. Role & Objective

Implement `IntervalsIcuUploader` conforming to `IUploader` and `IConfigurable` for
API-key-authenticated uploads to Intervals.icu. Reference implementation for non-OAuth sinks.

**Deliverables:** `serviceId()` → `"intervals"`, API-key auth, `POST /api/v1/athlete/{id}/activities`
with `.tcx.gz` body, synchronous completion, `ISecretStore` + Redactor integration.

## 2. Strict Boundary Box & Constraints

1. **Reuse abstractions:** Include `StravaUploader.h` for `ISecretStore`, `IHttpClient`, `HttpResponse`. Do NOT duplicate.
2. **Zero Heap std::function (GRAVE-05):** `SecretRegistrarFn` set once at boot.
3. **Secret Storage (§10.4):** Keys: `intervals.api_key`, `intervals.athlete_id`.
4. **HTTP Auth:** Basic Auth: `Basic base64(API_KEY:)` (key as username, empty password).
5. **Config namespace:** `"echenon_intervals"`, schema v1.

**Forbidden:** No `StravaUploader.*`, `UploadQueue.*`, `web/`, or `test/` edits.

## 3. Line-Item Instructions

- [ ] Create `IntervalsIcuUploader.h`: inherit `IUploader` + `IConfigurable`.
- [ ] DI: `setSecretStore()`, `setHttpClient()`, `setFileSystem()`, `setSecretRegistrar()`.
- [ ] `begin()`, `loadCredentials()`, `setApiCredentials()`, `testConnection()`, `disconnect()`.
- [ ] `isConfigured()` = both fields non-empty. `isAuthenticated()` = same. `needsTokenRefresh()` = false.
- [ ] `postUpload()`: read `.tcx.gz`, POST with Basic Auth, handle 401→PERMANENT, 429→RETRYABLE.
- [ ] `pollUploadStatus()`: SUCCESS immediately.
- [ ] Config: `"echenon_intervals"`, persist `athlete_id`, `enabled`, `athlete_name`.
- [ ] Static `base64Encode()` helper.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.2` → `pio test -e native --verbose`.

## Completion Report

_Pending execution._
