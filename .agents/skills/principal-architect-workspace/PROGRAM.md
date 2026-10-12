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

---

## Programme Status

Phase and PR numbering in this table is canonical; other documents follow it.

| PR | Phase | Title | Dependencies | Status |
|---|---|---|---|---|
| PR-01 | 0 | Signal Logger (Observe) | `main` | ✅ Merged 2026-10-11 (PR #1, `5897f6f`) |
| DR-1 | — | Design Freeze Review ([record](reviews/DR-1-design-freeze.md)) | PR-01 evidence + [DR-1 prep bench](pr-dr1-prep/PROJECT.md) | 🟡 Evidence complete (PR #2); [§2A recommendations](reviews/DR-1-design-freeze.md) drafted, awaiting a joint decision |
| PR-02 | 1 | Calibration + Power (+ live WS feed) | PR-01, **DR-1** | ⬜ Not started |
| PR-03 | 2 | BLE Foundation + HR Relay | PR-01 ✅; gates proposed as in-PR go/no-go (WO3, WO5) | 📝 Planned: [7 WOs](pr-03-hr-relay/PROJECT.md), awaiting approval |
| PR-04a | 3 | Passive Tap Board (+ manual OTA) | PR-02, DR-1 | ⬜ Not started |
| PR-05 | 3 | FTMS Telemetry (+ Control Point access layer) | PR-02, PR-03 | ⬜ Not started |
| PR-06 | 4 | Session + TCX + Strava | PR-02 | ⬜ Not started |
| PR-07 | 5 | MITM Board + Motor Control + Safety | PR-02, DR-1 | ⬜ Not started |
| PR-08 | 6 | ERG + Control Point targets | PR-05, PR-07 | ⬜ Not started |
| PR-09 | 7 | Integration / RC, Portal, UC1–UC6 | PR-06, PR-08 | ⬜ Not started |

### Gates

| Gate | Required Before | Verification | Status |
|---|---|---|---|
| **Bench ready**: ≥ 2-channel scope with single-shot trigger; motor-current measurement (shunt + scope or DC clamp); JST-SM 9-pin and JST 3-pin Y / back-probe harness; series tap resistors | PR-01's first WO that connects to the bike | Owner confirms kit on hand; recorded in PR-01 Session Log | PARTIAL: bench in use since 2026-10-03; scope and current measurement still to be confirmed (WO4 items C and D) |
| **Scope characterisation** of both motor wires (both directions, at rest; 10 ms/div and µs/div) | Connecting PR-01 motor-sense taps (GPIO 6/7) | Captures committed as Phase 0 register row #1 evidence. Either result lets logging proceed (÷4 divider safe to ~13 V); result feeds DR-1 | OPEN: taps were connected before the captures (safe through the divider); captures still owed |
| **Phase 0 register complete** ([`docs/measurements/phase0/README.md`](../../../docs/measurements/phase0/README.md)) | PR-01 exit / DR-1 start | Every row has an Evidence file and a Result | OPEN |
| **DR-1: Design Freeze** | **PR-02, PR-04a, PR-07** (no soldering or permanent install before approval) | Chief Architect approves the [DR-1 record](reviews/DR-1-design-freeze.md) | OPEN |
| R3/R4/R5 standalone supplies | DR-1 | Decided as part of the switchable-supply and fail-safe strategy | OPEN |
| Echelon-app HR spike | PR-03 (proposed: WO3 go/no-go) | Row #12 INCONCLUSIVE with the nRF mock (Android could not discover it); retest with an ESP32 HR peripheral | OPEN |
| **Framework central-role acceptance evidence** | PR-03 entry | RFC §17.2 items 4–9 against a real 0x180D peripheral on a framework tag containing the WO-8 use-after-free fix | **OPEN** |
| Framework #226 (OTA check-on-boot deferral) | Removing Echenon's `autoCheck=false` enforcement | Upstream issue **closed**. PR-04a uses the framework option after the PR-03 WO1 bump (verify) | CLOSED upstream |

### Parallelism

- PR-03 can start once PR-01 merges and its entry gates close; it runs **in
  parallel** with DR-1 and PR-02.
- PR-02 and PR-03 share `EchenonModule.*`, `platformio.ini` and `main.cpp`. The PR
  that merges second rebases onto `main` and resolves the conflict.
- PR-04a, PR-05 and PR-06 can run **in parallel** once their own dependencies merge.

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
