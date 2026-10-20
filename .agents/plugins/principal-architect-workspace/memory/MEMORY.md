# Principal Architect Memory
<!-- Last updated: 2026-10-16T23:06:00+01:00 -->

## Active Programs
| Program | Plan | Status |
|---|---|---|
| Echenon Firmware | [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) · [PROGRAM.md](../PROGRAM.md) | **Two releases (2026-10-12):** v1 Passive (PR-01 ✅, DR-1, PR-02–PR-06, PR-09) and v2 Active (provisional: DR-2 prep, DR-2, PR-07, PR-10, PR-08, PR-11–PR-13). DR-1 prep done (PR #2); **DR-1 in progress** (re-scoped to v1, §2A joint decision pending); PR-03 planned (awaiting approval). |

## Active Projects
| Project | Status | Modules | Context |
|---|---|---|---|
| PR-03 HR Relay (Phase 2) | **ACTIVE** — WO1–WO4 ✅, WO5 ⚠️ PARTIAL (2/11 pass, framework F1 fix landed at `1e90ec68`, re-run pending). WO3e–WO7 pending | BLE (HRM: Repeater + Client + Parsing), BLEStack (framework fix merged), Build System, Test Tooling | [PROJECT.md](../pr-03-hr-relay/PROJECT.md) |

## Recently Completed (last 5)
| Project | Completed | Modules | Key Outcome |
|---|---|---|---|
| [DR-1 Prep: evidence addendum](../pr-dr1-prep/PROJECT.md) | 2026-10-12 (PR #2, `5168b1e`) | Sensor Acquisition, Encoder (PCNT) | 28% "missed clicks" withdrawn; stock runaway failure mode found; console tolerant of encoder faults |
| [PR-01 Signal Logger (Phase 0)](../pr-01-signal-logger/PROJECT.md) | 2026-10-11 (PR #1, `5897f6f`) | Build System / Hardware, Sensor Acquisition, Web Portal, Encoder (PCNT) | Phase 0 logger + 11/13 register rows; reversed pot reference; console stock behaviour characterised |

## Older Projects
2 completed projects.
Full project history: [PROJECT_INDEX.md](PROJECT_INDEX.md)
Cross-cutting module index: [MODULE_INDEX.md](MODULE_INDEX.md)

## Notes
- **BLE (2026-10-15):** WiFi coex eliminated as the cause of Pixel connect failures (WO3d). Leading hypothesis: ESP32-S3 ↔ Pixel controller interop (PR-03 WO3e side-swap tests). Android phone reliability is a PR-05 entry gate; "BLE works without WiFi" is a v1 gate.
- **Releases (2026-10-12):** v1 Passive `1.0.0` (PR-01…06, PR-14, PR-09; read-only FTMS; diagnostics with an optional save-to-flash) / v2 Active `2.0.0` (DR-2, PR-07, 10, 08, 11, 12, 13). UART0 (GPIO 43/44) is reserved for a future ANT+ module.
- **Release structure (owner decision 2026-10-12):** **v1 "Passive"** = UC1–UC3,
  permanent tap board, BLE-only (FTMS **read-only** + HR relay), Strava upload,
  **no resistance control of any kind**; ESP32 on 9 V via Waveshare VIN with a
  VIN/USB switch (buck dropped; back-feed check open). **v2 "Active"** =
  UC4–UC6 + portal HR-zone control; mode 1 relay (H-bridge/PWM, encoder
  emulation, Control Point targets) and mode 2 console-off (pot-reference
  replica); gated by [DR-2](../reviews/DR-2-design-freeze.md) and the v1
  release. The encoder-MITM-in-v1 / "v1.1" drafts are superseded. **ANT+ not
  supported**; UART0 GPIO 43/44 reserved on every board for a future nRF51422
  add-on.
- **Merge protocol (2026-10-12):** submit the PR, then STOP. The owner takes it to a Chief Architect review; after that review, the owner merges. Agents never merge.
- 2026-09-25 (superseded 2026-10-12 by the release structure above): Programme was 9 PRs + DR-1 (passive-first). Phase / PR numbering in
  [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) is canonical. DR-1 gates PR-02 and PR-04a
  ([record](../reviews/DR-1-design-freeze.md)); DR-2 gates PR-07. Hardware risk is
  concentrated in v2 (PR-07 onward).
- PR-01 WO1 bumps the framework submodule (`framework/`) to `v0.0.12-dev.82` with
  BLE off. PR-03 turns BLE on; its framework central-role acceptance gate is OPEN.
- Hardware and bench work is always a `complexity: interactive` WO with evidence
  committed under `docs/measurements/`.
- Latest review: [2026-10-04 PR-01 progress review](../reviews/2026-10-04-pr01-progress-review.md) (earlier: [2026-09-25 readiness review](../reviews/2026-09-25-readiness-review.md)).
  Superseded plans live in `archive-*`; treat them as prior art only.
