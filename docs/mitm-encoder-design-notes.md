# MITM Encoder — Design Notes

**Status:** Early thinking — not yet a work order  
**Date:** 2026-10-04  
**Context:** Findings from encoder & resistance analysis (see `encoder-resistance-analysis.md`)

---

## 1. Core Requirement

The Echelon app must remain functional and continue to report resistance
level, calories, power output, etc. This means the Echelon console must
continue to receive encoder signals and control the motor. We cannot
bypass the console — we must work *through* it.

## 2. Internal Resolution

Use **256 internal levels** (`uint8_t`) as the canonical resistance scale.
External mappings:

```
echelon_level  = internal_level * 32 / 256     // Echelon 1-32
peloton_level  = internal_level * 100 / 256    // Peloton 1-100
internal_level = echelon_level * 256 / 32      // reverse
```

256 gives ~0.06 mm displacement resolution per step on the eccentric cam.

## 3. Architecture: Intercept, Translate, Forward

```
Physical Encoder
       │
       ▼
 ┌─────────────┐
 │  ESP32 PCNT  │  ← hardware debounce, acceleration
 │  (ingest)    │
 └──────┬──────┘
        │
        ▼
 ┌─────────────┐
 │  Translation │  ← 256-level internal model
 │  & Remap     │     linearisation, acceleration curves
 └──────┬──────┘
        │
        ▼
 ┌─────────────┐
 │  Quadrature  │  ← synthesise clean A/B signals
 │  Output GPIO │     to Echelon console encoder input
 └──────┬──────┘
        │
        ▼
 Echelon Console
 (motor control, app reporting, calories, power)
```

The ESP32 sits between the physical encoder and the console's encoder
input. It ingests the noisy signal via PCNT, translates it through the
internal 256-level model, and outputs a clean synthesised quadrature
signal that the console's decoder can reliably read.

## 4. Pot Feedback — Monitoring Only

The pot remains connected to both the Echelon console (for its motor
control feedback loop) AND the ESP32 ADC (for monitoring). The ESP32
does NOT control the motor — it only observes the pot to confirm what
level the console actually reached.

```
level_from_pot = constrain(round((wiper_adc - 125) / 37.7) + 1, 1, 32)
```

## 5. Goalseek Risk

The console's motor control loop drives the motor until the pot matches
a target position. If our synthesised encoder signal commands a position
between two "natural" Echelon levels, the console will goalseek to the
nearest valid level.

**Key question:** What does the console do if the pot never converges to
its expected value?

Possible outcomes:
- **Snaps to nearest level** (most likely — console already uses ~15 ADC
  tolerance band)
- **Hunts between two levels** (possible if tolerance is tight)
- **Timeout and stop** (safe but annoying)

**Mitigation:** Only synthesise encoder signals that correspond to exact
Echelon level boundaries. The 256-level internal model provides smooth
reporting and linearisation, but the actual signals sent to the console
snap to the nearest of the 32 valid positions.

## 6. Linearisation Strategy

The eccentric cam means equal encoder clicks do NOT produce equal
resistance changes (see §7 of `encoder-resistance-analysis.md`). To give
the rider perceptually linear resistance steps:

**Option A — Linearise at the encoder output:**
Vary the number of synthesised quadrature pulses per internal level
change. More pulses at the cam extremes (levels 1–5, 28–32), fewer
in the middle (levels 14–20) where the cam is steepest.

**Option B — Linearise at the reporting layer:**
Send unmodified encoder signals to the console (1:1 pass-through with
debounce only), but report linearised resistance values to connected
apps (Peloton, Zwift, etc.) via FTMS/BLE.

**Option C — Hybrid:**
Pass through with debounce to the console. Report linearised values to
apps. Optionally inject extra encoder pulses for fine-grained control
when driven by an external app (e.g., ERG mode in Zwift).

> [!NOTE]
> Option B is the lowest risk and easiest to implement. Option C adds
> the most value for smart trainer integration. Option A is the most
> complex and highest risk (goalseek divergence).

## 7. Open Questions

1. How many encoder detents per Echelon level? (B02 measured ~1.4
   quadrature steps per level via ADC — PCNT measurement needed for
   true count)
2. Does the console accept arbitrary quadrature frequency, or does it
   have a minimum pulse width requirement?
3. What is the console's pot tolerance band? (Need to test by injecting
   encoder pulses that land between levels)
4. Can we detect ERG mode commands from the Echelon app via BLE sniffing?
5. What is the console's behaviour when encoder pulses arrive during
   motor seek? (Does it queue, ignore, or reset?)

## 8. Dependencies

- **EncoderModule** on GPIO 8/9 (requires releasing from ADC scan)
- **Two spare GPIOs** for synthesised quadrature output
- **MITM board** PCB design (Phase 5+)
- **BLE FTMS** integration (PR-05)

## 9. References

- [`docs/encoder-resistance-analysis.md`](encoder-resistance-analysis.md) — signal quality, calibration, cam model
- [`docs/measurements/phase0/B02-*`](measurements/phase0/) — encoder characterisation data
- [`docs/measurements/phase0/B03-*`](measurements/phase0/) — resistance calibration data
- `framework/modules/EncoderModule/` — PCNT-based encoder module
