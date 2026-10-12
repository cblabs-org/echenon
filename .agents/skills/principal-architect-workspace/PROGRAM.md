# Echenon Firmware — Programme

> **Programme:** Build the complete Echenon firmware — an ESP32-based FTMS bridge
> for the Echelon EX-3 spin bike, using the ESP32 App Foundation framework.
>
> **Chief Architect tier.** This document tracks the macro-level PR dependency
> graph. Each PR is delegated to a Tier 2 Principal Architect who decomposes it
> into Work Orders.
>
> **Specification:** [`SPEC.md`](../../../SPEC.md)
> **Implementation Strategy:** [`docs/implementation-strategy.md`](../../../docs/implementation-strategy.md)
> **Use Case Analysis:** [`docs/use_case_analysis.md`](../../../docs/use_case_analysis.md)
> **Hardware Specification:** [`docs/hardware_specification.md`](../../../docs/hardware_specification.md)
> **Upstream RFC:** BLEManager (framework PR #235, tag `v0.0.12-dev.82` or later)
> **Latest review:** [2026-09-25 readiness review](reviews/2026-09-25-readiness-review.md)
> **Design freezes:** [DR-1 (v1 Passive)](reviews/DR-1-design-freeze.md) · [DR-2 (v2 Active)](reviews/DR-2-design-freeze.md)

---

## Programme Status

Phase and PR numbering in this table is canonical; other documents follow it.
The programme has two releases (owner decision 2026-10-12; see
[PROGRAM-PLAN.md "Releases"](PROGRAM-PLAN.md#releases)): **v1 "Passive"** (UC1–UC3,
no resistance control) and **v2 "Active"** (UC4–UC6 + HR-zone auto-resistance;
provisional, decomposed after DR-2).

| Release | PR | Phase | Title | Dependencies | Status |
|---|---|---|---|---|---|
| v1 | PR-01 | 0 | Signal Logger (Observe) | `main` | ✅ Merged 2026-10-11 (PR #1, `5897f6f`) |
| v1 | DR-1 | — | v1 Passive Design Freeze ([record](reviews/DR-1-design-freeze.md)) | PR-01 evidence + [DR-1 prep bench](pr-dr1-prep/PROJECT.md) | ✅ Approved with conditions 2026-10-12 |
| v1 | PR-02 | 1 | Calibration + Power (+ live WS feed) | PR-01, **DR-1** | ⬜ Not started |
| v1 | PR-03 | 2 | BLE Foundation + HR Relay | PR-01 ✅; gates proposed as in-PR go/no-go (WO3, WO5) | 📝 Planned: [7 WOs](pr-03-hr-relay/PROJECT.md), awaiting approval |
| v1 | PR-04a | 3 | Passive Tap Board (VIN + VIN/USB switch, manual OTA, UART0 reserved) | PR-02, DR-1 | ⬜ Not started |
| v1 | PR-05 | 3 | FTMS Telemetry, **read-only** (Target Setting Features = 0) | PR-02, PR-03 | ⬜ Not started |
| v1 | PR-06 | 4 | Session + TCX + Strava | PR-02 | ⬜ Not started |
| v1 | PR-14 | 4 | Diagnostics & Run-Data Persistence ([stub](pr-14-diagnostics/PROJECT.md)) | PR-06 | ⬜ Not started |
| v1 | PR-09 | 7 | v1 Integration & Release (UC1–UC3) | PR-04a, PR-05, PR-14 | ⬜ Not started |
| v2 | DR-2 prep | — | Active-design bench evidence ([project](pr-dr2-prep/PROJECT.md)) | DR-1 (timing: owner) | ⬜ Provisional |
| v2 | DR-2 | — | v2 Active Design Freeze ([record](reviews/DR-2-design-freeze.md)) | DR-2 prep | ⬜ Not started |
| v2 | PR-07 | 5 | Active Intercept Board + Motor Control + Safety | **DR-2**, v1 release (PR-09) | ⬜ Provisional |
| v2 | PR-10 | 5 | Encoder Emulation + Relay Mode (mode 1) | PR-07 | ⬜ Provisional |
| v2 | PR-08 | 6 | ERG + Control Point targets + resistance setting | PR-10, PR-05 | ⬜ Provisional |
| v2 | PR-11 | 6 | HR-Zone Auto-Resistance (portal-native) | PR-08 | ⬜ Provisional |
| v2 | PR-12 | 6 | Console-Off Standalone Mode (mode 2, UC6) | PR-07, PR-10 | ⬜ Provisional |
| v2 | PR-13 | 7 | v2 Integration & Release (UC4–UC6) | PR-07, PR-10, PR-08, PR-11, PR-12 | ⬜ Provisional |

### Gates

| Gate | Required Before | Verification | Status |
|---|---|---|---|
| **Bench ready**: ≥ 2-channel scope with single-shot trigger; motor-current measurement (shunt + scope or DC clamp); JST-SM 9-pin and JST 3-pin Y / back-probe harness; series tap resistors | PR-01's first WO that connects to the bike | Owner confirms kit on hand; recorded in PR-01 Session Log | PARTIAL: bench in use since 2026-10-03; scope and current measurement still to be confirmed (WO4 items C and D) |
| **Scope characterisation** of both motor wires (both directions, at rest; 10 ms/div and µs/div) | Connecting PR-01 motor-sense taps (GPIO 6/7) | Captures committed as Phase 0 register row #1 evidence. Either result lets logging proceed (÷4 divider safe to ~13 V); result feeds DR-1 | ✅ MET: PR-01 WO4b delivered the A01 scope captures (A01-20261011, ±3.25 V flat DC, no PWM) and A01-20261012 the single-ended check (worst −0.14 V; no clamp diodes). Taps had been connected earlier, safely, through the divider |
| **Phase 0 register complete** ([`docs/measurements/phase0/README.md`](../../../docs/measurements/phase0/README.md)) | PR-01 exit / DR-1 start | Every row has an Evidence file and a Result | ✅ MET: rows closed; #10 waived for v1 (DR-1 §2A), #12 moved to PR-03 WO3 |
| **DR-1: v1 Passive Design Freeze** | **PR-02, PR-04a** (no soldering or permanent install before approval) | Chief Architect approves the [DR-1 record](reviews/DR-1-design-freeze.md) | ✅ APPROVED WITH CONDITIONS 2026-10-12 (§4) |
| **VIN / USB power-path check** | **Soldering** the PR-04a board | DR-1 condition 2.3: VIN or USB, never both. PR-04a WO1 reads the Waveshare VBUS/VIN path from the schematic and designs the selector (diode on VIN if not isolated); evidence committed | OPEN (PR-04a WO1) |
| **v1 release** (beta / stable tag, owner only, rule 03) | PR-07 | PR-09 exit criteria met; owner tags | OPEN |
| **DR-2: v2 Active Design Freeze** | **PR-07** (and all v2 PRs; no active-board soldering or interception before approval) | Chief Architect approves the [DR-2 record](reviews/DR-2-design-freeze.md) | NOT STARTED |
| R3/R4/R5 standalone supplies | DR-2 (none in v1, DR-1 §2.9) | Pot-reference replica and switched supplies decided at DR-2 §2.2 / §2.9 | ✅ DECIDED for v1: none (DR-1 §2.9). OPEN for v2 (DR-2) |
| Echelon-app HR spike | PR-03 (proposed: WO3 go/no-go) | Row #12 INCONCLUSIVE with the nRF mock (Android could not discover it); retest with an ESP32 HR peripheral | OPEN |
| **Framework central-role acceptance evidence** | PR-03 entry | RFC §17.2 items 4–9 against a real 0x180D peripheral on a framework tag containing the WO-8 use-after-free fix | **OPEN** |
| Framework #226 (OTA check-on-boot deferral) | Removing Echenon's `autoCheck=false` enforcement | Upstream issue **closed**. PR-04a uses the framework option after the PR-03 WO1 bump (verify) | CLOSED upstream |

### Parallelism

- PR-03 can start once PR-01 merges and its entry gates close; it runs **in
  parallel** with DR-1 and PR-02.
- PR-02 and PR-03 share `EchenonModule.*`, `platformio.ini` and `main.cpp`. The PR
  that merges second rebases onto `main` and resolves the conflict.
- PR-04a, PR-05 and PR-06 can run **in parallel** once their own dependencies merge.
- v2 PRs do not start until DR-2 is approved and v1 is released. DR-2 prep (bench
  evidence only) may overlap late v1 work.

---

## Programme Session Log

### 2026-09-17 — Programme Initialisation
- Specification finalised at ~1,460 lines with 13 sections and 7 user journeys
- Foundation BLE module RFC filed as upstream issue #225
- Initial PA-level decomposition (10 WOs in a single PR) was over-scoped for one PR
- Restructured into 7-PR programme with proper Chief Architect tiering
- Pre-researched WO content archived at `archive-pa-decomposition/` — PAs may
  reference this as prior art when decomposing their own WOs

### 2026-09-20 — Programme Restructured to Iterative Strategy
- Spec updated to ~2,450 lines with MITM architecture, hardware spec,
  use case analysis (6 use cases), and revised test plan (§13)
- Adopted iterative hardware+software co-development strategy
  (`docs/implementation-strategy.md`) with 7 phases (0–6)
- Key design evolution since initial programme:
  - Power architecture simplified (single 9V→3.3V buck, no 5V rail)
  - Motor voltage verification gate added (oscilloscope before GPIO connection)
  - Motor sense protection: voltage divider (30kΩ/10kΩ) replaces zener clamp
    for bidirectional motor + back-EMF safety
  - HR relay separated from FTMS as independent Phase 2 (zero bike risk, first
    user-facing feature, proves BLE stack early)
  - Full 12-wire passive observation in Phase 0 (all 9-pin + all 3-pin loom wires)
  - Echelon app never commands resistance (confirmed — no software conflict)
  - HR-zone auto-resistance: Approach A (external app via FTMS CP) for v1
- PRs restructured: old pr-01 through pr-06 renamed and rescoped to match phases
- Old PROJECT.md files archived (old scope recorded in prior art references)
- Dependency graph simplified: PR-02 ∥ PR-03, everything else serial

### 2026-09-25 — Programme Restructured per Chief Architect Design Audit
- Restructured from 7 PRs to 9 PRs + DR-1 hard gate.
- Framework BLE module (PR #235, tag `v0.0.12-dev.82`) merged and confirmed as PR-03 prereq. *(Overstated: corrected in the 2026-09-25 remediation entry below.)*
- Key decisions documented and enacted:
  - D2: Adopted the passive-first 9-PR restructure.
  - D3: Motor driver on its own rail (specifics deferred to DR-1).
  - D4: Zwift SIM mode (0x11) slated for v1.1, space left in FTMS design.
  - D5: BYOA (Bring-your-own-app) model adopted for Strava API credentials.

### 2026-09-25 — Readiness Review and G0 Remediation
- A Chief Architect [readiness review](reviews/2026-09-25-readiness-review.md)
  found that commit `2992155` applied the design audit partly as directives rather
  than specification. The owner approved docs-only remediation in chat on
  2026-09-25 (review §7). One set of shared decisions (phase and PR numbering,
  Phase 0/1 and Phase 3+ pin maps, build configuration, Phase 0 capture design,
  Phase 0 gates, naming, calibration split, dependencies, BLE, upload pipeline,
  ERG state machine and passive-first framing) was applied across SPEC, `docs/`
  and this workspace.
- **Correction to the entry above: the BLEManager gate is not met.** Of the three
  P-01 r4 conditions:
  - PR #235 is merged with its 🔴 items 1–9 fixed — met;
  - #236 (WifiManager fallback to an open AP) is fixed — met;
  - **central-role hardware acceptance is not evidenced.** The framework's
    `ACCEPTANCE.md` at `v0.0.12-dev.82` marks "Central Scan — N/A (by design)", and
    the only acceptance run was at `dev.67`, before the WO-8 use-after-free fix.
  Framework #226 (OTA check-on-boot) is still open. PR-03's entry now carries an
  explicit **OPEN** central-acceptance gate.
- Programme changes:
  - PR-02 is gated on **DR-1** as well as PR-01; calibration is passive (console
    levels as the resistance proxy); motor-driven calibration moves to PR-07.
  - PR-05 needs no new hardware; its dependency on the passive tap board is removed.
  - PR-06 has no PR-03 edge; its `ScanInhibit` use is compiled under `WITH_BLE`.
  - PR-02 / PR-03 overlap stated consistently (`EchenonModule.*`, `platformio.ini`,
    `main.cpp`; second to merge rebases).
  - Control Point split: PR-05 exposes 0x2AD9 and handles Request Control; PR-08
    implements target opcodes and Fitness Machine Status.
  - Manual OTA and `autoCheck=false` enforcement move into PR-04a, before the board
    is sealed.
- PR-01 mandate rewritten: SPEC §4.4.1 pin map (10 tap signals, all ADC1), series-
  resistor taps with no internal pulls, USB-C power, two-mode ADC-continuous capture
  with a WO0 spike, framework bump to `v0.0.12-dev.82` with BLE off in WO1, no
  `WITH_DISPLAY_U8G2` / `WITH_BUTTONS`, bench-ready and scope-characterisation gates,
  exit on the Phase 0 register, interactive hardware WOs.
- DR-1 decision-record skeleton created: [DR-1-design-freeze.md](reviews/DR-1-design-freeze.md).
- Cross-document consistency pass: SPEC verified against the framework headers at
  `v0.0.12-dev.82`; section references updated for the renumbered hardware
  specification (§2.1 bench equipment, §5.5 series resistors, §5.6–§5.8, §6.2
  passive tap board); pin-map references point at SPEC §4.4.1 / §4.4.2; the
  BLE connection budget is 4 connections / 6 activity slots (SPEC §7.8);
  Fitness Machine Status uses 0x08 Target Power Changed (SPEC §5.6);
  `SessionOverlay` is split between PR-06 (session API, download, history tab)
  and PR-09 (`/overlay` page, WebSocket).
- Next: after the Chief Architect reviews and commits the remediation, **PR-01 is GO**.

### 2026-10-04: PR-01 progress review (WO0–WO4a)
- **Done:** WO0 (ADC spike), WO1 (scaffold; framework at `v0.0.12-dev.82`),
  WO2 (capture engine), WO3 (dashboard, `echenon.local`), WO4a
  (`EncoderModule` via PCNT). WO4 is partial: items B–E are outstanding (rows #1, #2, #11).
  Work sits on the off-stack branch `fix/adc-gpio-conflict`; the PA resume
  bridge renames it.
- **Owner decisions recorded:** knob decoded by `EncoderModule` (PCNT);
  cadence tap 4.7 kΩ + 100 nF; encoder colours Black = A, Red = B,
  Orange = VCC; 256-level internal resistance scale. See the
  [DR-1 record](reviews/DR-1-design-freeze.md) §1.2.
- **Design-changing findings:**
  - The pot is referenced to about 1.67 V on Green, with Yellow at 0 V. This
    is the reverse of the SPEC, and the provisional standalone network must be
    redesigned at DR-1.
  - The stock console misses 28% of knob clicks; PCNT misses none.
  - The pot is linear in angle across the console's range, which simplifies
    S-01 for console-driven modes.
- **New DR-1 decisions:** §2.10 interception architecture, including an
  **encoder-only MITM** (the audit's H-05 option c), which could deliver ERG
  without the motor split; §2.11 resistance-scale mappings. See PROGRAM-PLAN
  "Candidate restructure".
- **Logger defects:** the burst export mislabels channels, motor events
  chatter, timestamps are per DMA read, encoder events are not logged, and
  the ride log wraps at 30 minutes. **WO4d must fix these before WO5.**
  SignalLogger moves behind `WITH_SIGNAL_LOGGER` after PR-01, because it
  uses about 7.5 MB of PSRAM.
- **Process:** interactive WOs may declare an exploration boundary;
  findings go to the Chief Architect or DR-1 as proposals. The bench
  wrap-up is to be committed into the PR-01 folder.
- Review: [2026-10-04 PR-01 progress review](reviews/2026-10-04-pr01-progress-review.md).

### 2026-10-11: PR-01 merged; DR-1 prep and PR-03 planned
- **PR-01 merged** as PR #1 (`5897f6f`, merge commit) with no review before the
  merge; the pre-merge review checkpoint was skipped. A post-hoc
  evidence-level review found nothing blocking. Branches are deleted and the
  records closed in the [PR-01 wrap-up](pr-01-signal-logger/PROJECT.md).
- **Register:** 11 of 13 rows PASS; #10 Open (expected DR-1 waiver); #12
  INCONCLUSIVE (moves to PR-03). Key evidence:
  - flat ±3.25 V DC motor drive, no PWM;
  - motor current ≤ 175 mA;
  - VIN tolerates 9 V;
  - no power-on calibration sweep;
  - the console tolerates a disconnected motor;
  - two working pot ramps per revolution;
  - Green is a 1.66 V / 2.4 kΩ reference;
  - 4 detents per level.
- **Open before DR-1:** "28% missed clicks" vs 4 detents per level is
  unreconciled, and it underpins the encoder-MITM case (§2.10). Row #1's
  single-ended sign is ambiguous. The tap cross-check and console
  fault-tolerance tests are not done. All of these go to
  [`pr-dr1-prep`](pr-dr1-prep/PROJECT.md) (one interactive bench session; owner
  approved the prep).
- **PR-03** decomposed into 7 WOs. Proposed change: both entry gates become
  in-PR go/no-go WOs. Awaiting owner approval. Framework latest tag
  `v0.0.12-dev.93` includes the WO-8 fix; #226 is closed.
- **DR-1** to be run jointly with the owner after the prep session. PR-02 waits
  on DR-1.

### 2026-10-12: DR-1 prep complete; DR-1 started
- **PR #2** (DR-1 prep evidence) merged at `5168b1e`, **again with no pre-merge
  review**. Two PRs in a row have skipped the agreed checkpoint. Proposal:
  evidence/docs-only PRs may be merged by the owner; firmware PRs must get a
  Chief Architect review before merge (consider GitHub branch protection).
- **Findings:**
  - The "28% missed clicks" case is withdrawn (4 detents per level by design).
  - No motor-sense clamps are needed.
  - The cadence tap is passive.
  - The console is tolerant of encoder faults.
  - **New stock failure mode:** knob-triggered runaway into continuous motor
    rotation.
- **DR-1 draft (§2A):**
  - Staged architecture: **v1 = encoder MITM** (rate-limited knob, ERG via
    click injection, NC bypass); **v1.1 = motor MITM** (PR-07, standalone UC6).
  - ESP32 powered from VIN (drop the external buck).
  - No R3/R4/R5 on v1 boards.
  - Proposed pin outputs.
  - Waive row #10; move row #12 to PR-03.
  - Programme impact: new PR-04b (encoder relay); PR-08 re-based off PR-07;
    PR-07 → v1.1.
- **Pending owner decisions:** DR-1 §2A (joint session); PR-03 decomposition
  and gate change (proposed 2026-10-11, not yet approved).

### 2026-10-12: Merge protocol (owner decision)
- PR #1 and PR #2 were merged by the owner personally.
- **From now on:** when a PR is submitted, the PA stops. The owner brings the
  PR to a Chief Architect review session, the findings are applied, and **the
  owner merges**. PAs and workers never merge. This applies to every PR,
  including docs and evidence PRs.

### 2026-10-12: Programme restructured into v1 Passive / v2 Active releases (owner decision)
- **Owner decision** (in chat, 2026-10-12): the programme ships as two explicit
  releases. This **supersedes** the DR-1 §2A draft "v1 = encoder MITM, v1.1 =
  motor MITM" and the PROGRAM-PLAN "Candidate Restructure"; their content is
  carried as draft input into [DR-2 §2A](reviews/DR-2-design-freeze.md).
- **v1 "Passive"** (UC1–UC3): today's tap circuitry wired permanently; BLE only
  (FTMS Indoor Bike Data **read-only**, HR re-broadcast); session recording and
  Strava upload. **No resistance control of any kind**: no Control Point
  targets, no encoder or motor interception. ESP32 on the bike's 9 V via the
  Waveshare **VIN** pin (6–21 V, row #11) with a **power switch on the VIN line**
  for USB-C use; the MP1584EN buck is dropped. Back-feed / power-path behaviour
  with USB and VIN both connected is an open item for DR-1 / PR-04a.
  PRs: PR-01 ✅, DR-1 (re-scoped to the v1 passive freeze), PR-02, PR-03,
  PR-04a, PR-05 (read-only; Target Setting Features = 0; Control Point omitted
  if FTMS v1.0 allows), PR-06, PR-09 (v1 integration and release).
- **v2 "Active"** (provisional; UC4–UC6 + portal-native HR-zone
  auto-resistance): mode 1 relay/active (signals in and re-emitted; H-bridge +
  PWM motor drive; encoder emulation toward the console; enables Control
  Point targets and HR-zone control) and mode 2 console-off (pot-reference
  replica: Green 1.66 V / 2.4 kΩ, Yellow ground). PRs: DR-2 prep, DR-2, PR-07,
  **new** PR-10 (encoder emulation + relay mode), PR-08 (now after PR-10 and
  PR-05), **new** PR-11 (HR-zone), **new** PR-12 (console-off standalone),
  **new** PR-13 (v2 integration and release). PR-07 depends on DR-2 and the v1
  release.
- **DR-1 moves to DR-2:** decisions 2.2, 2.4, 2.5, 2.10, and the v2 parts of
  2.3 (motor driver) and 2.7 (active outputs). DR-1 keeps 2.1, 2.3 (VIN),
  2.6, 2.7 (taps only), 2.8, 2.9 (none), 2.11 (report only).
- **Known v1 limitation:** the stock runaway (A09-20261012 #3) cannot be
  prevented by a passive board.
- **ANT+ not supported** (the ESP32-S3 has no ANT radio). A future nRF51422
  add-on over UART is possible: **UART0, GPIO 43 (TX, D1) / GPIO 44 (RX, D0),
  is reserved** on every board and pin map. Console and logging use USB-CDC.
- Docs updated (uncommitted, for Chief Architect review with the owner):
  PROGRAM-PLAN, this file, DR-1, new DR-2 skeleton, SPEC, implementation
  strategy, hardware spec, use-case analysis, PR mandates (PR-03 header tag
  only), new stubs `pr-10`…`pr-13` and `pr-dr2-prep`.
- **Open for the owner:** FTMS v1.0 conditional requirements (Control Point,
  `0x2AD6`, `0x2AD8` without target settings) to be confirmed in PR-05 WO1;
  PR-09 now depends on PR-04a, PR-05 and PR-06; DR-2 prep timing.

### 2026-10-12: Release-structure decisions (owner)
Decisions on the v1 Passive / v2 Active restructure:
1. **v1 FTMS is read-only.** Fitness Machine Feature + Indoor Bike Data only.
   No Control Point, Fitness Machine Status, 0x2AD6 or 0x2AD8. PR-05 WO1
   confirms the FTMS v1.0 conditional rules with a citation and checks
   Zwift / TrainerRoad. Fallback: a minimal Control Point that accepts
   Request Control and refuses every target.
2. **PR-09 dependencies** are agreed (now PR-04a, PR-05, PR-14).
3. **PR-06** depends only on PR-02 (agreed).
4. **DR-2 prep** may overlap late v1 work, **on the fly-wire / Y-harness rig
   only**, never on the installed v1 board.
5. **Versions:** v1 = `1.0.0` (`v1.0.0-beta.N` → `v1.0.0`); v2 = `2.0.0`.
   Development tags stay `v0.x.y-dev.N` until the first beta. The owner
   creates beta and stable tags.
6. **v1 keeps monitoring and diagnostics:**
   - The Green reference tap on GPIO 3 is kept as a monitor.
   - Stock-runaway **detection and alert** goes in PR-02.
   - New **PR-14, Diagnostics & Run-Data Persistence** (after PR-06, before
     PR-09):
     - a lightweight production diagnostics module;
     - an **option to save run data to flash, default off**, capped within
       LittleFS;
     - a reworked **query interface**: a list of runs, per-run download,
       and time-windowed / paged queries instead of whole-buffer dumps.
   - The full burst logger stays in the `-diag` env.
   - This refines the L-08 rule: production carries a *bounded* diagnostics
     module, not the full logger.

### 2026-10-12: DR-1 approved with conditions
- The owner approved the v1 Passive design freeze (DR-1 §4).
- **Conditions:**
  - VIN **or** USB power, never both (PR-04a WO1 checks the power path and
    designs the selector);
  - a recalibration UI in PR-02;
  - all 10 taps kept;
  - Peloton 0–100 mapped by power equivalence.
- **Unblocks PR-02 and PR-04a.** Open item: the PR-02 power-data source
  (owner to choose).
- DR-1 outputs (SPEC and hardware-spec text, PR-02 / PR-04a mandates) are in
  progress.

### 2026-10-12: DR-1 outputs drafted; power-data source decided (owner)
- DR-1 §3 outputs written (uncommitted, for Chief Architect review with the
  owner): SPEC §4–§6, §12.3, §13.3 (and the v1 rows of §13.1, §13.4, §13.6);
  hardware spec §3 and §6.2 (build spec); PR-02 and PR-04a mandates. v2
  `[DR-1]` markers relabelled `[DR-2]`.
- **Owner decision, PR-02 power-data source:** primary = Echelon app power
  entered through the recalibration UI over a level × cadence grid; community
  tables as alternative / seed (database-right caveat); reference power meter
  and BLE sniffing rejected. Echenon's power matches the Echelon app's model
  and is not independently accurate.
- Gates closed: scope characterisation, Phase 0 register, DR-1; R3/R4/R5
  decided (none in v1).
