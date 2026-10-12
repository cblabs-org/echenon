# DR-1: Design Freeze — Decision Record

| | |
|---|---|
| **Status** | **IN PROGRESS**: evidence complete (2026-10-12); draft recommendations in §2A for a joint decision with the owner |
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
| #4 | Pot linear in shaft angle over levels 1–32 (R² = 0.9989), `wiper = 39.1 × level + 97` counts, about 151° of travel. The cam makes magnet displacement `e·(1 − cos θ)`, so resistance against level is non-linear (monotonic within the range); braking against gap is a further unknown `g`. Hysteresis < 15 counts. The console overshoots on 98% of mid/high moves | B03 | §2.5, §2.6 simplified (P-C); 22 kΩ tap offset unverified (E-02) |
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
| 2.6 | **Power model**: empirical per-level table (level or 256-level internal position × cadence → watts), capturing the cam's `1 − cos θ` displacement and the gap-to-braking function together; no polynomial in level or angle; tolerance target | S-02 | #2, #4 | `[DR-1]` | OPEN |
| 2.7 | **Phase 3 pin map**: confirm or revise the provisional map (sense GPIO 6/7; L9110S or successor IA/IB GPIO 10/17; wiper 1; cadence 5; encoder A/B 8/9; no encoder-VCC sense). GPIO 6/7 are never both driver outputs and sense inputs | H-04 | #7, #8 | `[DR-1]` | OPEN |
| 2.8 | **Passive tap board design**: J1–J4 pass-through, tap network, buck and rail, housing fit | H-05 (option b), D2 | #6, #7, #10, #11 | `[DR-1]` | OPEN |
| 2.9 | **R3 / R4 / R5**: keep, remove or make switchable, per board. Must reflect the measured pot polarity (Green ≈ 1.67 V reference, Yellow = 0 V) | H-02, E-01 | #6, #7, #10 | `[DR-1]` | OPEN |
| 2.10 | **Interception architecture**: choose from Monitor / **Encoder MITM** (PCNT ingest, clean quadrature resynthesised to the console, NC fail-safe bypass) / Full MITM (pot simulation) / Console bypass (ESP32 H-bridge with PID; console drive used as a hint). Includes whether ERG is first delivered by encoder injection (32 levels, console drives the motor) and the resulting programme restructure (PR-04a scope, a new encoder-relay PR, PR-07 / PR-08 order) | H-05 (c), P-B | #8, #9, #13; console fault-tolerance tests §12 #1, #2, #3, #7, #9 | `[DR-1]` | OPEN |
| 2.11 | **Resistance scale mapping**: 256-level internal scale (owner decision); exact mappings and offsets to Echelon 1–32, FTMS Supported Resistance Level Range (0x2AD6) and 1–100 displays; linearisation option (design notes §6 A / B / C) | P-A, P-C | #4 | Scale decided; mappings `[DR-1]` | PARTLY DECIDED |

---

## 2A. Chief Architect draft recommendations (2026-10-12): for a joint decision

**Nothing in this section is decided until the owner approves it in §4.**
Evidence base: the Phase 0 register (PR #1) plus the DR-1 prep addendum
(PR #2: A01-20261012, A04-20261012, A07-20261012, A08-20261012, A09-20261012,
A10-20261012).

### Entry-condition dispositions
| Row | Disposition |
|---|---|
| #10 console rail with parallel supplies | **Waive.** No v1 board puts a supply onto a console rail (see 2.9). The GPIO 10 encoder-VCC tap saturates at 4095 counts (≥ 3.1 V at 12 dB attenuation), which shows the rail is present but cannot show its stability. If the PR-07 standalone network ever connects to a console rail, re-open this row |
| #12 Echelon app pairs a third-party HR device | **Moves to PR-03 WO3** (go/no-go). Not a DR-1 input |
| SPEC 0.9 (60-minute ride log) | **Not required for DR-1.** Optional; capture it during PR-02 rides |

### Recommendations
| § | Recommendation | Key evidence |
|---|---|---|
| 2.1 Motor sense | **Keep** the 30 kΩ / 10 kΩ ÷4 dividers on GPIO 6/7, sampled as ADC1 analog at ≥ 1 kHz; drive direction = whichever wire is high. **No clamp diodes**: the worst single-ended excursion is −0.14 V (−0.035 V at the GPIO), and 30 kΩ limits any transient to well under 0.1 mA. *Note:* that result is DMM-based, so a short spike could be missed; the 30 kΩ series resistor covers this | A01-20261011/12 |
| 2.2 Switchable standalone supplies | **Not on v1 boards.** Standalone operation (UC6, console absent) moves to the PR-07 board. There, the pot reference must reproduce the console's **1.66 V / 2.4 kΩ Thevenin source on Green**, e.g. a 3.3 V divider of about 4.7 kΩ / 4.7 kΩ (≈ 1.65 V, ≈ 2.35 kΩ), connected only when the console is absent. **Yellow is ground, and Green must never be tied to GND** | B01, A06 load test, A04-20261012 |
| 2.3 Driver and rail | Power the ESP32 from **VIN (9 V)** through its onboard MP2322 regulator, and **drop the external MP1584EN buck**. For PR-07, a MOSFET H-bridge with current limit and fault output (DRV8833 class) at 3.3 V: the motor draws ≤ 175 mA, so no separate rail is needed; add bulk capacitance at the driver. *Open item:* confirm the Waveshare 3V3 rail's output rating (MP2322 is a ~1 A part) covers ESP32 peak plus the motor | A02, A11 |
| 2.4 Fail-safe | **Normally-closed bypass on every intercepted path** (option a): the encoder A/B lines on an encoder-intercept board, and the motor lines on the PR-07 board. The ESP32 must actively hold each bypass open. The boot race is moot: there is no power-on sweep (row #13). **New risk:** the stock console's *runaway* (fast spin past level 32 leaves the motor turning continuously; recovery needs the motor unplugged). The ESP32 cannot stop this on a passive or encoder-only board. **Option for the owner:** an NC motor-cut relay on the passive board, closed in normal use (stock), which the ESP32 opens only when the wiper leaves the working range for more than N seconds | A09-20261012 #3, row #13 |
| 2.5 Cam-aware controller | **PR-07 only.** Required behaviour: track which working ramp the cam is on by detecting the dead zone (wiper leaves the 0–2120 ramp / floats high); never leave the console's ramp (165–1327 counts); run-time bound ≈ 1 revolution (~35 s at ~16 s per half); brake-and-approach (coast < 15 counts). Loop rate and gains are set in PR-07 | A03, A09 |
| 2.6 Power model | **Empirical per-level table** (level × cadence → W), measured in PR-02 through passive calibration. No polynomial. Tolerance target is set in PR-02 against whatever reference is available (≤ 15% without a power meter) | B03, P-C |
| 2.7 Phase 3+ pin map | **Taps (unchanged from Phase 0):** wiper 1, Green 3 (monitor), cadence 5 (4.7 kΩ + 100 nF), motor sense 6/7, encoder in 8/9 (PCNT). Yellow (2), Grey (4) and encoder VCC (10) are dropped on production boards. **New outputs, proposed:** encoder out A/B **GPIO 17/18**, encoder bypass control **GPIO 21**, optional motor-cut relay **GPIO 38**. **PR-07:** driver IN1/IN2 **GPIO 10/11**, motor bypass control **GPIO 47**. This replaces the provisional "L9110S on 10/17" | §4.4.1, board.h |
| 2.8 Passive tap board | J1/J2 (9-pin) and J3/J4 (3-pin) pass-through; the PR-01 tap network (22 kΩ taps, 4.7 kΩ + 100 nF cadence, ÷4 motor dividers); ESP32 on VIN; no standalone supplies; manual OTA. **If 2.10 adopts the encoder intercept**, J3/J4 go through the encoder bypass (NC) and two ESP32 outputs. Housing fit is checked in PR-04a | PR-01 evidence |
| 2.9 R3 / R4 / R5 | **Remove** from all v1 boards. The PR-07 standalone network is the switched 1.66 V Green reference (2.2) plus a cadence pull-up, connected only when the console is absent | B01, row #6 |
| 2.10 Interception architecture | **Recommend staged: v1 = Encoder MITM; v1.1 = motor MITM.** The "28% missed clicks" case was an artefact (4 detents per level by design; A08-20261012). The case for it now rests on three things. **(1) Robustness:** rate-limit and debounce the knob, so the stock runaway (A09 #3) cannot be triggered from the knob, and down-direction behaviour is consistent (6–10 detents vs 4 up). **(2) ERG without a motor split:** inject 4 detents per level, one level at a time, wait for seek-complete (motor-direction events), verify the level from the wiper, and never command beyond 1–32. The console tolerates every fault tested (#1, #2, #3, #7, #9) and updates its target mid-seek. **(3) Low hardware risk:** only the 3-pin loom is intercepted, behind an NC bypass. **Costs:** 32-level ERG granularity and console-speed seeks (~0.5 s per level). Motor MITM / console bypass (256 levels, PID, standalone UC6) becomes PR-07 in v1.1 | A08, A09-20261012, B03 |
| 2.11 Scale mapping | Internal `i ∈ 0…255`. Echelon `L = round(i × 31 / 255) + 1` and `i = round((L − 1) × 255 / 31)`, giving exact endpoints 0 ↔ 1 and 255 ↔ 32. FTMS `0x2AD6` advertises **1.0–32.0 step 1.0** in v1 (step 0.1 only with motor MITM). Apps get a **linearised** resistance from the per-level table (design-notes §6 option B). Option A (uneven pulse counts) is rejected | P-A, P-C |

### Programme impact if 2.10 is adopted
| PR | Change |
|---|---|
| PR-02 | Unblocked by DR-1. Passive calibration and the per-level power table |
| PR-04a | Passive tap board **+ encoder intercept with NC bypass** (+ optional motor-cut relay); ESP32 on VIN; manual OTA |
| **PR-04b (new)** | **Encoder relay** firmware: PCNT ingest → rate-limited quadrature resynthesis (4 detents per level), bypass control, wiper-verified level tracking, runaway detection and alert, native tests. Depends on PR-02 and PR-04a |
| PR-08 | ERG via encoder injection. Depends on PR-04b and PR-05 (no longer PR-07) |
| PR-07 | Motor MITM + standalone (UC6) → **v1.1**, after PR-09 |
| PR-09 | Depends on PR-06 and PR-08. UC6 standalone moves to v1.1 |

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
