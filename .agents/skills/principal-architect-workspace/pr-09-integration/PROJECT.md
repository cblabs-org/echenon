# PR-09: Integration, Portal & Polish (Phase 7 — Integration / RC)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-06 and PR-08 merged
> **Dev Phase:** Phase 7 — Integration / RC
> **Depends on:** PR-06, PR-08
> **Spec sections:** [SPEC](../../../../SPEC.md) §8.5 (coexistence matrix), §11.1–§11.5 (session overlay, trust boundaries), **§13 (testing; §13.10 is this PR's plan)**, **Appendix J: User Journeys**
> **Use cases:** [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 7

## Mandate

Final polish and integration to produce the release-candidate firmware.

1. **`SessionOverlay`** — standalone `/overlay` page with `/ws/session` WebSocket push
   at 2 Hz (the session API, download and history tab landed in PR-06).
2. **Portal polish** — Final polish on all portal pages.
3. **Final coexistence test** — The full SPEC §8.5 matrix. Progressive checks already
   ran in PR-03 and PR-05; this PR owns the final one.
4. **Use-case validation** — UC1–UC6 and every journey in Appendix J: User Journeys,
   run against the §13 test plan, as `complexity: interactive` WOs with evidence
   committed.
5. **Coupling audit** — Architecture matches the SPEC; ADR compliance.
6. **Documentation** — Comprehensive README.

**Verification:** `pio run` zero errors / warnings; tick budget < 10 ms; free DRAM above
40 KB; native tests pass; UC1–UC6 and all Appendix J journeys pass; coupling audit
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
