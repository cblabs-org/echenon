# PR-09: v1 Integration & Release (Phase 7 — v1 Integration / RC) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)). v2 integration is [PR-13](../pr-13-v2-integration/PROJECT.md).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-04a, PR-05 and PR-06 merged
> **Dev Phase:** Phase 7 — v1 Integration / RC
> **Depends on:** PR-04a, PR-05, PR-06 (and through them PR-02, PR-03, DR-1)
> **Spec sections:** [SPEC](../../../../SPEC.md) §8.5 (coexistence matrix), §11.1–§11.5 (session overlay, trust boundaries), **§13 (testing; §13.10 is this PR's plan)**, **Appendix J: User Journeys**
> **Use cases:** [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 7

## Mandate

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

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
