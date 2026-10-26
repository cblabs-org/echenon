---
schema_version: 1
pr_id: PR-05
github_pr: null
audit_round: 1
head_sha: dbe050c889e6ef4d1e9bd097b91c1e6c00d5429e
auditor_harness: antigravity
verdict: CHANGES_REQUESTED
findings:
- id: BLOCKING-1
  axis: spec_alignment
  file_path: src/echenon/EchenonModule.cpp
  line_range: L109-L113
  summary: 'FTMS resistance at Echelon level 32 reports 0 due to sentinel collision.
    EchenonModule::tick() passes getInternalLevel() (0–255 range via pre-existing
    levelToInternal()) as internal_resistance to FTMSServer::updateTelemetry(). At
    level 32, levelToInternal() returns 255, which encodeIndoorBikeData() treats as
    the UNCALIBRATED sentinel and encodes as resistance=0 in the BLE packet. Charter
    §1.1 Layer 2 defines R_ftms ∈ {0..254} with 255=UNCALIBRATED. The new normalizedToInternal()
    correctly implements the 0–254 mapping but is not used in the telemetry path.

    '
  remediation: 'Replace getInternalLevel() in the updateTelemetry call with LevelMapping::normalizedToInternal(LevelMapping::echelonContinuousToNormalized(continuousLevel)),
    guarded by calibration state. When !isPosDone(), pass INTERNAL_RESISTANCE_UNCALIBRATED
    (255). Add a test asserting that level 32 maps to internal=254 (not 255) in the
    FTMS path.

    '
- id: NON-BLOCKING-1
  axis: spec_alignment
  file_path: src/echenon/FTMSServer.cpp
  line_range: L164-L166
  summary: 'Advertising uses Priority::Normal and omits addServiceData16 and setAppearance.
    SPEC §7.3 specifies Priority::Required, a 3-byte service data block (availability
    + indoor bike type), and setAppearance(kIndoorBikeAppearance). The 0x1826 UUID
    does appear in the ADV payload, so basic discovery works, but FTMS-aware apps
    may not detect the device type before connecting.

    '
  remediation: 'Consider aligning with SPEC §7.3 by using Priority::Required and adding
    service data and appearance. If intentionally deferred, document the deviation
    in the hardware verification report.

    '
- id: NON-BLOCKING-2
  axis: spec_alignment
  file_path: src/echenon/FTMSServer.h
  line_range: L59
  summary: 'getProviderId() returns "ftms_server" but SPEC §7.3 shows "ftms". No functional
    impact (used only for BLEManager logging) but deviates from the specification.

    '
  remediation: 'Change return value to "ftms" or document the deviation.

    '
- id: NON-BLOCKING-3
  axis: invariants
  file_path: src/echenon/FTMSServer.h
  line_range: L44
  summary: 'updateTelemetry comment says "thread-safe store" but no atomic or mutex
    protection is used. HR callback also writes _lastHrBpm/_lastHrMs without synchronization.
    Currently safe because EventBus dispatches on the main loop, but the comment is
    misleading.

    '
  remediation: 'Change the comment to "must be called from main loop" or add std::atomic
    to the HR fields as a defensive measure.

    '
- id: NON-BLOCKING-4
  axis: spec_alignment
  file_path: src/echenon/FTMSServer.h
  line_range: L57-L62
  summary: 'Missing onSubscriptionChanged, onStackPaused, and onStackResumed overrides
    specified in SPEC §7.2-7.3. Without onStackPaused, the notification timer continues
    encoding packets during OTA downloads.

    '
  remediation: 'Add stub overrides for onStackPaused/onStackResumed to pause/resume
    the notification timer. onSubscriptionChanged is optional for v1.

    '
- id: NON-BLOCKING-5
  axis: test_quality
  file_path: test/test_native/test_ftms_server.cpp
  line_range: L400-L406
  summary: 'Notification cadence test documents that tick(0) does NOT fire (0 - 0
    = 0 < 500), creating a 500 ms dead zone at startup. This is correct behavior given
    the implementation but worth noting as a design choice.

    '
  remediation: 'No action required. Consider adding a comment in FTMSServer.cpp explaining
    the initial-tick delay behavior.

    '
- id: NON-BLOCKING-6
  axis: scope_hygiene
  file_path: src/echenon/EchenonModule.cpp
  line_range: L102-L108
  summary: 'Static bool s_ftmsPhaseSeeded flag follows the pre-existing pattern (s_sensorsStarted,
    s_powerStarted) but prevents reinitialization.

    '
  remediation: 'No action required. Consider migrating to member state in a future
    cleanup PR.

    '
---

<!-- pa:audit pr_id=PR-05 round=1 head_sha=dbe050c889e6ef4d1e9bd097b91c1e6c00d5429e verdict=CHANGES_REQUESTED blocking=1 major=6 minor=0 -->

**Audit Verdict:** CHANGES_REQUESTED (1 blocking, 6 non-blocking)

# Cold-Firewall Audit — PR-05 `pr-05-ftms-broadcast` — Round 1

**Branch:** `feat/pr-05-ftms-broadcast` → `main`
**HEAD:** `dbe050c889e6ef4d1e9bd097b91c1e6c00d5429e`
**Test Suite:** 153/153 native tests PASS (`pio test -e native`)
**Working Tree:** Clean (no uncommitted changes)

---


## Verdict: ❌ CHANGES_REQUESTED

One blocking defect identified (BLOCKING-1: resistance sentinel collision at level 32). The implementation is otherwise thorough, well-tested, and closely aligned with the charter. Six non-blocking items documented for consideration.

---


## Five-Axis Summary

| Axis | Status | Notes |
|------|--------|-------|
| `spec_alignment` | ⚠️ BLOCKING-1 + 3 non-blocking | Resistance at L32 sends 0; advertising deviates; provider ID differs; missing optional callbacks |
| `invariants` | ✅ PASS (1 non-blocking) | Thread-safety comment misleading but code safe today |
| `edge_cases` | ✅ PASS | NaN, infinity, negative, boundary values extensively tested. Uncalibrated sentinel handled. Power clamped. HR staleness at 5 s. |
| `test_quality` | ✅ PASS (1 non-blocking) | 153 tests with substantive assertions across all WOs. No vacuous assertions. |
| `scope_hygiene` | ✅ PASS (1 non-blocking) | All 23 files within `charter.allowed_modules`. No stray artifacts. |

---


## Required Action

Fix **BLOCKING-1** (FTMS internal resistance at level 32 uses pre-existing 0–255 mapping instead of charter-mandated 0–254 mapping, causing sentinel collision) and re-request audit.
