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

### 1.1 Preliminary bench findings (2026-10-04, not yet register results)

From the PR-01 bench reports B01–B04. Analysis is in the
[2026-10-04 PR-01 progress review](2026-10-04-pr01-progress-review.md). The
register stays the source of truth. These findings are carried here so the
decisions in §2 can be prepared early.

| Row | Finding | Source | Effect on decisions |
|---|---|---|---|
| #4 | Pot linear in shaft angle over levels 1–32 (R² = 0.9989), `wiper = 39.1 × level + 97` counts, about 151° of travel. Hysteresis < 15 counts. The console overshoots on 98% of mid/high moves | B03 | §2.5, §2.6 simplified (P-C); 22 kΩ tap offset unverified (E-02) |
| #6 | **Yellow = 0 V, Green ≈ 1.67 V.** The pot is referenced to about 1.67 V on Green, the opposite of the SPEC naming | B01 | §2.2, §2.9: the standalone network must be redesigned (E-01) |
| #7 | Cadence readable only through 4.7 kΩ + 100 nF; idle high, low on magnet | B01 | §2.7, §2.8; edge distortion to be confirmed (E-02) |
| #8 | Wire colours Black = A, Red = B, Orange = VCC. PCNT gives 0% bounce, 100% detection, `stepsPerDetent = 4`. **The stock console misses 28% of clicks** | B02, B04 | §2.5 (no Ch B conditioning needed: A08 superseded, E-03); §2.10 |

### 1.2 Owner decisions already taken (recorded, not re-opened)

| Decision | Date | Record |
|---|---|---|
| Knob decoded by framework `EncoderModule` (PCNT) on GPIO 8/9 | 2026-10-04 | PR-01 WO4a |
| Cadence tap = 4.7 kΩ in series + 100 nF | 2026-10-03 | B01 |
| 256 internal resistance levels (`uint8_t`) as the canonical scale; external scales (Echelon 1–32, 1–100) are mappings | 2026-10-04 | `docs/mitm-encoder-design-notes.md` §2 |

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
| 2.5 | **Cam-aware controller** (only needed where the ESP32 drives the motor; see 2.10): full-revolution angle tracking, working half and approach direction, angle-based level table, calibration by extremes, one-revolution run-time bound, loop rate and dead zone, reset-safe driver inputs. In console-driven modes, level comes from the linear wiper fit (B03). Decoder = `EncoderModule` (PCNT), already decided; no Ch B conditioning (A08 superseded) | S-01, M-02, M-01, H-06 | #3, #4, #5, #8, #13 | `[DR-1]` | OPEN |
| 2.6 | **Power model**: re-indexed on cam angle within the working half-revolution; tolerance target | S-02 | #2, #4 | `[DR-1]` | OPEN |
| 2.7 | **Phase 3 pin map**: confirm or revise the provisional map (sense GPIO 6/7; L9110S or successor IA/IB GPIO 10/17; wiper 1; cadence 5; encoder A/B 8/9; no encoder-VCC sense). GPIO 6/7 are never both driver outputs and sense inputs | H-04 | #7, #8 | `[DR-1]` | OPEN |
| 2.8 | **Passive tap board design**: J1–J4 pass-through, tap network, buck and rail, housing fit | H-05 (option b), D2 | #6, #7, #10, #11 | `[DR-1]` | OPEN |
| 2.9 | **R3 / R4 / R5**: keep, remove or make switchable, per board. Must reflect the measured pot polarity (Green ≈ 1.67 V reference, Yellow = 0 V) | H-02, E-01 | #6, #7, #10 | `[DR-1]` | OPEN |
| 2.10 | **Interception architecture**: choose from Monitor / **Encoder MITM** (PCNT ingest, clean quadrature resynthesised to the console, NC fail-safe bypass) / Full MITM (pot simulation) / Console bypass (ESP32 H-bridge with PID; console drive used as a hint). Includes whether ERG is first delivered by encoder injection (32 levels, console drives the motor) and the resulting programme restructure (PR-04a scope, a new encoder-relay PR, PR-07 / PR-08 order) | H-05 (c), P-B | #8, #9, #13; console fault-tolerance tests §12 #1, #2, #3, #7, #9 | `[DR-1]` | OPEN |
| 2.11 | **Resistance scale mapping**: 256-level internal scale (owner decision); exact mappings and offsets to Echelon 1–32, FTMS Supported Resistance Level Range (0x2AD6) and 1–100 displays; linearisation option (design notes §6 A / B / C) | P-A, P-C | #4 | Scale decided; mappings `[DR-1]` | PARTLY DECIDED |

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
