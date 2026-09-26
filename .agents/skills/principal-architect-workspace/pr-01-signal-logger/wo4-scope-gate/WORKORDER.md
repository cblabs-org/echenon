---
name: signal-logger-wo4
description: Handoff instructions for Work Order 4 — Bench setup + scope characterisation gate
complexity: interactive
---

# WO4 — Bench Setup + Scope Characterisation Gate

## Role Definition
> You are an expert senior software engineer assisting the user at the bench
> with oscilloscope and measurement equipment. This is a human-in-the-loop
> session — the user drives the physical work, and you guide them through the
> checklist, help interpret measurements, and commit evidence files.

## Objective

Complete the bench-ready verification, flash the Echenon firmware, perform the
motor-drive scope characterisation gate (hardware spec §5.2), measure motor
current, check Waveshare VIN tolerance, and commit all evidence to the Phase 0
register.

**After this WO, the scope gate is closed and motor-sense taps (GPIO 6/7) may
be connected.**

## Entry Conditions

- [ ] Scope (≥2 channels, single-shot capable) on the bench
- [ ] Motor current measurement tool (shunt resistor + DMM, or clamp)
- [ ] JST Y-harness or back-probe harness built per hardware spec §5.1
- [ ] Series resistors (22 kΩ × 8, 30 kΩ + 10 kΩ for ÷4 dividers × 2) ready
- [ ] Firmware flashed: branch `signal-logger/wo4-scope-gate` at the tip

## Context

- **Hardware spec §5.2:** Motor-drive characterisation gate — 6 scope captures:
  at rest, knob-up at 10 ms/div, knob-down at 10 ms/div, knob-up at µs/div,
  knob-down at µs/div, drive-end transient.
- **SPEC §13.2 tests:** 0.1 (flash), 0.5 (pin idle check), 0.11 (scope gate)
- **Register rows:** #1 (motor drive waveform), #2 (motor current), #11 (VIN)
- **Pin idle check (test 0.5):** With taps connected and logger off, then on,
  verify no tap pin drives or biases a console line.

## Checklist

### A. Flash and boot (SPEC test 0.1)
- [ ] Flash firmware via USB-C: `pio run -t upload -e ex3-waveshare-nano`
- [ ] Confirm WiFi setup AP or known network connection
- [ ] Confirm dashboard accessible at `echenon.local` or IP

### B. Pin idle check (SPEC test 0.5)
- [ ] With taps connected and Echenon unpowered: measure each tap pin (GPIO
  1–10) — none should have voltage present.
- [ ] Power Echenon via USB-C: measure each tap pin again — none should be
  driven HIGH or have significant current sourced/sunk.
- [ ] Record result: PASS / FAIL for each pin.

### C. Motor-drive characterisation gate (hardware spec §5.2, SPEC test 0.11)
- [ ] Connect scope Ch1 to Red (motor+) via the ÷4 divider, Ch2 to Black
  (motor−) via the ÷4 divider. GND to Brown.
- [ ] All 6 captures per §5.2:
  1. At rest (no knob movement)
  2. Knob up at 10 ms/div — expected: PWM drive waveform
  3. Knob down at 10 ms/div
  4. Knob up at µs/div — expected: individual PWM pulses
  5. Knob down at µs/div
  6. Drive end — transient when motor stops
- [ ] Save scope screenshots as:
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-at-rest.png`
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-up-10ms.png`
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-down-10ms.png`
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-up-us.png`
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-down-us.png`
  - `docs/measurements/phase0/A01-YYYYMMDD-motor-drive-end.png`
- [ ] **Key observation:** Is the motor drive PWM? What duty cycle? What
  frequency? Is there a flyback spike at drive end? Does the ÷4 divider
  bring it safely below 3.3 V?

### D. Motor current (register #2)
- [ ] Measure motor inrush and steady-state current under load (cam at max
  resistance). Method: shunt resistor + DMM, or clamp.
- [ ] Save evidence as `docs/measurements/phase0/A02-YYYYMMDD-motor-current.md`
  or `.csv`.

### E. Waveshare VIN tolerance (register #11)
- [ ] Check the Waveshare ESP32-S3-Nano datasheet for VIN maximum voltage.
- [ ] If the console's 9 V rail is to be used, confirm VIN handles 9 V.
- [ ] Record evidence as `docs/measurements/phase0/A11-YYYYMMDD-vin-tolerance.md`.

### F. Connect motor-sense taps
- [ ] **Only after the scope captures confirm the ÷4 divider output stays
  below 3.3 V:** connect the motor-sense tails (GPIO 6/7) to the breadboard.
- [ ] Verify on the dashboard that channels 5 and 6 show motor voltage when
  the knob is turned.

### G. Commit evidence
- [ ] `git add docs/measurements/phase0/A01-* docs/measurements/phase0/A02-* docs/measurements/phase0/A11-*`
- [ ] `git commit -m "pr-01/wo4: scope gate — motor drive, current, VIN evidence"`

## Verification Requirement
- All 6 scope captures committed as A01 evidence files.
- Motor current evidence committed as A02.
- VIN tolerance evidence committed as A11.
- Motor-sense taps connected and showing readings on the dashboard.

---

## Completion Report (written by the worker at task completion)

<!-- Append when complete:
### Status
complete | partial | blocked

### Register Rows Updated
- #1: (result summary)
- #2: (result summary)
- #11: (result summary)

### Scope Gate Result
- ÷4 divider safe: YES/NO
- Motor-sense taps connected: YES/NO

### Decisions Made / Out-of-Scope Findings / Blockers
-->
