# Encoder & Resistance Analysis — Echelon EX-3

**Date:** 2026-10-04  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.1.0-dev.4` on branch `fix/adc-gpio-conflict`  
**Author:** Automated bench analysis + operator observations

---

## 1. Purpose

Characterise the Echelon EX-3 rotary encoder signal quality, the
resistance control mechanism, and the relationship between encoder
input and physical resistance output. This analysis informs the design
of a future MITM encoder path that could improve the rider experience.

**Data files** (in `docs/measurements/phase0/`):

| File | Description |
|---|---|
| `B02-20261004-encoder-baseline-raw.csv` | Encoder channels, 60 s capture |
| `B02-20261004-encoder-baseline-full.csv` | All 10 ADC channels, 60 s capture |
| `B02-20261004-encoder-characterisation.md` | Encoder-only analysis detail |
| `B03-20261004-resistance-calibration.csv` | Full 32-level sweep, 6 min capture |
| `B03-20261004-resistance-calibration.md` | Calibration + motor seek detail |

---

## 2. Test Methods

### 2.1 Encoder Signal Quality (B02)

**Recording:** 60 seconds (1,646 rows × 50 ms decimation), ADC continuous
DMA at 83,333 Hz aggregate (~8,333 Hz per channel).

**Protocol:**
1. Phase 1 — Small adjustments: 1–2 encoder detent clicks, slow
2. Phase 2 — Medium turns: several clicks in succession
3. Phase 3 — Fast multi-click spins: rapid continuous rotation
4. Same pattern applied CW and CCW

### 2.2 Resistance Calibration Sweep (B03)

**Recording:** 363 seconds (7,258 rows × 50 ms), same ADC configuration.

**Protocol:**
1. Start at Echelon resistance level 1
2. Increase by one detent click, wait ~4 seconds for motor to settle
3. Repeat up to level 32
4. Decrease back down, one level at a time, to level 1

### 2.3 Measurement Limitations

- ADC decimation window is 50 ms; encoder states lasting < 50 ms appear as
  partial transitions or are merged with adjacent states
- The PCNT peripheral (used by the framework's EncoderModule) operates at
  hardware speed (~MHz) and would capture every edge; these ADC measurements
  represent a lower bound on signal quality
- Resistance force cannot be measured directly; it is inferred from the
  cam geometry and strap displacement model

---

## 3. Encoder Signal Quality

### 3.1 Quadrature Validity

All four Gray-code states observed:

| State | Enc A | Enc B | Confirmed |
|---|---|---|---|
| HH | HIGH | HIGH | ✅ |
| HL | HIGH | LOW | ✅ |
| LH | LOW | HIGH | ✅ |
| LL | LOW | LOW | ✅ |

VCC (CH9) remained steady at 4095 throughout — encoder supply is clean.

### 3.2 Bounce

| Metric | Value | Assessment |
|---|---|---|
| Total state runs | 246 | |
| Single-sample states (50 ms) | 97 | **39% — high bounce rate** |
| Stable states (≥ 100 ms) | 141 | |

> [!WARNING]
> **39% of observed states last only one 50 ms sample window.** These are
> contact bounce from the mechanical encoder. A digital interface without
> debouncing will miscount these as valid transitions.

### 3.3 Quadrature Transition Validity

| Metric | Value |
|---|---|
| Valid single-step transitions | 208 (89.3%) |
| Invalid (skipped states) | 25 (10.7%) |

In a clean quadrature signal, consecutive states should differ by exactly
one bit in Gray code. A "skipped state" means the encoder jumped over an
intermediate state — due to bounce, noise, or transitions faster than the
50 ms decimation window.

### 3.4 Direction Analysis

| Direction | Steps |
|---|---|
| CW (increase) | 74 |
| CCW (decrease) | 134 |
| **Net position** | **−60** |

### 3.5 Click Event Grouping

Transitions grouped into "click events" (clusters within 300 ms):

| Metric | Value |
|---|---|
| Total click groups | 54 |
| Steps per click (min) | 1 |
| Steps per click (max) | 5 |
| Steps per click (mean) | 2.1 |

### 3.6 Assessment

The encoder is a cheap mechanical incremental encoder with no hardware
debouncing. Signal quality is **moderate** — adequate for slow, deliberate
turns but unreliable during fast rotation. The 10.7% skip rate and 39%
bounce rate mean any consumer (including the Echelon console) will
miscount approximately 1 in 4 encoder clicks.

---

## 4. Encoder-to-Motor Correlation

### 4.1 Response Rate

| Metric | Value |
|---|---|
| Encoder clicks that triggered motor | 39 (72%) |
| Encoder clicks with no motor response | 15 (**28%**) |
| Total motor activations | 32 |

> [!IMPORTANT]
> **28% of encoder clicks produce no motor response.** This confirms the
> observation that "it sometimes takes several clicks to change the
> resistance." The console's encoder decoder is losing counts due to the
> noisy signal.

### 4.2 Response Rate vs Click Size

| Click size | Motor response rate | Avg motor duration |
|---|---|---|
| 1 step | 58% | ~300 ms |
| 2 steps | 75% | ~500 ms |
| 3+ steps | 92% | ~1,200 ms |

Larger click events (more detent steps in a group) are more likely to
trigger the motor. Single clicks frequently fail to register.

### 4.3 Encoder Position vs Wiper Correlation

Over a full 32-level sweep (1→32→1), the encoder accumulated 44 quadrature
steps on the way up — approximately **1.4 encoder steps per resistance
level**. The return path ended at +2 instead of 0, confirming a net drift
from miscounting.

---

## 5. Resistance Level Calibration

### 5.1 Calibration Table

31 distinct ascending levels detected from a 32-level sweep. One level
missed at the step between levels 21→22 (75-count jump ≈ 2× average).

| Level | ADC (up) | ADC (down) | ADC avg | Voltage (V) |
|---|---|---|---|---|
| 1 | 162 | 162 | 162 | 0.131 |
| 2 | 184 | 200 | 192 | 0.155 |
| 3 | 224 | 236 | 230 | 0.185 |
| 4 | 259 | 272 | 266 | 0.214 |
| 5 | 298 | 308 | 303 | 0.244 |
| 6 | 340 | 348 | 344 | 0.277 |
| 7 | 374 | 374 | 374 | 0.301 |
| 8 | 409 | — | 409 | 0.329 |
| 9 | 443 | — | 443 | 0.357 |
| 10 | 481 | 483 | 482 | 0.388 |
| 11 | 526 | 525 | 526 | 0.423 |
| 12 | 560 | 561 | 561 | 0.452 |
| 13 | 598 | 600 | 599 | 0.483 |
| 14 | 632 | 632 | 632 | 0.509 |
| 15 | 669 | 671 | 670 | 0.540 |
| 16 | 713 | — | 713 | 0.575 |
| 17 | 747 | — | 747 | 0.602 |
| 18 | 785 | 781 | 783 | 0.631 |
| 19 | 821 | — | 821 | 0.662 |
| 20 | 857 | — | 857 | 0.691 |
| 21 | 896 | — | 896 | 0.722 |
| 22 | 971 | 969 | 970 | 0.782 |
| 23 | 1006 | 1008 | 1007 | 0.811 |
| 24 | 1045 | 1045 | 1045 | 0.842 |
| 25 | 1084 | 1085 | 1085 | 0.874 |
| 26 | 1122 | — | 1122 | 0.904 |
| 27 | 1161 | 1162 | 1161 | 0.936 |
| 28 | 1198 | — | 1198 | 0.965 |
| 29 | 1239 | 1238 | 1239 | 0.998 |
| 30 | 1274 | 1274 | 1274 | 1.027 |
| 31 | 1331 | 1331 | 1331 | 1.073 |

### 5.2 Linearity (Pot-to-Level)

The pot wiper is **almost perfectly linear** with resistance level.
The single reference formula adopted from B03 is:

```
wiper_adc = 39.1 × level + 97.0       (R² = 0.9989)
```

Inverting for level detection:

```
level = round((wiper_adc − 97.0) / 39.1)
```

> [!NOTE]
> An earlier preliminary fit `level = round((wiper_adc − 125) / 37.7) + 1` is superseded
> by the B03 single reference fit above.

| Metric | Value |
|---|---|
| Total range | 1,169 ADC counts (0.942 V) |
| Mean step size | 39.0 counts (31.5 mV) |
| Step stdev | 8.6 counts |
| Up/down hysteresis | < 15 ADC counts (< 12 mV) |

### 5.3 Operator Observations During Sweep

- Going up to 32 was fairly clean, with one glitch (missed click)
- Coming down: skipped at ~30 (went straight to 29)
- At level 16 (descending), the motor shifted but the display didn't change
- Consistent with the 28% encoder miss rate found in the signal quality test

---

## 6. Motor Seek Behaviour

### 6.1 Overshoot Classification

The console's motor control loop uses a bang-bang controller (full power
on, then reverse to correct):

| Behaviour | Count | % |
|---|---|---|
| Simple (single direction) | 13 | 23% |
| Overshoot + correct (FWD→REV or REV→FWD) | 39 | 68% |
| Multi-correction (3+ direction changes) | 5 | 9% |

### 6.2 Position-Dependent Overshoot

| Wiper Range | Level Range | Seeks | With Overshoot |
|---|---|---|---|
| < 400 (low) | 1–8 | 13 | **8%** |
| ≥ 400 (mid/high) | 9–32 | 44 | **98%** |

The motor overshoots on virtually every move at mid/high resistance levels
but cleanly reaches target at low levels. This is consistent with a
bang-bang controller where motor momentum causes overshoot at larger
displacements.

### 6.3 Seek Timing

| Pattern | Typical Duration |
|---|---|
| Simple (low levels) | 200–400 ms |
| Overshoot (ascending) | ~2,000 ms |
| Overshoot (descending) | ~2,500 ms |
| Multi-seek | 6,600–14,100 ms |

### 6.4 Motor Drive Consistency

Motor drive time per level is remarkably constant across mid/high levels:

| Metric | Value |
|---|---|
| FWD mean | 820 ms |
| REV mean (correction) | 640 ms |
| REV/FWD ratio | ~0.71 |

This constant drive time means the **angular displacement per level is
constant** — the motor moves the same amount each step.

---

## 7. Eccentric Cam Model

### 7.1 Physical Mechanism

The resistance mechanism works as follows:

1. A **DC motor** turns a shaft
2. At the end of the shaft is an **eccentric cam** (offset ~8 mm from centre)
3. The cam follower connects to a **strap wrapped around the flywheel**
4. Tightening the strap increases braking resistance
5. A **potentiometer** on the motor shaft provides position feedback
6. The total strap travel is approximately **15 mm**

### 7.2 Cam Geometry

The eccentric cam produces a sinusoidal displacement:

```
displacement(θ) = e × (1 − cos(θ))

where:
  e = 8 mm (eccentricity)
  θ = cam angle from minimum position
```

| Parameter | Value |
|---|---|
| Eccentricity | 8 mm |
| Max theoretical displacement (at 180°) | 16 mm |
| Actual total displacement | ~15 mm |
| **Angular sweep** | **0° → 151°** |
| Angular step per level | 4.87° |
| Peak displacement rate at | θ = 90° (**level ~19**) |

### 7.3 Key Insight: Pot Measures Angle, Not Displacement

The pot is mounted on the motor shaft and measures **angular position**.
Our data confirms the pot is linear with resistance level (R² = 0.9989).
This means the console commands **equal angular steps** per level.

However, the **actual strap displacement is sinusoidal**, not linear.
Equal angular steps produce **highly unequal displacement steps** due
to the eccentric cam profile:

```
Δdisplacement(level) = e × sin(θ) × Δθ
```

### 7.4 Displacement Per Level — What the Rider Feels

| Level Range | Cam Angle | Δ displacement/click | Rider Experience |
|---|---|---|---|
| **1–5** | 0°–19° | **0.03–0.20 mm** | Barely perceptible |
| **6–10** | 24°–44° | **0.25–0.45 mm** | Gentle increase |
| **11–15** | 49°–68° | **0.49–0.62 mm** | Noticeable steps |
| **16–20** | 73°–93° | **0.64–0.68 mm** | **Biggest jumps** |
| **21–25** | 97°–117° | **0.68–0.62 mm** | Starting to taper |
| **26–32** | 122°–151° | **0.59–0.35 mm** | Diminishing returns |

> [!IMPORTANT]
> The displacement step at **level 19 is 22× larger** than at level 2.
> A rider turning the knob at constant speed will feel resistance barely
> change at low levels, then suddenly jump in the mid-range, then taper
> off at the top. This is an inherent property of the eccentric cam
> geometry, not a software issue.

### 7.5 Predicted Displacement Curve

```
Level:  1   5   10   15   19   25   32
Angle:  0°  20°  44°  68°  88°  117° 151°
Disp:   0   0.5  2.2  5.0  7.7  11.6  15.0 mm

         ■
         ■ ■
       ■ ■ ■ ■
     ■ ■ ■ ■ ■ ■
   ■ ■ ■ ■ ■ ■ ■ ■ ■
 ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■
 1   5   10   15   20   25   30  32
              Level
```

### 7.6 Residual Analysis

The pot data residuals from the linear fit show a subtle S-curve (±1.7%)
that is consistent with either:
1. Cheap carbon-track pot non-linearity (±2% typical)
2. Motor overshoot correction settling differently by position
3. The sinusoidal cam contributing a small second-order effect

The dominant behaviour is overwhelmingly linear because the pot measures
angle, and the console commands equal angle steps.

---

## 8. Implications for MITM Design

### 8.1 Encoder Improvement

The framework's `EncoderModule` uses the ESP-IDF **PCNT peripheral** with:
- Hardware-level edge detection at MHz speed
- Configurable glitch filter (1–1023 APB clock cycles)
- Velocity-based acceleration
- Normalised detent counting

This would eliminate the 39% bounce rate and 10.7% skip rate, bringing the
effective encoder miss rate from **28% down to near zero**.

### 8.2 Linearised Resistance Steps

To give the rider **truly equal resistance increments per level**, the MITM
could command non-uniform angular steps that compensate for the sinusoidal
cam profile:

```
θ(level) = arccos(1 − level/32 × (1 − cos(θ_max)))
```

Instead of equal angular steps (4.87° each), this would produce:
- **Larger** angular steps at low and high levels (where the cam is flat)
- **Smaller** angular steps in the middle (where the cam is steepest)

### 8.3 Level Detection from Pot

The ESP32 can determine the current resistance level in real time using the B03 single reference formula:

```cpp
int currentLevel = constrain(
    round((wiperADC - 97.0f) / 39.1f),
    1, 32
);
```

### 8.4 Clean Signal Re-Synthesis

For a full MITM encoder path:
1. **Ingest** the physical encoder via PCNT (clean, debounced)
2. **Translate** encoder detents to resistance commands (with optional
   acceleration and linearisation)
3. **Synthesise** a clean quadrature output to the Echelon console via
   two spare GPIOs
4. **Monitor** the pot position to confirm the console acted correctly

---

## 9. Summary of Findings

| Finding | Value | Impact |
|---|---|---|
| Encoder bounce rate | 39% | Console miscounts clicks |
| Encoder skip rate | 10.7% | Direction errors |
| Console click miss rate | 28% | "Takes several clicks to change" |
| Pot linearity (angle) | R² = 0.999 | Clean level detection |
| Step size | 39 ADC / level | 12-bit ADC easily resolves |
| Hysteresis | < 15 ADC counts | Negligible |
| Motor overshoot (mid/high) | 98% of moves | 2 s seek time |
| Motor overshoot (low) | 8% of moves | 300 ms clean |
| Cam type | Eccentric, 8 mm offset | Sinusoidal displacement |
| Cam sweep | 0° → 151° | 4.87° per level |
| Peak displacement rate | Level 19 (θ = 90°) | Mid-range feels steepest |
| Displacement ratio (max/min) | 22× | Levels 1–5 feel "dead" |
| Resistance curve | Sinusoidal (not linear) | Rider feels uneven steps |
