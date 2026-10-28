---
schema_version: 1
wo_id: WO-19.9
pr_id: PR-19
title: IntervalsIcuUploader main.cpp Wiring
complexity: mechanical
status: COMPLETED
allowed_files:
- src/main.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 7d290ef50e4766993754ee3b5bee84a48269311a950bd3bb6241eb5d329ab9df
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_ae273c94ea82
  commit_sha: 6ccc63811fcb08a762b4b34c62794301f3209a87
  files_modified:
  - src/main.cpp
  verification_summary: 'pio test -e native --verbose: ================ 234 test cases:
    234 succeeded in 00:00:00.556 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-19.9 — IntervalsIcuUploader main.cpp Wiring

## 1. Role & Objective

Wire the `IntervalsIcuUploader` (owned by `EchenonModule`) into the DI graph
in `src/main.cpp` `setup()`. This connects it to `KeyManager`, `Redactor`,
`UploadQueue`, and `SessionOverlay` so the Intervals.icu sink is operational
at runtime.

## 2. Strict Boundary Box & Constraints

1. Only modify `src/main.cpp`.
2. All new lines go inside the existing `#ifdef WITH_SESSION` block in `setup()`.
3. Follow the same DI wiring pattern used by `stravaUploader` (lines 139-147).
4. Do NOT add a new `#ifdef WITH_INTERVALS` guard — the IntervalsIcuUploader
   is always compiled as part of EchenonModule.
5. Preserve all existing code and comments.

**Forbidden:** No `src/echenon/` edits. No `web/` edits. No `test/` edits.

## 3. Line-Item Instructions

### 3.1 Add DI wiring after the `#ifdef WITH_STRAVA` block (after line 147)

Insert the following lines immediately after the `#endif` that closes the
`#ifdef WITH_STRAVA` block (currently line 147), and BEFORE line 148
(`sessionOverlay.setSessionManager(...)`):

```cpp
    echenonModule.intervalsUploader().setSecretStore(&app.getKeyManager());
    echenonModule.intervalsUploader().setSecretRegistrar(echenon::IntervalsIcuUploader::defaultRegisterSecret);
    echenonModule.intervalsUploader().begin();
    uploadQueue.registerSink(&echenonModule.intervalsUploader());
    app.registerModule(&echenonModule.intervalsUploader());
    sessionOverlay.setIntervalsUploader(&echenonModule.intervalsUploader());
```

**Line-by-line rationale:**
- `setSecretStore`: Connects to KeyManager NVS for encrypted API key storage.
- `setSecretRegistrar`: Registers API key with Redactor for log sanitization.
- `begin()`: Loads persisted config and API credentials from NVS.
- `registerSink`: Adds IntervalsIcuUploader to UploadQueue's multi-sink registry.
- `registerModule`: Enables IConfigurable and ITickable lifecycle management.
- `setIntervalsUploader`: Connects to SessionOverlay for REST API routes.

### 3.2 Verification

After editing, the file should have the intervals wiring between the Strava
`#endif` and `sessionOverlay.setSessionManager(...)`, following the same
pattern as the Strava block.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.9` → `pio test -e native --verbose`. All tests must pass.

## Completion Report

_Pending execution._
