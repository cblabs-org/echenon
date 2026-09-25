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

---

## Programme Status

| PR | Phase | Title | Dependencies | Status |
|---|---|---|---|---|
| PR-01 | 0 | Signal Logger (amended) | `main` | ⬜ Not started |
| DR-1 | — | Design Freeze Review | PR-01 evidence | ⬜ Not started |
| PR-02 | 1 | Calibration + Power (+ live WS feed) | PR-01 | ⬜ Not started |
| PR-03 | 2 | BLE Foundation + HR Relay | PR-01, framework ≥ v0.0.12-dev.82 | ⬜ Not started |
| PR-04a | — | Passive Tap Board | PR-02, DR-1 | ⬜ Not started |
| PR-05 | 3 | FTMS Telemetry | PR-02, PR-03 | ⬜ Not started |
| PR-06 | 4 | Session + TCX + Strava | PR-02 | ⬜ Not started |
| PR-07 | 5 | MITM Board + Motor Control + Safety | PR-02, DR-1 | ⬜ Not started |
| PR-08 | 6 | ERG + Control Point | PR-05, PR-07 | ⬜ Not started |
| PR-09 | 7 | Integration, Portal, UC1–UC6 | PR-06, PR-08 | ⬜ Not started |

### Hardware Gates

| Gate | Required Before | Verification |
|---|---|---|
| Oscilloscope: motor voltage | PR-01 motor wire taps | Visual confirmation on scope |
| **DR-1: Design Freeze** | **PR-04a and PR-07 (No Soldering)** | **Chief Architect approves Phase 0 data** |
| R3/R4/R5 Standalone Power | DR-1 (moved from PR-07) | Resolved as part of fail-safe strategy |
| Echelon-app HR spike | PR-03 entry | Tablet displays mock HR via nRF Connect |

### Parallelism

PR-02 and PR-03 can run **in parallel** after PR-01 merges.
PR-04a, PR-05, and PR-06 can run **in parallel** after their respective dependencies.

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
- Framework BLE module (PR #235, tag `v0.0.12-dev.82`) merged and confirmed as PR-03 prereq.
- Key decisions documented and enacted:
  - D2: Adopted the passive-first 9-PR restructure.
  - D3: Motor driver on its own rail (specifics deferred to DR-1).
  - D4: Zwift SIM mode (0x11) slated for v1.1, space left in FTMS design.
  - D5: BYOA (Bring-your-own-app) model adopted for Strava API credentials.
