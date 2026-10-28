---
schema_version: 1
wo_id: WO-19.3
pr_id: PR-19
title: REST API Extensions
complexity: standard
status: PENDING
allowed_files:
- src/echenon/SessionOverlay.h
- src/echenon/SessionOverlay.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 5bebc5361a57fcb00b11ae437ca856921ec7d0d18ff42732f75e6e3868d6e3c9
completion: null
---

# WO-19.3 — REST API Extensions

## 1. Role & Objective

Extend the REST API to support multi-sink upload operations, per-sink status reporting,
and single-sink force-push/retry from the portal.

**Key Deliverables:**
- Extend `GET /api/session/history` response with per-sink sync status array.
- Add `POST /api/session/upload` with `{session_id, service_id}` for single-sink retry.
- Add `GET /api/upload/sinks` listing all registered sinks and states.
- Add `GET /api/upload/status` for aggregate sink health.
- Wire Intervals.icu credential routes: `GET /api/intervals/status`, `POST /api/intervals/credentials`, `POST /api/intervals/test`, `POST /api/intervals/disconnect`.

## 2. Strict Boundary Box & Constraints

1. Only modify `SessionOverlay.h` and `SessionOverlay.cpp`.
2. Use the existing `checkAuth` and `sendResp` helper patterns.
3. Follow SPEC.md §10 REST conventions.
4. Route handlers must use non-capturing lambdas with `this` pointer (GRAVE-05 compliance for httpd task).

**Forbidden:** No `UploadQueue.*`, `StravaUploader.*`, or `web/` edits.

## 3. Line-Item Instructions

- [ ] Extend `GET /api/session/history` response: add `"sync"` array per ride with `[{service_id, status, remote_id, remote_url, error}]`.
- [ ] Modify `POST /api/session/upload`: accept `{session_id, service_id}` body; enqueue single-sink retry via `UploadQueue`.
- [ ] Add `GET /api/upload/sinks`: iterate `UploadQueue` sinks, return `[{service_id, name, configured, authenticated, enabled}]`.
- [ ] Add `GET /api/upload/status`: return aggregate `{pending_count, sinks, last_error}`.
- [ ] Add `GET /api/intervals/status`: return `{configured, authenticated, athlete_id, athlete_name, enabled}`.
- [ ] Add `POST /api/intervals/credentials`: accept `{athlete_id, api_key}`, call `setApiCredentials()`.
- [ ] Add `POST /api/intervals/test`: call `testConnection()`, return result.
- [ ] Add `POST /api/intervals/disconnect`: call `disconnect()`, return `{ok: true}`.
- [ ] Wire `IntervalsIcuUploader*` reference in `SessionOverlay` (add member pointer + setter).
- [ ] Add PROGMEM asset serving for `/intervals_settings.js` (asset created in WO-19.5).

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.3` → `pio test -e native --verbose`.

## Completion Report

_Pending execution._
