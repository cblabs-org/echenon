# C01 — Edges Per Level + Step Interval

**Date:** 2026-10-24  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.3.0-dev.12` on branch `calibration-power/wo6-rig-host`  
**Data file:** [`evidence/chartest1_edges_per_level.csv`](evidence/chartest1_edges_per_level.csv)

---

## 1. Purpose

Determine the exact number of synthesized quadrature edges the Echelon
console requires to register one resistance level change, and the reliable
inter-edge timing for the calibration rig.

### Background

Phase 0 measurements (B02, B04, A08) established that the physical encoder
produces 4 physical detents per level, and the ESP-IDF PCNT peripheral in
4× mode counts 16 steps per detent. However, the console's own encoder
decoder operates independently of our PCNT — we need to know how many
**synthesized GPIO edges** (from the CalibrationRig firmware) the console
actually requires.

---

## 2. Test Method

### Hardware Setup

The CalibrationRig firmware synthesizes quadrature encoder signals on two
GPIOs wired to the console's encoder input:

| Signal | GPIO | Waveshare Silk | Physical Wire |
|---|---|---|---|
| Encoder Ch A | 17 | D8 | Red pad |
| Encoder Ch B | 18 | D9 | Black pad |
| Cadence pulse | 11 | A4 | 2N2222 base (NPN to cadence reed) |

> [!NOTE]
> The original encoder must be physically disconnected during rig testing
> to prevent conflicting signals on the same lines.

### Protocol

1. Start at level 1 (HOME the console by stepping 256 edges DOWN)
2. Issue 8 UP edges (one full quadrature cycle × 2)
3. Wait 3.5 seconds for the console to seek
4. Read the pot ADC value and ADB app resistance
5. Record whether a level change occurred
6. Repeat for 4 levels (1→5)

### Verification

Each level change is triple-verified:
- **Pot ADC** — wiper voltage change confirms motor moved
- **Derived level** — firmware's `LevelMapping` from pot ADC
- **ADB resistance** — screen scraping the Echelon Fit app via `adb shell uiautomator dump`

---

## 3. Results

### 3.1 Edges Per Level

**8 quadrature edges = 1 console resistance level.** 100% match across
all 4 test transitions.

| Start Level | Edges Sent | End Level | ADB Resistance | Result |
|---|---|---|---|---|
| 1 | 8 | 2 | 2 | ✅ |
| 2 | 8 | 3 | 3 | ✅ |
| 3 | 8 | 4 | 4 | ✅ |
| 4 | 8 | 5 | 5 | ✅ |

### 3.2 Reconciliation with Phase 0

| Measurement | Phase 0 (B02/B04/A08) | Phase 3 (C01) | Explanation |
|---|---|---|---|
| Physical detents per level | 4 | — | Mechanical encoder property |
| PCNT steps per level | 16 | — | PCNT 4× counting mode |
| **Console decoder edges** | (not directly measured) | **8** | Console uses 2× counting mode |

The console's encoder decoder uses **2× counting mode** (counts on both
rising and falling edges of one channel, using the other for direction).
This reconciles all prior measurements:

```
4 detents × 4 PCNT edges/detent = 16 PCNT steps
4 detents × 2 console edges/detent = 8 console edges
```

### 3.3 Step Interval Reliability (C02)

Three inter-edge intervals were tested across multiple level transitions:

| Interval | Reliability | Notes |
|---|---|---|
| 100 ms | 100% | Fast, all edges registered |
| 150 ms | 100% | **Selected for production** — reliable with margin |
| 200 ms | 100% | Conservative, slightly slow |

> [!TIP]
> 150 ms was selected as the production step interval: fast enough for
> efficient sweeping (~1.2s per level change) while maintaining 3× margin
> over the 50 ms minimum pulse width discovered in C04.

---

## 4. Raw Data Format

The CSV file contains one row per edge:

| Column | Description |
|---|---|
| `step` | Edge index (0-based) |
| `pot_adc` | Pot wiper ADC reading after this edge |
| `derived_level` | Firmware LevelMapping result |
| `app_resistance` | ADB-scraped Echelon Fit resistance |
| `enc_a` | Encoder Ch A state (0/1) |
| `enc_b` | Encoder Ch B state (0/1) |

The quadrature pattern repeats every 4 edges: `00 → 10 → 11 → 01 → 00…`

---

## 5. Firmware Parameters Confirmed

| Parameter | Value | Source |
|---|---|---|
| `STEPS_PER_LEVEL` | 8 | C01 (100% empirical match) |
| `STEP_INTERVAL_MS` | 150 | C02 (selected from 100/150/200ms) |
| Direction mapping | UP = increase resistance | C01 (ADB verified) |
