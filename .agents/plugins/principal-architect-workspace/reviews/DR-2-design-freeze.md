# DR-2: v2 Active Design Freeze — Decision Record

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) "Releases".

| | |
|---|---|
| **Status** | **NOT STARTED.** Skeleton created 2026-10-12 when the programme was restructured into v1 Passive / v2 Active (owner decision). Items moved here from DR-1 are carried as *draft input*, not decisions |
| **Owner** | Chief Architect (Tier 1), decided jointly with the owner |
| **Entry condition** | v1 released (PR-09); [DR-2 prep](../pr-dr2-prep/PROJECT.md) evidence merged, with every item below backed by committed evidence |
| **Gates** | PR-07 (and through it PR-10, PR-08, PR-11, PR-12, PR-13). No active-board soldering or motor/encoder interception before approval |
| **Programme** | [PROGRAM.md](../PROGRAM.md) · [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) |
| **Predecessor** | [DR-1: v1 Passive Design Freeze](DR-1-design-freeze.md) (the v1 tap network, power path and pin map are inherited, not re-opened) |
| **Design basis** | [`docs/mitm-encoder-design-notes.md`](../../../../docs/mitm-encoder-design-notes.md) §10 (architecture spectrum), §11 (console as signal source), §12 (console fault tolerance) |

---

## 1. Inputs: evidence

DR-2 relies on the Phase 0 register (rows #1–#13, as used by DR-1) **plus** the
active-design bench evidence gathered by [`pr-dr2-prep`](../pr-dr2-prep/PROJECT.md).
Evidence lives under `docs/measurements/` with the register naming convention.

| # | Evidence needed | Source | Feeds decision |
|---|---|---|---|
| P1 | Encoder-emulation timing toward the console: minimum quadrature pulse width and edge spacing the console accepts | DR-2 prep (split encoder loom, signal source) | §2.10, §2.12 |
| P2 | Console behaviour when emulated clicks arrive during a motor seek | DR-2 prep (extends A09-20261012 #9) | §2.10, §2.12 |
| P3 | Wiper behaviour in the dead zone and during the stock runaway (A09-20261012 #3) | DR-2 prep | §2.4, §2.5 |
| P4 | H-bridge drive of the motor with the console disconnected: current, stall, coast, direction mapping | DR-2 prep | §2.3, §2.5 |
| P5 | Pot-reference replica: a ≈ 4.7 kΩ / 4.7 kΩ divider from 3.3 V reproducing the console's 1.66 V / 2.4 kΩ source on Green | DR-2 prep | §2.2, §2.9 |
| — | Register rows #1, #2, #3, #5, #6, #9, #10, #13 | Phase 0 register (PR-01, DR-1 prep) | as below |

Row #10 (console rail with parallel supplies) was **waived at DR-1** for v1. It
re-opens here if any v2 standalone network can connect to a console rail.

---

## 2. Decisions to take

Values not yet decided are written `[DR-2: …]` in the specs until this record closes.
Section numbers are kept from DR-1 so cross-references stay stable.

| § | Decision | Audit ref | Evidence | Decision | Status |
|---|---|---|---|---|---|
| 2.2 | **Switchable standalone supplies** (mode 2, console off): pot-reference replica on Green, cadence pull-up, encoder supply; connected only when the console is absent | H-02 | #6, #10, P5 | `[DR-2]` | OPEN |
| 2.3 | **Motor driver and rail** (v2 part of DR-1 §2.3; decision D3): driver part, current limit, fault output, bulk capacitance, whether the 3.3 V rail can carry it | H-03, D3 | #1, #2, #11, P4 | `[DR-2]` | OPEN |
| 2.4 | **Fail-safe strategy**: normally-closed bypass on every intercepted path; boot-race handling; runaway protection | H-05 | #9, #13, P3 | `[DR-2]` | OPEN |
| 2.5 | **Cam-aware controller**: angle tracking across the dead zone, working ramp and approach direction, run-time bound, loop rate, brake-and-approach | S-01, M-02, M-01, H-06 | #3, #4, #5, #8, #13, P3, P4 | `[DR-2]` | OPEN |
| 2.7 | **v2 pin map** (v2 part of DR-1 §2.7): active outputs added to the frozen v1 tap map; UART0 (GPIO 43/44) stays reserved | H-04 | #7, #8 | `[DR-2]` | OPEN |
| 2.9 | **R3 / R4 / R5 replacement** for mode 2: the switched standalone network | H-02, E-01 | #6, #7, #10, P5 | `[DR-2]` | OPEN |
| 2.10 | **Interception architecture**: mode 1 (relay/active: sensors and controls in as signals, H-bridge + PWM motor drive, encoder emulation toward the console) and mode 2 (console off) | H-05 (c), P-B | #8, #9, #13, P1, P2 | Owner direction 2026-10-12: console as signal source (design notes §11). Detail `[DR-2]` | DIRECTION SET |
| 2.11 | **Resistance scale mapping (v2 part)**: FTMS `0x2AD6` step size and target mappings once the ESP32 owns the motor | P-A, P-C | #4 | `[DR-2]` | OPEN |
| 2.12 | **Encoder emulation** toward the console: pulse timing, rate limit, behaviour during a seek | — | P1, P2 | `[DR-2]` | OPEN |

---

## 2A. Draft input carried from DR-1 §2A (2026-10-12)

**Nothing in this section is decided.** It is the Chief Architect's 2026-10-12
draft, moved here when the owner put all interception into v2.

| § | Draft content | Key evidence |
|---|---|---|
| 2.2 / 2.9 | The console's pot reference is a **1.66 V / 2.4 kΩ Thevenin source on Green**. Replica: a 3.3 V divider of about **4.7 kΩ / 4.7 kΩ** (≈ 1.65 V, ≈ 2.35 kΩ), connected only when the console is absent. **Yellow is ground; Green must never be tied to GND.** Plus a switched cadence pull-up and encoder supply. No R3/R4/R5 as originally specified | B01, A06 load test, A04-20261012 |
| 2.3 | MOSFET **H-bridge with current limit and fault output (DRV8833 class)** at 3.3 V, PWM-driven. Motor draws ≤ 175 mA, so a separate rail may not be needed; bulk capacitance at the driver. *Open:* confirm the Waveshare 3V3 rail rating (MP2322, ~1 A) covers ESP32 peak plus the motor | A02, A11 |
| 2.4 | **Normally-closed bypass on every intercepted path** (encoder A/B and motor lines). The ESP32 actively holds each bypass open. Boot race is moot (no power-on sweep, row #13). **Runaway protection:** the stock console can be driven into continuous rotation by a fast knob spin past level 32 (A09-20261012 #3). Options: rate-limit the emulated encoder so the console never sees the triggering burst; an NC motor-cut relay the ESP32 opens when the wiper leaves the working range for more than N seconds | A09-20261012 #3, row #13 |
| 2.5 | Track which working ramp the cam is on by detecting the dead zone (wiper leaves the 0–2120 ramp / floats high); never leave the console's ramp (165–1327 counts); run-time bound ≈ 1 revolution (~35 s at ~16 s per half); brake-and-approach (coast < 15 counts). Loop rate and gains set in PR-07 | A03, A09 |
| 2.7 | **Proposed v2 outputs:** encoder out A/B **GPIO 17/18**; encoder bypass control **GPIO 21**; driver IN1/IN2 **GPIO 10/11**; motor bypass control **GPIO 47**; optional motor-cut relay **GPIO 38**. Replaces the provisional "L9110S on 10/17". GPIO 10 is free because the encoder-VCC tap is dropped on production boards (DR-1). **GPIO 43/44 (UART0) remain reserved** | §4.4.1, board.h |
| 2.10 | Encoder relay: PCNT ingest → rate-limited quadrature resynthesis (4 detents per level up; 6–10 observed down), wiper-verified level tracking. The console tolerates every fault tested (#1, #2, #3, #7, #9) and updates its target mid-seek. Motor ownership per design notes §11 | A08, A09-20261012, B03 |
| 2.11 | Internal `i ∈ 0…255` (owner decision, unchanged). With the ESP32 owning the motor, `0x2AD6` may advertise a finer step than v1's 1.0 | P-A, P-C |

---

## 3. Outputs: documents revised at DR-2

| Output | Document and section | Driven by |
|---|---|---|
| v2 pin map; `[DR-2]` markers resolved | [SPEC](../../../../SPEC.md) §4.4.2 (v2 rows), §4.6–§4.8 | 2.3, 2.7, 2.9 |
| Encoder emulation, motor control, operating modes | SPEC §5.4–§5.6 | 2.4, 2.5, 2.10, 2.12 |
| Motor-driven calibration | SPEC §6.3 | 2.5 |
| FTMS Control Point targets and `0x2AD6` step | SPEC §7.3–§7.4 | 2.11 |
| Active board design; PROVISIONAL banner removed | [Hardware spec](../../../../docs/hardware_specification.md) §7–§8 | 2.2–2.5, 2.7, 2.9 |
| v2 PR decomposition (PR-07, PR-10, PR-08, PR-11, PR-12, PR-13) | PR PROJECT.md files | all |

---

## 4. Chief Architect approval

| | |
|---|---|
| **Decision** | ☐ Approved · ☐ Approved with conditions · ☐ Rejected (re-run DR-2 prep items) |
| **Conditions / open items** | |
| **Approved by** | |
| **Date** | |
| **Evidence baseline** | Echenon commit `[hash]` |
| **Owner acknowledgement** | *(the owner's own words, quoted from chat)* |

Approval unblocks PR-07. Record it in the [PROGRAM.md](../PROGRAM.md) session log.
