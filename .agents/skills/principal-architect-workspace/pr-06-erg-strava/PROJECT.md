# PR-06: ERG Mode + Cloud Upload (Phase 5 — Close the Loop)

> **Programme:** Echenon Firmware
> **PR target base:** PR-05 (`ftms-broadcast` branch tip)
> **Dev Phase:** Phase 5 — ERG + Strava
> **Spec sections:** §5.6 (ERG), §7.4 (Control Point), §9.1–9.6, §10.1–10.6
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 5

## Mandate

Close the control loop and add cloud upload. External apps can now command
resistance, and rides upload to Strava automatically.

1. **`ERGController`** — FTMS Control Point → power table lookup → target
   resistance level. Mode-aware: `0x01` (Success) in Override, `0x05` (Control
   Not Permitted) in Intercept.
2. **`SessionManager`** — Auto-start on cadence, auto-end after 180s idle.
3. **`RideStore`** — PSRAM ring buffer for ride data.
4. **`TCXGenerator`** — 1 Hz trackpoints to LittleFS.
5. **`StravaUploader`** — OAuth 2.0, offline queue, auto-retry.
6. **`UploadManager`** — Registry + portal tab.

**Hardware:** Same intercept board. No changes.

After this PR merges, ERG mode works and rides upload to Strava automatically.

## Prior Art

- [wo6-session/WORKORDER.md](../archive-pa-decomposition/wo6-session/WORKORDER.md)
- [wo8-strava/WORKORDER.md](../archive-pa-decomposition/wo8-strava/WORKORDER.md)
- [Old PR-04 scope](../archive-pr-v1/pr-04-session-recording/PROJECT.md)
- [Old PR-06 scope](../archive-pr-v1/pr-06-cloud-upload/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
