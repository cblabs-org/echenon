---
name: signal-logger-wo5
description: Handoff instructions for Work Order 5 — Register campaign (ride + burst + remaining rows)
complexity: interactive
---

# WO5 — Register Campaign: Ride + Burst + Remaining Rows

## Role Definition
> You are an expert senior software engineer assisting the user at the bike
> and bench. This is a human-in-the-loop session for the sustained measurement
> campaign that fills all remaining Phase 0 register rows.

## Objective

Complete all remaining register rows (#3–#10, #12–#13), run the 60-minute
ride log test (SPEC test 0.9), validate burst capture triggers (SPEC test
0.10), and capture the power-on behaviour (SPEC test 0.12). Close the
Phase 0 register (SPEC test 0.13).

## Entry Conditions

- [ ] WO4 scope gate closed — motor-sense taps connected
- [ ] Firmware on `signal-logger/wo5-register-campaign` branch at the tip
- [ ] All taps connected (GPIO 1–10)
- [ ] Dashboard accessible at `echenon.local` or IP

## Checklist

### A. Signal measurements (register rows #3–#8)

- [ ] **#3 — Pot mV over one full cam revolution:**
  Slowly turn the console knob through a full revolution. Download ride log
  CSV. Record wiper (ch0) mV over angle. Note coast distance after drive
  stops. Evidence: `A03-YYYYMMDD-pot-revolution.csv`

- [ ] **#4 — Wiper, Yellow, Green at every console level:**
  Set each console level (both up and down). Record ch0 (wiper), ch1
  (Yellow/HI), ch2 (Green/LO) at each level. Evidence:
  `A04-YYYYMMDD-level-table.csv`

- [ ] **#5 — Pot shape over one revolution; click-offs; working half:**
  Full-revolution capture with ch0. Identify the working half, click-off
  points, dead zone. Evidence: `A05-YYYYMMDD-pot-shape.csv`

- [ ] **#6 — Green and Yellow reference voltages + source impedance:**
  With taps connected, measure ch1 (Yellow) and ch2 (Green) at rest.
  Then apply a known load (e.g., 10 kΩ to GND on Yellow) and measure
  voltage drop to estimate source impedance. Evidence:
  `A06-YYYYMMDD-reference-voltages.md`

- [ ] **#7 — Cadence circuit (Orange, Grey, Brown):**
  Pedal or simulate cadence events. Record ch4 (Orange/cadence) and ch3
  (Grey/return). Note open and closed states, pulse shape, frequency at
  steady cadence. Evidence: `A07-YYYYMMDD-cadence.csv`

- [ ] **#8 — Encoder (counts/detent, pull direction, Ch B, VCC):**
  Turn the encoder knob slowly. Download event log CSV — count edges per
  detent on ch7 (Enc A). Record ch8 (Enc B) analog levels from ride log.
  Record ch9 (Enc VCC) voltage. Supplements the WO0 spike Ch B data.
  Evidence: `A08-YYYYMMDD-encoder-on-bike.csv`

### B. Console tolerance and rail (register rows #9–#10)

- [ ] **#9 — Console tolerates disconnected motor lines:**
  Disconnect both motor wires (Red and Black) while the console is powered.
  Turn the knob. Does the console still operate (knob moves, display shows
  levels)? Does it fault? Evidence: `A09-YYYYMMDD-motor-disconnect.md`

- [ ] **#10 — Console 3.3 V rail with parallel supplies:**
  With the Echenon logger powered via USB-C and connected to the console
  harness, measure the console's 3.3 V rail. Is there interaction? Evidence:
  `A10-YYYYMMDD-console-rail.md`

### C. BLE app spike (register row #12)

- [ ] **#12 — Echelon app pairs a `0x180D` peripheral:**
  Using nRF Connect on the P12 tablet (or phone), create a mock Heart Rate
  peripheral advertising `0x180D`. Open the Echelon app and attempt to pair.
  Does it pair? Does it show HR data? Evidence:
  `A12-YYYYMMDD-ble-pair-spike.md`

### D. 60-minute ride log (SPEC test 0.9)

- [ ] Ride the EX-3 for 60 minutes with the logger running.
- [ ] Download the ride log CSV from the dashboard.
- [ ] Verify: file is non-empty, all 10 channels have data, timestamps span
  ~60 minutes. Evidence: `A09-YYYYMMDD-ride-log-60min.csv` (or keep in the
  download as-is).

### E. Burst capture validation (SPEC test 0.10)

- [ ] **Manual burst:** Trigger from the dashboard, download CSV.
- [ ] **Motor-drive onset burst:** Turn the knob to start the motor while
  burst is armed. Download CSV.
- [ ] **Power-on burst:** Power-cycle the Echenon, let the auto-burst
  complete, download CSV.
- [ ] Verify each CSV has the expected columns and data.

### F. Power-on capture (SPEC test 0.12, register #13)

- [ ] With the scope set to single-shot, power on the console and Echenon
  simultaneously. Capture the first few seconds of all channels via the
  power-on burst. Also capture scope data if available.
  Evidence: `A13-YYYYMMDD-power-on.csv` and/or scope screenshots.

### G. Commit all evidence

- [ ] `git add docs/measurements/phase0/A03-* docs/measurements/phase0/A04-* ... A13-*`
- [ ] `git commit -m "pr-01/wo5: register campaign — rows 3-10, 12-13, ride + burst evidence"`
- [ ] Verify all 13 register rows have evidence files.

## Verification Requirement
- All 13 register rows have evidence files committed.
- 60-min ride log CSV is valid.
- All three burst capture triggers produce valid CSVs.
- SPEC test 0.13 (register complete) passes.

---

## Completion Report (written by the worker at task completion)

<!-- Append when complete:
### Status
complete | partial | blocked

### Register Rows Updated
- #3 through #13: (result summary for each)

### SPEC Tests Verified
- 0.9: 60-min ride log: PASS/FAIL
- 0.10: Burst captures (manual, motor-onset, power-on): PASS/FAIL
- 0.12: Power-on capture: PASS/FAIL
- 0.13: Register complete: PASS/FAIL
-->
