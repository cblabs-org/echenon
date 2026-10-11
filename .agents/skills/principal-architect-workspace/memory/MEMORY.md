# Principal Architect Memory
<!-- Last updated: 2026-10-11T23:00:00+01:00 -->

## Active Programs
| Program | Plan | Status |
|---|---|---|
| Echenon Firmware | [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) · [PROGRAM.md](../PROGRAM.md) | 9 PRs + DR-1. **PR-01 merged (2026-10-11).** Next: DR-1 prep bench, then DR-1; PR-03 planned (awaiting approval). |

## Active Projects
| Project | Status | Modules | Context |
|---|---|---|---|
| DR-1 Prep: Phase 0 evidence addendum | WO1 ready (interactive, run with the owner) | Sensor Acquisition, Encoder (PCNT), Hardware | [PROJECT.md](../pr-dr1-prep/PROJECT.md) |
| PR-03 HR Relay (Phase 2) | Planned: 7 WOs, awaiting approval | BLE (HRM), Build System | [PROJECT.md](../pr-03-hr-relay/PROJECT.md) |

## Recently Completed (last 5)
| Project | Completed | Modules | Key Outcome |
|---|---|---|---|
| [PR-01 Signal Logger (Phase 0)](../pr-01-signal-logger/PROJECT.md) | 2026-10-11 (PR #1, `5897f6f`) | Build System / Hardware, Sensor Acquisition, Web Portal, Encoder (PCNT) | Phase 0 logger + 11/13 register rows; reversed pot reference; console stock behaviour characterised |

## Older Projects
1 completed project.
Full project history: [PROJECT_INDEX.md](PROJECT_INDEX.md)
Cross-cutting module index: [MODULE_INDEX.md](MODULE_INDEX.md)

## Notes
- 2026-09-25: Programme is 9 PRs + DR-1 (passive-first). Phase / PR numbering in
  [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) is canonical. DR-1 gates PR-02, PR-04a and
  PR-07 ([record](../reviews/DR-1-design-freeze.md)). Hardware risk is concentrated
  in PR-07 (MITM board).
- PR-01 WO1 bumps the framework submodule (`framework/`) to `v0.0.12-dev.82` with
  BLE off. PR-03 turns BLE on; its framework central-role acceptance gate is OPEN.
- Hardware and bench work is always a `complexity: interactive` WO with evidence
  committed under `docs/measurements/`.
- Latest review: [2026-10-04 PR-01 progress review](../reviews/2026-10-04-pr01-progress-review.md) (earlier: [2026-09-25 readiness review](../reviews/2026-09-25-readiness-review.md)).
  Superseded plans live in `archive-*`; treat them as prior art only.
