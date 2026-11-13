---
schema_version: 1
charter:
  pr_id: PR-09
  programme_id: prog-default
  slug: pr-09-integration
  title: v1 Beta Release Readiness, Audit Remediation & Builder Onboarding
  branch: feat/pr-09-integration
  base_branch: main
  depends_on: []
  allowed_modules:
  - src/main.cpp
  - src/echenon/
  - hardware/ex3-waveshare-nano/
  - tools/calibration-rig/
  - web/
  - test/
  - docs/
  - SPEC.md
  - README.md
  - LICENSE
  - CHANGELOG.md
  - CONTRIBUTING.md
  - .gitignore
  required_skills:
  - framework-expert
  - framework-design-system
  - pio-manager
  - hardware-in-the-loop-test
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: dbd0b6df909a1b036049151bc82207ee68bc6aef5bc11aa8cf1f44942073eb25
status: APPROVED
approved_at: '2026-11-13T23:13:38.026649Z'
github_pr: null
work_orders:
- id: WO-09.1
  slug: wo1-security-mcp-encapsulation
  title: MCP Contributor Tools, Portal Auth Guards, Persistence Fixes & Encapsulation
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 854b1dcaf3e3618b3b7d19f87863d9b917ffa821
- id: WO-09.2
  slug: wo2-house-style-core
  title: File Headers & Doxygen House Style — Core, Sensors, Power & Rig
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 450fb751dd979bf0855d16ac1bffb692ca6c567f
- id: WO-09.3
  slug: wo3-house-style-ble-sinks
  title: File Headers, GRAVE-05 Justification Comments & FTMS Guard — BLE & Data Sinks
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 6b63b9cca95d1abe078f69e8307ad1f8f3734b3a
- id: WO-09.4
  slug: wo4-portal-design-tokens
  title: Portal CSS Design Tokens, Reset Wiring, Contrast & Accessibility Polish
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-09.5
  slug: wo5-beta-hardware-guide
  title: Standalone v1 Passive Tap Board Build Guide & Hardware Spec Errata
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-09.6
  slug: wo6-public-repo-docs
  title: LICENSE, Beta Recruitment README, CHANGELOG, CONTRIBUTING & SPEC Sync
  complexity: standard
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-09.7
  slug: wo7-public-repo-hygiene
  title: Evidence Redaction (SSID/BSSID/URL) & .gitignore Hardening
  complexity: mechanical
  status: PENDING
  attempts: 0
  commit_sha: null
- id: WO-09.8
  slug: wo8-release-audit-verify
  title: Multi-Env Firmware Builds, Native Tests & Beta Release Audit Report
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
---

## 2. Scope

_See legacy content below._


# PR-09: v1 Integration & Release (Phase 7 — v1 Integration / RC) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)). v2 integration is [PR-13](../pr-13-v2-integration/PROJECT.md).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-04a, PR-05 and PR-06 merged
> **Dev Phase:** Phase 7 — v1 Integration / RC
> **Depends on:** PR-04a, PR-05, PR-06 (and through them PR-02, PR-03, DR-1)
> **Spec sections:** [SPEC](../../../../SPEC.md) §8.5 (coexistence matrix), §11.1–§11.5 (session overlay, trust boundaries), **§13 (testing; §13.10 is this PR's plan)**, **Appendix J: User Journeys**
> **Use cases:** [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 7

## 1. Mandate

Integration and polish to produce the **v1 release-candidate firmware** on the
installed passive tap board. A beta or stable tag needs the owner (rule 03).

1. **`SessionOverlay`** — standalone `/overlay` page with `/ws/session` WebSocket push
   at 2 Hz (the session API, download and history tab landed in PR-06).
2. **Portal polish** — Final polish on all portal pages.
3. **v1 coexistence test** — The SPEC §8.5 matrix for the v1 configuration.
   Progressive checks already ran in PR-03 and PR-05; this PR owns the v1 one
   (PR-13 repeats it for v2).
4. **Use-case validation** — UC1–UC3 and the v1 journeys in Appendix J: User Journeys,
   run against the §13 test plan, as `complexity: interactive` WOs with evidence
   committed.
5. **Coupling audit** — Architecture matches the SPEC; ADR compliance.
6. **Documentation** — Comprehensive README, including the v1 known limitation
   (stock runaway) and the VIN/USB switch procedure.
7. **Release candidate** — Release-candidate firmware and release notes; the owner
   tags beta / stable.

**Verification:** `pio run` zero errors / warnings; tick budget < 10 ms; free DRAM above
40 KB; native tests pass; UC1–UC3 and the v1 Appendix J journeys pass; coupling audit
passes; full coexistence stress test passes.

## Prior Art

- [wo7-overlay/WORKORDER.md](../archive-pa-decomposition/wo7-overlay/WORKORDER.md)
- [wo10-integration/WORKORDER.md](../archive-pa-decomposition/wo10-integration/WORKORDER.md)
- [Archived PR-07 (v2)](../archive-pr-v2/pr-07-integration-polish/PROJECT.md)

## 3. Work Order Ledger

| WO | Slug | Scope | Complexity | Model | Status | Commit |
|---|---|---|---|---|---|---|
| `WO-09.1` | `wo1-security-mcp-encapsulation` | `IMCPContributor` tools, CalibrationRig HTTP→MCP migration, `AuthManager` guards, calibration persistence fixes, zero `extern Application app;`, `board.h` v1 finalisation | `standard` | `gemini-4-argon-medium` | `PENDING` | — |
| `WO-09.2` | `wo2-house-style-core` | Apache-2.0 headers & Doxygen house style across 32 core/sensor/power/rig files | `mechanical` | `gemini-4-hydrogen-high` | `PENDING` | — |
| `WO-09.3` | `wo3-house-style-ble-sinks` | Apache-2.0 headers, `FTMSServer` `millis()` `#ifdef UNIT_TEST` guard, & `GRAVE-05` comments across 31 BLE/session/sink files | `mechanical` | `gemini-4-hydrogen-high` | `PENDING` | — |
| `WO-09.4` | `wo4-portal-design-tokens` | CSS `var(--token, #fallback)` compliance, `/api/echenon/calibration/reset` wiring, `window.apiFetch` consistency, WCAG AA contrast & SVG stroke polish across 12 `web/` files | `standard` | `gemini-4-hydrogen-high` | `PENDING` | — |
| `WO-09.5` | `wo5-beta-hardware-guide` | Standalone v1 Passive Tap Board builder guide (`docs/beta-build-guide.md`) with mechanical `VIN`/`USB-C` interlock warning & `hardware_specification.md` `C1 ×5` errata fix | `standard` | `gemini-4-argon-medium` | `PENDING` | — |
| `WO-09.6` | `wo6-public-repo-docs` | `LICENSE`, beta-recruitment `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, & `SPEC.md` beta OTA deferral note | `standard` | `gemini-4-argon-medium` | `PENDING` | — |
| `WO-09.7` | `wo7-public-repo-hygiene` | Redact home WiFi SSID (`<REDACTED_SSID>`), BSSIDs (`XX:XX:XX:*`), & personal Gemini URL across 10 tracked doc/log files + `.gitignore` hardening | `mechanical` | `gemini-4-hydrogen-medium` | `PENDING` | — |
| `WO-09.8` | `wo8-release-audit-verify` | 10-Dimension Release Audit (`Beta` channel `v0.3.0-beta.1`), `native` test suite, 3-env firmware build matrix, ELF DRAM inspection & audit report | `interactive` | `gemini-4-argon-medium` | `PENDING` | — |

## 4. Session Log

- **2026-11-13 — Owner Plan & Design Approval:** Human explicitly approved autonomous execution of `PR-09` (`pr-09-integration`) to prepare **Echenon `v0.3.0-beta.1`** (v1 Passive Tap Board):
  - CalibrationRig endpoints moved 100% from HTTP to `IMCPContributor` (`/api/mcp`), and full `EchenonModule` `IMCPContributor` + `AuthManager::checkPortalAuth()` guards enforced on all active `/api/echenon/*` portal endpoints (`WO-09.1`).
  - Dimension 2 encapsulation fixes (`extern Application app;` removal, `FTMSServer` `#ifdef UNIT_TEST`, `static` composition root objects) approved.
  - Hardware power safety uses a mechanical `VIN` / `USB-C` interlock (**one or the other, NEVER both**); OTA (`WITH_OTA`) remains deferred post-beta (USB-C updates in beta).
  - Submodule visibility (`matthewmcneill/esp32-app-foundation`) handled via parallel process and tracked as final owner launch gate before flipping repos public.
  - Per Rule 03 & programme governance, PR merge to `main`, `v0.3.0-beta.1` tagging, and `deploy_release.py` remain strictly owner-gated after Step 6A Cold-Firewall Audit.

## Status
**Plan Status:** APPROVED
