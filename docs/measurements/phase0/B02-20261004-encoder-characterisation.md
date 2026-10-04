# B02 — Encoder Characterisation Report

**Date:** 2026-10-04  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.1.0-dev.4` on branch `fix/adc-gpio-conflict`  
**Sampling:** ADC continuous DMA, 83,333 Hz aggregate, ~8,333 Hz per channel  
**Decimation:** 50 ms windows (min/mean/max per channel)  
**Data files:**
- [`B02-20261004-encoder-baseline-raw.csv`](B02-20261004-encoder-baseline-raw.csv) — encoder-only columns
- [`B02-20261004-encoder-baseline-full.csv`](B02-20261004-encoder-baseline-full.csv) — all 10 ADC channels

---

## 1. Test Method

### Hardware
- Encoder signals tapped via 4.7 kΩ series resistors + 100 nF decoupling caps
- Encoder wiring: Black=Enc A (D5), Red=Enc B (D6), Orange=VCC (D7)
- All other sensors connected: cadence, motor sense, resistance pot

### Test Protocol
60-second recording (1,646 decimation rows at 50 ms cadence):

1. **Phase 1 — Small adjustments:** 1–2 encoder detent clicks at a time, slow
2. **Phase 2 — Medium turns:** several clicks in succession  
3. **Phase 3 — Fast multi-click spins:** rapid continuous rotation

Same pattern applied in both CW and CCW directions.

### Measurement Limitations
The ADC decimation window is 50 ms. Any encoder state that persists for
less than 50 ms may appear as a partial transition (mid-value mean) or be
merged with adjacent states. The PCNT peripheral (used by the framework's
EncoderModule) operates at hardware speed (~MHz) and would capture every
edge; this ADC analysis represents a lower bound on signal quality.

---

## 2. Encoder Signal Quality

### 2.1 Quadrature States

All four valid Gray-code states observed:

| State | A | B | Observed |
|---|---|---|---|
| HH | HIGH | HIGH | ✅ |
| HL | HIGH | LOW | ✅ |
| LH | LOW | HIGH | ✅ |
| LL | LOW | LOW | ✅ |

VCC (CH9) remained steady at 4095 throughout — encoder supply is clean.

### 2.2 Bounce Analysis

| Metric | Value | Assessment |
|---|---|---|
| Total state runs | 246 | |
| Single-sample states (50 ms) | 97 | **39% — high bounce rate** |
| Stable states (≥ 100 ms) | 141 | |

> [!WARNING]
> **39% of observed states last only one sample window (50 ms).** These are
> likely contact bounce from the mechanical encoder. A digital encoder
> interface without debouncing would miscount these as valid transitions.

### 2.3 Quadrature Transition Validity

| Metric | Value |
|---|---|
| Valid single-step transitions | 208 (89.3%) |
| Invalid (skipped states) | 25 (10.7%) |

In a clean quadrature signal, consecutive states should differ by exactly
one bit in Gray code (e.g. HH→HL, HL→LL, LL→LH, LH→HH). A "skipped
state" means the encoder jumped over an intermediate state — either due to
mechanical bounce, electrical noise, or transitions faster than the 50 ms
decimation window.

> [!NOTE]
> The 10.7% skip rate is likely inflated by the 50 ms sampling window. At
> fast rotation speeds, two quadrature transitions within a single 50 ms
> window would appear as a skipped state. The PCNT peripheral would not
> have this limitation.

### 2.4 Direction Analysis

| Direction | Steps | Notes |
|---|---|---|
| CW (increase) | 74 | Phase 1–2 of test |
| CCW (decrease) | 134 | Phase 2–3 of test |
| **Net position** | **−60** | |

### 2.5 Click Event Grouping

Encoder transitions were grouped into "click events" — clusters of
transitions within 300 ms of each other:

| Metric | Value |
|---|---|
| Total click groups | 54 |
| Steps per click (min) | 1 |
| Steps per click (max) | 5 |
| Steps per click (mean) | 2.1 |

---

## 3. Encoder-Motor Correlation

### 3.1 Response Rate

| Metric | Value |
|---|---|
| Encoder clicks that triggered motor | 39 (72%) |
| Encoder clicks without motor response | 15 (28%) |
| Total motor activations | 32 |

> [!IMPORTANT]
> **28% of encoder clicks produced no motor response.** This confirms the
> user's observation that "it sometimes takes several clicks to change the
> resistance setting." The console's encoder decoder is likely losing counts
> due to the noisy signal, or has an internal dead-zone that requires
> multiple detent steps before acting.

### 3.2 Motor Activation Timeline

32 motor events observed during the 84-second test window:

| # | Time (ms) | Direction | Duration | Notes |
|---|---|---|---|---|
| 1 | 1114438 | REV | 2,091 ms | Initial decrease |
| 2 | 1117498 | REV | 714 ms | |
| 3–7 | 1121170–1126270 | FWD | 255–408 ms | Series of short increases |
| 8 | 1129789 | REV | 663 ms | Direction reversal |
| 9 | 1131013 | FWD | 918 ms | |
| 10–11 | 1133410–1135501 | REV/FWD | 663–918 ms | Oscillating |
| 14 | 1144120 | REV | **51 ms** | Very brief — possibly spurious |
| 15 | 1144681 | FWD | **5,967 ms** | Extended motor run (fast spin) |
| 16 | 1151158 | REV | 1,836 ms | Long reversal |
| 17–31 | 1153555–1194797 | Mixed | 153–2,244 ms | Return-to-start phase |

### 3.3 Motor Duration vs Click Size

| Click steps | Avg motor duration | Motor response rate |
|---|---|---|
| 1 step | ~300 ms | 58% |
| 2 steps | ~500 ms | 75% |
| 3+ steps | ~1,200 ms | 92% |

Larger click events (more detent steps) are more likely to trigger the
motor and produce longer motor runs. Single clicks frequently fail to
register with the console.

### 3.4 Wiper Correlation

The resistance pot wiper tracked the motor activations:
- **Start:** Wiper = 442 (approximately resistance level 13)
- **Peak:** Wiper ≈ 1040 (approximately level 28)
- **End:** Wiper = 177 (approximately level 4)

The wiper-to-motor correlation confirms the console's closed-loop control:
encoder clicks → console decoder → motor drive → pot position feedback.

---

## 4. Key Findings

### 4.1 Encoder Hardware Quality: Moderate

The encoder produces valid quadrature signals but with significant mechanical
bounce (39% of states last ≤ 50 ms). This is typical of low-cost incremental
encoders without hardware debouncing.

### 4.2 Console Decoder: Weak

The Echelon console's encoder decoder appears to have:
- **Poor debouncing** — it misses or miscounts ~28% of encoder clicks
- **No acceleration** — fast rotation does not proportionally increase the
  rate of resistance change
- **Possible dead-zone** — small 1-click adjustments often fail to register

### 4.3 MITM Opportunity

An ESP32-based MITM encoder path could significantly improve the user
experience:

1. **ESP32 PCNT peripheral** — hardware-level edge detection at MHz speed
   with configurable glitch filter (1–1023 APB clock cycles). This would
   eliminate the 50 ms sampling limitation and capture every edge.

2. **Framework EncoderModule** — provides:
   - Hardware debouncing via PCNT glitch filter
   - Velocity-based acceleration (configurable dead-zone, cutoff, max multiplier)
   - Normalised detent counting (stepsPerDetent = 2 or 4)
   - Atomic delta consumption for thread-safe polling

3. **Clean signal re-synthesis** — the ESP32 could output a clean, debounced
   quadrature signal to the Echelon console's encoder input, eliminating
   the missed clicks and optionally adding acceleration.

### 4.4 Recommended Next Steps

1. **Enable `WITH_ENCODER`** — add the framework's EncoderModule on GPIO 8/9
   (release from ADC scan). Measure position accuracy and compare against
   ADC baseline.
2. **Tune PCNT glitch filter** — find the optimal filter width that eliminates
   bounce without losing valid fast transitions.
3. **Characterise detent-per-click** — determine `stepsPerDetent` (likely 2
   or 4) by correlating PCNT raw counts with physical detent clicks.
4. **Phase 5+ MITM design** — output synthesised quadrature to the console
   via two spare GPIOs through the MITM board.

---

## 5. Raw Data Reference

### Datasets

| File | Rows | Columns | Description |
|---|---|---|---|
| [`B02-...-raw.csv`](B02-20261004-encoder-baseline-raw.csv) | 1,646 | 10 | Encoder channels only (ts, EncA, EncB, VCC × min/mean/max) |
| [`B02-...-full.csv`](B02-20261004-encoder-baseline-full.csv) | 1,646 | 31 | All 10 ADC channels (ts + 10 × min/mean/max) |

### Column Map (full dataset)

| Index | Column | Signal |
|---|---|---|
| 0 | ts | Timestamp (ms since boot) |
| 1–3 | Wiper min/mean/max | Resistance pot wiper (CH0) |
| 4–6 | Hi min/mean/max | Pot high reference (CH1) |
| 7–9 | Lo min/mean/max | Pot low reference (CH2) |
| 10–12 | CadR min/mean/max | Cadence return (CH3) |
| 13–15 | Cad min/mean/max | Cadence (CH4) |
| 16–18 | MotP min/mean/max | Motor sense + (CH5) |
| 19–21 | MotN min/mean/max | Motor sense − (CH6) |
| 22–24 | EncA min/mean/max | Encoder A (CH7) |
| 25–27 | EncB min/mean/max | Encoder B (CH8) |
| 28–30 | VCC min/mean/max | Encoder VCC (CH9) |
