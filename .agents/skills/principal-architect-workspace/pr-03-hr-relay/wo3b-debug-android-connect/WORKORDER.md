---
name: hr-relay-wo3b
description: CA-directed bench session — cold-protocol BLE investigation (consolidated CA review 2026-10-14)
complexity: interactive
---

# WO3b — Debug Android BLE Connection Failure (Consolidated CA Review)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo3b-debug-android-connect` (base: `hr-relay/wo3-app-pairing-gate`)
> **Worktree:** `<worktree>`
> **Directive:** Chief Architect consolidated review — owner-approved 2026-10-14.
>   Supersedes all earlier CA notes on WO3b.

## Role Definition

> You are an expert senior software engineer continuing the WO3b interactive
> session. The desk research and C1–C7 paper corrections are done. This session
> focuses on the cold-protocol bench tests (T6', T1b, T8', T3, T4, T5, T7, T9)
> and finishing the paper.
>
> You are NOT the Principal Architect. Do NOT trigger or act as PA.

## Prior Work (do not repeat)

- **Paper corrections C1–C7:** All applied and committed (`0fb9a4d`).
- **Partial bench tests:** T1 (HCI snoop), T2 (phone isolation), T6 (partial
  baseline) done in the prior session.
- **Key discovery:** Cold-start connection failure pattern. After ≥10 min idle,
  first 1–4 Pixel connection attempts fail; once one succeeds, rapid cycling
  is 100% reliable.
- **Prior interim writeup:** Available at the artifact path but the paper in
  `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md` is authoritative.

## CA's Reading of the Evidence (Part 1)

1. **Owner observation (O1):** The P12 (Echelon app) connects on the FIRST
   attempt even straight after an ESP32 reset, and immediately after a Pixel
   disconnect. The cold-start failure has only been seen on the Pixel 10 Pro
   Fold.
2. **PR-03 is NOT blocked.** UC1 HR relay to P12 works. The phone question
   gates PR-05 (phone FTMS), not PR-03. WO4 proceeds in parallel.
3. **Leading hypothesis:** WiFi/BLE coexistence. The Pixel's tighter connection
   window (18.75 ms interval → ~112 ms to establish, vs ~290 ms for the P12)
   makes it the first client to lose out when the radio is busy.
   **Competing hypothesis:** A Pixel-side connection path that differs between
   cold and warm attempts (e.g. accept-list connect, multi-PHY initiation).
   Neither is proven. The tests decide.
4. **Rapid-cycle results are NOT valid evidence from now on.** Use the COLD
   protocol for every comparison.

---

## Part 2: Paper Corrections Still To Do (K1–K11)

Apply these to `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`.
Cross-check each against the referenced files.

### K1. Rename "ESP-IDF Source Code Confirmation"
→ "Source review: status per finding". Mark each item Verified / Plausible /
Not applicable, per K2–K6.

### K2. F2 (advertising priority values 0x05 / 0x0E–0x10): NOT VERIFIABLE
`esp32s3.rom.ld:917` gives only the ADDRESS of `rwip_priority`; a linker
script contains no values. Remove the numbers or cite a real source.

### K3. F3 (`_coex_core_ble_conn_dyn_prio_get = NULL`): VERIFIED
At `components/bt/controller/esp32c3/bt.c:474`. The S3 builds this controller
per `components/bt/CMakeLists.txt:7–8`. Its effect inside the closed
controller is an inference → **PLAUSIBLE**.

### K4. F4 (6-event establishment deadline): cite Bluetooth Core Spec
Remove the citation of `host/nimble/nimble/nimble/controller/src/ble_ll_conn.c`:
that is the open-source NimBLE controller, which the ESP32-S3 does NOT use.

### K5. F5 (controller wake-up latency, clock drift after idle): NOT APPLICABLE
Build has `# CONFIG_BT_CTRL_MODEM_SLEEP is not set`. Those defines only act
in the sleep path (`bt.c:1145–1152`). Remove it.

### K6. Mechanisms 2 and 4: no evidence
Add alternatives: (a) WiFi activity differed between test sets; (b) a
phone-side cold vs warm connection path.

### K7. "ESP32 logs nothing" interpretation
Means `CONNECT_IND` was never received: either the radio was unavailable
during the advertiser's receive window, or the request was addressed
elsewhere. T1 snoop captured no failure against the ESP32 → T1b is required.

### K8. Record WiFi-scan finding (CONFIRMED by CA)
- `framework/web/portal/wifi.js:550–561`: WiFi tab auto-starts a scan when opened.
- `framework/modules/WifiManager/WifiManager.cpp:632`: MCP scan uses 120 ms/channel.
ESP-IDF warns: "Should use default active scan time parameter for WiFi scan
when Bluetooth is enabled".

### K9. Firmware hygiene
Record what changed between v0.1.0-dev.9 and dev.10, and which diagnostic
overrides (`sdkconfig.diag.defaults`, `platformio.ini`) were active for each
existing result. Re-label T6 accordingly.

### K10. Add owner observation O1
(Part 1, item 1 above) to the paper.

### K11. Remove every `file:///` and `/Users/` link
All links must be relative before committing.

---

## Part 3: Cold Test Protocol (applies to every remaining test)

**COLD attempt =**
1. No BLE connection to the ESP32 from ANY client for **≥ 3 minutes**
   (unless a test says otherwise).
2. Portal **closed** in every browser/tab. **Never open the WiFi tab** (it
   starts a scan). No MCP calls.
3. ONE connect attempt from the client. Wait up to 30 s.
   - **PASS** = ESP32 serial shows ">>> Central connected" AND the client
     lists service 0x180D.
   - **FAIL** = anything else. Also note whether the serial showed "0x3e" or
     nothing at all.

**Primary metric** = first-attempt PASS count out of 6 cold attempts.

For every attempt record: **test ID, firmware tag + active overrides, client,
PASS/FAIL, what the serial logged, timestamp.**

---

## Part 4: Tests — execute in this order

### T6' Baseline, current firmware, cold protocol
- **P12 (Echelon app) ×6.** This is also the PR-03 acceptance check; expected
  6/6.
- **Pixel (nRF Connect) ×6.**

### T1b TOP PRIORITY — Pixel HCI snoop with cold failure + warm success
One Pixel HCI snoop capture containing **ONE cold FAILURE** and **ONE warm
SUCCESS**.

Pixel: Developer options → Enable Bluetooth HCI snoop log → reproduce →
Take bug report → `btsnoop_hci.log` → Wireshark.

Compare cold vs warm **side by side**:
- The command used: `LE Create Connection` vs `LE Extended Create Connection`,
  and the initiator filter policy (direct address vs accept list)
- The peer address (does it match the ESP32's current address?)
- Initiating PHYs
- Connection interval and supervision timeout
- Phone-side completion status and disconnect reason

**State clearly what differs.**

### T8' Does recent ESP32 activity "warm" the Pixel?
The P12 connects and then disconnects; within 10 s, ONE Pixel attempt (the
Pixel itself having been idle ≥ 3 min). **6 trials.**
- Pixel passes → the warm state is on the **ESP32 side** (radio/coex/WiFi).
- Pixel fails → the warm state is on the **phone side**.

### T3 CONTROL — decisive for coex
A bare NimBLE HR peripheral that **never starts WiFi** (advertise 0x180D,
synthetic HR), built as a separate scratch PlatformIO project under
`tmp/ble-control/` (not in `src/`, not committed).

Same board, cold protocol, **Pixel ×6.**

If the rule-03 flash hook blocks the scratch sketch, stop and ask the owner.

**Decision point:**
- Pixel ≥ 5/6 where T6' failed → **coex confirmed** → run T4.
- No improvement → **NOT coex** → skip T4, continue with T5 and T9, escalate.

### T4 Only if T3 confirms coex
Echenon diag firmware with a **LOCAL override**
`WiFi.setSleep(WIFI_PS_MIN_MODEM)` after `app.begin()` (not committed):
- **T4a:** Pixel ×6, cold protocol.
- **T4b:** P12 ×6, cold protocol (must not regress).
- **T4c:** 10 WiFi association cycles on the owner's network (connect time,
  any AUTH_EXPIRE).

### T5 Repeat best config on DEFAULT env
Repeat the decisive test (T3 if it failed, T4a otherwise) on the DEFAULT env
(`ex3-waveshare-nano`, no `WITH_SIGNAL_LOGGER`), cold protocol, **Pixel ×6.**

### T7 WiFi dependency
Strap relaying to the P12, router powered off for 3 min, then switch the
strap off and on. Record: AppState transitions (serial), whether the HR
relay continues, and whether the strap reconnects.

### T9 Risk sizing for PR-05 (if available)
One more modern phone (non-Pixel Android, or an iPhone), cold protocol ×6.

### Optional (ask owner first, only if T3/T4 ambiguous)
Diag build with `CONFIG_ESP_COEX_GPIO_DEBUG=y`, coex grant lines captured on
the owner's Rigol DHO804 alongside a GPIO toggled on BLE connect events.
Do NOT experiment with `esp_coex_status_bit_set()`: public BLE status bits
are mesh-only.

---

## Part 5: Rules

- **DO NOT** choose or implement a coex fix or architecture option (A–F).
  Escalate the results.
- **DO NOT** edit anything under `framework/`, `SPEC.md`, `PROGRAM*.md`, or
  `reviews/*`.
- Write upstream issue **DRAFTS** (do NOT file them; CA reviews first) for
  `matthewmcneill/esp32-app-foundation`. Keep them app-neutral, no Echenon
  names:
  - **(i)** Coex comments mislabel the default as `PREFER_WIFI` and rely on
    the deprecated `esp_coex_preference_set()`.
  - **(ii)** WiFi power-save policy with BLE enabled (written after T3/T4).
  - **(iii)** BLE requires a STA connection: no BLE at boot without STA, and
    `RUNNING→CONFIGURING` regression stops reconnects (written after T7;
    refs `Application.cpp:471`, `BLEManager.cpp ~L855`).
  - **(iv)** WiFi scans use a non-default active scan time and auto-start
    when the WiFi tab opens, while BLE is active.
- Revert all diagnostic overrides before committing. Commit only docs and
  evidence.
- Evidence doc must use **relative links only** (no `file:///` or `/Users/`).
- **Flash only with owner permission.**
- **Rule 03 (Tag → Build → Flash):** Tag before flash. For scratch sketch in
  T3, ask owner if the hook blocks.
- **Rule 04 (PlatformIO MCP tools only):** Use MCP tools; never `pio` CLI.

---

## Part 6: Return

1. Update `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md` with
   K1–K11 and one section per test (T6', T1b, T8', T3, T4, T5, T7, T9).
   Each section has an attempt-level table and a one-line result.
2. Update the WO3b Completion Report (below) with per-test results and a
   "Leading hypothesis after testing" paragraph.
3. Commit to `hr-relay/wo3b-debug-android-connect`.
4. Give the owner this prompt:
   > "WO3b complete. Chief Architect: review the cold-protocol results (T6',
   > T1b, T8', T3–T9) and the four upstream issue drafts, then decide the
   > coex direction and the PR-05 gate."

---

## Completion Report (written by the worker at task completion)

<!-- Written by the worker at task completion -->

### Status
(pending)

### Paper Corrections Applied (K-series)
- K1 (rename section): (pending)
- K2 (F2 not verifiable): (pending)
- K3 (F3 verified/plausible): (pending)
- K4 (F4 cite Core Spec): (pending)
- K5 (F5 not applicable): (pending)
- K6 (mechanisms 2+4 alternatives): (pending)
- K7 (CONNECT_IND interpretation): (pending)
- K8 (WiFi scan finding): (pending)
- K9 (firmware hygiene): (pending)
- K10 (owner observation O1): (pending)
- K11 (relative links): (pending)

### Test Results (cold protocol)
- T6' (baseline cold): (pending)
- T1b (HCI snoop cold vs warm): (pending)
- T8' (ESP32 warm-up): (pending)
- T3 (no-WiFi control): (pending)
- T4 (PS_MIN_MODEM, if run): (pending)
- T5 (default env): (pending)
- T7 (WiFi dependency): (pending)
- T9 (second phone, if available): (pending)

### Leading Hypothesis After Testing
(pending)

### Upstream Issue Drafts
- (i) Coex default mislabelling: (pending)
- (ii) WiFi power-save policy: (pending)
- (iii) BLE requiring STA: (pending)
- (iv) WiFi scan timing: (pending)

### Out-of-Scope Findings
- (pending)
