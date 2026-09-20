# Echenon Firmware — Programme

> **Programme:** Build the complete Echenon firmware — an ESP32-based FTMS bridge
> for the Echelon EX-3 spin bike, using the ESP32 App Foundation framework.
>
> **Chief Architect tier.** This document tracks the macro-level PR dependency
> graph. Each PR is delegated to a Tier 2 Principal Architect who decomposes it
> into Work Orders.
>
> **Specification:** [`SPEC.md`](../../SPEC.md)
> **Implementation Strategy:** [`docs/implementation-strategy.md`](../../docs/implementation-strategy.md)
> **Use Case Analysis:** [`docs/use_case_analysis.md`](../../docs/use_case_analysis.md)
> **Hardware Specification:** [`docs/hardware_specification.md`](../../docs/hardware_specification.md)
> **Upstream RFC:** [BLEManager — issue #225](https://github.com/matthewmcneill/esp32-app-foundation/issues/225)

---

## Programme Status

| # | PR | Phase | Status | PA | Depends On |
|---|---|---|---|---|---|
| 1 | [pr-01-signal-logger](pr-01-signal-logger/PROJECT.md) | Phase 0: Observe | ⬜ Not started | — | `main` |
| 2 | [pr-02-calibration-power](pr-02-calibration-power/PROJECT.md) | Phase 1: Calibrate | ⬜ Not started | — | PR-01 |
| 3 | [pr-03-hr-relay](pr-03-hr-relay/PROJECT.md) | Phase 2: HR Relay | ⬜ Not started | — | PR-01 |
| 4 | [pr-04-intercept-board](pr-04-intercept-board/PROJECT.md) | Phase 3: MITM | ⬜ Not started | — | PR-02 |
| 5 | [pr-05-ftms-broadcast](pr-05-ftms-broadcast/PROJECT.md) | Phase 4: FTMS | ⬜ Not started | — | PR-03, PR-04 |
| 6 | [pr-06-erg-strava](pr-06-erg-strava/PROJECT.md) | Phase 5: ERG+Cloud | ⬜ Not started | — | PR-05 |
| 7 | [pr-07-integration-polish](pr-07-integration-polish/PROJECT.md) | Phase 6: Polish | ⬜ Not started | — | PR-06 |

### Hardware Gates

| Gate | Required Before | Verification |
|---|---|---|
| Oscilloscope: motor voltage = 3.3V DC (not 9V PWM) | PR-01 motor wire taps | Visual confirmation on scope |
| Intercept board assembled and installed | PR-04 | Console powers on normally |
| Standalone sensor power (R3/R4/R5) | PR-07 UC6 test | Sensors work without console |

### Parallelism

PR-02 and PR-03 can run **in parallel** after PR-01 merges. All other PRs are
serial.

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
