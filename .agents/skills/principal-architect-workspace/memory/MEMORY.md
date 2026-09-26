# Principal Architect Memory
<!-- Last updated: 2026-09-26T13:37:00+01:00 -->

## Active Programs
| Program | Plan | Status |
|---|---|---|
| Echenon Firmware | [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) · [PROGRAM.md](../PROGRAM.md) | 9 PRs + DR-1. **PR-01 approved, WO0 next.** |

## Active Projects
| Project | Status | Modules | Context |
|---|---|---|---|
| PR-01 Signal Logger (Phase 0) | WO2 done — WO3/6 | Build System / Hardware, Sensor Acquisition, Web Portal | [PROJECT.md](../pr-01-signal-logger/PROJECT.md) |

## Recently Completed (last 5)
| Project | Completed | Modules | Key Outcome |
|---|---|---|---|

## Older Projects
0 completed projects.
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
- Latest review: [2026-09-25 readiness review](../reviews/2026-09-25-readiness-review.md).
  Superseded plans live in `archive-*`; treat them as prior art only.
