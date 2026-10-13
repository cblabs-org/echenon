---
name: hr-relay-wo3b
description: CA-directed bench session — Android BLE 0x3e investigation with paper corrections
complexity: interactive
---

# WO3b — Debug Android BLE Connection Failure (CA-directed bench session)

> **PR:** PR-03 HR Relay (Phase 2) — v1 Passive
> **Project:** [PROJECT.md](../PROJECT.md)
> **Branch:** `hr-relay/wo3b-debug-android-connect` (base: `hr-relay/wo3-app-pairing-gate`)
> **Worktree:** `/Users/mcneillm/Documents/Projects/Echenon-hr-relay`
> **Directive:** Chief Architect review — owner-approved 2026-10-13

## Role Definition

> You are an expert senior software engineer working interactively with the
> owner on a time-boxed (≈2 hour) diagnostic bench session. You are NOT the
> Principal Architect. Execute the tests below in order, apply the paper
> corrections, and draft upstream issues.

## Background

An initial desk-research session produced
`docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`. The Chief Architect
reviewed it and identified 7 corrections (C1–C7) plus a 7-test bench plan
(T1–T7). This WORKORDER supersedes the original investigation plan.

---

## PART 1: PAPER CORRECTIONS (C1–C7)

Apply these to `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`.
Cross-check each one against the referenced files before editing.

### C1. Coex preference: the framework already runs BALANCE, not WIFI

Evidence to verify:
- ESP-IDF 5.5.5 header:
  `~/.platformio/packages/framework-espidf/components/esp_coex/include/esp_coexist.h`
  lines 115–126: `esp_coex_preference_set()` is `@deprecated`, doc comment
  says "Default, it prefer to balance."
- `grep -rn esp_coex_preference_set framework/modules src` → no calls.

Edits:
- Remove the "Key insight: PREFER_WIFI vs PREFER_BALANCE" argument.
- Remove the "razor's edge" rows that depend on it.
- Note that test-matrix rows 1–2 (`WIFI_PS_NONE` + `PREFER_BALANCE`) ARE
  the current, failing configuration.
- Add a finding: the framework comments in
  `framework/modules/BLEManager/BLEStack.cpp` (~L270–273) and
  `framework/hardware/_common/sdkconfig.ble.defaults` (~L302–315) wrongly
  call the default "PREFER_WIFI".

### C2. NimBLE connection reattempt is ALREADY enabled

Evidence: `sdkconfig.ex3-waveshare-nano` and `sdkconfig.ex3-waveshare-nano-diag`
contain:
```
CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT=y
CONFIG_BT_NIMBLE_MAX_CONN_REATTEMPT=3
```
This produces the "Reattempt advertising; reason: 0x3e" log line. In the
peripheral role it only restarts advertising; it cannot fix a central-side
failure.

Edit: change "Not set ❌" to "Already enabled". Remove it from proposed
mitigations.

### C3. Do NOT recommend Bluetooth controller modem sleep

Evidence: ESP-IDF issue
[espressif/esp-idf#2467](https://github.com/espressif/esp-idf/issues/2467)
(cited in the paper) is titled "LE Connection fail rate 10-15% after enabling
BT Modem sleep".
Current build: `# CONFIG_BT_CTRL_MODEM_SLEEP is not set` (correct for
mains-powered).

Edit: remove it from the SmartSpin2k mitigations to adopt.

### C4. WIFI_PS_NONE is the LEADING SUSPECT, not a confirmed cause

- [espressif/esp-idf#9595](https://github.com/espressif/esp-idf/issues/9595)
  is a crash bug that Espressif fixed. It does not show that `WIFI_PS_NONE`
  is prohibited.
- Our serial log prints "Coexist!!! Wi-Fi station would only keep waked when
  available" — suggests the coex scheduler still time-slices under PS_NONE.

Edits:
- Rename "Confirmed mechanism" to "Leading hypothesis".
- Remove "WiFi never yields / no idle slots" as a stated fact.

### C5. Fix issue statuses in Appendix A

- [espressif/esp-idf#17871](https://github.com/espressif/esp-idf/issues/17871)
  is CLOSED (paper says Open).
- [espressif/esp-idf#12948](https://github.com/espressif/esp-idf/issues/12948)
  is OPEN and concerns ESP32-C3 (paper says Closed).

From now on, write every issue reference as `owner/repo#number` with a full
URL. Framework issues: `matthewmcneill/esp32-app-foundation#N`.

### C6. Record the SPEC/build divergence — DO NOT change it

- SPEC.md §8.5 lists `CONFIG_ESP_COEX_POWER_MANAGEMENT=y`.
- Built sdkconfig has `# CONFIG_ESP_COEX_POWER_MANAGEMENT is not set`.

Add this as an open finding for the CA. Do NOT edit SPEC.md.

### C7. Reinterpret SmartSpin2k

SmartSpin2k runs `WIFI_PS_NONE` + default (BALANCE) coex preference — the
same as us — and works. So the difference must be elsewhere.

List these candidates in the paper:
- Chip/controller: classic ESP32 vs ESP32-S3
- ESP-IDF version
- Our WiFi traffic during tests (diag dashboard polling `/api/echenon/live`,
  mDNS)
- Diag build's ADC DMA / interrupt load (`WITH_SIGNAL_LOGGER`)

---

## PART 2: BENCH TESTS (T1–T7)

**Definitions** (use for every attempt):
- **ATTEMPT** = Tap Connect once in nRF Connect (or the app). Wait up to 30 s.
  Disconnect fully, then wait 10 s before the next attempt.
- **PASS** = ESP32 serial log shows ">>> Central connected" for that client
  AND the client lists service 0x180D within 30 s.
- **FAIL** = Serial log shows "Reattempt advertising; reason: 0x3e", OR no
  connection within 30 s.
- For every attempt, record: **test ID, client, attempt number, PASS/FAIL,
  timestamp, WiFi state (STA connected / AP only / off).**
- Unless a step says otherwise, use the current diag firmware.

### T1. Pixel HCI snoop (during a normal failing attempt)

1. Pixel: Settings → Developer options → "Enable Bluetooth HCI snoop log" →
   turn BT off/on → reproduce one failure → "Take bug report" (full).
2. Extract `btsnoop_hci.log` and open in Wireshark.
3. Record from the phone's LE Create Connection / CONNECT_IND:
   - Connection interval (min/max)
   - Supervision timeout
   - ChSel (CSA#2) bit
   - PHY
   - Phone's own connection-complete status code

### T2. Phone-side isolation

Disconnect every other Bluetooth device from the Pixel (watch, earbuds, car,
etc.). **10 attempts.**

### T3. CONTROL — the decisive test: BLE with NO WiFi at all

Build a bare NimBLE heart-rate peripheral sketch that **never starts WiFi**:
- Advertise 0x180D
- Send synthetic HR
- Build as a SEPARATE scratch PlatformIO project under `tmp/ble-control/`
  (not in `src/`, not committed)

Flash to the same board. **10 attempts with Pixel; 10 with P12.**

Do NOT use `BLEConfig.advertiseInConfiguring` — in CONFIGURING the softAP is
still transmitting, so WiFi is not idle.

If the rule-03 flash hook blocks the scratch sketch, stop and ask the owner.

**Decision point:**
- Pixel PASSES ≥ 9/10 → **coexistence confirmed.** Continue to T4.
- Pixel FAILS → **not coexistence** (interop or phone-side). Skip T4, go to T5.

### T4. Only if T3 passed: WIFI_PS_MIN_MODEM override

Apply a **local-only** override in `src/main.cpp` (after `app.begin()`):
```cpp
#include <WiFi.h>
WiFi.setSleep(WIFI_PS_MIN_MODEM);
```
Do NOT commit this to the PR.

1. **T4a:** 10 Pixel attempts with STA connected and portal dashboard CLOSED.
2. **T4b:** 10 Pixel attempts with the dashboard OPEN.
3. **T4c:** 10 WiFi association cycles on the owner's network (reboot → STA
   connected). Record connect time and any AUTH_EXPIRE.

### T5. Repeat decisive result on DEFAULT env

Repeat the decisive test (T3 if it failed, T4a otherwise) on the DEFAULT env
(`ex3-waveshare-nano`, no `WITH_SIGNAL_LOGGER`). **10 attempts.**

### T6. Baseline rates — CURRENT firmware, STA connected

**10 attempts each** for the P12 and the Mac (bleak).

### T7. WiFi-dependency check

With the current firmware:
1. Strap connected and relaying HR to the P12.
2. Power off the router for 3 minutes.
3. Switch the strap off and on.

Record: AppState transitions (serial), whether the HR relay continues, and
whether the strap reconnects.

---

## PART 3: RULES

- **DO NOT** choose or implement a coexistence fix. Do not pick architecture
  option (A–F).
- **DO NOT** edit anything under `framework/`. Any fix belongs in the
  framework repo as an app-neutral issue/PR.
- **Write issue DRAFTS** (do not file them) for:
  - (i) The mislabelled coex default and the deprecated API (C1)
  - (ii) The WiFi power-save policy, depending on T3/T4 results
  - (iii) BLE requiring a STA connection (T7)
  Use no Echenon names in drafts.
- **DO NOT** edit `SPEC.md`, `PROGRAM*.md`, or `reviews/*`.
- Evidence doc must use **relative links only** (no `file:///` or `/Users/`).
- **Flash only with owner permission.**

## Rules — Tag/Build/Flash

- **Rule 03 (Tag → Build → Flash):** Tag before flash. Dev tags are
  autonomous. For the scratch sketch in T3, ask the owner if the hook blocks.
- **Rule 04 (PlatformIO MCP tools only):** Use MCP tools; never `pio` CLI.

---

## PART 4: WHEN DONE

1. Revert all local diagnostic overrides (`WIFI_PS_MIN_MODEM`, adv interval
   patches, scratch sketches). The commit must match the WO3 codebase plus
   evidence.
2. Commit the updated paper and evidence.
3. Update the WO3b Completion Report: per-test results, plus a "Leading
   hypothesis after testing" paragraph.
4. Give the owner this return prompt:
   > "WO3b bench session complete. Chief Architect: read
   > `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md` (T1–T7 results)
   > and the three upstream issue drafts, then decide the coex direction."

---

## Completion Report (written by the worker at task completion)

<!-- Written by the worker at task completion -->

### Status
(pending)

### Paper Corrections Applied
- C1 (coex preference): (pending)
- C2 (conn reattempt): (pending)
- C3 (BT modem sleep): (pending)
- C4 (leading hypothesis): (pending)
- C5 (issue statuses): (pending)
- C6 (SPEC divergence): (pending)
- C7 (SmartSpin2k reinterpretation): (pending)

### Test Results
- T1 (HCI snoop): (pending)
- T2 (phone isolation): (pending)
- T3 (no-WiFi control): (pending)
- T4 (PS_MIN_MODEM, if run): (pending)
- T5 (default env): (pending)
- T6 (baseline): (pending)
- T7 (WiFi dependency): (pending)

### Leading Hypothesis After Testing
(pending)

### Upstream Issue Drafts
- (i) Coex default mislabelling: (pending)
- (ii) WiFi power-save policy: (pending)
- (iii) BLE requiring STA: (pending)

### Out-of-Scope Findings
- (pending)
