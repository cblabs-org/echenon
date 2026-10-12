# A01 Addendum — Single-Ended Motor-Wire Sign Convention (DMM)

**Date:** 2026-10-12
**Bench session:** DR-1 prep WO1
**Instrument:** Digital multimeter, DC voltage mode
**Reference:** Brown wire (console GND)
**Firmware:** `v0.1.0-dev.7` (env `ex3-waveshare-nano`)

## Purpose

The original A01 measurement (2026-10-11) reported ±3.25 V on the motor
wires, but this was a **differential** reading (Red vs Black). DR-1 §2.1
needs to know whether either wire goes below 0 V **against GND**, because
the ESP32's absolute minimum GPIO voltage is −0.3 V.

## Method

DMM probed each motor wire individually to Brown (GND), with the console
driving forward (knob up) and reverse (knob down), and at rest.

## Results

| Wire | At rest | Forward | Reverse |
|---|---|---|---|
| **Red** to GND | 0.00 V | **+3.30 V** | 0.03–0.12 V |
| **Black** to GND | 0.00 V | 0.01–0.10 V | **+3.30 V** |

### Transient observation

One brief flicker to **−0.14 V** was observed on the Black wire during a
forward→reverse motor reversal. This is consistent with back-EMF from the
H-bridge switching and is within the ESP32's −0.3 V absolute minimum.
Behind the existing ÷4 voltage divider, this would present as −0.035 V at
the GPIO — completely safe.

## Interpretation

1. **The console motor driver is a 3.3 V H-bridge.** One wire is driven to
   +3.3 V while the other stays near 0 V; direction is set by swapping
   polarity. Both wires are at 0 V when the motor is idle.

2. **The original A01 "−3.25 V on reverse" was a differential artifact.**
   During reverse: Red ≈ 0 V, Black ≈ +3.3 V, so a probe measuring
   Red-minus-Black reads −3.3 V. No wire actually reaches −3.25 V against
   GND.

3. **No clamping diodes are required on the motor sense network.** The ÷4
   dividers see a maximum of 3.3/4 = 0.825 V and a minimum of −0.035 V
   (transient), both well within the ESP32 ADC input range.

4. **The −0.14 V transient during reversal is standard H-bridge back-EMF.**
   Any active MITM motor driver would include flyback diodes as standard
   practice, which would suppress this entirely.

## Proposal for DR-1

- The sense network for motor-direction detection (H-01) does **not** need
  clamping diodes. The ÷4 dividers are sufficient.
- The "GPIO clamped at −3.25 V" concern from A01 is resolved — it was a
  measurement error (differential vs single-ended reference confusion).
