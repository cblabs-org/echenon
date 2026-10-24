---
schema_version: 1
pr_id: PR-05
github_pr: null
audit_round: 2
head_sha: 1ee282482c1bd8f202c1091bc59a5ce3860a3b14
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-05 round=2 head_sha=1ee282482c1bd8f202c1091bc59a5ce3860a3b14 verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)

# Cold-Firewall Audit — PR-05 `pr-05-ftms-broadcast` — Round 2

**Branch:** `feat/pr-05-ftms-broadcast` → `main`
**HEAD:** `1ee282482c1bd8f202c1091bc59a5ce3860a3b14`
**Test Suite:** 155/155 native tests PASS (`./.pio/build/native/program`)
**Working Tree:** Clean (no uncommitted changes)

---


## Verdict: ✅ APPROVED

All Round 1 findings (`BLOCKING-1` and `NON-BLOCKING-1` through `NON-BLOCKING-5`) have been cleanly remediated and verified via native unit tests (`155/155` passing) and live over-the-air BLE + Calibration Rig hardware capture (`v0.3.0-dev.18`, `docs/measurements/phase3/C08-20261024-ftms-verification.md`).

---


## Round 1 Remediation Verification

- **`BLOCKING-1` (Resolved):** [`EchenonModule::tick()`](src/echenon/EchenonModule.cpp) now passes `LevelMapping::normalizedToInternal(LevelMapping::echelonContinuousToNormalized(continuousLevel))` (`0..254`) when `_calibrationWizard.isPosDone()` is true and `LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED` (`255`) otherwise. Verified by unit test `FTMSServerTest.Level32MapsTo254AndEncodes254NotZero` and live BLE `0x2AD2` capture at Echelon Level 32 (`64 02 00 00 00 00 fe 00 00 00 00`, `resistance = 254`).
- **`NON-BLOCKING-1` (Resolved):** [`FTMSServer::contributeAdvertising()`](src/echenon/FTMSServer.cpp) now registers `0x1826` with `Priority::Required`, includes the 3-byte FTMS Service Data AD structure (`01 20 00`), and sets `kIndoorBikeAppearance = 0x0485`. Verified in live primary ADV capture (`02010605030d1826180319850406162618012000`, `20/31` bytes).
- **`NON-BLOCKING-2` (Resolved):** [`FTMSServer::getProviderId()`](src/echenon/FTMSServer.h) now returns `"ftms"`.
- **`NON-BLOCKING-3` (Resolved):** [`FTMSServer.h`](src/echenon/FTMSServer.h) doc comment updated to reflect the main-loop calling contract.
- **`NON-BLOCKING-4` (Resolved):** [`FTMSServer`](src/echenon/FTMSServer.h) implements `onStackPaused()` / `onStackResumed()` and wires `StubControlPointCallbacks` under `#if FTMS_STUB_CONTROL_POINT`. Verified by `FTMSServerTest.ProviderIdAndStackPauseResume`.
- **`NON-BLOCKING-5` (Resolved):** [`FTMSServer::tick()`](src/echenon/FTMSServer.cpp) documents the initial 500 ms settle window at `tick(0)`.

---


## Five-Axis Summary

| Axis | Status | Notes |
|------|--------|-------|
| `spec_alignment` | ✅ PASS | Unified metric hierarchy (`LevelMapping` + `PowerEngine`), FTMS `0x1826` (`0x2ACC` = `0x00004482`, `0x2AD2` 11-byte 2 Hz notify, ADV `0x0485` + `01 20 00`), and `/ws/echenon/live` (`peloton_resistance`, `speed_kph`) match Charter & SPEC |
| `invariants` | ✅ PASS | Zero BLE dependencies in `FTMSData.h`, 250 ms phase offset relative to `LiveFeed`, stack pause/resume respected |
| `edge_cases` | ✅ PASS | Level 32 (`254` vs `255` sentinel), NaN/Inf, negative/clamped bounds, and 5 s HR staleness timeout all handled and tested |
| `test_quality` | ✅ PASS | 155/155 native tests pass with non-vacuous assertions + live hardware `bleak` & Calibration Rig multi-level verification |
| `scope_hygiene` | ✅ PASS | All modified files reside within `charter.allowed_modules` and the PR workspace ledger; clean working tree |
