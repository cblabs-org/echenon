# PR-07: Integration, Portal & Polish (Phase 6 — Use Case Validation)

> **Programme:** Echenon Firmware
> **PR target base:** PR-06 (`erg-strava` branch tip)
> **Dev Phase:** Phase 6 — Polish
> **Spec sections:** §11.1–11.4, §13, Appendix A
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 6
> **Use cases:** [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md)

## Mandate

Final integration, portal polish, OTA, and full use case validation.

1. **`SessionOverlay`** — Standalone `/overlay` page with WebSocket 2 Hz push.
2. **`Calibration`** portal tab — pot wizard, power tier, community matrix.
3. **Session History** portal tab.
4. **OTA firmware update** via portal.
5. **Final `EchenonModule` wiring** — all sub-components composed.
6. **Coupling audit** + ADR compliance (ADR-16, ADR-17, ADR-39).
7. **Use Case Validation** — all 6 use cases (UC1–UC6) pass end-to-end.
8. **Standalone test** — console disconnected, verify R3/R4/R5 sensor power.
9. **README.md** — comprehensive build/flash/setup instructions.

**Hardware:** Same intercept board. Also test standalone mode (console
disconnected) for UC6.

After this PR merges, the firmware is release-candidate quality with all 6 use
cases validated.

## Prior Art

- [wo7-overlay/WORKORDER.md](../archive-pa-decomposition/wo7-overlay/WORKORDER.md)
- [wo9-calibration-portal/WORKORDER.md](../archive-pa-decomposition/wo9-calibration-portal/WORKORDER.md)
- [wo10-integration/WORKORDER.md](../archive-pa-decomposition/wo10-integration/WORKORDER.md)
- [Old PR-05 scope](../archive-pr-v1/pr-05-web-portal/PROJECT.md)
- [Old PR-07 scope](../archive-pr-v1/pr-07-integration-polish/PROJECT.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
