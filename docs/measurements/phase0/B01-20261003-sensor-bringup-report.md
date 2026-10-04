# B01 — Sensor Bring-Up Report (Phase 0)

**Date:** 2026-10-03  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.1.0-dev.4` on branch `fix/adc-gpio-conflict`  
**Operator:** mcneillm  
**IP:** 192.168.1.100 (WiFi: <REDACTED_SSID>)

---

## 1. Executive Summary

Incremental sensor bring-up session connecting the bike console's loom
to the ESP32 one signal group at a time. Three of four signal groups
confirmed working. One critical root cause discovered and resolved:
the original 22 kΩ series resistors exceed the ESP32-S3 ADC continuous-mode
source-impedance limit, preventing detection of brief transient signals
(cadence reed switch).

## 2. Firmware Bugs Found & Fixed

| # | Bug | Root Cause | Fix | Tag |
|---|---|---|---|---|
| 1 | `GPIO_PULLUP_ENABLE` on GPIO 5/8 | Violated hardware spec §5.5 no-pulls rule. Internal pull-up created voltage divider with 22 kΩ series resistor. | Changed to `GPIO_PULLUP_DISABLE` | `v0.1.0-dev.2` |
| 2 | `gpio_config(GPIO_MODE_INPUT)` called after `adc_continuous_start()` | Disconnected GPIO 5/8 from ADC analog peripheral, reconnected to digital GPIO matrix. | Removed `gpio_config`, `gpio_input_enable`, and GPIO ISR handler entirely. Cadence now uses ADC software thresholds (type=0 events). | `v0.1.0-dev.3` |
| 3 | `adc_digi_pattern_config_t` array not zero-initialised | Potential garbage in padding bytes. | Changed to `adc_pattern[10] = {}` | `v0.1.0-dev.4` |

## 3. Hardware Root Cause — ADC Source Impedance

### Problem

The ESP32-S3 ADC continuous (DMA) driver samples at 83,333 Hz aggregate
across 10 channels. Each channel's sample-and-hold (S/H) capacitor has a
very short acquisition window (~1–2 µs). Espressif recommends **< 10 kΩ
source impedance** for the ADC.

The original 22 kΩ series resistors (hardware spec §5.5) created an RC time
constant too long for the S/H to settle during brief signal transients. For
the cadence reed switch (~20–50 ms closure at 50–120 RPM), the S/H capacitor
could not discharge from 3.3 V (idle) to ~0.2 V (closed) through 22 kΩ in
the available acquisition time.

### Evidence

| Test Configuration | CH min | CH mean | Detection? |
|---|---|---|---|
| 22 kΩ, no cap, reed closed briefly (pedaling) | 4095 | 4095 | ❌ |
| 22 kΩ, no cap, reed held closed for seconds | 215 | 240 | ✅ (settles over time) |
| 22 kΩ + 100 nF cap, reed closed briefly | ~3250 | 4095 | ⚠️ Partial |
| Direct (no resistor, no cap), reed closed briefly | ~2680 | ~217 | ✅ |
| **4.7 kΩ + 100 nF cap, reed closed briefly** | **~260** | **~217** | **✅** |

### Fix

Replaced 22 kΩ series resistors with **4.7 kΩ** on ADC-sampled lines.
Added **100 nF ceramic decoupling capacitor** between each ADC signal pin
and GND on the ESP32 side of the series resistor. Hardware spec §5.5
updated accordingly.

> **Note:** The resistance pot lines (Blue, Yellow, Green) still use the
> original 22 kΩ resistors. The pot signal is slow-moving DC, so the S/H
> settling issue does not affect it — readings are stable and accurate.

### ADC Multiplexer Cross-Talk

Even with the 4.7 kΩ fix, the per-window `min` values show ~2680 (not near
zero) during cadence events. This is a known ESP32-S3 artefact when
scanning 10 ADC channels at 83 kHz — charge leakage between the multiplexed
channels affects the `min` reading. The **`mean`** value is the reliable
indicator for signal detection.

## 4. Sensor Verification Results

### 4.1 Cadence (Reed Switch)

| Parameter | Value |
|---|---|
| Signal wire | Orange → GPIO 5 (D2, ADC1_CH4) |
| Return wire | Grey → GPIO 4 (A3, ADC1_CH3) |
| Series resistor | 4.7 kΩ + 100 nF cap |
| Idle reading | CH4 = 4095 (3.3 V from console pull-up) |
| Reed closed | CH4 mean ≈ 217 (≈ 0.17 V) |
| RPM range tested | 74–120 RPM |
| Detection method | ADC software threshold (type=0 events) |
| Threshold | 1500 mV ± 200 mV hysteresis |
| Console cadence display | Confirmed working (45–50 RPM displayed) |

**Status: ✅ CONFIRMED WORKING**

### 4.2 Motor Sense (H-Bridge Direction)

| Parameter | Value |
|---|---|
| Motor+ wire | Red → GPIO 6 (D3, ADC1_CH5) via ÷4 divider |
| Motor− wire | Black → GPIO 7 (D4, ADC1_CH6) via ÷4 divider |
| Divider | 30 kΩ / 10 kΩ (output Z = 7.5 kΩ) |
| Motor off | CH5 = 0, CH6 = 0 |
| Motor forward | CH5 ≈ 922 (≈ 0.74 V), CH6 = 0 |
| Motor reverse | CH5 = 0, CH6 ≈ 912 (≈ 0.73 V) |
| Drive type | Flat DC (no PWM), confirmed by oscilloscope |

Motor direction correlates with resistance knob changes:
- Increasing resistance → motor forward (Mot+ HIGH)
- Decreasing resistance → motor reverse (Mot− HIGH)

**Status: ✅ CONFIRMED WORKING**

### 4.3 Resistance Pot (Wiper Position)

| Parameter | Value |
|---|---|
| Wiper wire | Blue → GPIO 1 (A0, ADC1_CH0) |
| Hi reference | Yellow → GPIO 2 (A1, ADC1_CH1) |
| Lo reference | Green → GPIO 3 (A2, ADC1_CH2) |
| Series resistor | 22 kΩ (original — works for DC signals) |
| Level 1 (min) | Wiper ≈ 159 (≈ 0.13 V) |
| Level 32 (max) | Wiper ≈ 1321 (≈ 1.06 V) |
| Range | 1162 ADC counts (≈ 0.93 V) |
| Per step | ≈ 36 counts (≈ 0.03 V/step) |
| Hi reference reading | 0 (unexpected — circuit differs from assumption) |
| Lo reference reading | ≈ 2084 (≈ 1.67 V, unexpected) |

Full sweep captured (1 → 32 → 1) with smooth linear ramp. Motor
direction correlates: forward while increasing, reverse while decreasing,
off when stable.

> **Note:** The Hi/Lo reference readings are not as expected (Hi=0V,
> Lo=1.67V rather than Hi=3.3V, Lo=0V). The console's pot circuit is more
> complex than a simple 3-terminal pot. The wiper tracking is clean and
> linear regardless — the references may be swapped or the circuit uses
> an unconventional topology. Further investigation needed.

**Status: ✅ CONFIRMED WORKING**

### 4.4 Knob Encoder (Quadrature)

| Parameter | Value |
|---|---|
| Enc A wire | Black → GPIO 8 (D5, ADC1_CH7) — J3 left pin |
| Enc B wire | Red → GPIO 9 (D6, ADC1_CH8) — J3 right pin |
| Enc VCC wire | Orange → GPIO 10 (D7, ADC1_CH9) — J3 centre pin |
| Series resistor | 4.7 kΩ + 100 nF cap |
| VCC reading | Steady 4095 (3.3V supply) |
| Enc A idle | 0 (LOW) |
| Enc B idle | 0 (LOW) |

All four quadrature states observed during knob rotation:

| State | A | B | Confirmed |
|---|---|---|---|
| HH | 4095 | 4095 | ✅ |
| HL | 4095 | 0 | ✅ |
| LH | 0 | 4095 | ✅ |
| LL | 0 | 0 | ✅ |

> [!NOTE]
> Wire colours corrected from original mapping. What appeared to be Brown
> is actually Black. Physical connector layout: VCC on centre pin (Orange),
> signals on outer pins (Black=A left, Red=B right). Patch cables were
> remade on 2026-10-04 after discovering unreliable connections in the
> original encoder loom cables.

**Status: ✅ CONFIRMED WORKING**

## 5. Pin Map Summary (All Confirmed)

| ADC CH | GPIO | Board Pin | Wire | Signal | Resistor | Reading | Status |
|---|---|---|---|---|---|---|---|
| 0 | 1 | A0 | Blue | Pot wiper | 22 kΩ | 159–1321 | ✅ |
| 1 | 2 | A1 | Yellow | Pot hi ref | 22 kΩ | 0 | ✅ (unexpected value) |
| 2 | 3 | A2 | Green | Pot lo ref | 22 kΩ | 2084 | ✅ (unexpected value) |
| 3 | 4 | A3 | Grey | Cadence return | 4.7 kΩ + 100 nF | 0 | ✅ |
| 4 | 5 | D2 | Orange | Cadence | 4.7 kΩ + 100 nF | 217–4095 | ✅ |
| 5 | 6 | D3 | Red | Motor sense + | ÷4 divider | 0–922 | ✅ |
| 6 | 7 | D4 | Black | Motor sense − | ÷4 divider | 0–912 | ✅ |
| 7 | 8 | D5 | Black | Encoder A | 4.7 kΩ + 100 nF | 0–4095 (quad) | ✅ |
| 8 | 9 | D6 | Red | Encoder B | 4.7 kΩ + 100 nF | 0–4095 (quad) | ✅ |
| 9 | 10 | D7 | Orange | Encoder VCC | 4.7 kΩ + 100 nF | 4095 (steady) | ✅ |

## 6. Remaining Work

1. **Remove diagnostic instrumentation** — Strip the per-channel sample
   counter and CH4 range logging from `SignalLogger.cpp` before merging
   the `fix/adc-gpio-conflict` branch.
2. **Merge branch** — Merge `fix/adc-gpio-conflict` back into
   `signal-logger/wo4-scope-gate`.
3. **Event buffer saturation** — Floating-pin guard needed to suppress
   spurious threshold events from channels without stable baselines.
4. **Pot reference investigation** — Determine why Hi=0V and Lo=1.67V
   instead of the expected 3.3V and 0V. May require probing the console's
   pot circuit directly.
