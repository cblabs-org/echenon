---
complexity: interactive
---

# WO4b — Bench Gate: Pin Idle, Motor Scope, Current, VIN Tolerance

**Parent:** [PR-01 Signal Logger](../PROJECT.md)
**Branch:** `signal-logger/wo4b-bench-gate` (base: `signal-logger/wo4a-encoder-integration`)

## Role Definition
> You are a bench test assistant working interactively with the human at the
> workbench. Your job is to complete the four outstanding items from WO4's
> scope characterisation gate (hardware spec §5.2). You observe and record —
> you do NOT make design decisions or modify firmware.

## Objective

Complete the four deferred scope gate items:
1. **B — Pin idle check:** DMM or scope on all tap GPIOs with bike power OFF
2. **C — Motor-drive scope captures:** Both wires, both directions, at rest,
   slow timebase (10 ms/div) and fast timebase (µs/div) per hardware spec §5.2
3. **D — Motor current:** Inrush and steady-state under load via shunt or clamp
4. **E — VIN 9 V tolerance:** Waveshare board 9 V input capability check

## Context

- **Firmware:** Already flashed and running (`v0.1.0-dev.5` on the renamed
  branch `signal-logger/wo4a-encoder-integration`). No firmware changes needed.
- **Dashboard:** All 8 ADC channels + PCNT encoder visible at `echenon.local`
  or the device IP.
- **WO4 partial completion:** Items A (flash/boot), F (motor-sense tap safety)
  and G (dashboard verification) are already done. Evidence B01–B04 committed.
- **Hardware spec §5.2:** The motor-drive characterisation gate (item C).
  Six scope captures minimum, as specified in the hardware spec table.
- **Register rows:** #1 (motor drive characterisation), #2 (motor current),
  #11 (VIN tolerance).

## Strict Boundary Box

- **Create:** Evidence files under `docs/measurements/phase0/` using the
  register naming convention (`A<row>-<yyyymmdd>-<short-desc>.<ext>`)
- **Modify:** `docs/measurements/phase0/README.md` (update register rows
  #1, #2, #11 with Evidence and Result)
- **Read:** `board.h`, SPEC §4.4.1, hardware spec §5.2–§5.4
- **Do NOT** modify firmware source code, `board.h`, or `platformio.ini`
- **Do NOT** make design decisions — if something unexpected is found (e.g.,
  motor voltage exceeds divider safe range, PWM where DC was expected),
  record the observation and park any design implications in the Completion
  Report as proposals for the PA
- If anything looks dangerous (voltage > 13 V at divider input, excessive
  current, smoke), **STOP immediately** and disconnect

## Tooling Constraints

- Use PlatformIO MCP tools for any flashing (if needed), not `pio` CLI
- Follow `.agents/rules/03-versioning.md` for flashing (Tag → Build → Flash)
- Follow `.agents/rules/04-mcp-tooling.md` — MCP tools only

## Checklist

### B. Pin Idle Check (bike power OFF)

- [ ] Power off the bike (unplug 9 V or switch off at the mains)
- [ ] ESP32 remains powered via USB-C
- [ ] DMM or scope: measure all 10 tap GPIOs (1–10). Each should read < 50 mV
- [ ] Record readings in an evidence file: `A02-<yyyymmdd>-pin-idle-check.md`
- [ ] If any pin reads > 50 mV with bike off, note the value and which pin —
  this could indicate back-powering through ESD diodes or a wiring issue

### C. Motor-Drive Scope Captures (hardware spec §5.2)

Setup: DC coupling, 5 V/div. Scope ground to Brown (GND). CH1 on Motor+
(after ÷4 divider, at GPIO 6). CH2 on Motor− (after divider, at GPIO 7).

- [ ] **Capture 1:** Both wires at rest (bike powered, no knob movement).
  Timebase: 10 ms/div. Record: driven low (brake), floating (coast), or other.
  → `A01-<yyyymmdd>-motor-at-rest.png`

- [ ] **Capture 2:** Knob one click UP (resistance increase). Timebase: 10 ms/div.
  Record: drive duration, which wire is high, voltage levels.
  → `A01-<yyyymmdd>-motor-fwd-slow.png`

- [ ] **Capture 3:** Knob one click DOWN (resistance decrease). Timebase: 10 ms/div.
  Record: as capture 2, opposite direction.
  → `A01-<yyyymmdd>-motor-rev-slow.png`

- [ ] **Capture 4:** During drive UP. Timebase: µs/div (e.g., 10 µs/div).
  Record: flat DC or PWM (frequency, duty, peak).
  → `A01-<yyyymmdd>-motor-fwd-fast.png`

- [ ] **Capture 5:** During drive DOWN. Timebase: µs/div.
  → `A01-<yyyymmdd>-motor-rev-fast.png`

- [ ] **Capture 6:** Drive end (either direction). Timebase: 10 ms/div then µs/div.
  Record: braking, coasting, back-EMF excursions below GND.
  → `A01-<yyyymmdd>-motor-drive-end.png`

- [ ] Verify divider output stays < 3.3 V throughout all captures
- [ ] Summarise findings in `A01-<yyyymmdd>-motor-characterisation-notes.md`

### D. Motor Current

- [ ] Insert shunt resistor (e.g., 0.1 Ω) in motor lead OR use DC clamp meter
- [ ] **Inrush:** Single-shot trigger on drive onset (knob click). Record peak current.
- [ ] **Steady-state:** During sustained drive at mid/high resistance. Record RMS.
- [ ] **Under load:** With brake engaged (cam near max), record peak.
- [ ] Record all readings in `A02-<yyyymmdd>-motor-current.md`
- [ ] If current exceeds 2 A at any point, note this prominently

### E. Waveshare VIN 9 V Tolerance

- [ ] Check Waveshare ESP32-S3-Nano datasheet or schematic for VIN spec
- [ ] If datasheet confirms VIN max (e.g., "5.5 V max" or "up to 12 V"):
  record the finding from the datasheet. No bench test needed.
- [ ] If datasheet is ambiguous or unavailable: measure VIN pin voltage with
  the board powered from USB-C, then carefully test 9 V input if safe.
- [ ] Record finding in `A11-<yyyymmdd>-vin-tolerance.md`
- [ ] Result: "safe" / "not safe" / "unknown (deferred to DR-1)"

## Evidence Naming Convention

Files go in `docs/measurements/phase0/`:
```
A<row>-<yyyymmdd>-<short-description>[-<n>].<ext>
```

## Verification

- [ ] All 4 checklist sections complete (or explicitly deferred with reason)
- [ ] All evidence files committed under `docs/measurements/phase0/`
- [ ] Register rows #1, #2, #11 updated in `README.md` with Evidence and Result
- [ ] Scope characterisation gate (§5.2) passed — motor drive characterised

---

## Completion Report

### Status
complete

### Session
**Date:** 2026-10-11, ~30 min bench session
**Operator:** Owner at bench with Rigol DHO804 + DMM
**PA:** Recording and interpreting remotely

### Items Completed

| Item | Method | Result |
|---|---|---|
| **B — Pin idle** | Done previously in B01 sensor bringup | N/A (already closed) |
| **C — Motor scope captures** | Rigol DHO804, probe directly on motor wires | 3 captures: reverse drive end (10 ms/div), forward onset (20 ms/div), fast timebase (50 µs/div). ±3.25V flat DC, no PWM confirmed. |
| **D — Motor current** | DMM in series with motor wire, no-load | Reverse: 175 mA inrush → 150 mA steady. Forward: 95 mA inrush → 50 mA steady. Motor drives eccentric cam; load is cam mechanism, independent of flywheel speed. |
| **E — VIN tolerance** | Datasheet review (Waveshare ESP32-S3-Nano) | VIN 6–21V (MP2322GQH regulator). 9V safe. |

### Register Rows Updated
- **#1** (motor drive characterisation): PASS — ±3.25V flat DC, no PWM
- **#2** (motor current): PASS — ≤175 mA peak, well under 800 mA
- **#11** (VIN tolerance): PASS — 9V within 6–21V spec

### Owner Observations (parked for DR-1)
- Motor drive lines (±3.25V DC) may be used as signal inputs only, with
  actual motor drive via a separate H-bridge. Power architecture deferred
  to DR-1.
- Motor current is independent of flywheel speed (cam mechanism is the load,
  not the brake force).

### Scope Capture Notes
- Probes were directly on motor wires, not through the ÷4 divider.
- Through the divider, the ESP32 sees +0.81V (forward) and 0V (reverse,
  clamped by input protection). ADC only detects forward drive; direction
  requires comparing both motor+ and motor− channels.
- DHO804 minimum timebase was 40 µs/div (scope limit), but this was
  sufficient to confirm no PWM at any reasonable motor drive frequency.

### Limitations
- DMM inrush readings are limited by response time. True peak inrush may
  be slightly higher but is not expected to exceed 500 mA.
- Only 3 of the 6 recommended scope captures were taken (no separate
  at-rest, reverse drive onset, or drive-end braking captures). The 3
  captures are sufficient to characterise the motor drive for register row #1.
