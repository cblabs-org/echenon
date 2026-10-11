# Phase 0 Register (PR-01 Exit Checklist)

This directory holds the Phase 0 measurement evidence: oscilloscope
captures, CSV logs (ride logs and burst captures), photos and notes.

This register is the **PR-01 exit checklist** and the input to the Design
Freeze Review (DR-1). It is the only document referred to as "Appendix A".
PR-01 exits when **every row has an Evidence file and a Result**. Either
outcome of a measurement is acceptable; the result feeds DR-1.

Audit codes (H-, S-, M-, B-, P-) refer to the
[2026-09-23 Chief Architect audit](../../../.agents/skills/principal-architect-workspace/reviews/2026-09-23-chief-architect-audit.md).
The Phase 0/1 pin map is SPEC §4.4.1 ([`../../../SPEC.md`](../../../SPEC.md));
the bench equipment is in the
[hardware build guide](../../hardware_specification.md) §2.1 and the bench
setup and procedures in §5. The PR-01 tests that produce this evidence are
SPEC §13.2.

## Evidence file naming

```
A<row>-<yyyymmdd>-<short-description>[-<n>].<ext>
B<session>-<yyyymmdd>-<short-description>.<ext>
```

- `<row>` is the register row number, zero-padded to two digits (`A01` … `A13`). `A<row>-…` denotes row-specific evidence files.
- `<session>` denotes bench session reports (`B01` … `B04`) that span multiple register rows. Dual prefix scheme: `A<row>-…` for row-specific evidence, `B<session>-…` for bench session reports.
- `<short-description>` is lowercase, hyphen-separated, and names the
  condition (for example `red-black-knob-up-10ms`).
- `-<n>` numbers repeats of the same condition.
- `<ext>` is the native format: `.csv` for logger exports, `.png` / `.csv`
  for scope captures, `.jpg` for photos, `.md` for notes and reports.

Examples: `A01-20261002-red-black-knob-up-10us.png`,
`A04-20261011-levels-1-32.csv`, `B01-20261003-sensor-bringup-report.md`.

A row that needs several files lists them all in the Evidence file column;
a short `A<row>-…-notes.md` or B-report may summarise them.

## Register

| # | SPEC assumption | Current evidence | Phase 0 measurement | Design impact if wrong | Evidence file | Result / status |
|---|---|---|---|---|---|---|
| 1 | Console motor drive is 3.3 V DC bang-bang | DMM average only | Scope both wires, both directions, and at rest (brake or coast?), at 10 ms/div and µs/div (build guide §5.2) | Sense network (H-01), relay logic, motor rail | `A01-20261011-motor-characterisation.md`, `A01-20261011-motor-rev-end.png`, `A01-20261011-motor-fwd-onset.png`, `A01-20261011-motor-fast-timebase.png` | **PASS** — ±3.25V flat bidirectional DC, no PWM. Forward +3.24V, reverse −3.25V. Clean transitions. Confirmed at 50 µs/div (12.5 MSa/s). |
| 2 | Motor current ≤ 800 mA, intermittent | None | Shunt or clamp: start inrush, and running current with the worm under brake load at cam-max | Driver choice, rail, brownout (H-03) | `A02-20261011-motor-current.md` | **PASS** — ≤175 mA peak (reverse inrush), ≤150 mA steady-state. Forward: 95 mA inrush, 50 mA steady. Motor drives eccentric cam; load is independent of flywheel speed. Well under 800 mA. |
| 3 | Motor stops within one level from 20 Hz feedback | None | Log pot mV against time for **one full cam revolution** (this also gives the run-time bound) and coast distance after the drive stops | Loop rate, dead zone, braking profile (M-02, M-01) | `A03-20261011-pot-revolution.csv`, `A03-20261011-pot-revolution-burst.csv`, `A03-chart-pot-revolution-run1.png` | **PASS** — Pot mV vs time logged for full revolution. Motor run time ~16 s per half-revolution. Coast distance minimal (<15 ADC counts). |
| 4 | Wiper 1160–1700 mV, linear over levels 1–32 | DMM across the *outer* terminals ("strange"); r2: the cam makes it sinusoidal | Wiper, Yellow and Green at every console level, both directions; resistance proxy (console level, or power at fixed cadence) against angle | Level table, calibration method, 100-level claim (S-01) | `A04-20261011-levels-1-32.csv`, `A04-20261011-levels-1-32-events.csv`, `A04-chart-levels-1-32.png`, `B03-20261004-resistance-calibration.md`, `B03-20261004-resistance-calibration.csv` | **PASS** — Wiper sweeps 165–1327 ADC counts (133–1069 mV) across levels 1–32. Highly linear (R² = 0.9989). Single reference formula from B03: `wiper = 39.1 × level + 97`. Step size ~37.5 ADC / 30.2 mV. Minimal hysteresis (<15 ADC counts). |
| 5 | The pot's shape over one revolution (number of sweeps, click-off points) relative to cam-min and cam-max; which half the console works in | Teardown notes say two 180° sweeps (owner unsure, r3); r2: continuous cam, no end stops | Drive a full revolution; mark the click-offs and the resistance extremes | Angle tracking, mirror-position ambiguity (S-01) | `A03-20261011-pot-revolution.csv`, `A03-20261011-pot-revolution-run2.csv`, `A03-chart-pot-revolution-run1.png` | **PASS** — Continuous 360° rotation: two symmetrical working ramps (0–2120 ADC counts), two dead zones (gap crossing ~5°), two non-working plateaus. Console utilizes working ramp half from 165 to 1327 counts. |
| 6 | Green = GND and Yellow = 3.3 V from a stiff source | None; #4 suggests a bias | Measure both (GPIO 3 and GPIO 2 taps), plus source impedance (load test) | R4 and the Green–GND tie (H-02) | `B01-20261003-sensor-bringup-report.md` | **PASS (measured fact)** — Unexpected polarity: Yellow = 0 V (always), Green = 1.66 V (≈ 2,060–2,084 counts) active voltage source with 2.4 kΩ source impedance. R4 and Green–GND standalone network requires redesign at DR-1. |
| 7 | Cadence: Orange pulled up by the console, reed closes to GND | Colour mapping contradicts itself (ex3 §7 vs §8) | Orange, Grey and Brown voltages open and closed (GPIO 5 and GPIO 4 taps, plus DMM) | R3 path; whether Grey needs grounding | `B01-20261003-sensor-bringup-report.md` | **PASS** — Orange (GPIO 5) confirmed working with 4.7 kΩ series + 100 nF capacitor. Pull-up provided by console (3.3 V idle, ~0.17 V on reed closure). Grey (GPIO 4) is 0 V return. |
| 8 | Encoder common = 3.3 V, channels pulled down by the console | DMM | Logic and analog capture: counts per detent, pull direction, Ch B pulse shape (~1.5–2 V?); encoder VCC on GPIO 10 | Standalone pull-downs, decoder and Ch B conditioning (H-06) | `B01-20261003-sensor-bringup-report.md`, `B02-20261004-encoder-characterisation.md`, `B04-20261004-pcnt-encoder-comparison.md`; `A08-20260926-chb-adc-spike.csv` (superseded) | **PASS** — Encoder wires confirmed: Black = Ch A, Red = Ch B, Orange = VCC (steady 3.3 V). PCNT peripheral (`EncoderModule`, `stepsPerDetent=4`) achieves 0% bounce, 100% click detection (B04). Note: `A08` is **superseded by B04** (wire-colour correction shows PCNT decodes both channels cleanly). |
| 9 | Console tolerates its motor lines being disconnected (Override with console connected) | None | Unplug the motor at the hub, turn the knob, watch the console and app | UC4 and UC5 viability | `A09-20261011-motor-disconnect.csv`, `A09-20261011-motor-disconnect-events.csv`, `A09-chart-motor-disconnect.png` | **PASS** — Console tolerates motor disconnect without faulting; continues to display target level and respond to knob. Resumes normal seeking immediately when reconnected. |
| 10 | Console's 3.3 V rail tolerates parallel supplies via R4/R5 | None | Console rail voltage at idle, active and sleep | Back-feed risk (H-02) | — | Open |
| 11 | The Waveshare VIN pin cannot take 9 V | Not checked | Board datasheet or schematic | May remove the external buck for the SoC (H-03) | `A11-20261011-vin-tolerance.md` | **PASS** — Datasheet confirms VIN 6–21V (MP2322GQH regulator). 9V safe. Power architecture deferred to DR-1 (owner: H-bridge option under consideration). |
| 12 | Echelon app pairs any `0x180D` peripheral | None | nRF Connect emulation spike | PR-03 premise (B-04) | `A12-20261011-ble-pair-spike.md` | **INCONCLUSIVE** — nRF Connect mock HRM on Android phone advertised `0x180D` with full GATT service (`0x2A37`, `0x2A38`, `0x2A39`). Mac (bleak) connected and enumerated all characteristics successfully. Android tablet and a second Android phone could not discover the mock peripheral (both discovered other BLE devices). Echelon app explicitly prompts for HRM pairing, confirming it scans for `0x180D`. Test could not determine whether the app would pair because the mock was not visible to Android. **Handoff: PR-02 to build ESP32 mock HRM BLE server with NimBLE for definitive test.** |
| 13 | **(r3)** What the console does at power-on: whether it runs a calibration sweep, how long after 9 V is applied, and how it finds its reference (click-off, extremes, or a stored value) | Owner recollection only | **Scope in single-shot mode** on the motor wires and wiper, triggered by applying 9 V. The logger runs from USB-C throughout Phase 0 ([hardware build guide](../../hardware_specification.md) §3.5 and §5.3), so it is already logging (power-on burst trigger) before the bike is switched on. Repeat from several starting positions, including a cold start after the cam was left on each half. | Boot-time relay race (H-05), standalone homing and persisted half (S-01), console desync after Override (S-01) | `A13-20261011-power-on-capture.csv`, `A13-chart-power-on-capture.png` | **PASS** — Console powers on to level 1 (pot ≈ 165); no auto-calibration sweep at boot; motor seek only begins when a class is started in the companion app. |
