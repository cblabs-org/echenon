# WO3d: BLE A/B/A′ Coexistence Experiment — Systematic Debugging Report

> Investigation following the [systematic-debugging](/.agents/skills/systematic-debugging/) protocol.
> All four phases completed. Root cause of the original hypothesis (H-COEX) **eliminated**.

---

## Phase 1: Root Cause Investigation

### 1.1 Error / Symptom Description

**Bug:** Google Pixel 10 Pro Fold fails to connect to the Echenon ESP32-S3 via
BLE approximately 70–75% of the time. Each failure reports:

| Layer | Error | Meaning |
|-------|-------|---------|
| Android (Java) | `android_status = 147` | `GATT_CONN_FAIL_ESTABLISH` |
| HCI | `reason = 0x3E` | "Connection Failed to be Established" |
| BLE Spec | Vol 6 Part B §4.5.2 | Supervision timeout during establishment window |

**When it succeeds**, the connection holds perfectly for 30+ seconds with zero
drops. The failure is **binary** — connect or don't — not gradual degradation.

### 1.2 Reproduction

**Highly reproducible.** An automated test rig (`tools/ble-test/pixel_runner.py`)
drives 50 connection attempts per batch via ADB, monitoring Android logcat for
`BluetoothGatt.onConnectionState()` results.

- **Reproduction rate:** ~70–75% failure (stable across 150 attempts over 3 batches)
- **Exact steps:** Open nRF Connect → tap CONNECT on `Echenon-7226` → observe logcat
- **30s inter-attempt spacing** to eliminate cooldown effects
- **30s connection hold** on success to verify stability

### 1.3 Recent Changes

The failure predates the current work. It was first observed during early PR-03
(HR Relay) integration testing on 2026-10-12 (see
[A12 pairing notes](A12-20261012-esp32-hr-pairing.md)). The WO3b investigation
paper ([BLE-0x3e-debug-2026-10-13.md](BLE-0x3e-debug-2026-10-13.md)) documents
the full prior investigation. No firmware changes were made between the start of
the investigation and the A/B/A′ experiment.

### 1.4 Evidence Gathered (Multi-Component System)

Evidence was gathered at **every component boundary**:

| Component | Instrumentation | Evidence File |
|-----------|----------------|---------------|
| **Android BLE stack** | logcat `BluetoothGatt` events | `batch-*/attempts.csv` |
| **Android HCI controller** | bugreport → `btsnooz_hci.log` | `batch-*/raw/bugreport-*.zip` (×8 total) |
| **ESP32 NimBLE host** | Serial log (BLE events, connect/disconnect) | `batch-*/raw/esp32_serial_*.log` |
| **ESP32 NimBLE events** | Parsed event CSV (timestamps, addresses) | `batch-*/esp32_events_*.csv` |
| **WiFi coex arbiter** | Controlled variable: present (A, A′) vs absent (B) | Firmware switch |

#### HCI Trace Analysis (Smoke Test Failure)

From bugreport HCI snoop on attempt 3 (FAIL):

| Time | Event | Detail |
|------|-------|--------|
| 13:05:09.419 | `LE_EXT_CREATE_CONNECTION` | Pixel initiates (filter_policy=1, phys=1M+2M+Coded) |
| **13:05:09.467** | **`LE_ENH_CONNECTION_COMPLETE SUCCESS`** | Link established — handle=64, itvl=18.75ms |
| **13:05:09.585** | **`DISCONNECTION_COMPLETE reason=0x3E`** | Failed — only **118ms** after establishment |

Comparison with passing attempts:

| Attempt | Create→Complete | Conn Update | Hold Duration |
|---------|----------------|-------------|---------------|
| 1 (PASS) | 131ms | itvl→48.75ms at +3s ✅ | 37.8s |
| 2 (PASS) | 274ms | itvl→48.75ms at +6s ✅ | 38.0s |
| **3 (FAIL)** | **48ms** | **never** | **0.118s** |

**Key finding:** The Pixel's controller *successfully* establishes the link
(CONNECTION_COMPLETE status=0x00), but the ESP32's controller fails to respond
on the data channel within 6 connection events (6 × 18.75ms = 112.5ms). The
118ms measured matches the spec's establishment deadline exactly.

### 1.5 Data Flow Trace

```
Pixel BLE Controller
  ├── Sends CONNECT_IND on advertising channel ─────── ✅ received by ESP32
  ├── Transitions to data channel                ─────── ✅ Pixel ready
  ├── Sends first data-channel packets (×6)      ─────── ❌ ESP32 misses all 6
  └── Supervision timeout → 0x3E                 ─────── ❌ link dropped

ESP32 BLE Controller
  ├── Receives CONNECT_IND                       ─────── ✅ (NimBLE logs event)
  ├── Should transition to data channel          ─────── ❓ delayed/missed
  └── First 6 connection events missed           ─────── ❌ radio unavailable
```

---

## Phase 2: Pattern Analysis

### 2.1 Working vs Broken Comparison

| Factor | Working (Lenovo P12) | Broken (Pixel 10 Pro Fold) |
|--------|---------------------|---------------------------|
| Connection interval | ~36.25ms (longer) | 18.75ms (shorter) |
| Establishment window | ~217ms (6 × 36.25) | ~112ms (6 × 18.75) |
| First-attempt success | ~100% (Owner O1) | ~26–32% |
| Android version | Android 14 | Android 17 |
| BLE stack | Qualcomm | Tensor G5 (Samsung/Exynos) |

**Key difference:** The Pixel's shorter connection interval gives the ESP32
half the establishment window. This makes it more sensitive to any scheduling
latency on the ESP32 side.

### 2.2 Hypothesis History

The WO3b investigation paper identified four hypotheses:

| ID | Hypothesis | Prior Status |
|----|-----------|--------------|
| **H-COEX** | WiFi coexistence holds the radio at critical BLE moments | **Leading** |
| **H-PHONE** | Pixel stack/controller mishandles the connection | Open |
| **H-WIN** | Pixel's short establishment window increases sensitivity | Contributing |
| **H-UPD** | Established links drop after connection-parameter update | Unresolved |

H-COEX was the leading suspect based on:
- ESP-IDF `WIFI_PS_NONE` known to cause BLE failures (multiple prior issues)
- Serial log showed coex warning: `"Coexist!!! Wi-Fi station would only keep waked when available"`
- Dynamic priority escalation callback is NULL on ESP32-S3

### 2.3 Reference Implementation

A **control peripheral** was built (`tools/ble-test/control-peripheral/`) —
identical ESP32-S3 hardware, same BLE MAC, same NimBLE advertising parameters,
but **zero WiFi code**. Pure ESP-IDF, no framework overhead. This isolates
WiFi as the single controlled variable.

---

## Phase 3: Hypothesis and Testing

### 3.1 Hypothesis

> **"WiFi coexistence (H-COEX) is the dominant cause of the 0x3E connection
> failures. Removing WiFi will increase the Pixel's connection success rate
> from ~26% to ≥96%."**

This is a clear, testable, falsifiable hypothesis with pre-agreed decision
rules.

### 3.2 Experiment Design: A/B/A′ Protocol

The experiment uses a classic A/B/A′ reversal design to control for temporal
drift:

| Batch | Firmware | WiFi? | Purpose |
|-------|----------|:-----:|---------|
| **A** | Echenon | ✅ | Baseline (with WiFi) |
| **B** | Control peripheral | ❌ | **Decisive test** (no WiFi) |
| **A′** | Echenon | ✅ | Reproducibility check |

**Fixed conditions:** Same phone, same position (~1m), same BLE MAC, same
advertising interval (50ms), same connection protocol (nRF Connect via ADB),
30s hold, 30s spacing.

**Pre-agreed decision rules** (from WO3b test strategy, §7):

| Outcome | Conclusion | Action |
|---------|-----------|--------|
| B ≥ 96% and A,A′ ≤ 85% | H-COEX confirmed | Escalate for WO3e (power-save experiment) |
| B ≈ A (within ~10 pp) | **Not coex** | Analyse failure trace; escalate |
| A vs A′ disagree > 15 pp | Temporal drift | Find drift; rerun |

### 3.3 Results

| Batch | PASS | FAIL | Rate | Longest Fail Streak | PASS Latency (avg) |
|-------|:----:|:----:|:----:|:-------------------:|:------------------:|
| **A** (Echenon + WiFi) | 13 | 37 | **26.0%** | 15 | 961ms |
| **B** (Control, no WiFi) | 16 | 34 | **32.0%** | 6 | 838ms |
| **A′** (Echenon + WiFi) | 14 | 36 | **28.0%** | 9 | 819ms |

- **150 total attempts** across 3 batches
- **Zero connection drops** during 30s hold (H-UPD not observed)
- **All failures** were `android_status=147` / HCI `0x3E`

### 3.4 Decision Rule Evaluation

```
  A  = 26.0%    B = 32.0%    A′ = 28.0%

  Δ(A, B)  =  6.0 pp  ← within 10 pp threshold
  Δ(A′, B) =  4.0 pp  ← within 10 pp threshold
  Δ(A, A′) =  2.0 pp  ← excellent reproducibility (no drift)
```

| Rule | Condition | Applies? |
|------|-----------|----------|
| **1** | B ≥ 96% and A,A′ ≤ 85% | ❌ B = 32% — far below 96% |
| **2** | B ≈ A within ~10 pp | **✅ TRIGGERED** — Δ = 6 pp and 4 pp |
| **3** | A vs A′ disagree > 15 pp | ❌ Δ = 2 pp — no drift |

### 3.5 Verdict

> **Decision Rule 2: WiFi coexistence is NOT the dominant cause of the BLE
> connection failures.**

Removing WiFi entirely produced an **identical** failure rate (32%) to the
full Echenon firmware with WiFi active (26–28%). The 6 pp delta is within
normal stochastic variance and, if anything, slightly favours WiFi being
present.

**H-COEX is eliminated as the leading hypothesis.**

---

## Phase 4: Outcome and Revised Root Cause Assessment

### 4.1 What We Now Know

The 0x3E failures are **not caused by WiFi coexistence**. They occur at
the BLE radio / NimBLE stack / ESP32 controller level, independent of WiFi.

### 4.2 Revised Hypothesis Standings

| ID | Hypothesis | Status After A/B/A′ |
|----|-----------|---------------------|
| ~~H-COEX~~ | ~~WiFi holds the radio at critical BLE moments~~ | **❌ ELIMINATED** (Rule 2) |
| **H-PHONE** | Pixel stack/controller mishandles the connection | **Promoted to leading** |
| **H-WIN** | Pixel's short establishment window increases sensitivity | **Contributing (confirmed)** |
| H-ESP32 | ESP32 NimBLE/controller scheduling latency | **New — needs investigation** |
| H-UPD | Established links drop after parameter update | **Not observed** (0/150 drops) |

### 4.3 Root Cause Mechanism (Current Understanding)

```
Pixel initiates connection with 18.75ms interval
  → ESP32 receives CONNECT_IND
  → ESP32 controller fails to service first 6 data-channel events
  → 112.5ms establishment deadline expires
  → 0x3E reported to both sides
```

This happens **regardless of WiFi**, suggesting the bottleneck is:

1. **ESP32 BLE controller scheduling** — the closed-source BLE controller
   binary may have timing issues on ESP32-S3 with NimBLE
2. **NimBLE host task latency** — the FreeRTOS task may not process the
   connection indication fast enough
3. **Advertising→connection transition timing** — the 50ms advertising
   interval may interact poorly with the Pixel's aggressive 18.75ms
   connection interval

### 4.4 Recommended Next Investigations

These follow the systematic debugging protocol — each tests a **single variable**:

| # | Test | Variable | Expected Outcome |
|---|------|----------|-----------------|
| 1 | Vary advertising interval (25ms, 100ms, 200ms) | Adv timing | Determines if 50ms is the pathological value |
| 2 | Different central (iPhone, nRF52 dongle) | Phone stack | Isolates Pixel-specific behaviour |
| 3 | NimBLE debug logging (ble_hs_log) | Host scheduling | Reveals where the host task delays |
| 4 | ESP-IDF Bluedroid stack (non-NimBLE) | BLE stack | Rules out NimBLE-specific issues |

### 4.5 Side Findings

1. **Android 17 logcat format change** — `BluetoothGatt.onClientConnectionState()`
   renamed to `onConnectionState()`, and `state=2` became `connected=true/false`.
   Fixed in `pixel_runner.py` (commit `1f85047`).

2. **12 bonded BLE devices** on the Pixel — heavy bond table may create
   controller scheduling pressure. Worth investigating as a contributing factor.

3. **Reason 0x22 observed once** (LMP Response Timeout) in early HCI traces —
   a separate failure mode where an *established* link drops due to ESP32
   unresponsiveness. Not reproduced in the 150-attempt A/B/A′ run.

---

## Evidence Inventory

All evidence committed on branch `hr-relay/wo3d-ble-ab-run`.

### Committed files (under `docs/measurements/phase2/evidence/wo3d-ab-run/`)

| File | Description |
|------|-------------|
| `report.md` | This systematic debugging report |
| `batch-a/attempts.csv` | 50 rows — Echenon + WiFi results |
| `batch-a/esp32_events_batch-a.csv` | ESP32 BLE events (first 29 attempts) |
| `batch-a/esp32_events_batch-a-cont.csv` | ESP32 BLE events (remaining 21 attempts) |
| `batch-a/summary.md` | Batch A statistical summary |
| `batch-b/attempts.csv` | 50 rows — Control peripheral (no WiFi) results |
| `batch-b/esp32_events_batch-b.csv` | ESP32 BLE events |
| `batch-b/summary.md` | Batch B statistical summary |
| `batch-a-prime/attempts.csv` | 50 rows — Echenon + WiFi (repeat) results |
| `batch-a-prime/esp32_events_batch-a-prime.csv` | ESP32 BLE events |
| `batch-a-prime/summary.md` | Batch A′ statistical summary |
| `.gitignore` | Excludes `raw/` subdirectories |

### Raw data (gitignored, retained locally in `raw/` subdirs)

| Directory | Contents |
|-----------|----------|
| `batch-a/raw/` | 4× bugreport ZIPs, 2× ESP32 serial logs |
| `batch-b/raw/` | 2× bugreport ZIPs, 1× ESP32 serial log |
| `batch-a-prime/raw/` | 2× bugreport ZIPs, 1× ESP32 serial log |

### Related documents

| Document | Location |
|----------|----------|
| WO3b investigation paper | [BLE-0x3e-debug-2026-10-13.md](../../BLE-0x3e-debug-2026-10-13.md) |
| T6′ HCI cross-correlation | [evidence/t6-prime-2026-10-14/ANALYSIS.md](../t6-prime-2026-10-14/ANALYSIS.md) |
| A12 initial pairing notes | [A12-20261012-esp32-hr-pairing.md](../../A12-20261012-esp32-hr-pairing.md) |
| WO3d Workorder | [WORKORDER.md](../../../../.agents/skills/principal-architect-workspace/pr-03-hr-relay/wo3d-ble-ab-run/WORKORDER.md) |

### Test apparatus

| Tool | Location | Purpose |
|------|----------|---------|
| `pixel_runner.py` | `tools/ble-test/pixel_runner.py` | ADB-driven BLE connect/disconnect automation |
| `esp32_log.py` | `tools/ble-test/esp32_log.py` | ESP32 serial capture and event extraction |
| `snoop_parse.py` | `tools/ble-test/snoop_parse.py` | HCI snoop log decoder |
| Control peripheral | `tools/ble-test/control-peripheral/` | WiFi-free ESP32-S3 BLE peripheral (PIO project) |
