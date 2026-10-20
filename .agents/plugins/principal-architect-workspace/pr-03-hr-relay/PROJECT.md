---
schema_version: 1
charter:
  pr_id: PR-03
  slug: pr-03-hr-relay
  title: HR Armband Relay (Phase 2 — HR relay)
  branch: feat/pr-03-hr-relay
  base_branch: main
  allowed_modules:
  - src/echenon/
  - docs/
  - hardware/
  - framework/
  - platformio.ini
  charter_locked: false
status: IN_PROGRESS
work_orders:
- id: WO-03.1
  slug: wo1-ble-enable
  title: Turn BLE On
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.2
  slug: wo2-hr-repeater
  title: HRMRepeater (HR Peripheral)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.3
  slug: wo3-app-pairing-gate
  title: App Pairing Gate (GO / NO-GO)
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.32
  slug: wo3b-debug-android-connect
  title: Debug Android BLE Connection Failure (Consolidated CA Review)
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.33
  slug: wo3c-ble-test-rig
  title: BLE Test Rig
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.34
  slug: wo3d-ble-ab-run
  title: BLE A/B/A′ Run
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.4
  slug: wo4-hr-client
  title: HRMClient (HR Strap Central)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.5
  slug: wo5-strap-acceptance
  title: Strap Acceptance
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.6
  slug: wo6-coexistence
  title: P12 v1 Topology Coexistence
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: null
- id: WO-03.7
  slug: wo7-doc-sync
  title: Doc Sync + F3 Fix + PR-03 Wrap-Up
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: null
---
## 2. Scope

_See legacy content below._


# PR-03: HR Armband Relay (Phase 2 — HR relay) — v1

> **Release:** **v1 "Passive"** ([PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases)).

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

## 1. Mandate

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

## 3. Work Order Ledger

Project name for branches: `hr-relay`. Linear stack from `main`. Proposed
2026-10-11; **approved by the owner 2026-10-12**.

| | WO | Branch (base) | Complexity | Scope | Tests (SPEC §13.5) | Status |
|---|---|---|---|---|---|---|
| **[x] [wo1-ble-enable](wo1-ble-enable/WORKORDER.md):** | Turn BLE on | `hr-relay/wo1-ble-enable` (`main`) | `standard` | (a) Bump the framework to the latest tag containing the WO-8 fix and #226 (currently `v0.0.12-dev.93`). (b) Local `[ble]` mix-in, `sdkconfig.ble.defaults` in the chain, Echenon `MAX_CONNECTIONS=4` / `BLE_MAX_ACT=6`. (c) `BLEManager` composition per SPEC §3.5 with no Echenon providers yet. (d) Split envs: the default `ex3-waveshare-nano` drops `WITH_SIGNAL_LOGGER`, and a new `ex3-waveshare-nano-diag` keeps it (L-08). (e) Record DRAM and heap with BLE on. (f) Native env still builds. (g) Fix the diag-env logger crash during large ride-log CSV downloads (DR-1 prep finding 4; L-06 export race / httpd stack) | 2.1 (advertising, name `Echenon-XXXX`) | ✅ Complete (`579e5c9`) |
| **[x] [wo2-hr-repeater](wo2-hr-repeater/WORKORDER.md):** | HR peripheral | `hr-relay/wo2-hr-repeater` (wo1) | `standard` | `HRMRepeater` (`IBLEServiceProvider`, `0x180D` / `0x2A37`, Body Sensor Location from source); an EventBus HR event and payload; a synthetic HR source under `WITH_HR_SYNTHETIC` (diagnostic only); native tests for `0x2A37` encoding | — | ✅ Complete (`1e4a46d`) |
| **[x] [wo3-app-pairing-gate](wo3-app-pairing-gate/WORKORDER.md):** | **Go/no-go: Echelon app pairs `Echenon-XXXX`** | `hr-relay/wo3-app-pairing-gate` (wo2) | `interactive` | Owner with the P12: the Echelon app lists and pairs `Echenon-XXXX` and shows the synthetic HR. Evidence `A12-<date>-esp32-hr-pairing.md`; closes register row #12. **On failure, STOP the PR and escalate** | 2.3 (synthetic) | ✅ CONDITIONAL (`5a0350f`) |
| **[ ] [wo3b-debug-android-connect](wo3b-debug-android-connect/WORKORDER.md):** | Debug Android BLE 0x3e | `hr-relay/wo3b-debug-android-connect` (wo3) | `interactive` | CA-directed investigation: (a) Android connects with WiFi STA disconnected; (b) diag build with `WIFI_PS_MIN_MODEM` (local override) — if Android connects, check for `AUTH_EXPIRE`; (c) Android HCI snoop log of connection parameters; (d) only if a–c inconclusive, test 100 ms advertising interval. Evidence in `docs/measurements/phase2/`. File one upstream issue (power-save / coex finding + adv interval if data supports). | — | ⏸️ PAUSED — superseded by WO3c/WO3d test strategy |
| **[x] [wo3c-ble-test-rig](wo3c-ble-test-rig/WORKORDER.md):** | Build BLE A/B test rig | `hr-relay/wo3c-ble-test-rig` (wo3b) | `standard` | Build automated test tooling under `tools/ble-test/`: (a) `pixel_runner.py` — adb/uiautomator-driven nRF Connect test runner; (b) `esp32_log.py` — serial capture with wall-clock timestamps; (c) `control-peripheral/` — standalone PlatformIO project (NimBLE HR, no WiFi, same BLE presentation as Echenon); (d) `snoop_parse.py` — btsnoop HCI log parser; (e) optional `mac_runner.py` (bleak). Dry run 5 attempts on current firmware. No changes to `src/`, `framework/`, `SPEC.md`, `PROGRAM*.md` or `reviews/*`. | — | ✅ Complete (`b454d5d`) |
| **[x] [wo3d-ble-ab-run](wo3d-ble-ab-run/WORKORDER.md):** | Run A/B/A′ BLE experiment | `hr-relay/wo3d-ble-ab-run` (wo3c) | `interactive` | Automated A/B/A′ experiment using WO3c rig: Batch A (Echenon, WiFi STA connected, ~50 attempts), Batch B (control peripheral, no WiFi, ~50 attempts), Batch A′ (Echenon again). Decision rules pre-agreed with CA. **Result: Decision Rule 2 triggered — H-COEX ELIMINATED.** A=26%, B=32%, A′=28% (Δ within 10 pp). Failure is ESP32 NimBLE/controller scheduling, not WiFi coex. | — | ✅ Complete (`62984c6`) |
| **[x] [wo4-hr-client](wo4-hr-client/WORKORDER.md):** | HR strap client | `hr-relay/wo4-hr-client` (wo3d) | `standard` | `HRMClient` (`IBLEClientProvider`). A pure `0x2A37` parser (uint8/uint16, contact flags, energy, RR, BPM = 0) with native tests. Three-state contact machine; 10 s stale timeout; battery `0x2A19`; publishes the HR event (no direct repeater call). `slowRetryMs = 15000` set through BLEManager config, with the mechanism verified and recorded | 2.4 logic (native) | ✅ Complete (`e40653f`) |
| **[x] [wo5-strap-acceptance](wo5-strap-acceptance/WORKORDER.md):** | Strap and central acceptance | `hr-relay/wo5-strap-acceptance` (wo4) | `interactive` | Beat HW901, plus a second standard strap if available: portal scan and pair, end-to-end relay to the Echelon app, contact loss, auto-sleep and reconnect ≤ 20 s, factory reset. Also **RFC §17.2 items 4–9 as consumer evidence**. Evidence in `docs/measurements/phase2/`. The PA files an app-neutral upstream report | 2.2–2.6 | ✅ PASS 11/11 (`5866e3d` + re-run with F4 fix on `v0.3.0-dev.1`). Tests 2.5 (~7s reconnect) and 2.6 (~15s reconnect post-factory-reset) verified after framework `v0.0.12-beta.3` (PR #301). |
| **[-] [wo3e-side-swap-tests](wo3e-side-swap-tests/WORKORDER.md):** | Side-swap BLE interop tests | `hr-relay/wo3e-side-swap-tests` (wo5) | `interactive` | CA-directed: isolate whether the connection failure is ESP32-S3-specific or peripheral-generic. Apply CA corrections C1-C4 to the WO3d report. Using the existing rig, 50 attempts each: X1–X5. | — | ⏭️ DEFERRED — owner decision 2026-10-17. Android phone reliability is PR-05 entry gate, not PR-03. Sufficient data collected in WO3d. |
| **[x] [wo6-coexistence](wo6-coexistence/WORKORDER.md):** | P12 v1 topology coexistence | `hr-relay/wo6-coexistence` (wo5) | `interactive` | **v1 topology on the P12:** HR strap (central) + P12 tablet + portal WebSocket at 2 Hz + HTTPS POST, 5-10 minutes. Measure HR gaps, connection drops, heap and DRAM. **Programme gate:** "v1 BLE topology proven on the P12". | 2.7 | ✅ PASS (`49a364f`) — **Programme gate cleared.** Max HR gap 1,200 ms (≤2,500 ms). 326 notifications / 325.3 s. 30/30 HTTP OK. Zero dropped events. Heap stable (min 81 KB). |
| **[x] [wo7-doc-sync](wo7-doc-sync/WORKORDER.md):** | Docs + F3 fix | `hr-relay/wo7-doc-sync` (wo6) | `standard` | SPEC §3.5, §7.8, §8 and §13.5 match what was built; register row #12 final; fix F3 (BPM=0 relay); the PR-03 PROJECT.md wrap-up; CA review remediation (B3/B4/B5/S2/S4/S5/S8/S9) | — | ✅ Complete (`d3f6b32`) + CA remediation commits (`a174874`..`HEAD`) — F3 fixed, SPEC verified, 45/45 native tests, builds pass. |

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

## 4. Session Log

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
**Plan Status:** **CA REVIEW REMEDIATION COMPLETE — READY FOR RE-REVIEW** — WO1–WO7 ✅, WO5 11/11 PASS (tests 2.5/2.6 re-run PASS with F4 fix), WO6 ✅ PROGRAMME GATE CLEARED, WO3e deferred. All CA blockers (B1–B5) and required should-fix (S1/S2/S8) resolved. Upstream issues filed (F2 #303, F5 #304). PR #3 updated and force-pushed.

### 2026-10-18: CA review remediation complete
- CA review on PR #3 requested changes: 5 blockers (B1–B5), 9 should-fix (S1–S9)
- B1 (F4): Framework fix merged (PR #301 on esp32-app-foundation), `pair()` now calls `stopDiscovery()`. Framework bumped to `v0.0.12-beta.3` (`726dce2d`). Tests 2.5/2.6 re-run pending with new firmware.
- B2: SPEC §8.2/§8.4 diff applied by owner (`a3516c9`) — NO_CONTACT aligned with BPM=0 relay
- B3: `_anySubscriber` flag removed from HRMRepeater — NimBLE handles per-peer tracking (`a174874`)
- B4: Absolute paths replaced with `<worktree>` placeholders (`3ccde3b`)
- B5: Framework submodule bumped to `v0.0.12-beta.3` (`dd07166`)
- S1: PR description updated (RFC 9 partial, H-INTEROP wording)
- S2: Upstream report scrubbed (SSID/IP removed, item 9 → PARTIAL) (`0bb2bbd`)
- S4: Cross-thread `_repeater` call removed from Client (`8b86a07`)
- S5: `slowRetryMs` override documented as SPEC-mandated (`789e673`)
- S8: `main` merged into stack tip — clean merge, no conflicts
- S9: Mock bugreport zip deleted (`34d1ce9`)
- S3: Upstream issues filed — F2 (#303: connectable field), F5 (#304: portal stale state). F4 was already #300 (fixed via PR #301).
- **Remaining:** None — all CA blockers and required should-fix resolved. Ready for re-review.

### 2026-10-18: Tests 2.5/2.6 re-run PASS — F4 fix verified (interactive session)
- **Firmware:** `v0.3.0-dev.1` (framework `v0.0.12-beta.3` with F4 fix).
- **Test 2.5 (auto-reconnect):** Beat removed from arm → auto-sleep disconnect after ~3 min → put back on → **reconnected in ~7s**. PASS (criterion ≤ 20s).
- **Test 2.6 (factory reset + reconnect):** Factory reset via API → reconfigured WiFi → re-paired Beat via portal + API → removed from arm → auto-sleep disconnect → put back on → **reconnected in ~15s**. PASS (criterion ≤ 20s). `slowRetryMs = 15000` confirmed surviving factory reset.
- **F4 fix confirmed:** `discovery.active: false` after `pair()` in both tests — the lingering discovery scan no longer blocks the reconnect scheduler.
- **Evidence:** `docs/measurements/phase2/wo5-rerun-2.5-2.6-evidence.md`
- **WO5 final score:** 11/11 PASS (was 9/11 before F4 fix).

### 2026-10-17: WO7 complete — F3 fixed, SPEC verified, PR-03 ready for submission (PA session)
- **Commit:** `d3f6b32` on `hr-relay/wo7-doc-sync`.
- **F3 fixed:** Removed `if (sample->bpm > 0)` guard in `HRMRepeater.cpp:87`.
  BPM=0 (off-skin signal) now relayed transparently to downstream apps.
- **SPEC verification:** §3.5, §7.8, §8.1–§8.5, §13.5 all match implementation.
  No discrepancies found.
- **Build:** 45/45 native tests pass. `ex3-waveshare-nano`: RAM 53,164 B (16.2%),
  Flash 1,527,583 B (24.3%). `-diag`: builds clean.
- **Upstream report:** `docs/upstream-central-acceptance-report.md` verified
  app-neutral, PR #296 referenced, WO5 evidence included. Ready for PA review.
- **Note:** Worker flagged legacy `file:///` URLs in `docs/measurements/phase0/`
  — outside WO7 boundary box, deferred to WO7 or doc cleanup.
- **All WOs complete.** Next: push stack → open PR → STOP.

### 2026-10-17: WO6 PASS — programme gate cleared (PA session)
- **Commit:** `49a364f` on `hr-relay/wo6-coexistence`.
- **Firmware:** `v0.1.0-dev.15` (no re-flash — same as WO5 re-run).
- **v1 topology sustained >5 minutes:**
  Beat HW901 (central) + P12 Echelon Fit (peripheral) + WiFi portal (WebSocket)
  + 30 HTTP requests across 3 bursts + WiFi network scan during test.
- **Results:**
  - Max HR gap: **1,200 ms** (criterion ≤ 2,500 ms) — **PASS**
  - 326 `0x2A37` notifications over 325.3 s (~1.003 Hz mean)
  - 90.8% of gaps within 900–1,100 ms, zero gaps > 1,500 ms
  - 30/30 HTTP requests OK, `droppedEvents: 0`
  - Beat central connection stable throughout
  - P12 peripheral connection stable (48.75 ms interval, MTU 185)
  - Portal WebSocket maintained with periodic interaction
  - `heap_min_free` = 81,080 B >> 20,480 B, heap Δ = −712 B (negligible)
- **Gate decision: PASS.** "v1 BLE topology proven on the P12" — cleared.
- **WO3e DEFERRED** (owner decision, 2026-10-17): Android phone investigation
  parked for PR-05. Sufficient data from WO3d.
- **Evidence:** `docs/measurements/phase2/wo6-coexistence/evidence.md`.
- **Next:** WO7 (doc-sync + F3 fix, standard complexity — spawn worker).

### 2026-10-17: WO5 re-run MOSTLY PASS — 9/11 pass, F4 auto-reconnect blocker (PA session)
- **Commit:** `5866e3d` on `hr-relay/wo5-strap-acceptance`.
- **Tag:** `v0.1.0-dev.15` (framework at `1e90ec68`, MAC fix).
- **Framework fix confirmed:** Central connections now target the correct MAC
  address. Beat HW901 connects instantly via `NimBLEAddress(ble_addr_t)`.
- **Results:**
  - 2.2 (scan+pair) **PASS** — correct address, BSL=0x04, 0x2A37 subscribed.
  - 2.3 (end-to-end relay) **PASS** — Beat→ESP32→P12 Echelon Fit, 82–100 BPM.
  - 2.4 (contact loss) **PASS** — HR absent within ~2s (criterion ≤12s).
  - 2.5 (auto-reconnect) **FAIL** — F4: discovery scan blocks reconnect scheduler.
  - 2.6 (factory reset) **BLOCKED** — depends on 2.5.
  - RFC 4 (coex) **PASS** — 10/10 HTTP 200 OK, HR uninterrupted.
  - RFC 5 (cycling) **PASS** — 3 forget/re-pair cycles, no crash, `droppedEvents: 0`.
  - RFC 6 (DRAM) **PASS** — `heap_min_free` = 77,376 B >> 20,480 B.
  - RFC 7 (boot) **PASS** — clean boot, both providers.
  - RFC 8 (peripheral re-adv) **PASS** — P12 BT toggle, HR resumed.
  - RFC 9 (central vs peripheral) **PASS** — strap removed, P12 stayed.
- **New findings:**
  - F3 (consumer): `HRMRepeater.cpp:89` filters `bpm > 0`, suppressing BPM=0
    relay. One-liner fix in consumer code.
  - F4 (framework, MAJOR): Portal discovery scan persists and blocks the
    reconnect scheduler's scan-and-connect cycle. Only one concurrent BLE scan
    supported. Auto-reconnect non-functional after any manual Pair.
  - F5 (portal UI): stale BLE status display (cosmetic).
- **PA assessment:** F3 is a consumer code fix (one line, `HRMRepeater.cpp`).
  F4 is a framework issue — the discovery scan not being torn down after
  a manual pair contends with the reconnect scheduler. Tests 2.5/2.6 cannot
  pass until F4 is fixed in the framework. However, F4 does NOT block WO6
  (P12 coexistence gate) because WO6 tests steady-state relay with manual
  pairing, not auto-reconnect. F4 also does NOT block PR-03 submission —
  it is a pre-existing framework limitation, not a regression introduced by
  PR-03. The PA will surface F4 as a framework issue draft.
- **Upstream report:** Updated at `docs/upstream-central-acceptance-report.md`.
- **Evidence:** `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`.

### 2026-10-16: Framework submodule bumped — WO5 unblocked (PA session)
- **Commit:** `f366c16` on `hr-relay/wo5-strap-acceptance`.
- Framework submodule updated to `1e90ec68` (PR #296 merged to `main`): fixes
  MAC byte-order inversion in `BLEStack::executeCentralConnect()` (L640) and
  `executeCentralForget()` (L748).
- **WO5 re-run now unblocked.** The 9 blocked tests (2.2–2.6, RFC 4/5/8/9)
  should pass with the corrected MAC address handling.
- Next: tag, build, flash, re-run WO5 (interactive — bridge prompt to owner).

### 2026-10-16: WO5 PARTIAL — framework central-role blocker found (interactive session)
- **Branch:** `hr-relay/wo5-strap-acceptance` at `5264701`.
- **Tag:** `v0.1.0-dev.14` (flashed `ex3-waveshare-nano`, no synthetic HR).
- **Result: 2/11 PASS, 9/11 BLOCKED** by a critical framework bug.
- **RFC 6 (DRAM) PASS:** `heap_min_free` = 93,404 B >> 20,480 B threshold.
  Post-testing: 86,496 B — no leak detected across ~10+ scan/connect cycles.
- **RFC 7 (boot) PASS:** Clean boot with `hr_client` + `hr_repeater` registered,
  BLE advertising `0x180D`, WiFi coex active, no errors.
- **BLOCKER F1 (CRITICAL): MAC address byte-order inversion in `BLEStack.cpp`.**
  `executeCentralConnect()` at line 640 uses `NimBLEAddress(addr.bytes, type)`
  which applies `std::reverse_copy()` on `BLEPeerAddress.bytes` (already LE).
  Result: every central connection targets a byte-reversed MAC and times out
  with `status=13` (`BLE_ERR_CONN_ESTABLISHMENT`). **ALL `IBLEClientProvider`
  connections are broken.** Fix: use `NimBLEAddress(ble_addr_t)` constructor
  (2 lines changed at L640 and L741).
- **F2 (minor):** `BLEScanResult.connectable` never populated from scan callback
  (`BLEManager.cpp:1136-1140`). Defaults to `false` for all devices. Cosmetic.
- **Mac control test:** bleak/CoreBluetooth connects to the Beat instantly,
  proving the strap is connectable and the issue is entirely framework-side.
- **SmartSpin2k comparison:** Uses `pClient->connect(advertisedDevice)` directly,
  bypassing the address storage layer — avoids the bug entirely.
- **Dead ends:** Scan parameter widening (50% duty), `connectable: false` red
  herring, 10+ forget/re-pair cycles — all failed identically because the
  underlying address is always reversed.
- **Upstream report draft:** `docs/upstream-central-acceptance-report.md` — ready
  for PA review before filing.
- **Evidence:** `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`.
- **Programme impact:** PR-03 is **BLOCKED** until the framework byte-order fix
  lands. WO5 must be re-run after the fix. WO3e, WO6, WO7 also blocked (all
  require a working central connection). This also explains the WO3d 0x3e
  failures: the Pixel connection attempts that *did* succeed must have been
  using a different code path (or the 0x3e investigation measured a different
  symptom — the peripheral-mode Pixel->ESP32 connection, not central-mode).

### 2026-10-15: CA WO3d review — H-COEX accepted, WO3e ordered (owner-approved)
- **CA verdict:** H-COEX ELIMINATED (A 26%, B 32%, A' 28%; Rule 2). Clean experiment.
- **Corrections C1-C4 required to `wo3d-ab-run/report.md`:**
  - C1: `LE_ENH_CONNECTION_COMPLETE status=0x00` on the phone means the phone SENT
    `CONNECT_IND`, not that the link was established. Remove "successfully establishes
    the link".
  - C2: ~80% of failures are "CONNECT_IND never received" (silent misses), ~20% are
    0x3e (request heard, early events missed). Fix the data-flow diagram.
    A: 37 failures / 8 logged 0x3e (29 silent); B: 34/5 (29); A': 36/9 (27).
  - C3: Rename H-PHONE to **H-INTEROP** — 18.75 ms interval cannot explain silent
    misses (interval only matters after CONNECT_IND received). Leading hypothesis:
    ESP32-S3 controller misses or rejects the Pixel's CONNECT_IND.
  - C4: Remove unverified chipset/"BLE stack" row. Reconcile P12 interval (~36.25 ms
    in table vs 48.75 ms in logs). Remove Android versions unless from bug report.
- **WO3e added** (side-swap tests, interactive, 50 attempts each):
  - X1: Pixel -> non-ESP32 peripheral (eBeat or nRF advertiser)
  - X2: Control peripheral rebuilt for classic ESP32 (esp32dev)
  - X3: P12 via nRF Connect rig (quantify P12 reliability)
  - X4: If available: non-Pixel Android, iPhone
  - X5: Only if X1/X2 point at ESP32-S3 -> escalate for sniffer (no purchases without owner)
  - Do NOT sweep adv intervals or switch to Bluedroid
- **Upstream draft status revised:**
  - (i) coex default comments: **KEEP**
  - (ii) power-save policy: **WITHDRAW**
  - (iii) BLE requires STA, HIGH priority: **KEEP**
  - (iv) scan timing, LOW priority: **KEEP**
  - A new espressif/esp-idf issue may follow X1/X2 results. Draft only.
- **Gate revisions:**
  - **WO6** = v1 topology on the P12 only (strap + P12 + portal + HTTPS POST).
    Android phone excluded from PR-03 scope.
  - **Android phone reliability** (>= 95% single attempt, Pixel + one other Android)
    is now a **PR-05 ENTRY gate**, not PR-03.
- **WO4:** CA approved. Rebase already complete (`e40653f`).
- **PR-03 is not blocked.**

### 2026-10-15: WO4 complete — HRMClient rebased and verified (PA session)
- **Branch:** `hr-relay/wo4-hr-client` at `e40653f`.
- **Rebase:** `git rebase --onto hr-relay/wo3d-ble-ab-run hr-relay/wo3-app-pairing-gate
  hr-relay/wo4-hr-client`. Conflict on `PROJECT.md` (expected — WO3b-WO3d
  insertions not present in the pre-rebase PA prep commit). Resolved by keeping
  wo3d's version (`git checkout --ours`). Second commit (HRMClient code) applied
  cleanly.
- **Post-rebase verification:**
  - `env:native`: **46/46** tests pass (32 WO1-WO2 + 1 WO3d + 13 WO4 parser).
  - `env:ex3-waveshare-nano`: builds clean (RAM 53,164 B, Flash 1,527,651 B).
  - `env:ex3-waveshare-nano-diag`: builds clean (RAM 53,768 B, Flash 1,677,351 B).
- **WO4 deliverables (from worker Completion Report):**
  - `HRMClient`: `IBLEClientProvider` + `ITickable`. Scan, connect, subscribe
    to HR strap. 3-state contact FSM (ACTIVE/NO_CONTACT/DISCONNECTED). 10 s
    stale timeout publishes HR=0. Battery `0x2A19`, Body Sensor Location `0x2A38`.
  - `HRParsing.h`: Standalone inline `parse0x2A37()` (BLE-free, native-testable).
  - EventBus publish via 4-slot ring buffer on `"echenon.hr"` — no direct
    `HRMRepeater` call (strict architectural decoupling).
  - `slowRetryMs = 15000`: set via `bleManager.applyConfig()` with `JsonDocument`
    after `bleManager.begin(app)`. Mechanism verified: `BLEManager::applyConfig()`
    accepts `"slowRetryMs"` in the `"ble"` config namespace and propagates to
    all active reconnect policies.
- **No flash** — build-only. No GPIO 43/44 references in new code.
- **Next:** WO5 (interactive — strap acceptance with Beat HW901).

### 2026-10-15: WO3d A/B/A′ COMPLETE — H-COEX ELIMINATED (interactive session)
- **Branch:** `hr-relay/wo3d-ble-ab-run` at `f8f4985`.
- **Result (Decision Rule 2 triggered):** WiFi coexistence is **NOT** the
  dominant cause of 0x3E connection failures.
  - Batch A (Echenon + WiFi): **26.0%** pass (13/50)
  - Batch B (control, no WiFi): **32.0%** pass (16/50)
  - Batch A′ (Echenon + WiFi): **28.0%** pass (14/50)
  - Δ(A, B) = 6 pp, Δ(A′, B) = 4 pp — both within 10 pp threshold.
  - A vs A′ = 2 pp — excellent reproducibility, no temporal drift.
- **150 total attempts, zero drops during 30s holds** (H-UPD not observed).
- **HCI trace confirmed mechanism:** Pixel establishes link (CONNECTION_COMPLETE
  status=0x00) but ESP32 misses first 6 data-channel events within 112.5ms
  (6 × 18.75ms). This happens identically with and without WiFi.
- **Revised hypothesis standings:**
  - ~~H-COEX~~ → **ELIMINATED** (Rule 2)
  - H-PHONE → **Promoted to leading** (Pixel's 18.75ms interval gives half
    the establishment window of the P12's ~36.25ms)
  - H-ESP32 → **New** (NimBLE/controller scheduling latency, independent of WiFi)
  - H-UPD → **Not observed** (0/150 drops)
- **Programme impact:**
  - WO3e (power-save experiment) **NOT triggered** — Rule 2 applies, not Rule 1.
  - The phone question still gates **PR-05**, not PR-03. PR-03 is unblocked.
  - `WIFI_PS_NONE` is no longer the prime suspect. The upstream WiFi PS issue
    draft (#ii) may need revision or withdrawal.
  - WO6 coexistence gate ("v1 BLE topology proven") remains — but the
    expected failure mode is now NimBLE scheduling, not WiFi coex.
- **Evidence:** `docs/measurements/phase2/evidence/wo3d-ab-run/report.md`
  (322-line systematic debugging report). Raw bugreports and serial logs
  in gitignored `raw/` directories.
- **Side findings:** Android 17 logcat format change (fixed in `pixel_runner.py`,
  commit `1f85047`); 12 bonded BLE devices on the Pixel may contribute.
- **Next:** Escalate to CA for direction on the phone/NimBLE question and
  the PR-05 gate implications. WO4 (HRMClient) may proceed — needs rebase.

### 2026-10-14: CA test strategy approved — WO3c/WO3d added (PA session)
- **CA document:** `reviews/2026-10-14-wo3b-ble-test-strategy.md` (on `origin/main`
  @ `8e56da0`). **Supersedes** all earlier CA notes on WO3b (consolidated message,
  K/A correction lists, revised `t8-setup-plan.md`).
- **Owner approval:** 2026-10-14. Recorded here as provenance anchor.
- **WO3b evidence committed** (`8264444`): paper corrections (K1–K11), T6'
  `ANALYSIS.md` (446-line HCI/nRF cross-correlation, 6/12 pass rate). Raw data
  (serial logs, bugreport ZIP, HCI dumps) retained locally under gitignored
  `raw/` directories — contain WiFi SSID/BSSID.
- **WO3c added** (`ble-test-rig`, standard): automated test rig —
  `pixel_runner.py`, `esp32_log.py`, control peripheral (no WiFi), `snoop_parse.py`.
- **WO3d added** (`ble-ab-run`, interactive): A/B/A′ experiment (~50 attempts
  per batch). Decision rules pre-agreed: B ≥ 48/50 + A,A′ ≤ 85% → H-COEX
  confirmed → WO3e; B ≈ A → not coex; A/A′ disagree → rerun.
- **WO4 base changed** from `wo3b` to `wo3d`. WO4 branch (`90519d6`) needs
  rebase. WO4 may proceed in parallel (CA confirmed).
- **Key CA findings incorporated:**
  - F6: Pixel connect path is normal and identical every time (HCI verified)
  - F8: No failing attempt captured in HCI trace (ring buffer overwrite)
  - "Cold/warm" distinction retired → "clustering" (F3)
  - Success criteria for eventual fix: ≥ 95% Pixel (n ≥ 100), zero 30-min
    drops, P12 unchanged, 10/10 WiFi associations no AUTH_EXPIRE

### 2026-10-12: CA response — WO3b inserted, WO6 widened (PA session)
- **CA analysis:** The prime suspect for Android 0x3e is `WifiManager::applyPowerSave()`
  forcing `WIFI_PS_NONE` (`WifiManager.cpp` L230–231), violating ESP-IDF's
  coex requirement for modem sleep. The advertising interval is NOT the likely
  cause (phone discovered the device successfully). Secondary factor: Android
  phone's tighter initial connection interval.
- **WO3b inserted** (`hr-relay/wo3b-debug-android-connect`, interactive):
  CA-directed investigation — WiFi-off test, `WIFI_PS_MIN_MODEM` override,
  HCI snoop, adv interval only if inconclusive.
- **WO4 base changed** from `wo3` to `wo3b` — WO4 worker is mid-execution
  on the old base; will be rebased onto WO3b when both complete.
- **WO6 widened** to full v1 topology: HR strap (central) + P12 + Android
  phone + portal WebSocket 2 Hz + HTTPS POST, 5–10 min.
- **Programme gate added:** "v1 BLE topology proven" (WO6, including Android)
  required before PR-05. On failure, escalate to CA — do not pick architecture
  option.
- **Upstream issue:** File one app-neutral issue after WO3b with power-save /
  coex evidence. Do NOT file adv-interval issue yet.
- **Relative links:** CA noted committed docs must use relative paths, not
  `file:///`. Will fix in WO7 doc-sync.

### 2026-10-12: WO3 CONDITIONAL PASS — gate closed with risks (interactive session)
- **Branch:** `hr-relay/wo3-app-pairing-gate` at `5a0350f`.
- **Tag:** `v0.1.0-dev.8` (flashed `ex3-waveshare-nano-diag` with
  `WITH_HR_SYNTHETIC`).
- **Test 2.1 (advertising):** PASS — `Echenon-7226` visible in nRF Connect
  and bleak. GATT table: `0x180D` → `0x2A37` (Notify) + `0x2A38` (Read) +
  `0x180A` (DIS). 20/20 HR notifications at 1 Hz, sine 60–120 BPM confirmed.
- **Test 2.3 (app pairing gate):** PASS — Echelon Fit on Lenovo Tab P12
  discovered and paired `Echenon-7226`. App displayed synthetic HR (99 BPM
  observed). Clean connection: 48.75 ms interval, 5.0 s supervision timeout,
  MTU 185.
- **Verdict: CONDITIONAL PASS** — register row #12 closed for the P12 target.
  Two risks escalated to the Chief Architect.
- **Evidence:** `docs/measurements/phase2/A12-20261012-esp32-hr-pairing.md`.
- **Risk 1 — Android BLE 0x3e (BLOCKING for generic relay):** Google Pixel
  10 Pro Fold can discover but not connect. Same phone connects to eBeat
  via Strava. Root cause: `PREFER_WIFI` + `WIFI_PS_NONE` coex dead zone
  starves BLE connection events. Blocks Strava/Zwift/Wahoo phone use case.
  Framework-level fix required.
- **Risk 2 — Multi-connection radio coex (ARCHITECTURE):** Full Phase 2/3
  topology (1 central + 2 peripheral + WiFi STA on single ESP32-S3 radio)
  may be infeasible. If single-peripheral already fails Android, 3-connection
  is strictly harder. Requires CA review (DR-2 scope) to determine if
  architecture needs to change (WiFi-off, dual-chip, reduced conn count).
- **BLE risk report** written and delivered to owner for CA escalation.

### 2026-10-13: WO3b research phase complete — PAUSED for CA escalation (interactive session)
- **Branch:** `hr-relay/wo3b-debug-android-connect` at `633de75` (diag tag
  `v0.1.0-dev.9`).
- **CA corrections applied (2026-10-13 14:33):**
  - Device: Google Pixel 10 Pro Fold (not Samsung). Tablet: Lenovo Tab P12.
  - 0x3e mechanics: failure is in the establishment window (post-CONNECT_IND),
    not in discovery or advertising.
  - Removed unfounded "Samsung aggressive interval" reasoning.
  - CA revised the test order: Steps 0–6 (supersedes original A–D).
- **Baseline test executed (WiFi STA connected, `WIFI_PS_NONE`):**
  Mac ✅ (30ms/720ms), Tab P12 ✅ (48.75ms/5000ms), Pixel ❌ × 2 (0x3e).
- **Step 0 desk search DONE:** 13 esp-idf/nimble issues curated. Key finding:
  `WIFI_PS_NONE` is explicitly prohibited by ESP-IDF when BLE is active
  (esp-idf#9595, #2467, #11280). `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT`
  exists as a built-in mitigation.
- **SmartSpin2k codebase analysis DONE:** Shipping product on original ESP32
  runs BLE FTMS server + HR client + WiFi web UI using `PREFER_BALANCE` +
  `WIFI_PS_NONE`. Proves the combination works with additional mitigations
  (conn reattempt, BT controller modem sleep, paced WiFi reconnect).
- **Framework coex history reconstructed from PA memory:** dev.67–dev.83
  timeline. Critical finding: the dev.78 test of `PREFER_BALANCE` (which
  caused `AUTH_EXPIRE`) likely ran with modem sleep, not `WIFI_PS_NONE`.
  **`PREFER_BALANCE` + `WIFI_PS_NONE` was never tested together.**
- **Upstream issue #294 neutralised:** removed Echenon-specific references.
- **Evidence:**
  [`BLE-0x3e-debug-2026-10-13.md`](wo3b-debug-android-connect/../../../../docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md)
  (742 lines — serial evidence, desk search, SmartSpin2k comparison,
  framework coex history, architectural tension matrix, suggested test
  matrix for CA).
- **Decision needed from CA:** Should the hardware test plan include
  `PREFER_BALANCE` + `WIFI_PS_NONE` as an additional test case (not in the
  current WO3b plan)? This is potentially a framework-level fix that would
  resolve the 0x3e and benefit all consumer apps.
- **Hardware tests Steps 1–6 NOT STARTED** — paused pending CA review.

### 2026-10-12: WO2 complete (PA session)
- **Branch:** `hr-relay/wo2-hr-repeater` at `1e4a46d`.
- `HRMRepeater` (`IBLEServiceProvider`): `0x180D` / `0x2A37` (Notify) / `0x2A38` (Read).
  `Priority::Normal` for advertising. `getProviderId()` = `"hr_repeater"`.
- `HREvents.h`: `HrSample { bpm, contactState, receivedAtMs }` on topic `"echenon.hr"`.
  `HRContactState` enum: UNKNOWN, ACTIVE, NO_CONTACT, DISCONNECTED.
- `HREncoding.h`: Standalone `encode0x2A37()` — BLE-free, inline, testable.
  Flags `0x00` (uint8 BPM, no contact/energy/RR). BSL on `0x2A38` characteristic.
- `HRSynthetic`: `ITickable`, sine 60–120 BPM (10 s period), 1 Hz EventBus
  publish under `WITH_HR_SYNTHETIC`. Ring buffer for EventBus payload safety.
- Composition: `bleManager.registerServiceProvider(&echenon.hrmRepeater())`
  before `app.begin(config)`; `hrmRepeater.subscribe()` after `bleManager.begin(app)`.
- Native tests: 8 new `0x2A37` encoding tests (32/32 total pass).
- **Build metrics (with HRMRepeater):**
  - `ex3-waveshare-nano`: RAM 53,100 B (16.2%), Flash 1,523,979 B (24.2%)
  - `ex3-waveshare-nano-diag`: RAM 53,536 B (16.3%), Flash 1,549,127 B (24.6%)
- No flash (owner's parallel work). Build-only verification.
- **Next:** WO3 (interactive app pairing gate).

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
