# PR-06: Session Recording + TCX + Strava Upload

> **Programme:** Echenon Firmware
> **PR target base:** PR-02 (calibration-power branch tip)
> **Dev Phase:** Phase 4 — Cloud
> **Spec sections:** §9.1–9.6, §10.1–10.6

## Mandate

Implement session lifecycle management and Strava upload functionality.

1. **`SessionManager`** — Auto-start on cadence, crash recovery checkpoints, trailing-zero trim.
2. **`RideStore`** — PSRAM + LittleFS checkpoints for ride data buffer.
3. **`TCXGenerator`** — Gzipped, 1 Hz TCX generation.
4. **`StravaUploader`** — BYOA OAuth credential model, async upload, split pipeline, `ScanInhibit` during upload.
5. **`UploadQueue`** — Replaces old UploadManager, uses `IDataSource` execution model.

**Verification:** Rides auto-detected, crash recovery works, valid gzipped TCX generated, Strava upload succeeds (duplicate handling, offline queue works).

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
