# Echenon Firmware — Programme

> **Programme:** Build the complete Echenon firmware — an ESP32-based FTMS bridge
> for the Echelon EX-3 spin bike, using the ESP32 App Foundation framework.
>
> **Chief Architect tier.** This document tracks the macro-level PR dependency
> graph. Each PR is delegated to a Tier 2 Principal Architect who decomposes it
> into Work Orders.
>
> **Specification:** [SPEC.md (artifact)](file:///Users/mcneillm/.gemini/antigravity/brain/c73eec9a-9856-41f9-bd09-f97217b4c7f2/SPEC.md)
> **Upstream RFC:** [BLEManager — issue #225](https://github.com/matthewmcneill/esp32-app-foundation/issues/225)

---

## Programme Status

| # | PR | Status | PA | Depends On |
|---|---|---|---|---|
| 1 | [pr-01-scaffold-sensors](pr-01-scaffold-sensors/PROJECT.md) | ⬜ Not started | — | `main` |
| 2 | [pr-02-power-engine](pr-02-power-engine/PROJECT.md) | ⬜ Not started | — | PR-01 |
| 3 | [pr-03-ble-stack](pr-03-ble-stack/PROJECT.md) | ⬜ Not started | — | PR-02 |
| 4 | [pr-04-session-recording](pr-04-session-recording/PROJECT.md) | ⬜ Not started | — | PR-02 |
| 5 | [pr-05-web-portal](pr-05-web-portal/PROJECT.md) | ⬜ Not started | — | PR-03, PR-04 |
| 6 | [pr-06-cloud-upload](pr-06-cloud-upload/PROJECT.md) | ⬜ Not started | — | PR-04 |
| 7 | [pr-07-integration-polish](pr-07-integration-polish/PROJECT.md) | ⬜ Not started | — | PR-05, PR-06 |

---

## Programme Session Log

### 2026-09-17 — Programme Initialisation
- Specification finalised at ~1,460 lines with 13 sections and 7 user journeys
- Foundation BLE module RFC filed as upstream issue #225
- Initial PA-level decomposition (10 WOs in a single PR) was over-scoped for one PR
- Restructured into 7-PR programme with proper Chief Architect tiering
- Pre-researched WO content archived at `archive-pa-decomposition/` — PAs may
  reference this as prior art when decomposing their own WOs
