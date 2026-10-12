# DR-1: v1 Passive Design Freeze — Decision Record

> **Release:** **v1 "Passive"**. Re-scoped on 2026-10-12 when the owner split the
> programme into v1 Passive / v2 Active releases. Interception, motor drive and
> standalone operation moved to [DR-2: v2 Active Design Freeze](DR-2-design-freeze.md).

| | |
|---|---|
| **Status** | **APPROVED WITH CONDITIONS (2026-10-12)**, see §4. Outputs (§3) in progress |
| **Owner** | Chief Architect (Tier 1) |
| **Entry condition** | PR-01 merged; every row of the [Phase 0 register](../../../../docs/measurements/phase0/README.md) has an Evidence file and a Result |
| **Gates** | PR-02, PR-04a (v1). No soldering or permanent installation before approval. PR-07 is gated by [DR-2](DR-2-design-freeze.md) |
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
| 1 | Console motor drive waveform (both wires, both directions, at rest; 10 ms/div and µs/div) | `A01-<yyyymmdd>-…` | Open | §2.1 sense network; DR-2 §2.3 driver |
| 2 | Motor current: inrush and running under brake load at cam-max | `A02-<yyyymmdd>-…` | Open | §2.6 power model; DR-2 §2.3 driver |
| 3 | Pot mV over one full cam revolution; coast distance after drive stops | `A03-<yyyymmdd>-…` | Open | DR-2 §2.5 controller (run-time bound, loop rate, dead zone) |
| 4 | Wiper, Yellow, Green at every console level, both directions; resistance proxy against angle | `A04-<yyyymmdd>-…` | Open | §2.11 level reporting, PR-02 passive calibration; DR-2 §2.5 |
| 5 | Pot shape over one revolution; click-offs; working half | `A05-<yyyymmdd>-…` | Open | DR-2 §2.5 controller (angle tracking) |
| 6 | Green = GND and Yellow = 3.3 V; source impedance | `A06-<yyyymmdd>-…` | Open | §2.9 R3/R4/R5 (none in v1); DR-2 §2.2 |
| 7 | Cadence: Orange / Grey / Brown open and closed | `A07-<yyyymmdd>-…` | Open | §2.7 pin map, §2.8 passive tap board, §2.9 R3 |
| 8 | Encoder common and channel pulls; counts per detent; Ch B analog shape (WO0) | `A08-<yyyymmdd>-…` | Open | §2.7 pin map; DR-2 §2.10 |
| 9 | Console tolerates disconnected motor lines | `A09-<yyyymmdd>-…` | Open | v1 known limitation (runaway); DR-2 §2.4 |
| 10 | Console 3.3 V rail with parallel supplies | `A10-<yyyymmdd>-…` | Open | §2.9 (waived for v1) |
| 11 | Waveshare VIN tolerance of 9 V | `A11-<yyyymmdd>-…` | Open | §2.3 VIN power, §2.8 passive tap board power |
| 12 | Echelon app pairs a third-party `0x180D` peripheral | `A12-<yyyymmdd>-…` | Open | PR-03 entry gate (not a DR-1 decision) |
| 13 | Console power-on behaviour (single-shot scope, logger on USB) | `A13-<yyyymmdd>-…` | Open | §2.8 power-on behaviour; DR-2 §2.4, §2.5 |

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
| #4 | Pot linear in shaft angle over levels 1–32 (R² = 0.9989), `wiper = 39.1 × level + 97` counts, about 151° of travel. The cam makes magnet displacement `e·(1 − cos θ)`, so resistance against level is non-linear (monotonic within the range); braking against gap is a further unknown `g`. Hysteresis < 15 counts. The console overshoots on 98% of mid/high moves | B03 | §2.6, §2.11 simplified (P-C); 22 kΩ tap offset unverified (E-02) |
| #6 | **Yellow = 0 V, Green ≈ 1.67 V.** The pot is referenced to about 1.67 V on Green, the opposite of the SPEC naming | B01 | §2.9 (none in v1); DR-2 §2.2 redesigns the standalone network (E-01) |
| #7 | Cadence readable only through 4.7 kΩ + 100 nF; idle high, low on magnet | B01 | §2.7, §2.8; edge distortion to be confirmed (E-02) |
| #8 | Wire colours Black = A, Red = B, Orange = VCC. PCNT gives 0% bounce, 100% detection, `stepsPerDetent = 4`. **The stock console misses 28% of clicks** | B02, B04 | No Ch B conditioning needed (A08 superseded, E-03); "28%" later withdrawn (A08-20261012) |

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

**Scope (2026-10-12, owner decision):** DR-1 freezes the **v1 Passive** board
only: today's tap circuitry wired permanently, no resistance control of any kind.
Decisions 2.2 (switchable supplies), 2.4 (fail-safe), 2.5 (cam-aware controller)
and 2.10 (interception architecture), and the v2 parts of 2.3 (motor driver) and
2.7 (active outputs), have **moved to [DR-2](DR-2-design-freeze.md)**. Section
numbers are kept so cross-references stay stable.

| § | Decision (v1) | Audit ref | Evidence rows | Decision | Status |
|---|---|---|---|---|---|
| 2.1 | **Motor sense network**: the taps stay in v1 for **monitoring only** (motor-direction events, seek detection); divider values, analog vs logic sensing, sample rate | H-01 | #1 | `[DR-1]` | OPEN |
| 2.2 | *Moved to DR-2* (switchable standalone supplies) | — | — | — | MOVED |
| 2.3 | **v1 power path**: ESP32 on the bike's 9 V via the Waveshare **VIN** pin (6–21 V, row #11); **power switch on the VIN line** so USB-C can be used instead; external MP1584EN buck dropped. *Open item:* back-feed / power-path behaviour with USB and VIN both connected is verified at DR-1 or PR-04a. The motor driver moves to DR-2 §2.3 | H-03, D3 | #11 | `[DR-1]` | OPEN |
| 2.4 | *Moved to DR-2* (fail-safe). v1 intercepts nothing, so it needs no bypass | — | — | — | MOVED |
| 2.5 | *Moved to DR-2* (cam-aware controller). In v1, level comes from the linear wiper fit (B03); decoder = `EncoderModule` (PCNT), already decided | — | — | — | MOVED |
| 2.6 | **Power model**: empirical per-level table (level or 256-level internal position × cadence → watts), capturing the cam's `1 − cos θ` displacement and the gap-to-braking function together; no polynomial in level or angle; tolerance target | S-02 | #2, #4 | `[DR-1]` | OPEN |
| 2.7 | **v1 pin map (taps only)**: confirm the tap subset of the Phase 0/1 map; no outputs except **GPIO 43/44 (UART0) reserved** for a future ANT+ add-on | H-04 | #7, #8 | `[DR-1]` | OPEN |
| 2.8 | **Passive tap board design**: J1–J4 pass-through, tap network, VIN power and switch, housing fit | H-05 (option b), D2 | #6, #7, #10, #11 | `[DR-1]` | OPEN |
| 2.9 | **R3 / R4 / R5**: none on the v1 board. Must reflect the measured pot polarity (Green ≈ 1.67 V reference, Yellow = 0 V) | H-02, E-01 | #6, #7, #10 | `[DR-1]` | OPEN |
| 2.10 | *Moved to DR-2* (interception architecture) | — | — | — | MOVED |
| 2.11 | **Resistance scale mapping (v1)**: 256-level internal scale (owner decision); report Echelon level 1–32 and the internal scale; FTMS reports level only, **no targets** | P-A, P-C | #4 | Scale decided; mappings `[DR-1]` | PARTLY DECIDED |

---

## 2A. Chief Architect draft recommendations (2026-10-12, re-scoped to v1): for a joint decision

**Nothing in this section is decided until the owner approves it in §4.**
Evidence base: the Phase 0 register (PR #1) plus the DR-1 prep addendum
(PR #2: A01-20261012, A04-20261012, A07-20261012, A08-20261012, A09-20261012,
A10-20261012). The original draft's interception, motor-driver and standalone
material is carried into [DR-2 §2A](DR-2-design-freeze.md) as draft input.

### Entry-condition dispositions
| Row | Disposition |
|---|---|
| #10 console rail with parallel supplies | **Waive.** The v1 board puts no supply onto a console rail (see 2.9). The GPIO 10 encoder-VCC tap saturates at 4095 counts (≥ 3.1 V at 12 dB attenuation), which shows the rail is present but cannot show its stability. Re-opens at DR-2 if a v2 standalone network can connect to a console rail |
| #12 Echelon app pairs a third-party HR device | **Moves to PR-03 WO3** (go/no-go). Not a DR-1 input |
| SPEC 0.9 (60-minute ride log) | **Not required for DR-1.** Optional; capture it during PR-02 rides |

### Recommendations (v1 only)
| § | Recommendation | Key evidence |
|---|---|---|
| 2.1 Motor sense | **Keep** the 30 kΩ / 10 kΩ ÷4 dividers on GPIO 6/7 for monitoring, sampled as ADC1 analog at ≥ 1 kHz; drive direction = whichever wire is high. **No clamp diodes**: the worst single-ended excursion is −0.14 V (−0.035 V at the GPIO), and 30 kΩ limits any transient to well under 0.1 mA. *Note:* that result is DMM-based, so a short spike could be missed; the 30 kΩ series resistor covers this | A01-20261011/12 |
| 2.3 Power path | Power the ESP32 from **VIN (9 V, White/Brown)** through its onboard MP2322 regulator; **drop the external MP1584EN buck**. Fit a **power switch on the VIN line** (VIN off ⇒ USB-C powers the board). **Open item for DR-1 / PR-04a:** verify back-feed and power-path behaviour with USB and VIN both connected (does VBUS back-feed the 9 V line, or VIN back-feed the USB host?); until verified, the procedure is "switch VIN off before plugging USB". No motor driver in v1 | A11 |
| 2.6 Power model | **Empirical per-level table** (level × cadence → W), measured in PR-02 through passive calibration. No polynomial. Tolerance target is set in PR-02 against whatever reference is available (≤ 15% without a power meter) | B03, P-C |
| 2.7 v1 pin map | **Taps (unchanged from Phase 0):** wiper 1, Green 3 (monitor), cadence 5 (4.7 kΩ + 100 nF), motor sense 6/7, encoder in 8/9 (PCNT). Yellow (2), Grey (4) and encoder VCC (10) are dropped on the v1 board. **No outputs.** **GPIO 43 (TX, D1) / GPIO 44 (RX, D0) = UART0, reserved** for a future nRF51422 ANT+ module; console and logging use native USB-CDC. The v2 outputs are proposed in DR-2 §2A | §4.4.1, board.h |
| 2.8 Passive tap board | J1/J2 (9-pin) and J3/J4 (3-pin) straight pass-through; the PR-01 tap network (22 kΩ taps, 4.7 kΩ + 100 nF cadence, ÷4 motor dividers); ESP32 on VIN with the VIN/USB switch; no standalone supplies; manual OTA; UART0 pads left free. Housing fit is checked in PR-04a | PR-01 evidence |
| 2.9 R3 / R4 / R5 | **None in v1.** The standalone network (pot-reference replica, cadence pull-up, encoder supply) is a v2 / DR-2 item | B01, row #6 |
| 2.11 Scale mapping | Internal `i ∈ 0…255`. Echelon `L = round(i × 31 / 255) + 1` and `i = round((L − 1) × 255 / 31)`, giving exact endpoints 0 ↔ 1 and 255 ↔ 32. In v1 Echenon **reports** level 1–32 and the internal scale (FTMS Indoor Bike Data resistance field, portal, TCX); it accepts **no targets**. Whether `0x2AD6` is exposed at all with no Resistance Target Setting is a PR-05 check against FTMS v1.0. Apps get a **linearised** resistance from the per-level table (design-notes §6 option B). Option A (uneven pulse counts) is rejected | P-A, P-C |

### Known limitation of v1: stock runaway
The stock console can be driven into a **runaway** (a fast knob spin past level
32 leaves the motor turning continuously; recovery needs the motor unplugged;
A09-20261012 #3). **A passive board cannot prevent it**: v1 intercepts neither the
knob nor the motor. v1 may *detect* it (wiper leaves the working range while the
motor-sense taps show continuous drive) and alert in the portal. Prevention
(rate-limited encoder emulation, NC motor-cut relay) is a v2 / DR-2 §2.4 item.

---

### Owner decisions on §2A (2026-10-12)
- **FTMS v1 read-only** (Feature + Indoor Bike Data only), confirmed in PR-05 WO1.
- **Green reference tap on GPIO 3 kept** as a v1 monitor (pin map 2.7).
- **Runaway detection and alert in v1** (PR-02); prevention is v2 (DR-2).
- **Save-to-flash diagnostics, default off**, go in PR-14 (does not affect the board design).
- The remaining §2A rows still need the joint DR-1 approval in §4.

---

## 3. Outputs: documents revised at DR-1

| Output | Document and section | Driven by |
|---|---|---|
| Hardware configuration and v1 pin map; `[DR-1]` markers resolved | [SPEC](../../../../SPEC.md) §4.3, §4.4.2 (v1 rows; PROVISIONAL marking removed for v1), §4.6–§4.8 (v1 parts) | 2.1, 2.3, 2.7, 2.9 |
| Sensor acquisition, level mapping, encoder monitoring | SPEC §5.1–§5.4 (§5.5–§5.6 are v2, DR-2) | 2.1, 2.11 |
| Power engine, passive calibration method, NVS schema | SPEC §6.1–§6.4 | 2.6, 2.11 |
| Build flags (`HAS_*` for the v1 passive configuration) | SPEC §12.3 | 2.7 |
| DR-1 checks D.1–D.7 | SPEC §13.3 | all |
| Power architecture: VIN, VIN/USB switch, back-feed check | [Hardware spec](../../../../docs/hardware_specification.md) §3 | 2.3 |
| Passive tap board design finalised (placeholder replaced) | Hardware spec §6.2 (§6.2.2 lists the decisions) | 2.8 |
| PR mandates adjusted where a decision changes scope | `pr-02`, `pr-04a`, `pr-05` PROJECT.md | all |

---

## 4. Chief Architect approval

| | |
|---|---|
| **Decision** | ☐ Approved · ☑ **Approved with conditions** · ☐ Rejected |
| **Scope** | v1 Passive design freeze: decisions 2.1, 2.3, 2.6, 2.7, 2.8, 2.9, 2.11 as recommended in §2A (v1), with the dispositions in §2A (waive row #10; row #12 → PR-03 WO3) and the known v1 limitation (stock runaway detected, not prevented) |
| **Conditions** | **2.3:** the board is powered from **9 V (VIN) or USB, never both at once**. PR-04a WO1 reads the Waveshare VBUS/VIN power path from the schematic before any soldering and designs a selector (disconnect 9 V in the USB position; add a diode on the VIN path if the board does not isolate the two). **2.6:** PR-02 includes a **recalibration UI**: the user enters values read from the console or app screen (level, power), and the firmware correlates them with live sensor readings to update the per-level table. **2.7:** all 10 PR-01 taps are kept on the v1 board (diagnostics); GPIO 43/44 are reserved. **2.8:** the v1 board is the PR-01 detection circuit made permanent. **2.11:** the internal 0–255 scale is canonical, with per-app mappings: Echelon 1–32 (direct); **Peloton 0–100 by power equivalence** (our per-level power table against the published Peloton power curve); FTMS resistance-level field; power, cadence and HR for Garmin, Zwift and Strava |
| **Open items carried** | PR-02 power-data source (Echelon app readings / sniffing the console's BLE broadcast / reference power meter / community tables), to be chosen by the owner at PR-02 planning. VIN/USB power-path check (PR-04a WO1) |
| **Approved by** | Owner, with the Chief Architect |
| **Date** | 2026-10-12 |
| **Evidence baseline** | Echenon `main` @ `fb1a06e`; Phase 0 register as merged in PR #1 (`5897f6f`) and PR #2 (`5168b1e`) |
| **Owner acknowledgement** | *"2.1 - ok / 2.3 - we should design to have EITHER usb OR 9v in. Not both. / 2.6 - ok (we may want a general re-calibration UI where the user enters data from the screen and correlates it into the sensor readings) / 2.7 - ok / 2.8 - ok - circuit basically as it is now for the detection rounds / 2.9 - ok / 2.11 - ok. (we should be able to translate into data that makes sense to the peloton app too (100 levels?) as well as garmin, strava or whatever?"* (chat, 2026-10-12) |

Approval unblocks **PR-02** and **PR-04a** (soldering allowed under the 2.3 condition). The v2 decisions remain open in [DR-2](DR-2-design-freeze.md).
