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
// B03 single reference formula: wiper = 39.1 * level + 97
level_from_pot = constrain(round((wiper_adc - 97.0) / 39.1), 1, 32)
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


---

## 10. Architecture Spectrum

There are four levels of MITM integration, ranging from passive
monitoring to full simulation. The decision depends on how much control
we need vs. hardware complexity.

```
 Passive ◄────────────────────────────────────► Full Control
   Tap        Encoder MITM     Full MITM      Console Bypass
```

| Level | Encoder | Pot | Motor | What it gives us |
|---|---|---|---|---|
| **Monitor** | Tap | Tap | Console | Read-only telemetry |
| **Encoder MITM** | Intercept + resynth | Tap | Console | Clean input, console stays in control |
| **Full MITM** | Intercept + resynth | **Simulate** | Console | Tell console any level we want |
| **Console bypass** | Intercept | Read | **ESP32 H-bridge** | Total control, console is display-only |

### Pot Simulation Considerations

If we simulate the pot, we can decouple the reported level from the
physical motor position. This enables:
- Reporting arbitrary levels to the Echelon app
- ERG mode (external app sets target, ESP32 drives to it)
- Non-standard resistance curves (e.g., truly linear, exponential)

**Hardware options for pot simulation:**
- **DAC output** (e.g., MCP4725) — simplest, I²C, 12-bit
- **Digital potentiometer** (e.g., MCP4131) — drop-in replacement for
  the physical pot, SPI, maintains the console's expected impedance
- **PWM + RC filter** — cheapest, but resolution and ripple may be issues

**Risk:** If we simulate the pot, the console's motor control loop will
try to goalseek to the position we're reporting. If the physical motor
position doesn't match the simulated pot value, the motor will run
continuously trying to reach an unreachable target. We would need to
either:
1. Also control the motor (full bypass), or
2. Always keep the simulated pot value tracking the actual motor position,
   only deviating transiently to "nudge" the console

> [!NOTE]
> This is a future design decision. The first step is Encoder MITM
> (intercept + debounce + resynth) which requires no additional hardware
> beyond the MITM board's existing GPIOs. Pot simulation and console
> bypass are Phase 5+ options that depend on how much control the
> connected apps (Peloton, Zwift) actually need.


---

## 11. Console as Signal Source (Motor Ownership Model)

An alternative architecture: **own the motor** but use the console's
motor drive outputs as a **hint signal** rather than discarding them.

```
Physical Encoder
       │
       ▼
 ┌─────────────┐
 │   ESP32      │
 │   PCNT       │
 └──────┬──────┘
        │ clean encoder
        ▼
 ┌─────────────┐      ┌─────────────┐
 │  Quadrature  │─────▶│  Echelon    │
 │  Output      │      │  Console    │
 └─────────────┘      └──────┬──────┘
                              │ motor drive (FWD/REV voltage)
                              ▼
                       ┌─────────────┐
                       │  ESP32 ADC   │  ← read as "hint"
                       │  (sense)     │     (console wants to go FWD/REV)
                       └──────┬──────┘
                              │
                              ▼
                       ┌─────────────┐
                       │  ESP32 PID   │  ← smooth motor control
                       │  H-Bridge    │     using pot feedback
                       └──────┬──────┘
                              │
                              ▼
                         DC Motor
```

In this model:
- The console **thinks** it's driving the motor (its H-bridge outputs
  are connected to the ESP32's ADC, not the motor)
- The ESP32 reads the console's drive direction (FWD/REV/OFF) as a
  **hint** for what the console wants
- The ESP32 drives the motor itself via its own H-bridge with a proper
  **PID controller** using pot feedback
- The pot can remain connected to the console for level display, OR be
  simulated

**Advantages:**
- Smooth motor control (no bang-bang overshoot)
- Console continues to report correct levels to the app
- ESP32 can add soft-start, ramp-down, position hold
- Motor seek time could drop from ~2 s to < 500 ms
- Console's drive signal tells us the intended direction without
  needing to decode its internal state

**Risks:**
- Requires H-bridge hardware on the MITM board
- Console may detect motor current anomalies if it monitors back-EMF
- Console may timeout if pot doesn't converge at expected rate

## 12. Console Fault Tolerance — Tests Needed

Before committing to any MITM architecture, we need to understand how
the Echelon console handles non-conformant scenarios. These tests can
be performed with the current hardware by manipulating signals:

| # | Test | Method | What we learn |
|---|---|---|---|
| 1 | **Encoder disconnect** | Unplug encoder during operation | Does console timeout? Error display? |
| 2 | **Encoder stall** | Hold encoder still, observe timeout | How long before console gives up? |
| 3 | **Fast encoder burst** | Spin encoder rapidly (>10 clicks/sec) | Does console queue or drop pulses? |
| 4 | **Pot disconnect** | Disconnect pot wiper during motor seek | Does motor run forever? Safety cutoff? |
| 5 | **Pot clamp** | Hold pot at fixed value while encoder clicks | Does console detect non-convergence? |
| 6 | **Motor disconnect** | Disconnect motor, send encoder clicks | Does console detect stall current? |
| 7 | **Reverse encoder** | Swap A/B wires | Does console handle reverse gracefully? |
| 8 | **Slow convergence** | Add resistance to motor circuit | Tolerance for slow pot response? |
| 9 | **Mid-seek encoder** | Send encoder clicks while motor is seeking | Queue, ignore, or reset? |
| 10 | **Power cycle during seek** | Power off/on while motor is moving | Does it home or resume? |

> [!IMPORTANT]
> Tests 4 and 6 should be done cautiously — a motor running without a
> feedback signal or load could be a safety issue. Have a kill switch
> (power strip) within reach.

These tests will determine which MITM architecture is safe and which
console assumptions we can violate without bricking the user experience.
