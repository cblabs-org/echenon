> ⚠️ **SUPERSEDED — prior art only.** This was part of the v1 / initial programme plan, restructured on 2026-09-25 to 9 PRs + DR-1 (design audit and readiness review). **Current owner:** [PR-06](../../pr-06-session-upload/PROJECT.md). **Superseded by:** SPEC §10.1–§10.6 in the current [SPEC](../../../../../SPEC.md). See [PROGRAM.md](../../PROGRAM.md) and [PROGRAM-PLAN.md](../../PROGRAM-PLAN.md).

# PR-06: Cloud Upload (Strava)

> **Programme:** Echenon Firmware
> **PR target base:** PR-04 (`session-recording` branch tip)
> **Spec sections:** §10.1–10.6
> **Parallel with:** PR-05 (no file overlap)

## Mandate

Implement the cloud upload pipeline:

1. **`IUploader`** — extensible interface for future upload targets.
2. **`StravaUploader`** — OAuth 2.0 authorization flow, token storage via
   Foundation `KeyManager`, token refresh, TCX upload via Strava API v3.
   Implements `IDataSource` for Foundation `DataManager`-managed execution with
   `FetchContext`, deadline tokens, bounded transport, and health state machine
   with exponential backoff.
3. **`UploadManager`** — uploader registry, manages retry queue (up to 5 pending
   sessions in NVS), implements `IWebContributor` for upload settings portal tab.
4. **Upload Settings portal tab** — per-uploader connection status, "Connect
   Strava" OAuth button, auto-upload toggle.

Offline resilience: failed uploads stay in LittleFS queue and retry when WiFi
returns. Strava OAuth tokens registered with `Redactor::instance().registerSecret()`
for log safety.

**Radio coexistence:** `StravaUploader` must acquire a `ScanInhibit` RAII token
from BLEManager before starting any upload. While the token is alive, BLE
discovery and reconnect scans are suspended — connections and advertising
continue unaffected. This prevents WiFi/BLE radio contention during the HTTPS
upload (see SPEC §8.5).

After this PR merges, completed rides auto-upload to Strava. Failed uploads
retry automatically.

## Prior Art

- [wo8-strava/WORKORDER.md](../../archive-pa-decomposition/wo8-strava/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
