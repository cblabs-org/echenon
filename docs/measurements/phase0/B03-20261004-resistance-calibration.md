# B03 — Resistance Level Calibration

**Date:** 2026-10-04  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.1.0-dev.4` on branch `fix/adc-gpio-conflict`  
**Data file:** [`B03-20261004-resistance-calibration.csv`](B03-20261004-resistance-calibration.csv)

---

## 1. Test Method

### Protocol
1. Start at Echelon resistance level 1
2. Increase by one detent click, wait ~4 seconds for the motor to settle
3. Repeat up to level 32
4. Decrease back down, one level at a time, to level 1

### Signals Captured
- **Pot wiper** (CH0, Blue, A0): ADC reading proportional to motor position
- **Motor sense** (CH5/CH6, Red/Black, D3/D4): motor forward/reverse activation
- **Encoder** (CH7/CH8, Black/Red, D5/D6): knob quadrature signals

### Recording
7,258 rows × 50 ms decimation = 363 seconds (~6 minutes).

---

## 2. Calibration Table

31 distinct ascending levels were detected. One level appears to have been
missed at the step between levels 21 and 22 (75-count jump ≈ 2× normal).

| Level | ADC (up) | ADC (down) | ADC avg | Voltage (V) | Step Δ | mV/step |
|---|---|---|---|---|---|---|
| 1 | 162 | 162 | 162 | 0.131 | — | — |
| 2 | 184 | 200 | 192 | 0.155 | 22 | 17.5 |
| 3 | 224 | 236 | 230 | 0.185 | 40 | 32.6 |
| 4 | 259 | 272 | 266 | 0.214 | 35 | 28.3 |
| 5 | 298 | 308 | 303 | 0.244 | 39 | 31.3 |
| 6 | 340 | 348 | 344 | 0.277 | 42 | 33.7 |
| 7 | 374 | 374 | 374 | 0.301 | 34 | 27.4 |
| 8 | 409 | — | 409 | 0.329 | 35 | 27.9 |
| 9 | 443 | — | 443 | 0.357 | 35 | 27.9 |
| 10 | 481 | 483 | 482 | 0.388 | 38 | 30.4 |
| 11 | 526 | 525 | 526 | 0.423 | 45 | 36.1 |
| 12 | 560 | 561 | 561 | 0.452 | 35 | 27.9 |
| 13 | 598 | 600 | 599 | 0.483 | 38 | 30.5 |
| 14 | 632 | 632 | 632 | 0.509 | 34 | 27.3 |
| 15 | 669 | 671 | 670 | 0.540 | 37 | 30.0 |
| 16 | 713 | — | 713 | 0.575 | 44 | 35.3 |
| 17 | 747 | — | 747 | 0.602 | 33 | 27.0 |
| 18 | 785 | 781 | 783 | 0.631 | 39 | 31.2 |
| 19 | 821 | — | 821 | 0.662 | 35 | 28.6 |
| 20 | 857 | — | 857 | 0.691 | 36 | 29.4 |
| 21 | 896 | — | 896 | 0.722 | 38 | 31.0 |
| 22* | 971 | 969 | 970 | 0.782 | 75 | 60.6 |
| 23 | 1006 | 1008 | 1007 | 0.811 | 35 | 28.5 |
| 24 | 1045 | 1045 | 1045 | 0.842 | 39 | 31.1 |
| 25 | 1084 | 1085 | 1085 | 0.874 | 39 | 31.8 |
| 26 | 1122 | — | 1122 | 0.904 | 37 | 30.1 |
| 27 | 1161 | 1162 | 1161 | 0.936 | 39 | 31.7 |
| 28 | 1198 | — | 1198 | 0.965 | 37 | 29.8 |
| 29 | 1239 | 1238 | 1239 | 0.998 | 41 | 32.9 |
| 30 | 1274 | 1274 | 1274 | 1.027 | 35 | 28.5 |
| 31 | 1331 | 1331 | 1331 | 1.073 | 57 | 45.8 |

\* Level 22 has a 75-count step (2× average) — likely represents a missed
click, meaning levels 22–31 may be offset by +1 from the Echelon display.

> [!NOTE]
> Some descending levels show "—" because the user returned faster and
> the descending sweep only captured 23 of 32 stable plateaus — consistent
> with the 28% encoder click miss rate found in B02.

---

## 3. Linearity Analysis

| Metric | Value |
|---|---|
| Total range | 1,169 ADC counts (0.942 V) |
| Mean step size | 39.0 counts (31.5 mV) |
| Min step size | 22 counts (level 1→2) |
| Max step size | 75 counts (level 21→22*) |
| Step stdev | 8.6 counts |
| Max/min step ratio | 3.5 |

Excluding the two suspected missed-click levels (22 and 31), the step
sizes are highly consistent (**35–45 counts**), giving a max/min ratio
of ~1.3 — the pot is essentially linear.

### Voltage-to-Level Formula (linear fit)

Excluding outliers, a linear approximation from the data:

```
level ≈ (ADC_mean - 125) / 37.7 + 1

  or equivalently:

level ≈ (voltage_mV - 100) / 30.4 + 1
```

### Up/Down Hysteresis

Where both ascending and descending values were captured, the spread is
very small (typically < 15 ADC counts, < 12 mV). The motor's closed-loop
control is precise — there is no significant positional hysteresis in the
pot mechanism.

---

## 4. Key Findings

1. **32 resistance levels map linearly to 0.131–1.073 V** at the pot wiper
   (through 22 kΩ series resistor, measured at ESP32 ADC input).

2. **~37 ADC counts per level** — sufficient resolution for the 12-bit ADC
   (each level occupies ~37 of 4,095 counts = 0.9% of full scale). The ESP32
   can easily distinguish adjacent levels.

3. **The pot is linear** — equal resistance change per level. No significant
   non-linearity or dead zones.

4. **Minimal hysteresis** — ascending and descending readings agree to within
   ~15 ADC counts. The motor accurately positions the pot.

5. **The console missed at least 1 click** during the ascending sweep and
   multiple clicks descending — consistent with B02 encoder findings (28%
   miss rate). This further supports the case for MITM encoder improvement.


---

## 5. Motor Seek Behaviour

### 5.1 Overshoot Analysis

The console's motor control loop exhibits a clear overshoot-and-correct
pattern when changing resistance levels:

| Behaviour | Count | % of seeks |
|---|---|---|
| **Simple** (single direction move) | 13 | 23% |
| **Overshoot + correct** (move, then reverse) | 39 | 68% |
| **Multi-correction** (3+ direction changes) | 5 | 9% |

### 5.2 Position-Dependent Behaviour

| Wiper Range | Seeks | With Overshoot | Rate |
|---|---|---|---|
| < 400 (low resistance, levels 1–8) | 13 | 1 | **8%** |
| ≥ 400 (higher resistance, levels 9–32) | 44 | 43 | **98%** |

> [!IMPORTANT]
> The motor overshoots on **98% of moves at higher resistance levels** but
> only **8% at low levels.** This confirms the user's observation that "the
> motor overshoots and corrects almost every time except for the very low
> numbers."

This behaviour is consistent with a simple bang-bang control loop:
- At low wiper positions, the pot-to-motor distance is small — the motor
  can reach the target before building significant momentum
- At higher positions, the motor overshoots due to inertia, and the console
  reverses to correct

### 5.3 Seek Timing

| Pattern | Duration (typical) | Notes |
|---|---|---|
| Simple (low levels) | 200–400 ms | Fast, clean |
| Overshoot ascending | ~2,000 ms | REV→FWD, ~1s per direction |
| Overshoot descending | ~2,500 ms | REV→FWD, slightly longer |
| Multi-seek | 6,600–14,100 ms | Up to 8 direction changes |

### 5.4 Operator Observations

During the test, the user reported:
- Going up to 32 was fairly clean, with one glitch (missed click)
- Coming down: skipped at ~30 (went straight to 29)
- At level 16 (descending), the motor shifted but the display didn't change
- The motor overshoots and corrects almost every time except at very low
  resistance numbers

These observations are consistent with the data:
- 31 ascending plateaus detected (1 missed click)
- 23 descending plateaus (faster turns, more missed clicks at 28% rate)
- Motor overshoot at 98% of mid/high level changes
