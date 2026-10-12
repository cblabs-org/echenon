# PR-03: HR Armband Relay (Phase 2 — HR relay)

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-01 merged
> **Dev Phase:** Phase 2 — HR relay
> **Depends on:** PR-01; entry gates below
> **Spec sections:** [SPEC](../../../../SPEC.md) §3.5 (composition), §7.8 (connection budget, device name), §8.1–§8.5 (HR relay, BLE integration, reconnect policy, coexistence), §13.5 (tests)
> **Strategy:** [`docs/implementation-strategy.md`](../../../../docs/implementation-strategy.md) Phase 2
> **Parallel with:** PR-02. **Shared files:** `EchenonModule.*`, `platformio.ini`, `main.cpp`. Whichever of PR-02 / PR-03 merges second rebases onto `main` and resolves the conflict.

## Entry gates

| Gate | Status |
|---|---|
| PR-01 merged; framework at `v0.0.12-dev.82` or later | ✅ Met (PR #1, `5897f6f`, 2026-10-11) |
| Echelon-app HR acceptance (register row #12) | **Approved 2026-10-12: becomes the WO3 go/no-go.** The nRF Connect mock was not discoverable by Android (row #12 INCONCLUSIVE), so the definitive test needs an ESP32 HR peripheral, which is this PR's WO2 |
| Framework central-role acceptance evidence (audit P-01) | **Approved 2026-10-12: becomes WO5.** At `v0.0.12-dev.93` the framework's `ACCEPTANCE.md` still marks central "N/A", although the WO-8 fix is included. Echenon runs RFC §17.2 items 4–9 against the Beat strap as consumer evidence. The PA files an app-neutral upstream report |

> Changing these gates from *entry* to *in-PR go/no-go* is a programme change
> and needs the owner's approval. If WO3 fails, PR-03 stops and escalates:
> UC1's HR path would need redesign.

## Mandate

Turn BLE on and implement the HR relay — the simplest useful BLE feature —
proving dual-role BLE, WiFi coexistence and multi-client connections.

1. **Enable BLE** — Declare the `[ble]` mix-in locally in Echenon's
   `platformio.ini` (`-D WITH_BLE`, `h2zero/esp-nimble-cpp@^2.1.0`, `BLEManager`)
   with `framework/hardware/_common/sdkconfig.ble.defaults` in the sdkconfig chain.
   Echenon's `hardware/ex3-waveshare-nano/sdkconfig.defaults` sets
   `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` and `CONFIG_BT_CTRL_BLE_MAX_ACT=6` (3 peripheral
   + 1 central; SPEC §7.8), applied after `sdkconfig.ble.defaults`. If the
   central-acceptance gate requires a newer framework tag, this WO performs that bump.
2. **`BLEManager` integration** — Composition per SPEC §3.5 using the real API:
   providers are owned by `EchenonModule` and exposed through accessors, registered
   with `BLEManager` before `app.begin(config)`, then `bleManager.begin(app)`. Confirm
   the order against the framework headers and docs.
3. **`HRMClient`** (`IBLEClientProvider`) — Scan, connect and subscribe to the HR
   armband; uses `BLENotification.charUuid16`. Publishes HR on the EventBus; it
   makes no direct calls into `HRMRepeater`. Reconnect policy per SPEC §8.4: fast
   backoff {2, 4, 8, 16, 32} s, then an indefinite slow retry every 15 s while
   RUNNING. **`slowRetryMs` is set through the BLEManager configuration**
   (`BLEConfig.slowRetryMs`, a persisted IConfigurable field, default 60000), not
   through the copy returned by `getReconnectPolicy()`. The WO verifies the exact
   mechanism against the framework headers and records it.
4. **`HRMRepeater`** (`IBLEServiceProvider`) — Subscribes to HR events and
   re-broadcasts them as `0x180D`.
5. **Progressive coexistence test** — BLE and WiFi run simultaneously without panics
   or DRAM exhaustion; reconnect recovery criterion ≤ 20 s. The final coexistence
   test belongs to PR-09.

**Hardware:** No bike hardware needed: ESP32 + HR armband + P12 tablet. Tests with
the armband and the tablet are `complexity: interactive` WOs with evidence committed.

After this PR merges, the Echelon app pairs with **`Echenon-XXXX`** (base name
`Echenon`, MAC suffix added by the framework) as an HR device and displays live
heart rate. First user-facing feature with zero bike risk.

## Prior Art

- [wo5-hrm/WORKORDER.md](../archive-pa-decomposition/wo5-hrm/WORKORDER.md) — predates BLEManager's real API
- [Old PR-03 scope](../archive-pr-v1/pr-03-ble-stack/PROJECT.md) (was FTMS+HR combined)

## Work Order Ledger

Project name for branches: `hr-relay`. Linear stack from `main`. Proposed
2026-10-11; **approved by the owner 2026-10-12**.

| | WO | Branch (base) | Complexity | Scope | Tests (SPEC §13.5) | Status |
|---|---|---|---|---|---|---|
| **[x] [wo1-ble-enable](wo1-ble-enable/WORKORDER.md):** | Turn BLE on | `hr-relay/wo1-ble-enable` (`main`) | `standard` | (a) Bump the framework to the latest tag containing the WO-8 fix and #226 (currently `v0.0.12-dev.93`). (b) Local `[ble]` mix-in, `sdkconfig.ble.defaults` in the chain, Echenon `MAX_CONNECTIONS=4` / `BLE_MAX_ACT=6`. (c) `BLEManager` composition per SPEC §3.5 with no Echenon providers yet. (d) Split envs: the default `ex3-waveshare-nano` drops `WITH_SIGNAL_LOGGER`, and a new `ex3-waveshare-nano-diag` keeps it (L-08). (e) Record DRAM and heap with BLE on. (f) Native env still builds. (g) Fix the diag-env logger crash during large ride-log CSV downloads (DR-1 prep finding 4; L-06 export race / httpd stack) | 2.1 (advertising, name `Echenon-XXXX`) | ✅ Complete (`579e5c9`) |
| **[ ] [wo2-hr-repeater](wo2-hr-repeater/WORKORDER.md):** | HR peripheral | `hr-relay/wo2-hr-repeater` (wo1) | `standard` | `HRMRepeater` (`IBLEServiceProvider`, `0x180D` / `0x2A37`, Body Sensor Location from source); an EventBus HR event and payload; a synthetic HR source under `WITH_HR_SYNTHETIC` (diagnostic only); native tests for `0x2A37` encoding | — | ⬜ |
| **[ ] [wo3-app-pairing-gate](wo3-app-pairing-gate/WORKORDER.md):** | **Go/no-go: Echelon app pairs `Echenon-XXXX`** | `hr-relay/wo3-app-pairing-gate` (wo2) | `interactive` | Owner with the P12: the Echelon app lists and pairs `Echenon-XXXX` and shows the synthetic HR. Evidence `A12-<date>-esp32-hr-pairing.md`; closes register row #12. **On failure, STOP the PR and escalate** | 2.3 (synthetic) | ⬜ |
| **[ ] [wo4-hr-client](wo4-hr-client/WORKORDER.md):** | HR strap client | `hr-relay/wo4-hr-client` (wo3) | `standard` | `HRMClient` (`IBLEClientProvider`). A pure `0x2A37` parser (uint8/uint16, contact flags, energy, RR, BPM = 0) with native tests. Three-state contact machine; 10 s stale timeout; battery `0x2A19`; publishes the HR event (no direct repeater call). `slowRetryMs = 15000` set through BLEManager config, with the mechanism verified and recorded | 2.4 logic (native) | ⬜ |
| **[ ] [wo5-strap-acceptance](wo5-strap-acceptance/WORKORDER.md):** | Strap and central acceptance | `hr-relay/wo5-strap-acceptance` (wo4) | `interactive` | Beat HW901, plus a second standard strap if available: portal scan and pair, end-to-end relay to the Echelon app, contact loss, auto-sleep and reconnect ≤ 20 s, factory reset. Also **RFC §17.2 items 4–9 as consumer evidence**. Evidence in `docs/measurements/phase2/`. The PA files an app-neutral upstream report | 2.2–2.6 | ⬜ |
| **[ ] [wo6-coexistence](wo6-coexistence/WORKORDER.md):** | Progressive coexistence | `hr-relay/wo6-coexistence` (wo5) | `interactive` | HR relay, portal open and a large HTTPS POST for 5 minutes; a scripted `bleak` client for repeatability. Measure the HR gap, heap and DRAM | 2.7 | ⬜ |
| **[ ] [wo7-doc-sync](wo7-doc-sync/WORKORDER.md):** | Docs | `hr-relay/wo7-doc-sync` (wo6) | `standard` | SPEC §3.5, §7.8, §8 and §13.5 match what was built; register row #12 final; the PR-03 PROJECT.md wrap-up | — | ⬜ |

**File overlaps:**
- Inside the stack: `EchenonModule.*` and `main.cpp` change in WO1, WO2 and WO4 (additive, linear).
- Against other projects:
  - `pr-dr1-prep` writes only `docs/measurements/phase0/` and one analysis
    doc, so there is no overlap. If WO1 merges first, the bench session must
    flash the new `-diag` env.
  - PR-02 (after DR-1) shares `EchenonModule.*`, `platformio.ini` and
    `main.cpp`. PR-03 will almost certainly merge first, so PR-02 rebases.

**Standing instructions for every WO:** Rules 03 (Tag → Build → Flash) and 04
(PlatformIO MCP tools only). Surface findings rather than filing them.
Framework issues are written app-neutral, with no Echenon specifics.

## Session Log

### 2026-10-11: Decomposition proposed (Chief Architect)
- PR-01 merged, so PR-03 is unblocked under PROGRAM-PLAN (runs in parallel with DR-1).
- Framework state: latest tag `v0.0.12-dev.93`, which includes the WO-8 fix;
  #226 is closed. Central-role acceptance is still not evidenced upstream.
- Proposed turning both entry gates into in-PR go/no-go WOs (WO3, WO5), because
  neither can be settled without ESP32 firmware.
- **Awaiting owner approval** of the ledger and the gate change. Once approved,
  record it here and then hand off to a PA.

### 2026-10-12: Decomposition approved (owner)
- **Owner approval (in chat, 2026-10-12):** *"approved - give me the PR-03 bridge
  prompt"*. This approves the 7-WO ledger above and the gate change (row #12
  becomes the WO3 go/no-go; central-role acceptance becomes WO5). Recorded here
  as the provenance anchor per `.agents/rules/00-operating-model.md`.
- PR-03 belongs to the **v1 Passive** release (programme restructure in
  progress, 2026-10-12). Its scope is unchanged.
- **Merge protocol:** the PA submits the PR and STOPS. The owner brings it to a
  Chief Architect review; after that review, the owner merges.

## Status
**Plan Status:** IN PROGRESS — WO1 complete, WO2 next

### 2026-10-12: WO1 complete (PA session)
- **Branch:** `hr-relay/wo1-ble-enable` at `579e5c9`.
- Framework bumped from `v0.0.12-dev.82` to `v0.0.12-dev.93` (includes WO-8
  central fix and #226).
- BLE enabled: `[ble]` mix-in, `sdkconfig.ble.defaults` in chain,
  `MAX_CONNECTIONS=4` / `BLE_MAX_ACT=6`.
- `BLEManager` composed in `main.cpp` per SPEC §3.5 (no providers yet).
  `bleManager.begin(app)` called after `app.begin(config)` — confirmed as the
  correct lifecycle from `BLEManager.hpp` (binds `_app`, spawns FreeRTOS
  worker, registers DIS, locks registration).
- Envs split: `ex3-waveshare-nano` (default, no logger) and
  `ex3-waveshare-nano-diag` (with `WITH_SIGNAL_LOGGER`).
- L-06 CSV export crash fixed: RAII `ExportActiveGuard` pauses capture during
  export; chunked 2 KB response buffers; `vTaskDelay` yields; error handling
  on `sendChunk`.
- `setDefaultHostname("echenon")` added for mDNS.
- **Build metrics (BLE on, no providers):**
  - `ex3-waveshare-nano`: RAM 53,084 B (16.2%), Flash 1,522,175 B (24.2%)
  - `ex3-waveshare-nano-diag`: RAM 53,484 B (16.3%), Flash 1,544,447 B (24.5%)
  - `native`: 24/24 tests pass.
- **Next:** WO2 (HRMRepeater).
