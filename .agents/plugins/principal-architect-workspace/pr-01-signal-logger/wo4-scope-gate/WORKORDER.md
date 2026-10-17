---
schema_version: 1
wo_id: WO-01.40
pr_id: PR-01
title: Bench Setup + Scope Characterisation Gate
complexity: interactive
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
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

## Completion Report

### Status
**partial** — Session redirected to higher-priority encoder characterisation
and MITM design analysis. Scope gate items (B, C, D, E) deferred.

### Checklist Status

| Section | Item | Status |
|---|---|---|
| A | Flash and boot | ✅ `v0.1.0-dev.1` → `v0.1.0-dev.5` |
| A | WiFi connection | ✅ `<REDACTED_SSID>` at 192.168.1.100 |
| A | Dashboard accessible | ✅ `echenon.local` and IP |
| B | Pin idle check | ⬜ Not performed |
| C | Motor-drive scope captures (6×) | ⬜ Not performed |
| D | Motor current measurement | ⬜ Not performed |
| E | VIN tolerance check | ⬜ Not performed |
| F | Motor-sense taps connected | ✅ GPIO 6/7 reading on dashboard |
| G | Evidence committed | ✅ B01–B04 reports + CSVs |

### Register Rows Updated
- #1: ⬜ Motor drive waveform — scope captures not yet taken
- #2: ⬜ Motor current — not measured
- #11: ⬜ VIN tolerance — not checked

### Scope Gate Result
- ÷4 divider safe: **NOT YET VERIFIED** (motor-sense taps connected based on
  prior analysis, reads are within range, but no scope capture performed)
- Motor-sense taps connected: **YES** — GPIO 6/7 showing motor voltage on dashboard

### Work Performed Beyond Original Scope

The session pivoted to encoder and resistance characterisation after
discovering encoder signal quality issues during sensor bring-up:

**Evidence committed (B01–B04):**
- B01: All 10 sensor channels confirmed working
- B02: Encoder ADC baseline — 39% bounce, 10.7% skip, 28% miss rate
- B03: 32-level resistance calibration (R²=0.999), motor seek analysis,
  eccentric cam model (8mm offset, 0–151° sweep)
- B04: PCNT EncoderModule comparison — 0% bounce, 0% skip, 100% detection

**Code changes:**
- `gpio_config()` fix for ADC analog path (GPIO 5/8)
- Internal pull-ups disabled on tap pins
- EncoderModule wired on GPIO 8/9 (PCNT), ADC reduced 10→8 channels
- Diagnostic instrumentation added then stripped

**Design documents:**
- `docs/encoder-resistance-analysis.md` — consolidated signal analysis
- `docs/mitm-encoder-design-notes.md` — MITM architecture options, 256-level
  model, console fault tolerance test plan

### Decisions Made
- Cadence series resistors changed from 22kΩ to 4.7kΩ + 100nF (signal too weak)
- Encoder wire colours corrected: Black=A, Red=B, Orange=VCC
- GPIO 8/9 released from ADC scan for PCNT EncoderModule
- `stepsPerDetent=4` confirmed correct for this encoder
- 256 internal resistance levels adopted for MITM design

### Out-of-Scope Findings
- Eccentric cam produces 22× displacement variation across 32 levels — rider
  experience is inherently non-linear. Linearisation is a Phase 5+ concern.
- Console uses bang-bang motor control with 98% overshoot at mid/high levels.
  PID control from ESP32 could reduce seek time from ~2s to <500ms.
- Console fault tolerance must be tested before committing to MITM architecture
  (10 tests identified in design notes §12).

### Blockers
- Scope gate items (B, C, D, E) require dedicated bench time with scope.
  Recommend a separate interactive WO.

