# A04 Addendum — Tap Cross-Check (E-02): DMM vs ADC

**Date:** 2026-10-12
**Bench session:** DR-1 prep WO1
**Instruments:** Digital multimeter (DC voltage) and ESP32 ADC via
`GET /api/echenon/live`
**Reference:** Brown wire (console GND)
**Firmware:** `v0.1.0-dev.7` (env `ex3-waveshare-nano`)

## Purpose

Validate the absolute voltage accuracy of the ESP32 ADC readings against
DMM ground truth, measured at the **bike side of the 22 kΩ tap resistors**,
at three representative levels (1, 16, 32). This cross-checks the values
behind B03's `wiper = 39.1 × level + 97` formula and the 1.66 V Green
reference.

## Method

1. Set the console to level 1, 16, then 32 (by turning the knob).
2. At each level, simultaneously:
   - DMM probed wiper (Blue), Green, and Yellow to GND at the bike side
     of the 22 kΩ resistors.
   - `GET /api/echenon/live` captured the ADC mean counts on each channel.
3. ADC counts converted to mV using `mV = counts × 3300 / 4096`.

## Results

### DMM vs ADC comparison

| Level | Channel | DMM (mV) | ADC counts | ADC (mV) | Offset (mV) | Offset (%) |
|---|---|---|---|---|---|---|
| 1 | Wiper | 160 | 163 | 131 | +29 | +17.9%* |
| 1 | Green | 1780 | 2058 | 1658 | +122 | +6.9% |
| 1 | Yellow | 0 | 0 | 0 | 0 | — |
| 16 | Wiper | 640 | 712 | 574 | +66 | +10.4% |
| 16 | Green | 1790 | 2074 | 1671 | +119 | +6.7% |
| 16 | Yellow | 0 | 0 | 0 | 0 | — |
| 32 | Wiper | 1160 | 1329 | 1071 | +89 | +7.7% |
| 32 | Green | 1790 | 2080 | 1676 | +114 | +6.4% |
| 32 | Yellow | 0 | 0 | 0 | 0 | — |

\* Low-voltage relative error is magnified; the absolute offset is only 29 mV.

> [!IMPORTANT]
> **All offsets exceed the 2% threshold.** The DMM consistently reads higher
> than the ADC by 6–7% on the Green reference and 8–18% on the wiper. This
> is a systematic bias, not random noise.

### B03 formula validation

| Level | Predicted (counts) | Actual (counts) | Delta | Delta (%) |
|---|---|---|---|---|
| 1 | 136 | 163 | +27 | +19.8%* |
| 16 | 723 | 712 | −11 | −1.5% |
| 32 | 1348 | 1329 | −19 | −1.4% |

\* The B03 formula was derived from a least-squares fit across levels 1–32.
At the extreme low end the fit diverges; at levels 16 and 32 it is accurate
to within 1.5%.

### Green reference stability

| Metric | ADC | DMM |
|---|---|---|
| Range | 2058–2080 counts (18 mV) | 1780–1790 mV |
| Spread | 22 counts | 10 mV |

The Green reference is very stable across all levels — good to 18 mV ADC /
10 mV DMM.

## Interpretation

1. **The 6–7% systematic offset on Green is consistent across all levels.**
   This points to either ESP32 ADC nonlinearity at the ~1.7 V range, or the
   22 kΩ tap resistor interacting with the ADC's sampling circuit input
   impedance (drawing current during conversion, causing a voltage drop
   across the series resistance).

2. **The wiper offset scales with voltage** (29 mV at 160 mV, 66 mV at
   640 mV, 89 mV at 1160 mV), consistent with a proportional gain error
   in the ADC path.

3. **Yellow (0 V) has zero offset**, confirming the GND reference is shared
   correctly.

4. **The B03 formula remains valid for level discrimination.** The ADC-to-ADC
   relationship (wiper counts vs level) is internally consistent (R² =
   0.9989 from B03). The systematic offset affects absolute voltage
   reconstruction but not level detection, because the same bias applies to
   both wiper and reference channels.

5. **For MITM pot resynthesis**, the absolute voltage matters more than for
   passive monitoring. The 6–7% offset means a DAC targeting 1.79 V (DMM
   truth) based on the ADC reading of 1.66 V would produce the wrong
   voltage unless calibrated.

## Proposal for DR-1

- **The 6–7% ADC-to-DMM offset should be characterised as a calibration
  constant** if the MITM design uses ADC readings to set DAC output levels.
  A simple gain correction factor of ~1.07× applied to the ADC mV reading
  would bring it within 1% of DMM truth.
- **For passive monitoring (current architecture), the offset is
  irrelevant** — level detection works on relative ADC counts, not absolute
  voltages.
