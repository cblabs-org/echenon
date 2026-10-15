# WO3d: BLE A/B/A′ Experiment Report

## Experiment Summary

**Objective:** Determine whether WiFi coexistence (H-COEX) is the dominant cause
of Pixel BLE connection failures (0x3E / android status 147) against the
Echenon ESP32.

**Method:** A/B/A′ protocol — three batches of 50 connection attempts each:

| Batch | Firmware | WiFi Active? | When |
|-------|----------|:------------:|------|
| A     | Echenon  | Yes          | 2026-10-15 13:00–17:33 |
| B     | Control peripheral (no WiFi) | No | 2026-10-15 17:37–18:20 |
| A′    | Echenon  | Yes          | 2026-10-15 18:33–19:16 |

**Test Rig:**
- ESP32-S3 (Waveshare Nano), BLE MAC `3C:0F:02:DE:72:26`
- Google Pixel 10 Pro Fold, Android 17 (`CP3A.261005.005`)
- nRF Connect for Android, automated via ADB + logcat
- 30s connection hold, 30s inter-attempt spacing

## Results

| Batch | PASS | FAIL | Pass Rate | Longest Fail Streak | PASS Latency (avg) |
|-------|------|------|:---------:|:-------------------:|:------------------:|
| **A** (Echenon+WiFi) | 13 | 37 | **26.0%** | 15 | 961ms |
| **B** (Control, no WiFi) | 16 | 34 | **32.0%** | 6 | 838ms |
| **A′** (Echenon+WiFi) | 14 | 36 | **28.0%** | 9 | 819ms |

- Zero connection drops during 30s hold across all 150 attempts.
- All failures were `android_status=147` (HCI 0x93 / 0x3E — Connection Failed to be Established).

## Decision Rule Evaluation

### Pre-agreed decision rules (from WO3b test strategy):

**Rule 1:** B ≥ 48/50 (96%) and A,A′ ≤ 85% → H-COEX confirmed
- B = 32.0% → **DOES NOT APPLY**

**Rule 2:** B ≈ A (within ~10 percentage points) → Not coex-dominant
- Δ(A,B) = 6.0 pp ← within threshold
- Δ(A′,B) = 4.0 pp ← within threshold
- **✅ TRIGGERED**

**Rule 3:** A and A′ disagree > 15 pp → temporal drift, rerun needed
- Δ(A,A′) = 2.0 pp → **NO DRIFT** (excellent reproducibility)

## Verdict

**Decision Rule 2 applies: WiFi coexistence is NOT the dominant cause of the
BLE connection failures.**

Removing WiFi entirely (Batch B) produced an essentially identical failure
rate (32%) to the full Echenon firmware with WiFi active (26–28%). The 6 pp
difference is well within normal variance and may even slightly favour WiFi
being present.

The 0x3E connection establishment failures are occurring at the BLE radio /
NimBLE stack / Pixel BLE controller level, independent of WiFi activity.

## Root Cause Hypothesis

Based on the HCI trace analysis from the smoke test (see
[smoke test HCI analysis](../../../BLE-0x3e-debug-2026-10-13.md)):

The Pixel's BLE controller establishes the link-layer connection, but the
ESP32's NimBLE stack misses the first ~6 connection events within a ~118ms
supervision timeout window. This happens regardless of whether WiFi is active,
suggesting the issue is:

1. **NimBLE scheduling latency** — the host task may not process the controller's
   connection indication quickly enough
2. **Advertising interval / scan window mismatch** — the 50ms advertising
   interval may interact poorly with the Pixel's connection parameters
3. **ESP32 BLE controller firmware** — the ESP32's closed-source BLE controller
   may have timing issues independent of the WiFi coexistence arbitrator

## Escalation

Per the pre-agreed protocol, this result should be escalated to the Chief
Architect for decision on next steps. The coex investigation path (WO3e) is
**not recommended** — the evidence does not support WiFi as the cause.

Suggested next investigations:
1. Vary NimBLE advertising interval (25ms, 100ms, 200ms)
2. Test with a different central (iPhone, nRF dongle) to isolate Pixel-specific behaviour
3. Examine NimBLE connection event scheduling with debug logging
4. Test with ESP-IDF BLE stack (non-NimBLE) as a further control

## Evidence

All raw data is committed under `docs/measurements/phase2/evidence/wo3d-ab-run/`:

```
wo3d-ab-run/
├── batch-a/
│   ├── attempts.csv           (50 rows)
│   ├── summary.md
│   ├── esp32_serial_batch-a.log
│   ├── esp32_serial_batch-a-cont.log
│   ├── esp32_events_batch-a.csv
│   ├── esp32_events_batch-a-cont.csv
│   └── bugreport-*.zip        (×4)
├── batch-b/
│   ├── attempts.csv           (50 rows)
│   ├── summary.md
│   ├── esp32_serial_batch-b.log
│   ├── esp32_events_batch-b.csv
│   └── bugreport-*.zip        (×2)
├── batch-a-prime/
│   ├── attempts.csv           (50 rows)
│   ├── summary.md
│   ├── esp32_serial_batch-a-prime.log
│   ├── esp32_events_batch-a-prime.csv
│   └── bugreport-*.zip        (×2)
└── report.md                  (this file)
```
