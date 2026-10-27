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
| v1 | PR-02 | 1 | Calibration + Power (+ live WS feed) | PR-01, **DR-1** | ✅ Merged 2026-10-24 (`8ccccb6`) |
| v1 | PR-03 | 2 | BLE Foundation + HR Relay | PR-01 ✅ | ✅ Merged 2026-10-20 (PR #3, `e930314`) |
| v1 | PR-04a | 3 | Passive Tap Board (VIN + VIN/USB switch, manual OTA, UART0 reserved) | PR-02, DR-1 | ⬜ Deferred to pre-PR-09 (bench stays on USB-C during PR-05/06) |
| v1 | PR-05 | 3 | FTMS Telemetry, **read-only** (Target Setting Features = 0) | PR-02, PR-03 | ✅ Merged 2026-10-26 (PR #6, `a6c7a88`) |
| v1 | PR-06 | 4 | Session + TCX + Strava | PR-02 | ✅ Merged 2026-10-27 (`46837d2`, PR #7) |
| v1 | PR-15 | 4 | Portal Rework & Ride Page ([charter](pr-15-portal-ride/PROJECT.md)) | PR-06 | ✅ Merged 2026-10-27 (`57b352d`, PR #8) |
| v1 | — | — | fix(strava): OAuth callback redirect | — | ✅ Merged 2026-10-27 (`03be4c1`, PR #9) |
| v1 | PR-14 | 4 | Diagnostics & Run-Data Persistence ([stub](pr-14-diagnostics/PROJECT.md)) | PR-06 | ⬜ Not started |
| v1 | PR-09 | 7 | v1 Integration & Release (UC1–UC3) | PR-04a, PR-05, PR-14, PR-15 | ⬜ Not started |
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
| Echelon-app HR spike | PR-03 WO3 go/no-go | Echelon app on the P12 pairs `Echenon-XXXX` and shows HR (register row #12) | ✅ PASSED (PR-03 WO3) |
| **Framework central-role acceptance evidence** | PR-03 WO5 (in-PR, approved 2026-10-12) | RFC §17.2 items 4–9 against a real 0x180D peripheral on a framework tag containing the WO-8 use-after-free fix | ✅ MET (PR-03 WO5, evidence in `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`) |
| **v1 BLE topology on the P12** | PR-03 merge (WO6) | HR strap (central) + P12 (HR peripheral) + portal WebSocket + HTTPS POST, sustained; no HR gaps > 2.5 s, no link drops | ✅ MET (PR-03 WO6, evidence in `docs/measurements/phase2/wo6-coexistence/evidence.md`) |
| **BLE works without WiFi** (v1 requirement) | v1 release (PR-09); fix lands upstream in the framework | BLE advertising, strap reconnect and discovery continue at boot without STA and through a sustained STA outage | OPEN (framework issue draft (iii)) |
| **FTMS client connection reliability (Lenovo Tab P12 + macOS/iOS)** | **PR-05 WO-05.4** | Clean FTMS + HR coexistence and app acceptance on Lenovo Tab P12 (primary target) and macOS/iOS; Pixel 10 Pro Fold excluded per owner decision 2026-10-24 | ✅ SCOPED TO P12 (2026-10-24 owner decision) |
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

### 2026-10-15: WO3d result: WiFi coexistence eliminated; gates split
- **Experiment** (PR-03 WO3d, automated `tools/ble-test` rig, 150 attempts,
  Pixel 10 Pro Fold + nRF Connect):

  | Batch | Pass rate |
  |---|---|
  | A: Echenon, WiFi on | 26% |
  | **B: control peripheral, no WiFi** | **32%** |
  | A′: Echenon, WiFi on | 28% |

  **H-COEX eliminated:** the failure rate doesn't change without WiFi, and A
  and A′ agree, so there was no drift. No established-link drops across all
  150 30-second holds.
- **CA review of the evidence:**
  - About **80% of failures are silent on the ESP32**: the ESP32 never
    registered the phone's CONNECT_IND (A 29/37, B 29/34, A′ 27/36). The rest
    are 0x3e (request heard, early connection events missed).
  - The phone-side "connection complete 0x00" only shows that the phone sent
    the request, so the report's "Pixel establishes the link" claim was
    corrected.
  - The leading hypothesis is now **H-INTEROP**: the ESP32-S3 controller
    misses or rejects the Pixel's connection requests. A short connection
    interval cannot explain missed requests.
- **Next (PR-03 WO3e, side-swap tests with the rig):**
  - X1: Pixel → a non-ESP32 peripheral;
  - X2: the control firmware on a classic ESP32 (different controller);
  - X3: P12 via nRF Connect;
  - X4: other phones;
  - X5: an over-the-air sniffer, only if the evidence points at the S3 (owner
    approval before any purchase).

  An `espressif/esp-idf` issue may follow, as a draft for CA review first.
- **Upstream drafts:**
  - (i) mislabelled coex default: keep;
  - (ii) WiFi power-save policy: **withdrawn**;
  - (iii) BLE requires STA: keep, **high priority** (v1 requirement);
  - (iv) WiFi scan timing / auto-scan: keep, low priority.
- **Gates split:**
  - WO6 proves the **v1 topology on the P12** (PR-03 scope).
  - **Android phone reliability (≥ 95% single attempt, Pixel + one other
    Android) becomes a PR-05 entry gate.**
  - "BLE works without WiFi" is a v1 release gate.
  - PR-03 is **not blocked**: the P12 HR relay works.
- WO4 (HRMClient) approved to proceed (rebased onto WO3d).
- Strategy: [2026-10-14 WO3b test strategy](reviews/2026-10-14-wo3b-ble-test-strategy.md).


### 2026-10-18 → 2026-10-20: PR-03 merged; PR-02 WO plan drafted

- **PR-03 merged** as PR #3 (`e930314`, merge commit) after CA review and
  re-review. All B1–B5 blockers and S1–S9 should-fix items verified.
  - Tests 2.5 (auto-reconnect, ~7 s) and 2.6 (factory reset, ~15 s) PASS.
  - B3 two-subscriber test PASS: P12 + nRF Connect both received HR; nRF
    disconnected; P12 continued uninterrupted. Required framework fix #309.
  - S4 SPEC §8.1 updated: BSL reports `0x00` ("Other") until EventBus routing.
  - Framework bumped to `v0.0.12-beta.4` (includes #301 pair-stops-discovery
    and #311 re-advertise after peripheral connect).
  - Upstream issues filed and closed: #302, #303, #304, #309. Central
    acceptance report approved for filing.
- **Framework issue #309 filed and fixed.** BLE server did not re-advertise
  after first peripheral connection — blocked multi-device topologies (FTMS +
  HRM on separate devices). Fixed in PR #311 (`dbde7cc8`), tagged
  `v0.0.12-beta.4`.
- **PR-02 WO plan drafted** (9 WOs, uncommitted). Automated calibration rig
  approach approved by owner. Three open owner decisions: (1) Peloton 0–100
  mapping, (2) runaway timeout 10 s, (3) plan approval. The plan is in the
  working tree, ready for review.
- **PA plugin bug confirmed:** `pa-principal-architect` subagent type still
  fails (`code_search` not found, #11). `self`-type workaround used throughout.
- **PIO MCP server crash:** `NOISE_PORT_PATTERN` import error after restart;
  required rebuild. Owner authorized direct `pio` CLI as temporary workaround.

### 2026-10-20: PR-02 WO plan approved; Peloton mapping deferred to PR-05

- **Owner approved the 9-WO plan** (WO-02.1 through WO-02.9) for PR-02
  Calibration + Power Engine. Charter locked, status → APPROVED.
- **Owner decisions at approval:**
  1. **Peloton 0–100 mapping deferred to PR-05.** The mapping is consumed by FTMS
     telemetry; PR-02 implements only the Echelon 1–32 ↔ canonical 0–255 mapping.
     WO1 and WO9 scope adjusted. SPEC §5.3 Peloton mapping becomes a PR-05
     deliverable.
  2. **`runaway_timeout_s` = 10 s** confirmed as the NVS default (WO8).
- **GPIO 18 / GPIO 11 availability** remains open for the PA to confirm against
  the Waveshare ESP32-S3-Nano header pinout in WO5.
- **Next:** PA spins up to execute the 9-WO stack, beginning with WO-02.1
  (CadenceSensor + ResistanceSensor + Level Mapping).

### 2026-10-24: PR-02 (`WO-02.1`–`WO-02.9`) complete & Cold-Firewall audited (`PASS`)

- **All 9 Work Orders (`WO-02.1` through `WO-02.9`) completed and verified:**
  - `CadenceSensor` (`GPIO 1` ISR, `30 ms` debounce, `3 s` timeout) and `ResistanceSensor` (`GPIO 4` 16-sample moving average, 32-level calibration table lookup).
  - `CalibrationRig` + `QuadratureGenerator` (`WITH_CALIBRATION_RIG` compile-time gated on `GPIO 11` NPN open-collector cadence synthesis and `GPIO 17/18` Gray-code quadrature synthesis).
  - **Phase 3 characterization suite (`C01`–`C07`) completed in `docs/measurements/phase3/`:**
    - `C01`/`C02`: Proved **8 quadrature state transitions (2 full Gray-code cycles) = 1 Echelon console level** (`2x` console edge decoding) and deterministic homing to Level 1 via clamped over-stepping (`248+` edges `DOWN`).
    - `C03`: Full 32-level bidirectional sweep (`100%` ADB match across 62 transitions), establishing linear wiper transfer function $\text{pot\_adc} = 37.26\,\ell + 120.0$ ($R^2 = 0.9998$, range `170.5–1333.0 counts`) and `3.6 ADC count` mean directional hysteresis.
    - `C04`: Profiled bang-bang motor settle times (`867 ms` at L5, **`2,806 ms` with `90-count` overshoot-and-reversal at L15**, `1,561 ms` at L25), proved `100%` asynchronous mid-seek burst queuing at `25–50 ms/edge`, and isolated the **`75 ms/edge` timer-polling aliasing anomaly** (drops `1 level` per 72-edge burst in `3/3` trials). Documented in [`docs/digital-hardware-emulation-and-console-dynamics.md`](../../../docs/digital-hardware-emulation-and-console-dynamics.md), closing `DR-2` pre-work items **P1** and **P2**.
    - `C06`: Full 90-minute automated 320-point `(level 1..32 × cadence 40..130 RPM)` empirical power sweep (`C06-20261024-power-matrix-32x10.csv` & `PowerTableDefault.h`).
    - `C07`: Multivariable regression & first-principles electromechanical reverse-engineering ([`docs/power-physics-model.md`](../../../docs/power-physics-model.md)). Derived the 4-parameter physical model (`Physics P3r`: $151^\circ$ cam-driven exponential flux linkage $\exp(\beta(1-\cos\theta))$ + induction-machine eddy-current slip brake $\text{cad}^2/(1+(\text{cad}/\text{cad}_{\text{crit}})^2)$ + linear drivetrain friction $c_{\text{fric}}\,\text{cad}$), achieving **$R^2 = 0.999522$ (`3.77 W` RMSE) on raw `pot_adc`** and **$R^2 = 0.999627$ (`3.33 W` RMSE) on continuous level $\tilde{p}$**.
  - `PowerEngine` updated to evaluate `Physics P3r` analytically in Tier 1 (`DEFAULT` / `"ex3-default"`) while preserving bilinear lookup table interpolation (`computePowerTable`) for Tier 2/3 custom tables (`112/112` native unit tests pass; `ex3-waveshare-nano` firmware builds cleanly).
  - `RunawayDetector` (`10 s` default timeout outside `[155, 1345]` ADC window while motor active) and `/ws/echenon/live` 2 Hz telemetry + portal dashboard/calibration UI (`WO-02.4`, `WO-02.8`).
- **Step 6A Cold-Firewall Audit (`pa-auditor`):** **PASS (`APPROVED`)** with `0` blocking findings.
- **Unblocks:** `PR-04a` (Passive Tap Board), `PR-05` (FTMS Telemetry read-only), and `PR-06` (Session + TCX + Strava).

### 2026-10-24 (Session 2): PR-02 merged (`8ccccb6`); PR-05 & PR-06 Unified Metric Architecture & Charters approved

- **PR-02 merged to `main` (`8ccccb6`).**
- **Owner decisions for `PR-05` and `PR-06`:**
  1. **Sequential Execution Order:** Execute `PR-05` (`feat/pr-05-ftms-broadcast`) first, merge to `main`, then execute `PR-06` (`feat/pr-06-session-upload`) cleanly on top to eliminate merge conflicts on `EchenonModule.*`, `src/main.cpp`, `platformio.ini`, and `web/`.
  2. **Unified First-Principles Metric & Conversion Architecture (`docs/power-physics-model.md` §8):**
     - **Layer 0 (Hardware Fundamentals):** Potentiometer wiper ADC $\to$ normalized cam coordinate $\tilde{p} \in [0.0, 1.0]$; crank reed switch $\Delta t \to \text{cad} \ge 0\text{ RPM}$.
     - **Layer 1 (Physical Continuum):** `Physics P3r` mechanical torque $\tau(\tilde{p}, \text{cad})$ and power $P(\tilde{p}, \text{cad})$.
     - **Layer 2 (Forward Resistance Scales):** Echelon `1–32`, FTMS `0–254`, and **Peloton `0–100` ($R_{\text{pel}}(\tilde{p})$)** matched by braking-curve power equivalence at the standard reference cadence $\text{cad}_{\text{ref}} = 80\text{ RPM}$ (making $R_{\text{pel}}(\tilde{p})$ a pure, monotonic, 1-to-1 function of knob position $\tilde{p}$ that works at `0 RPM` and never drifts with cadence).
     - **Layer 3 (Virtual Road Speed $v(P)$):** Computed from mechanical power $P$ via flat-road aerodynamic + rolling resistance equilibrium ($0.245\,v^3 + 3.53\,v = P$, $v$ in $\text{m/s}$, $v_{\text{km/h}} = 3.6\,v$, with $v(0) = 0.0\text{ km/h}$), populating both FTMS `0x2AD2` `Instantaneous Speed` (`0.01 km/h` resolution) and `/ws/echenon/live` (`speed_kph`).
     - **Layer 4 (v2 Closed-Loop Inverse Control Laws):** Closed-form analytical inverses (`echelonToNormalized`, `pelotonToNormalized`, `targetPowerToNormalized`) shipped and unit-tested in `PR-05` (`WO-05.1`) ready for `PR-08` / `PR-11`.
  3. **FTMS v1.0 Read-Only Conformance (`PR-05`):** Expose `0x2ACC` (`Fitness Machine Feature = 0x00004482`, `Target Setting Features = 0x00000000`) and `0x2AD2` (`Indoor Bike Data`, 11-byte notification at 2 Hz with `Flags = 0x0264`). Exclude `0x2AD6`, `0x2AD8`, `0x2ADA`; omit `0x2AD9` by default with compile-time `#if FTMS_STUB_CONTROL_POINT` fallback ready if any client app requires `Request Control`.
  4. **Target BLE Device (`PR-05`):** Scope `PR-05` client testing and coexistence acceptance to **Lenovo Tab P12** (plus macOS/iOS); skip flaky Pixel 10 Pro Fold.
  5. **Session Auto-Start & Manual UI Control (`PR-06`):** Default auto-start threshold set to **`5 seconds`** of continuous `cadence > 0 RPM` (`autoStartDelaySec = 5`, configurable `1–60 s`) plus a new **`autoStartEnabled` (`bool`, default `true`)** config toggle so the rider can switch off auto-start and start/end sessions strictly from the portal UI.
  6. **Framework Submodule:** Synced to `v0.0.12-beta.4` (`337bd4b1a9a6c727688a2526a0e766bc9b9825e4`).
- **Charters & Work-Order Decompositions locked:**
  - `PR-05`: `WO-05.1` (`wo1-ftms-metrics`), `WO-05.2` (`wo2-ftms-server`), `WO-05.3` (`wo3-module-wiring`), `WO-05.4` (`wo4-hw-verification`).
  - `PR-06`: `WO-06.1` (`wo1-session-ridestore`), `WO-06.2` (`wo2-tcx-generator`), `WO-06.3` (`wo3-upload-queue`), `WO-06.4` (`wo4-strava-uploader`), `WO-06.5` (`wo5-portal-wiring`), `WO-06.6` (`wo6-hw-verification`).

### 2026-10-26: PR-05 Merged (`a6c7a88`, PR #6); Fundamental-First Pipeline Captured & PR-06 Charter Critically Reviewed

- **PR-05 (`FTMS Telemetry, read-only`) completed, audited (`Round 4 PASS`), and merged to `main` (`a6c7a88`, `v0.3.0-dev.23`, `167/167` native unit tests).**
- **Key Architectural & Hardware Learnings from `PR-05` (`WO-05.4`) Captured in `docs/power-physics-model.md` and `SPEC.md`:**
  1. **Direct Float-Wiper Fundamental $\to \tilde{p}$ Pipeline:** `ResistanceSensor::getFilteredWiperFloat()` preserves the unrounded 20 Hz EMA float wiper $w$, which maps directly to $\tilde{p} = \text{clamp}\!\big((w - \text{pot}_{\min}) / (\text{pot}_{\max} - \text{pot}_{\min}),\ 0,\ 1\big)$ without passing through a discrete 32-level lookup table in the forward path.
  2. **OLS Linear Endpoint Regression (`CalibrationWizard::fitLinearEndpoints`):** Passive calibration fits $(\text{pot}_{\min}, \text{pot}_{\max})$ via Ordinary Least Squares (OLS) linear regression $\text{pot}(\ell) = a + b \cdot (\ell - 1)$ across all recorded levels ($\ge 2$, defaulting to C07 bounds $[171.0, 1333.0]$), preventing single-level motor settling noise from injecting kinks into $\tilde{p}$.
  3. **Parametric Physics Recalibration (`RecalibrationUI::applyToPhysicsParams`):** Least-squares regression fits the multiplicative eddy-current gain $A$ (`params.a_scale`, clamped to $[0.25\,A_0, 4.0\,A_0]$) directly on the smooth `Physics P3r` surface (`EchenonModule::syncPhysicsCalibration()`).
  4. **Two-Layer Cadence Reed-Switch Conditioning (`CadenceSensor`):** Hardware ESP32-S3 IO-MUX pin glitch filter + leading-edge (`LOW`) ISR timestamp commit (`250 ms` minimum period = `240 RPM` ceiling) + `50 ms` (`REARM_HIGH_US`) continuous-HIGH open-release hysteresis to reject reed-switch break-bounce, plus wraparound-safe signed `int32_t` `millis()` timeout latching (`timedOut_`).
  5. **Lenovo Tab P12 Coexistence & App Acceptance:** `FTMSServer` (`0x1826`, `0x2ACC`, `0x2AD2` at 2 Hz) + `HRMRepeater` (`0x180D`) + `HRMClient` + `/ws/echenon/live` + `POST /api/config` verified over a 5-minute coexistence soak (`600` FTMS notifications, `max_gap_ms = 516.7 ms`, `0` drops, `free_heap_internal = 63,568 B`).
- **PR-06 Charter & Work Orders (`WO-06.1`–`WO-06.6`) Critically Reviewed & Adjusted:**
  1. **Fundamental-First `RideRecord` & `2 Hz -> 1 Hz` Anti-Aliased Sampling (`WO-06.1`):** `SessionManager::tick()` samples `(power, cadence, pTilde/internal_resistance, HR)` at `2 Hz` (`500 ms` intervals, in phase with `PowerEngine` and `FTMSServer`) and averages each pair of `500 ms` samples into the 1 Hz 8-byte `RideRecord` (`SPEC.md` §9.3–§9.4). `RideRecord` stores `uint8_t internal_resistance` (`0..254 = round(254 * pTilde)`, `255 = UNCALIBRATED`), preserving $8\times$ finer resolution of $\tilde{p}$ than discrete `1..32` levels.
  2. **Wraparound-Safe Signed `int32_t` `millis()` Timing & Mid-Ride NTP Back-Computation (`WO-06.1`):** All session timers (`500 ms` sample tick, `5 s` auto-start, `180 s` inactivity, `30 s` checkpoint flush) use `(int32_t)(now_ms - last_ms) >= threshold_ms`, and `start_epoch_s` is dynamically back-computed (`now_epoch - elapsed_s`) if NTP syncs mid-ride (`SPEC.md` §9.2).
  3. **Virtual Road Speed $v(P)$ & Distance Integration + Xert MPA 1 Hz Continuity (`WO-06.2`):** `TCXGenerator` integrates `PowerEngine::computeVirtualSpeedKph(rec.power_watts) / 3.6f` at 1 Hz to emit `<DistanceMeters>` and `<ns3:Speed>` alongside `<ns3:Watts>`. Coasting (`cadence == 0` while `ACTIVE`) always emits 1 Hz `<ns3:Watts>0</ns3:Watts>` trackpoints with no timestamp gaps so Xert MPA differential equations are never corrupted.
  4. **Pure Capability `IUploader` & Transport-Owning `UploadQueue` (`WO-06.3`, `WO-06.4`):** Aligned `IUploader` with `SPEC.md` §10.2 (`buildRequest` / `handleResponse` capability contract with `IHttpTransport` owned by `UploadQueue`), added `Redactor::registerSecret()` return-value check (`SPEC.md` §10.4), and added resumable deadline awareness in `STAGE_GENERATE_TCX_GZ`.
  5. **Portal Tab Ordering & Post-Ride Discard Endpoint (`WO-06.5`):** Assigned non-colliding tab orders (`Session = 80`, `Upload Services = 90`, after `Calibration = 60` and `Recalibration = 70`), added `POST /api/session/discard` (`SPEC.md` §11.3/§11.5), and supported `/api/session/stop` as an alias for `/api/session/end`.

### 2026-10-26 (Session 2): PR-06 Audit Approved & Pushed; Portal UX Audit; PR-15 Chartered

- **PR-06 Remediation Complete & Pushed:**
  - WO-06.7 (`05a6ca6`): `StreamingGzipEncoder` moved to PSRAM heap; EventBus payload safety.
  - WO-06.8 (`a497649`): File headers, `std::function` GRAVE-05 comments, CSS design tokens.
  - Tagged `v0.3.0-dev.24`, flashed and verified on bench (PSRAM 98.6% free, heap 111,903 B).
  - All 8/8 WOs `COMPLETED`. Pushed to origin (`b74b354..aecdc97`). PR #7 ready to merge.
- **Portal UX Audit (Live on `192.168.1.100`):**
  - Catalogued 9 bottom-bar tabs + 5 sub-tabs via `/api/tabs` — 9 icons unusable on phone.
  - Owner decisions: two-surface model (standalone `/ride` page for daily use, portal for config only), drop `/overlay` (OBS irrelevant), 6-tab portal (WiFi/Bluetooth/Echenon/Setup/Security/System), Setup tab with pills (Apps/Level Mapping/Power Tuning), `app.disableDefaultAppTab()`, strip signal logger from production.
  - Ride page: standalone `/ride` HTML, PWA manifest, dark theme, responsive 280–430px, designed for Android split-screen with Echelon app on Lenovo P12 and iPad Slide Over.
- **PR-15 Chartered:** Portal Rework & Ride Page (5 WOs: Portal Cleanup, Setup Tab, Ride Page, SPEC Updates, HW Verification). Inserted before PR-09 in programme table. PR-09 now depends on PR-15.
- **SPEC.md Update:** 12 sections identified for `/overlay` → `/ride` rename and §11 major rewrite.

### 2026-10-27: CA Session 2 — PR-06, PR-15, fix/strava merged; PR-14 chartered

- **Programme table updated:** PR-06 → ✅ Merged (`46837d2`, PR #7); PR-15 → ✅ Merged
  (`57b352d`, PR #8); fix(strava) → ✅ Merged (`03be4c1`, PR #9). `main` is at `03be4c1`.
- **All v1 dependencies for PR-14 are met.** PR-06 (RideStore, SessionManager, RideRecord,
  IRideFileSystem) and PR-02 (RunawayDetector) are on `main`.
- **PR-14 Charter written and locked** (7 WOs):
  - WO-14.1: DiagnosticsModule + Bounded Event Log (standard)
  - WO-14.2: Run-Data Persistence — LittleFS + Gzip (standard)
  - WO-14.3: Query REST API — streamed, paged, authenticated (standard)
  - WO-14.4: Portal Integration + Runaway Alert Surfacing (standard)
  - WO-14.5: Native Tests (≥ 25 new RUN_TEST cases) (standard)
  - WO-14.6: SPEC + Doc Sync (mechanical)
  - WO-14.7: Hardware Verification — 60-min ride with persistence (interactive)
- **Owner decisions at approval:**
  - D1: New `DiagnosticsModule` (not a SignalLogger refactor) — approved.
  - D2: Binary `.ride.gz` persistence with CSV-on-download — approved.
  - D3: 512 KB default cap for run-data on flash — approved.
  - D4: Event log scope: runaway alerts, calibration, boot, session start/end,
    firmware version changes — approved.
- **Next:** Tier 2 PA to decompose WOs into detailed specs, then execution.

