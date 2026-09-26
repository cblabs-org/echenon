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
```

- `<row>` is the register row number, zero-padded to two digits (`A01` … `A13`).
- `<short-description>` is lowercase, hyphen-separated, and names the
  condition (for example `red-black-knob-up-10ms`).
- `-<n>` numbers repeats of the same condition.
- `<ext>` is the native format: `.csv` for logger exports, `.png` / `.csv`
  for scope captures, `.jpg` for photos, `.md` for notes.

Examples: `A01-20261002-red-black-knob-up-10us.png`,
`A04-20261003-wiper-levels-1-32-up.csv`, `A13-20261004-poweron-single-shot-2.png`.

A row that needs several files lists them all in the Evidence file column;
a short `A<row>-…-notes.md` may summarise them.

## Register

| # | SPEC assumption | Current evidence | Phase 0 measurement | Design impact if wrong | Evidence file | Result / status |
|---|---|---|---|---|---|---|
| 1 | Console motor drive is 3.3 V DC bang-bang | DMM average only | Scope both wires, both directions, and at rest (brake or coast?), at 10 ms/div and µs/div (build guide §5.2) | Sense network (H-01), relay logic, motor rail | — | Open |
| 2 | Motor current ≤ 800 mA, intermittent | None | Shunt or clamp: start inrush, and running current with the worm under brake load at cam-max | Driver choice, rail, brownout (H-03) | — | Open |
| 3 | Motor stops within one level from 20 Hz feedback | None | Log pot mV against time for **one full cam revolution** (this also gives the run-time bound) and coast distance after the drive stops | Loop rate, dead zone, braking profile (M-02, M-01) | — | Open |
| 4 | Wiper 1160–1700 mV, linear over levels 1–32 | DMM across the *outer* terminals ("strange"); r2: the cam makes it sinusoidal | Wiper, Yellow and Green at every console level, both directions; resistance proxy (console level, or power at fixed cadence) against angle | Level table, calibration method, 100-level claim (S-01) | — | Open |
| 5 | The pot's shape over one revolution (number of sweeps, click-off points) relative to cam-min and cam-max; which half the console works in | Teardown notes say two 180° sweeps (owner unsure, r3); r2: continuous cam, no end stops | Drive a full revolution; mark the click-offs and the resistance extremes | Angle tracking, mirror-position ambiguity (S-01) | — | Open |
| 6 | Green = GND and Yellow = 3.3 V from a stiff source | None; #4 suggests a bias | Measure both (GPIO 3 and GPIO 2 taps), plus source impedance (load test) | R4 and the Green–GND tie (H-02) | — | Open |
| 7 | Cadence: Orange pulled up by the console, reed closes to GND | Colour mapping contradicts itself (ex3 §7 vs §8) | Orange, Grey and Brown voltages open and closed (GPIO 5 and GPIO 4 taps, plus DMM) | R3 path; whether Grey needs grounding | — | Open |
| 8 | Encoder common = 3.3 V, channels pulled down by the console | DMM | Logic and analog capture: counts per detent, pull direction, Ch B pulse shape (~1.5–2 V?); encoder VCC on GPIO 10 | Standalone pull-downs, decoder and Ch B conditioning (H-06) | — | Open |
| 9 | Console tolerates its motor lines being disconnected (Override with console connected) | None | Unplug the motor at the hub, turn the knob, watch the console and app | UC4 and UC5 viability | — | Open |
| 10 | Console's 3.3 V rail tolerates parallel supplies via R4/R5 | None | Console rail voltage at idle, active and sleep | Back-feed risk (H-02) | — | Open |
| 11 | The Waveshare VIN pin cannot take 9 V | Not checked | Board datasheet or schematic | May remove the external buck for the SoC (H-03) | — | Open |
| 12 | Echelon app pairs any `0x180D` peripheral | None | nRF Connect emulation spike | PR-03 premise (B-04) | — | Open |
| 13 | **(r3)** What the console does at power-on: whether it runs a calibration sweep, how long after 9 V is applied, and how it finds its reference (click-off, extremes, or a stored value) | Owner recollection only | **Scope in single-shot mode** on the motor wires and wiper, triggered by applying 9 V. The logger runs from USB-C throughout Phase 0 ([hardware build guide](../../hardware_specification.md) §3.5 and §5.3), so it is already logging (power-on burst trigger) before the bike is switched on. Repeat from several starting positions, including a cold start after the cam was left on each half. | Boot-time relay race (H-05), standalone homing and persisted half (S-01), console desync after Override (S-01) | — | Open |
