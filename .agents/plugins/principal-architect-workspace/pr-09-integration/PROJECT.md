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
github_pr: 20
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
  status: COMPLETED
  attempts: 1
  commit_sha: dd51431ed0096b88b986fd62d0898c3a6f38d210
- id: WO-09.5
  slug: wo5-beta-hardware-guide
  title: Standalone v1 Passive Tap Board Build Guide & Hardware Spec Errata
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 07b89b5fb9aa30f4da3b8e8acfaa012d70545231
- id: WO-09.6
  slug: wo6-public-repo-docs
  title: LICENSE, Beta Recruitment README, CHANGELOG, CONTRIBUTING & SPEC Sync
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: da55c9c4f6cf987cce17253d9eaab70cb56d099a
- id: WO-09.7
  slug: wo7-public-repo-hygiene
  title: Evidence Redaction (SSID/BSSID/URL) & .gitignore Hardening
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: b2cf0f08b87224eefe6ca4b7433f7ee85ead6255
- id: WO-09.8
  slug: wo8-release-audit-verify
  title: Multi-Env Firmware Builds, Native Tests & Beta Release Audit Report
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: 4ba5c82eb287daff0f9ae6507f2a2a6b7bf54d5e
- id: WO-09.9
  slug: wo9-audit-doc-cleanup
  title: Mask Privacy Grep Literals & Align SignalLogger Routes in Release Audit Doc
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 6d785f89008abbecb0f07d5d27167b49594e9d4b
- id: WO-09.10
  slug: wo10-mcp-server-wiring
  title: Wire Ring-2 MCPServer in Composition Root & Rig Read-Only Hint
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: cff35a568ed9e72f7517f67109a3562c98c90f07
- id: WO-09.11
  slug: wo11-release-audit-measured-sync
  title: Sync Release Audit Report with MCPServer Metrics & Live v0.3.0-dev.49 Bench
    Soak
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 5449b68131bb1bf70ceda578a13bb6ff926dbc60
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
| `WO-09.1` | `wo1-security-mcp-encapsulation` | `IMCPContributor` tools, CalibrationRig HTTP→MCP migration, `AuthManager` guards, calibration persistence fixes, zero `extern Application app;`, `board.h` v1 finalisation | `standard` | `gemini-4-argon-medium` | `COMPLETED` | `450fb751dd97` (`rcpt_c82896c33fdc`) |
| `WO-09.2` | `wo2-house-style-core` | Apache-2.0 headers & Doxygen house style across 32 core/sensor/power/rig files | `mechanical` | `gemini-4-hydrogen-high` | `COMPLETED` | `6b63b9cca95d` (`rcpt_6184086abe12`) |
| `WO-09.3` | `wo3-house-style-ble-sinks` | Apache-2.0 headers, `FTMSServer` `millis()` `#ifdef UNIT_TEST` guard, & `GRAVE-05` comments across 31 BLE/session/sink files | `mechanical` | `gemini-4-hydrogen-high` | `COMPLETED` | `dd51431ed009` (`rcpt_6d2094de781c`) |
| `WO-09.4` | `wo4-portal-design-tokens` | CSS `var(--token, #fallback)` compliance, `/api/echenon/calibration/reset` wiring, `window.apiFetch` consistency, WCAG AA contrast & SVG stroke polish across 12 `web/` files | `standard` | `gemini-4-hydrogen-high` | `COMPLETED` | `07b89b5fb9aa` (`rcpt_e0bca8f15496`) |
| `WO-09.5` | `wo5-beta-hardware-guide` | Standalone v1 Passive Tap Board builder guide (`docs/beta-build-guide.md`) with mechanical `VIN`/`USB-C` interlock warning & `hardware_specification.md` `C1 ×5` errata fix | `standard` | `gemini-4-argon-medium` | `COMPLETED` | `da55c9c4f6cf` (`rcpt_011b654df3ad`) |
| `WO-09.6` | `wo6-public-repo-docs` | `LICENSE`, beta-recruitment `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, & `SPEC.md` beta OTA deferral note | `standard` | `gemini-4-argon-medium` | `COMPLETED` | `f9b2b917e34d` (`rcpt_9d3d085f7d94`) |
| `WO-09.7` | `wo7-public-repo-hygiene` | Redact home WiFi SSID (`<REDACTED_SSID>`), BSSIDs (`XX:XX:XX:*`), & personal Gemini URL across 10 tracked doc/log files + `.gitignore` hardening | `mechanical` | `gemini-4-hydrogen-medium` | `COMPLETED` | `b2cf0f08b872` + `4ba5c82eb287` (`rcpt_86b7b3d4dbba`) |
| `WO-09.8` | `wo8-release-audit-verify` | 10-Dimension Release Audit (`Beta` channel `v0.3.0-beta.1`), `native` test suite, 3-env firmware build matrix, ELF DRAM inspection & audit report | `interactive` | `gemini-4-argon-medium` | `COMPLETED` | `5cc4819fc2f5` (`rcpt_30f66aaa6250`) |
| `WO-09.9` | `wo9-audit-doc-cleanup` | Mask privacy grep literals & align `SignalLogger` routes in `docs/release-audit-v0.3.0-beta.1.md` | `mechanical` | `gemini-4-hydrogen-medium` | `COMPLETED` | `98bbf715ef21` (`rcpt_70001776b36b`) |
| `WO-09.10` | `wo10-mcp-server-wiring` | Wire Ring-2 `MCPServer` in `src/main.cpp`, `ESP_LOGI` in `EchenonModule::registerMCPTools()`, & read-only `-32000` actionable hint in `calibration_sweep.py` | `standard` | `gemini-4-argon-medium` | `COMPLETED` | `3173600c84ab` (`rcpt_ccd8e2eafb52`) |
| `WO-09.11` | `wo11-release-audit-measured-sync` | Sync `docs/release-audit-v0.3.0-beta.1.md` with post-`MCPServer` build/ELF footprint & live `v0.3.0-dev.49` `/api/mcp` + bench soak telemetry | `mechanical` | `gemini-4-hydrogen-medium` | `PENDING` | — |

## 4. Session Log

- **2026-11-13 — Owner Plan & Design Approval:** Human explicitly approved autonomous execution of `PR-09` (`pr-09-integration`) to prepare **Echenon `v0.3.0-beta.1`** (v1 Passive Tap Board):
  - CalibrationRig endpoints moved 100% from HTTP to `IMCPContributor` (`/api/mcp`), and full `EchenonModule` `IMCPContributor` + `AuthManager::checkPortalAuth()` guards enforced on all active `/api/echenon/*` portal endpoints (`WO-09.1`).
  - Dimension 2 encapsulation fixes (`extern Application app;` removal, `FTMSServer` `#ifdef UNIT_TEST`, `static` composition root objects) approved.
  - Hardware power safety uses a mechanical `VIN` / `USB-C` interlock (**one or the other, NEVER both**); OTA (`WITH_OTA`) remains deferred post-beta (USB-C updates in beta).
  - Submodule visibility (`matthewmcneill/esp32-app-foundation`) handled via parallel process and tracked as final owner launch gate before flipping repos public.
  - Per Rule 03 & programme governance, PR merge to `main`, `v0.3.0-beta.1` tagging, and `deploy_release.py` remain strictly owner-gated after Step 6A Cold-Firewall Audit.
- **2026-11-14 — All 8 Work Orders Stamped & Integrated:** `WO-09.1` through `WO-09.8` completed sequentially on stacked branches (`450fb751dd97` → `5cc4819fc2f5`) with `269/269` `native` tests passing (`0.54 s`), all 3 firmware targets building green (`ex3-waveshare-nano`, `-diag`, `-rig`), privacy scrub verified (`0` matches outside `.agents/`), and formal 10-Dimension Beta Release Audit written to `docs/release-audit-v0.3.0-beta.1.md`. Fast-forwarded `feat/pr-09-integration` for Step 6A Cold-Firewall Audit.
- **2026-11-14 — Step 6A Audit Round 1 & Bounded `WO-09.9` Remediation:** Cold-Firewall Audit Round 1 (`93dba6d6`) returned `APPROVED` with 2 non-blocking documentation observations in `docs/release-audit-v0.3.0-beta.1.md` (`NON-BLOCKING-1` privacy grep literal masking and `NON-BLOCKING-2` `SignalLogger` route list alignment). Owner authorized Step 1 cleanup via `WO-09.9` (`98bbf715ef21`, `rcpt_70001776b36b`), followed by `APPROVED` Cold-Firewall Audit Round 2 (`87a410af0ef5`).
- **2026-11-14 — Live `v0.3.0-dev.48` Bench Test & Owner-Approved `WO-09.10`:** Live hardware verification on `/dev/cu.usbmodem101` (`v0.3.0-dev.48`) confirmed 2h 17m uptime (`8,235 s`, zero resets), 13/13 portal routes, BLE HRS/FTMS streaming, and `/api/echenon/rig/* -> 404`, while exposing `POST /api/mcp -> 404` because Ring-2 `MCPServer` was not wired in `src/main.cpp`. Owner explicitly approved `WO-09.10` remediation, live `v0.3.0-dev.49` `/api/mcp` + soak verification, `WO-09.11` measured audit sync, and Audit Round 3.
- **2026-11-14 — `WO-09.10` & Live `v0.3.0-dev.49` Hardware/MCP Gate Verified:** Ring-2 `MCPServer` wired and verified (`3173600c84ab`, `rcpt_ccd8e2eafb52`); `269/269` native tests (`0.551 s`), all 3 ESP32-S3 envs green (`ex3-waveshare-nano`: `58,996 B` RAM / `1,787,295 B` Flash; `mcpServer`: `104 B`), and `82.4 min` (`4,944 s`) live soak on `/dev/cu.usbmodem101` (`v0.3.0-dev.49`) verified zero faults, `14/14` gzipped portal routes (`/mcp.js` `200`), `GET /api/mcp/status` (`27` tools, `readOnly: true`), `POST /api/mcp` `initialize` + `tools/list` + `echenon_get_telemetry` + `-32000` `readOnly` guard, `/api/echenon/rig/* -> 404`, and BLE `Echenon` advertising (`0x180D`, `0x1826`). Syncing `docs/release-audit-v0.3.0-beta.1.md` via `WO-09.11`.

## Status
**Plan Status:** APPROVED
**Execution Status:** IN_PROGRESS (WO-09.11 authored; dispatching worker)
