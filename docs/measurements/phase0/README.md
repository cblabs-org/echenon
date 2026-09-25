# Phase 0 Deliverables

This directory contains the Phase 0 measurement evidence (oscilloscope captures, CSV logs, teardown photos). 

This register serves as the exit checklist for PR-01.

## Phase 0 hardware assumptions register

| # | SPEC assumption | Current evidence | Phase 0 measurement | Design impact if wrong |
|---|---|---|---|---|
| 1 | Console motor drive is 3.3 V DC bang-bang | DMM average only | Scope both wires, both directions, and at rest (brake or coast?) | Sense network (H-01), relay logic, motor rail |
| 2 | Motor current ≤ 800 mA, intermittent | None | Shunt or clamp: start inrush, and running current with the worm under brake load at cam-max | Driver choice, rail, brownout (H-03) |
| 3 | Motor stops within one level from 20 Hz feedback | None | Log pot mV against time for **one full cam revolution** (this also gives the run-time bound) and coast distance after the drive stops | Loop rate, dead zone, braking profile (M-02, M-01) |
| 4 | Wiper 1160–1700 mV, linear over levels 1–32 | DMM across the *outer* terminals ("strange"); r2: the cam makes it sinusoidal | Wiper, Yellow and Green at every console level, both directions; resistance proxy (console level, or power at fixed cadence) against angle | Level table, calibration method, 100-level claim (S-01) |
| 5 | The pot's shape over one revolution (number of sweeps, click-off points) relative to cam-min and cam-max; which half the console works in | Teardown notes say two 180° sweeps (owner unsure, r3); r2: continuous cam, no end stops | Drive a full revolution; mark the click-offs and the resistance extremes | Angle tracking, mirror-position ambiguity (S-01) |
| 6 | Green = GND and Yellow = 3.3 V from a stiff source | None; #4 suggests a bias | Measure both, plus source impedance (load test) | R4 and the Green–GND tie (H-02) |
| 7 | Cadence: Orange pulled up by the console, reed closes to GND | Colour mapping contradicts itself (ex3 §7 vs §8) | Orange, Grey and Brown voltages open and closed | R3 path; whether Grey needs grounding |
| 8 | Encoder common = 3.3 V, channels pulled down by the console | DMM | Logic capture: counts per detent, pull direction | Standalone pull-downs, decoder (H-06) |
| 9 | Console tolerates its motor lines being disconnected (Override with console connected) | None | Unplug the motor at the hub, turn the knob, watch the console and app | UC4 and UC5 viability |
| 10 | Console's 3.3 V rail tolerates parallel supplies via R4/R5 | None | Console rail voltage at idle, active and sleep | Back-feed risk (H-02) |
| 11 | The Waveshare VIN pin cannot take 9 V | Not checked | Board datasheet or schematic | May remove the external buck for the SoC (H-03) |
| 12 | Echelon app pairs any `0x180D` peripheral | None | nRF Connect emulation spike | PR-03 premise (B-04) |
| 13 | **(r3)** What the console does at power-on: whether it runs a calibration sweep, how long after 9 V is applied, and how it finds its reference (click-off, extremes, or a stored value) | Owner recollection only | **Scope in single-shot mode** on the motor wires and wiper, triggered by applying 9 V. Power the ESP32 logger **from USB** so it is already logging before the bike is switched on (buck disconnected, as §3.5 requires). Repeat from several starting positions, including a cold start after the cam was left on each half. | Boot-time relay race (H-05), standalone homing and persisted half (S-01), console desync after Override (S-01) |
