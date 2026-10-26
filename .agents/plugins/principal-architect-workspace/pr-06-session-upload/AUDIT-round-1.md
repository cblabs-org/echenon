---
schema_version: 1
pr_id: PR-06
github_pr: null
audit_round: 1
head_sha: b74b354ec7877d7652e83b60ce056d7fcd7b2c9a
auditor_harness: antigravity
verdict: APPROVED
findings: []
---

<!-- pa:audit pr_id=PR-06 round=1 head_sha=b74b354ec7877d7652e83b60ce056d7fcd7b2c9a verdict=APPROVED blocking=0 major=0 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 0 non-blocking)

# AUDIT-round-1: PR-06 — Session Recording, TCX Export & Strava Upload

**Auditor:** Tier 2b Cold-Firewall Auditor  
**Date:** 2026-10-26  
**HEAD:** `b74b354ec7877d7652e83b60ce056d7fcd7b2c9a`  
**Base:** `a6c7a88` (main)  
**Tests:** 199/199 PASSED (`pio test -e native`, 0.625 s)  
**Verdict:** ✅ **APPROVED**

---


## Summary

PR-06 delivers a complete, production-quality implementation of the ride session recording pipeline (SPEC §9), upload services (SPEC §10), and session overlay REST/portal surface (SPEC §11.3–§11.5). The implementation spans 19 source/web/test files (~9,958 insertions) across 6 work orders, all first-attempt successes. Every normative requirement from the Part I Charter and the referenced SPEC sections has a corresponding implementation with strong test coverage (199/199 native tests pass). No blocking issues were found.

---


## Axis 1: `spec_alignment` — ✅ PASS

All Charter §1.1–§1.3 goals and SPEC §9–§11 requirements have corresponding implementations:

| Requirement | Implementation | Evidence |
|---|---|---|
| **State Machine** IDLE→ACTIVE↔PAUSED→ENDED | `SessionManager` state machine with full transition logic | `SessionManager.cpp:180-377`, tested in `FullLifecycleTransitionsPauseBoundaryAndEventBus` |
| **Auto-start** configurable 1–60 s, default 5 s | `DEFAULT_AUTO_START_DELAY_SEC = 5`, clamped range | `SessionManager.h:56-58`, tested in `AutoStartRequiresContinuousCadenceForDefault5Seconds` |
| **Auto-start disable** (`session_auto_start = false`) | `_autoStartEnabled` toggle via IConfigurable | `SessionManager.h:117-118`, tested in `AutoStartDisabledStaysIdleUntilManualStart` |
| **Inactivity timeout** 180 s default, 30–3600 s | `DEFAULT_INACTIVITY_TIMEOUT_SEC = 180`, clamped | `SessionManager.h:60-62` |
| **RideRecord 8 bytes** packed, canonical `i∈0..254` | `__attribute__((packed))`, `static_assert(sizeof == 8)` | `RideStore.h:25-60`, test `PackedSizeAndConversionHelpers` |
| **2 Hz → 1 Hz averaging** | `processSample500ms()` averages consecutive pairs | `SessionManager.cpp:383-443`, test `TwoHzToOneHzFundamentalAveraging` |
| **PSRAM allocation** 14,400 records (4 hours) | `MAX_RECORDS = 14400`, `MALLOC_CAP_SPIRAM` on ESP32 | `RideStore.h:143` |
| **30 s LittleFS checkpoints** | `CHECKPOINT_INTERVAL_RECORDS = 30` | `RideStore.h:144`, test `Append100RecordsCheckpointsAndFinalizesSummary` |
| **Crash recovery ≥ 60 records** | `DEFAULT_RECOVERY_MIN_RECORDS = 60`, tested < 60 discard | `RideStore.h:146`, test `CrashRecoveryDiscardsUnder60AndRecoversAtLeast60Records` |
| **5-ride FIFO** | `MAX_STORED_RIDES = 5` | `RideStore.h:145`, test `FiveRideFifoEvictionOnSixthCompletedRide` |
| **Trailing zero trim** | `trimTrailingZeroCadence()` before finalize | `SessionManager.cpp:311`, test `ActiveCoastingPreservedInMiddleWhileTrailingZerosStripped` |
| **NTP back-computation** | `updateWallClock()` back-computes `start_epoch_s` | `SessionManager.cpp:150-166`, test `DeferredNtpWallClockBackComputation` |
| **TCX streaming** 4 KB scratch buffer | `SCRATCH_BUFFER_SIZE = 4096`, `ScratchBufferWriter` | `TCXGenerator.cpp:16-70`, test `Bounded4KBScratchBufferAndDeadlineCancellation` |
| **Virtual speed** `computeVirtualSpeedKph(P)/3.6f` | `computeTrackpointSpeedMps()` delegates correctly | `TCXGenerator.cpp:556-564` |
| **Coasting emits** 0W / 0 speed trackpoints | `cadence_rpm == 0` → `0.0f` speed, `0` watts | `TCXGenerator.cpp:557-558`, tested in XML schema test |
| **Pause gaps** no trackpoints | No emission during PAUSED | `SessionManager.cpp:504-511`, test coverage |
| **Gzip** RFC 1952 + CRC32 + ISIZE | `generateTcxGz()` with `computeCrc32()` | test `GzipStreamAndRideStoreFileRoundTripDecompression` |
| **HR omission** when no valid samples | Separate test | `OmitLapAndTrackpointHeartRateWhenNoValidHrSamplesExist` |
| **IUploader** pure capability, no I/O | `IUploader.h` interface | `IUploader.h:39-88` |
| **UploadQueue IDataSource** split-stage | `fetchUpdate(FetchContext&)` delegates to `stepQueue()` | `UploadQueue.cpp:817-827` |
| **4-stage progression** Generate→Refresh→Upload→Poll | Tested single-step-per-call | `FourStageProgressionRunsAtMostOneStagePerStepAndCleansUpTcxGz` |
| **Backoff** 30/60/120/300 s, max 5 attempts | `computeBackoffMs()`, `MAX_ATTEMPTS = 5` | `UploadQueue.cpp:204-212` |
| **BYOA OAuth** state nonce, single-use, 10 min TTL | `buildAuthorizeUrl()`, `validateAndConsumeState()` | `StravaUploader.cpp:398-437` |
| **State replay/expiry rejection** | Consumed on first use; expired → rejected | Tested: `StateNonceTtlExpiryAndMillisWraparound` |
| **Refresh-token-first persistence** | `putSecret(KEY_REFRESH_TOKEN)` before `putSecret(KEY_ACCESS_TOKEN)` | `StravaUploader.cpp:563-571` |
| **Redactor** `registerSecret()` return-value check | `registerSecretWithRedactor()` checks bool, counts warnings | `StravaUploader.cpp:307-330` |
| **Portal tabs** Session=80, Upload Services=90 | `registry.addTab("Session", ..., 80, ...)`, `registry.addTab("Upload Services", ..., 90, ...)` | `SessionOverlay.cpp:809-821` |

---


## Axis 2: `invariants` — ✅ PASS

| Invariant | Status | Evidence |
|---|---|---|
| **Signed `int32_t` `millis()` arithmetic** | ✅ All timer comparisons use `static_cast<int32_t>(now - prev) >= threshold` | `SessionManager.cpp:157,238,265,300,406,467,482,492,505`; `StravaUploader.cpp:423`; `UploadQueue.h:181` |
| **`ITickable`** | ✅ `SessionManager` implements `tick(uint32_t)` | `SessionManager.h:77` |
| **`IDataSource`** | ✅ `UploadQueue` implements all 5 required methods | `UploadQueue.h:187-192` |
| **`IConfigurable`** | ✅ Three distinct namespaces (`echenon_session`, `echenon_upload`, `echenon_strava`) | Headers verified |
| **`IWebContributor`** | ✅ `SessionOverlay` registers routes and tabs | `SessionOverlay.cpp:808-821` |
| **`IPreconditionContributor`** | ✅ `UploadQueue` registers preconditions | `UploadQueue.h:201-202` |
| **`#ifdef WITH_BLE` guards** | ✅ All BLEManager/ScanInhibit refs guarded | `UploadQueue.h:18,133,231`; `UploadQueue.cpp:443,476,516` |
| **ScanInhibit on TLS stages only** | ✅ Not on Generate stage | `UploadQueue.cpp:378` vs `:442-446,476-479,516-519` |
| **`DeadlineToken` budget checking** | ✅ Threaded through all stages | `UploadQueue.cpp:359,817-826` |
| **`Redactor::registerSecret()` return check** | ✅ Checks `RegisterResult`, warns on failure | `StravaUploader.cpp:298-330` |
| **No `Application*` stored member** | ✅ Narrow refs only | `main.cpp:47-48,87-106` |
| **ADR-17 tick budget** | ✅ Heavy work deferred to `fetchUpdate()` | Architecture verified |

---


## Axis 3: `edge_cases` — ✅ PASS

| Edge Case | Status | Evidence |
|---|---|---|
| **Coasting (cadence==0 while ACTIVE)** 1 Hz zero-watt trackpoints | ✅ | `TCXGenerator.cpp:557-558`, test coverage |
| **Pause gaps** no trackpoints | ✅ | `SessionManager.cpp:504-511` |
| **NTP back-computation** pre-sync start | ✅ | `SessionManager.cpp:150-166`, test |
| **`millis()` wraparound safe** | ✅ Comprehensive signed `int32_t` usage | All timer sites verified |
| **Crash recovery ≥ 60 records** | ✅ | test `CrashRecoveryDiscardsUnder60AndRecoversAtLeast60Records` |
| **OAuth state replay rejection** | ✅ Single-use | test `BuildAuthorizeUrlAndValidateSingleUseStateNonce` |
| **OAuth state expiry** | ✅ 10-min TTL | test `StateNonceTtlExpiryAndMillisWraparound` |
| **Refresh-token-first persistence** | ✅ line 563 before line 568 | `StravaUploader.cpp:563-571` |
| **Refresh failure abort** | ✅ No overwrite | test `RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens` |
| **Exponential backoff** after 5 retries | ✅ | test `ExponentialBackoffAndFailureAfterFiveRetryableAttemptsOrPermanentError` |
| **Ghost-session prevention** | ✅ Sustained cadence required | `SessionManager.cpp:463-473` |
| **Queue capacity** (max 5) | ✅ | test `DiscardSessionQueueCapacityAndJsonRoundTrip` |

---


## Axis 4: `test_quality` — ✅ PASS

**199/199 tests pass.** PR-06 adds 32 new tests across 4 test files:

| Test File | Count | Key Coverage |
|---|---|---|
| `test_session_manager.cpp` | 14 | `sizeof==8`, 2Hz→1Hz averaging, auto-start enable/disable, lifecycle+EventBus, discard, millis wraparound, NTP back-computation, coasting+trim, checkpoints, crash recovery, 5-ride FIFO, REST API, OAuth flow |
| `test_tcx_generator.cpp` | 5 | ISO-8601+CRC32, XML schema+virtual speed+coasting+pause+HR, HR omission, 4KB scratch+deadline, gzip round-trip |
| `test_upload_queue.cpp` | 5 | 4-stage single-step, refresh skip, backoff+5 attempts, discard+capacity+JSON, preconditions+config |
| `test_strava_uploader.cpp` | 7 | State nonce+single-use, TTL+wraparound, refresh-first+ordering, write failure abort, Redactor saturation, multipart+HTTP, poll+duplicate |

Tests exercise real behaviour with concrete assertions. No vacuous patterns.

---


## Axis 5: `scope_hygiene` — ✅ PASS

| Check | Status |
|---|---|
| All 19 source/test/web files within `allowed_modules` | ✅ |
| PA workspace files (`.agents/`) | ✅ Operational artifacts |
| `SPEC.md` and `docs/` within scope | ✅ |
| No out-of-scope framework changes | ✅ |
| No personal identifiers or stray artifacts | ✅ |
| Build flags `WITH_SESSION`/`WITH_STRAVA` properly enabled | ✅ |

---


## Strengths

1. **Rigorous signal fidelity:** 2 Hz→1 Hz anti-aliased averaging with canonical `i∈0..254` internal resistance preserving 8× finer resolution than discrete levels.
2. **Robust crash recovery:** 30 s checkpoint delta protocol with ≥ 60-record threshold.
3. **Security-first OAuth:** Single-use CSRF nonce with TTL, refresh-token-first persistence with abort-on-failure, comprehensive Redactor integration.
4. **Clean architectural boundaries:** `IUploader` is pure capability; `UploadQueue` owns transport and BLE coexistence. `ScanInhibit` properly scoped to TLS stages.
5. **Deep test coverage:** 32 new tests covering edge cases (wraparound, replay, expiry, write failures, Redactor saturation, gzip round-trip).


## Findings

No blocking findings. No non-blocking findings.
