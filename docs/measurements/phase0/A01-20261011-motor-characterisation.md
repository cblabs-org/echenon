# A01 — Motor Drive Characterisation

**Date:** 2026-10-11
**Register row:** #1 — Motor drive characterisation
**Equipment:** Rigol DHO804 oscilloscope, probe directly on motor wires (not through ÷4 divider)
**Operator:** Owner at bench, PA recording

## Summary

The EX-3 console drives the resistance-adjustment motor with **flat bidirectional DC** — no PWM.

## Findings

### Drive Voltage (direct on motor wires)

| Direction | Voltage |
|---|---|
| Forward (resistance increase) | +3.243 V |
| Reverse (resistance decrease) | −3.251 V |
| Idle (no drive) | ≈ 0 V (−26 mV to −71 mV) |

### Through ÷4 Divider (at ESP32 GPIO 6/7)

| Direction | Divider output | ADC sees |
|---|---|---|
| Forward | +0.81 V | +0.81 V (readable) |
| Reverse | −0.81 V | 0 V (clamped by input protection) |
| Idle | ≈ 0 V | 0 V |

**Note:** The ADC can only detect forward drive. Reverse drive is clamped to 0V by the ESP32's input protection diodes. Direction detection requires comparing both motor+ (GPIO 6) and motor− (GPIO 7) channels: the driven wire goes to +0.81V while the return wire stays at 0V.

### Modulation

**No PWM detected.** The drive signal is flat DC at all timescales examined:
- 20 ms/div: smooth rectangular pulse, no modulation
- 50 µs/div (12.5 MSa/s): flat noise floor only, no switching pattern

This confirms the B01 finding.

### Waveform Shape

- **Onset:** Clean step, < 1 ms rise time at 20 ms/div
- **Drive:** Sustained flat DC for the duration of the motor seek
- **Drive end:** Clean step back to 0V with small transient steps (back-EMF / coasting)

## Scope Captures

1. `A01-20261011-motor-rev-end.png` — Reverse drive ending: −3.25V → 0V (10 ms/div)
2. `A01-20261011-motor-fwd-onset.png` — Forward drive onset: 0V → +3.24V (20 ms/div)
3. `A01-20261011-motor-fast-timebase.png` — Fast timebase: no PWM (50 µs/div, 12.5 MSa/s)

## Result

**PASS** — Motor drive characterised: ±3.25V flat bidirectional DC, no PWM, clean transitions.
