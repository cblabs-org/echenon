# DR-1: Design Freeze — Decision Record

| | |
|---|---|
| **Status** | **NOT STARTED** |
| **Owner** | Chief Architect (Tier 1) |
| **Entry condition** | PR-01 merged; every row of the [Phase 0 register](../../../../docs/measurements/phase0/README.md) has an Evidence file and a Result |
| **Gates** | PR-02, PR-04a, PR-07. No soldering or permanent installation before approval |
| **Programme** | [PROGRAM.md](../PROGRAM.md) · [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) |
| **Charter** | [2026-09-23 design audit](2026-09-23-chief-architect-audit.md) §8.3 · [2026-09-25 readiness review](2026-09-25-readiness-review.md) RR-08 |

---

## 1. Inputs: Phase 0 register → evidence

Every row of the Phase 0 register must resolve to committed evidence under
`docs/measurements/phase0/`. The Evidence and Result columns are copied from the
register at DR-1 start; the register remains the source of truth. Evidence
files follow the register's naming convention,
`A<row>-<yyyymmdd>-<short-description>[-<n>].<ext>`; the Result column
shows each row's status until PR-01 fills it in. Rows 1–13 match the
register one-to-one; the tests that produce them are SPEC §13.2.

| # | Assumption under test | Evidence file | Result | Feeds decision |
|---|---|---|---|---|
| 1 | Console motor drive waveform (both wires, both directions, at rest; 10 ms/div and µs/div) | `A01-<yyyymmdd>-…` | Open | §2.1 sense network, §2.3 driver and rail |
| 2 | Motor current: inrush and running under brake load at cam-max | `A02-<yyyymmdd>-…` | Open | §2.3 driver and rail, §2.6 power model |
| 3 | Pot mV over one full cam revolution; coast distance after drive stops | `A03-<yyyymmdd>-…` | Open | §2.5 controller (run-time bound, loop rate, dead zone) |
| 4 | Wiper, Yellow, Green at every console level, both directions; resistance proxy against angle | `A04-<yyyymmdd>-…` | Open | §2.5 controller (level table), PR-02 passive calibration |
| 5 | Pot shape over one revolution; click-offs; working half | `A05-<yyyymmdd>-…` | Open | §2.5 controller (angle tracking) |
| 6 | Green = GND and Yellow = 3.3 V; source impedance | `A06-<yyyymmdd>-…` | Open | §2.2 switchable supplies, §2.9 R3/R4/R5 |
| 7 | Cadence: Orange / Grey / Brown open and closed | `A07-<yyyymmdd>-…` | Open | §2.7 pin map, §2.8 passive tap board, §2.9 R3 |
| 8 | Encoder common and channel pulls; counts per detent; Ch B analog shape (WO0) | `A08-<yyyymmdd>-…` | Open | §2.5 controller (encoder conditioning), §2.7 pin map |
| 9 | Console tolerates disconnected motor lines | `A09-<yyyymmdd>-…` | Open | §2.4 fail-safe, Override viability |
| 10 | Console 3.3 V rail with parallel supplies | `A10-<yyyymmdd>-…` | Open | §2.2 switchable supplies, §2.9 R4/R5 |
| 11 | Waveshare VIN tolerance of 9 V | `A11-<yyyymmdd>-…` | Open | §2.3 rail, §2.8 passive tap board power |
| 12 | Echelon app pairs a third-party `0x180D` peripheral | `A12-<yyyymmdd>-…` | Open | PR-03 entry gate (not a DR-1 decision) |
| 13 | Console power-on behaviour (single-shot scope, logger on USB) | `A13-<yyyymmdd>-…` | Open | §2.4 fail-safe (boot race), §2.5 controller (homing) |

Additional PR-01 inputs: the WO0 spike report (ADC-continuous throughput with WiFi
on; ADC + GPIO interrupt on one pad; encoder Ch B capture) and the scope
characterisation captures.

---

## 2. Decisions to take

Each decision records: the options considered, the evidence rows relied on, the
decision, and the SPEC / hardware-spec text it produces. Values not yet decided
are written `[DR-1: …]` in the specs until this record closes.

| § | Decision | Audit ref | Evidence rows | Decision | Status |
|---|---|---|---|---|---|
| 2.1 | **Motor sense network**: divider values, analog vs logic sensing, sample rate | H-01 | #1 | `[DR-1]` | OPEN |
| 2.2 | **Switchable standalone supplies**: whether and how Yellow / Green / encoder supplies are switched rather than hard-wired | H-02 | #6, #10 | `[DR-1]` | OPEN |
| 2.3 | **Motor driver and rail** (decision D3): driver part, current limit, fault output, separate rail, bulk capacitance, trimpot replacement | H-03, D3 | #1, #2, #11 | `[DR-1]` | OPEN |
| 2.4 | **Fail-safe strategy**: normally-closed bypass, passive-only, or encoder injection; boot-race handling | H-05 | #9, #13 | `[DR-1]` | OPEN |
| 2.5 | **Cam-aware controller**: full-revolution angle tracking, working half and approach direction, angle-based level table, calibration by extremes, one-revolution run-time bound, loop rate and dead zone, reset-safe driver inputs; decoder = `EncoderModule` (PCNT) with Ch B conditioning | S-01, M-02, M-01, H-06 | #3, #4, #5, #8, #13 | `[DR-1]` | OPEN |
| 2.6 | **Power model**: re-indexed on cam angle within the working half-revolution; tolerance target | S-02 | #2, #4 | `[DR-1]` | OPEN |
| 2.7 | **Phase 3 pin map**: confirm or revise the provisional map (sense GPIO 6/7; L9110S or successor IA/IB GPIO 10/17; wiper 1; cadence 5; encoder A/B 8/9; no encoder-VCC sense). GPIO 6/7 are never both driver outputs and sense inputs | H-04 | #7, #8 | `[DR-1]` | OPEN |
| 2.8 | **Passive tap board design**: J1–J4 pass-through, tap network, buck and rail, housing fit | H-05 (option b), D2 | #6, #7, #10, #11 | `[DR-1]` | OPEN |
| 2.9 | **R3 / R4 / R5**: keep, remove or make switchable, per board | H-02 | #6, #7, #10 | `[DR-1]` | OPEN |

---

## 3. Outputs: documents revised at DR-1

| Output | Document and section | Driven by |
|---|---|---|
| Hardware configuration and Phase 3 pin map; `[DR-1]` markers resolved | [SPEC](../../../../SPEC.md) §4.3, §4.4.2 (Phase 3+ map, PROVISIONAL marking removed), §4.6–§4.8 | 2.1, 2.3, 2.7, 2.9 |
| Sensor acquisition, level mapping, encoder, motor control, operating modes | SPEC §5.1–§5.6 | 2.1, 2.4, 2.5 |
| Power engine, calibration method, NVS schema (angle-indexed) | SPEC §6.1–§6.4 | 2.5, 2.6 |
| Build flags (`HAS_*` for the passive and intercept configurations) | SPEC §12.3 | 2.7 |
| DR-1 checks D.1–D.7 | SPEC §13.3 | all |
| Power architecture and rails | [Hardware spec](../../../../docs/hardware_specification.md) §3 | 2.3 |
| Intercept board design; PROVISIONAL banner removed | Hardware spec §7–§8 | 2.1–2.5, 2.7, 2.9 |
| Passive tap board design finalised (placeholder replaced) | Hardware spec §6.2 (§6.2.2 lists the decisions) | 2.8 |
| PR mandates adjusted where a decision changes scope | `pr-02`, `pr-04a`, `pr-07`, `pr-08` PROJECT.md | all |

---

## 4. Chief Architect approval

| | |
|---|---|
| **Decision** | ☐ Approved · ☐ Approved with conditions · ☐ Rejected (re-run Phase 0 items) |
| **Conditions / open items** | |
| **Approved by** | |
| **Date** | |
| **Evidence baseline** | Echenon commit `[hash]`, Phase 0 register revision `[hash]` |
| **Owner acknowledgement** | *(the owner's own words, quoted from chat)* |

Approval unblocks PR-02, PR-04a and PR-07. Record it in the
[PROGRAM.md](../PROGRAM.md) session log.
