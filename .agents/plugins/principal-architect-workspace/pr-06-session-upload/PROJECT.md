---
schema_version: 1
charter:
  pr_id: "PR-06"
  slug: "pr-06-session-upload"
  title: "Session Recording + TCX + Strava Upload"
  branch: "feat/pr-06-session-upload"
  base_branch: "main"
  allowed_modules:
    - "src/echenon/"
    - "docs/"
    - "framework/"
    - "platformio.ini"
  charter_locked: false
status: PLANNED
---

## 2. Scope

_See legacy content below._


# PR-06: Session Recording + TCX + Strava Upload (Phase 4 — Session + TCX + Strava)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-02 merged
> **Dev Phase:** Phase 4 — Session + TCX + Strava
> **Depends on:** PR-02 (no PR-03 edge; BLE interaction is compile-time optional)
> **Spec sections:** [SPEC](../../../../SPEC.md) §9.1–§9.6 (session recording), §10.1–§10.6 (upload services), §11.4–§11.5 (portal surfaces, trust boundaries), §13.7 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 4
> **Parallel with:** PR-04a, PR-05

## 1. Mandate

Implement session lifecycle management and Strava upload.

1. **`SessionManager`** — Auto-start on cadence, crash-recovery checkpoints,
   trailing-zero trim (audit U-03).
2. **`RideStore`** — PSRAM buffer with LittleFS checkpoints, sized against the
   2.75 MiB storage partition.
3. **`TCXGenerator`** — 1 Hz TCX written as **`tcx.gz`** (zlib stream in the generator).
4. **`UploadQueue`** (`IDataSource`) — **the only execution path.** The pipeline is
   split: each stage is a separate `fetchUpdate()` call within the 15 s deadline —
   generate `tcx.gz` → refresh token → `POST /uploads` → poll `GET /uploads/{id}`.
   A Strava duplicate response is treated as success. Offline sessions stay queued.
5. **`IUploader` / `StravaUploader`** — `IUploader` describes capabilities only; it
   does not execute uploads. `StravaUploader` implements the BYOA (bring-your-own
   API app) credential model: a validated OAuth `state` parameter, a documented
   Authorization Callback Domain, persistence of rotated refresh tokens, and Redactor
   re-registration on every refresh (audit U-04).
6. **Scan inhibit** — Uploads hold `BLEManager::ScanInhibit` **only under
   `#ifdef WITH_BLE`**, so this PR builds and runs with or without PR-03.
7. **`SessionOverlay` (session API part)** — `/api/session/*` REST API, the
   authenticated TCX download and the Session History portal tab. The standalone
   `/overlay` page and `/ws/session` WebSocket belong to PR-09.

No Echenon class may collide with framework names: no `ConfigManager`,
`DataManager` or `Config.h` (audit U-02).

**Verification:** Rides auto-detected; crash recovery restores a session; valid
`tcx.gz` generated; Strava upload succeeds including duplicate handling; the offline
queue drains when WiFi returns; builds pass with and without `WITH_BLE`. Native
tests cover the TCX generator and queue state machine. The Strava account and ride
tests are `complexity: interactive` WOs with evidence committed.

## Prior Art

- [wo6-session/WORKORDER.md](../archive-pa-decomposition/wo6-session/WORKORDER.md)
- [wo8-strava/WORKORDER.md](../archive-pa-decomposition/wo8-strava/WORKORDER.md) — superseded synchronous `UploadManager` model
- [Archived PR-06 (ERG + Strava)](../archive-pr-v2/pr-06-erg-strava/PROJECT.md)

## 3. Work Order Ledger
<!-- PA fills this in during planning -->

## 4. Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
