# WO3b: BLE Connection Reliability Test Strategy

| | |
|---|---|
| **From** | Chief Architect (Tier 1) |
| **To** | Principal Architect, PR-03 (HR Relay) |
| **Date** | 2026-10-14 |
| **Status** | **Owner-approved** strategy (chat, 2026-10-14: *"please write this whole test strategy into a document that we can send to a principal-architect"*). The PA records this approval in `pr-03-hr-relay/PROJECT.md` before spawning any worker. |
| **Supersedes** | Every earlier CA note on WO3b: the consolidated message, the K/A correction lists and the revised `t8-setup-plan.md`. Where they conflict, this document wins. |
| **Evidence so far** | `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`; `docs/measurements/phase2/evidence/t6-prime-2026-10-14/` (on branch `hr-relay/wo3b-debug-android-connect`) |

---

## 1. Why this document exists

The investigation has run in manual bench sessions. They are too slow for the
owner and too small to separate the hypotheses: with a 4/6 baseline, a
6-attempt test reports "≥ 5/6" 35% of the time when nothing has changed. This
document replaces manual batches with **an automated rig plus one decisive
experiment**, run mostly unattended.

**Owner time budget:** about 15 minutes hands-on per batch series. Everything
else is automated.

---

## 2. Current position (what we know)

| # | Fact | Confidence |
|---|---|---|
| F1 | **Lenovo Tab P12 + Echelon app** connects first time, every time, including straight after an ESP32 reset. The owner considers hidden app retries unlikely | Owner observation |
| F2 | **Mac (bleak)** has always connected (30 ms interval) | Small sample |
| F3 | **Pixel 10 Pro Fold + nRF Connect** single attempts: 6/12 passed in T6'. Failures **cluster** (a failure tends to be followed by more for tens of seconds); retries eventually succeed. Each failure returns in 160–300 ms (Android status 147, then 133) | T6' data |
| F4 | On most Pixel failures the ESP32 logs **nothing** (it never received CONNECT_IND). Once, it logged `0x3e` (received CONNECT_IND, then missed the first connection events) | ESP32 serial |
| F5 | The 160–300 ms failure time is consistent with a genuine link-layer establishment failure: no packet in the first 6 connection events, ≈ 112 ms at 18.75 ms, plus overhead | Bluetooth Core spec |
| F6 | The Pixel's connect path is normal and identical every time: it cancels its background accept-list connection to a paired device (`EF:BE:A7:3A:81:C6`, probably the owner's watch), adds the ESP32 to the accept list, then runs LE Extended Create Connection (60/30 ms scan, 10–20 ms interval, 1M+2M+Coded). Success takes 10–80 ms; the interval moves to 48.75 ms after 2.5–6 s | Pixel btsnooz, parsed by the CA |
| F7 | The ESP32 address is **public and stable** (`3C:0F:02:DE:72:26`) | Pixel btsnooz |
| F8 | **No failing attempt has been captured in an HCI trace.** The bug report holds only `btsnooz_hci.log`, a ~12-minute ring buffer covering 13:40:20–13:52:23 BST, while all failures happened 13:29:56–13:40:00. The full `btsnoop_hci.log` was not present | CA parse of the raw files |
| F9 | Two **established** links dropped (supervision timeout). In one, the phone disconnected normally but the ESP32 timed out ~5 s later, as if it never heard the phone's terminate packet. The phone's ~5 s of 100%-duty scanning after each connect is **not** the cause: surviving links had it too | ESP32 serial + btsnooz |
| F10 | Radio configuration: WiFi power-save `PS_NONE` (framework-forced); coex preference = ESP-IDF default (**balance**; the framework comments wrongly say "prefer WiFi"); NimBLE `CONN_REATTEMPT=y`; BT controller modem sleep off; BLE TX +9 dBm; 50 ms legacy advertising; ESP-IDF 5.5.5 | Build sdkconfig + ESP-IDF headers |
| F11 | The portal WiFi tab **auto-starts a WiFi scan** when opened, and framework scans use non-default timing (ESP-IDF warns about this with BT enabled) | Framework source |
| F12 | **BLE depends on WiFi STA**: no BLE at boot without STA; sustained STA loss regresses the app to CONFIGURING, where BLE reconnect and discovery stop | Boot case observed; mid-session case from code |

## 3. Hypotheses

| ID | Hypothesis | Status |
|---|---|---|
| **H-COEX** | WiFi holds the shared radio at critical BLE moments, so the ESP32 misses CONNECT_IND (silent failure) or early connection events (`0x3e`), and occasionally packets on live links | **Leading.** Fits F3, F4, F5 and F9 |
| **H-PHONE** | The Pixel's own stack or controller mishandles the connection | **Open.** Needs a failure in the HCI trace |
| H-WIN | The Pixel's short establishment window (~112 ms vs ~292 ms on the P12) increases its sensitivity | Contributing at most; it doesn't explain silent failures |
| H-UPD | Established links drop after the connection-parameter update | **Unresolved** (2 cases) |
| Retired | Cold/warm idle effect; address rotation; hidden P12 app retries; coex preference setting | Contradicted or ruled out |

**The decisive question:** does the Pixel failure rate collapse when the
ESP32 has **no WiFi at all**? The same board, the same phone, the same
automated runner, with only WiFi changing.

---

## 4. The rig to build (an autonomous WO)

Add **WO3c `ble-test-rig`** (`complexity: standard`) to the PR-03 ledger, on
branch `hr-relay/wo3c-ble-test-rig`, stacked on the current WO3b branch.
Everything below is **committed** under `tools/ble-test/`. It is reusable for
WO6 (coexistence) and the PR-05 "v1 BLE topology proven" gate.

### 4.1 Pixel runner: `tools/ble-test/pixel_runner.py`

A Python script on the Mac, driving the Pixel over **adb** (USB debugging on).

- **Attempt:** bring nRF Connect to the device page for `3C:0F:02:DE:72:26` and
  tap **Connect** once. Use `uiautomator dump` to locate controls by text and
  resource-id, not fixed coordinates. Verify that the expected screen is
  showing before every action; abort the batch with a clear message if it
  isn't.
- **Result source:** `adb logcat`. Android's `BluetoothGatt` logs
  `onClientConnectionState() - status=<n>`. `status=0` + connected = PASS;
  `status=147` / `133`, or no connection within 30 s = FAIL. Record the
  failure latency from the tap.
- **After a PASS:** hold the connection for **30 s** (watching for drops, which
  tests H-UPD), then tap **Disconnect**, and wait until the link is confirmed
  down.
- **Spacing:** start the next attempt **30 s** after the previous one ended,
  for both passes and failures.
- **Failure trace capture (fixes F8):** on the **first FAIL of a batch**, stop
  attempts and immediately run `adb bugreport <evidence_dir>/`. It must complete
  within ~5 min of the failure so the btsnooz ring buffer still contains it.
  Then resume. Capture at most 2 bug reports per batch.
- **Phone hygiene before each batch:** screen stays on (the "Stay awake"
  developer option), no other foreground apps, Bluetooth on. Log the Android
  build and the list of bonded devices (`adb shell dumpsys bluetooth_manager`
  summary) into the batch header.
- **Output:** `attempts.csv` (batch id, #, wall-clock time, result, Android
  status, latency_ms, held_30s Y/N, drop time if any), plus `summary.md` (pass
  k/n, longest run of consecutive FAILs, drops, firmware/config). Use relative
  paths only.

### 4.2 ESP32 log capture: `tools/ble-test/esp32_log.py`

Capture the board's serial output during every batch, and timestamp each line
with **host wall-clock time** so it lines up with logcat and the HCI trace.
Rule 04 applies: either use the PlatformIO MCP monitor capture, or make sure the
MCP monitor has released the port before opening it. Never fight the MCP port
lock. Extract per attempt: `Central connected`, `Central disconnected`
(+reason), and `Reattempt advertising; reason: 0x3e`.

### 4.3 No-WiFi control peripheral: `tools/ble-test/control-peripheral/`

A **standalone PlatformIO project**: no framework, and WiFi never initialised.
**No framework changes are needed for this test.**

- It must match the Echenon BLE presentation as closely as possible:
  - the same board (so the same public address);
  - the same NimBLE library and version (`esp-nimble-cpp@^2.1.0`, ESP-IDF 5.5.5);
  - the **same advertising** (legacy, 50 ms interval, 0x180D in the advertising
    data, the same name pattern);
  - the **same TX power (+9 dBm)**;
  - the same GATT (HR service 0x180D / 0x2A37 notifying a synthetic 1 Hz
    value);
  - the same post-connect behaviour (no immediate connection-parameter
    update; MTU 185).
- Log the same serial lines as Echenon (`>>> Central connected …`,
  `<<< Central disconnected … reason=…`), so §4.2 parses both.
- Why not switch WiFi off inside the Echenon firmware: the framework's
  WifiManager would keep reconnecting (scans), and STA loss regresses the app
  state (F12). Both would contaminate the control.
- Build and flash with the PlatformIO MCP tools. Rule 03 (Tag → Build → Flash)
  applies to Echenon firmware. If the flash hook blocks the control sketch,
  **stop and ask the owner**; do not bypass it.

### 4.4 Optional: Mac runner `tools/ble-test/mac_runner.py`

The same attempt/record loop using `bleak`, as a third client and a smoke test
for the rig. It is not a substitute for the Pixel, which is the only client
that fails.

### WO3c boundary

- Tools, docs and the control sketch only.
- **No changes to `src/`, `framework/`, `SPEC.md`, `PROGRAM*.md` or `reviews/*`.**
- Native unit tests for the CSV/summary logic, if they're cheap.
- Verify the rig end to end with a 5-attempt dry run on the current firmware.

---

## 5. The experiment (an interactive WO: the owner starts it, then it runs unattended)

Add **WO3d `ble-ab-run`** (`complexity: interactive`). Write a bridge prompt
for the owner.

### Fixed conditions for all batches
- The Pixel at ~1 m from the board, in the same position; the P12 and the Mac
  not connecting to the board.
- Portal closed in every browser; no MCP calls during a batch; the router and
  the home network as normal.
- Leave the watch (`EF:BE:…`) paired as normal; it reflects real-world use. One
  optional extra batch with the watch's Bluetooth off is listed below.
- Record the firmware tag, build env and all diagnostic overrides in each batch
  header. Use the **default** env (`ex3-waveshare-nano`, no
  `WITH_SIGNAL_LOGGER`) for Echenon batches unless the owner says otherwise.

### Batches (run in order, ~50 attempts each, about 45–60 min each)

| Batch | Firmware on the board | Purpose |
|---|---|---|
| **A** | Echenon, WiFi STA connected | Baseline rate, clustering, drops. First failure → bug report |
| **B** | **Control peripheral (no WiFi)** | **Decisive for H-COEX** |
| **A′** | Echenon again (same build as A) | Checks that conditions didn't drift between A and B |
| C (optional) | Echenon, with the watch's Bluetooth off | Phone-side factor (H-PHONE) |

### Decision rules (decided before the run)

| Outcome | Conclusion | Next step |
|---|---|---|
| B ≥ 48/50, and A and A′ both clearly lower (≤ 85%) | **H-COEX confirmed** | WO3e: power-save experiment using the same rig. Echenon with a local `WIFI_PS_MIN_MODEM` override, and a hybrid (PS_NONE while associating, MIN_MODEM once connected), each one batch. Plus 10 WiFi association cycles on the owner's network (connect time, `AUTH_EXPIRE`). **Escalate to the CA before WO3e** |
| B ≈ A (within ~10 points) | **Not coex.** H-PHONE / interop | Analyse the captured failure trace (§6); escalate to the CA. Do not run WO3e |
| A and A′ disagree by > 15 points | Conditions drifted; the result is inconclusive | Find the drift (network load, position, phone state); rerun A/B/A′ |
| Drops (H-UPD) in any batch | A separate finding | Report the drop times relative to the parameter update and the ESP32 vs phone reasons |

---

## 6. Trace analysis (for any captured failure)

From each bug report: extract `FS/data/misc/bluetooth/logs/btsnooz_hci.log`.
Confirm that its time span contains the failure time from `attempts.csv`. If it
doesn't, say so; don't infer. Then compare the failing attempt with a passing
one:

1. the create-connection command and parameters;
2. whether the phone reports **LE Connection Complete status 0x00 followed by a
   Disconnection Complete with reason 0x3E**, or something else;
3. whether the ESP32 serial shows anything at that moment;
4. what the phone's controller was doing just before (background scans,
   cancels).

The CA's parser for btsnoop files is in its scratchpad. Re-implement a
committed version as `tools/ble-test/snoop_parse.py` (LE commands/events with
wall-clock times).

---

## 7. Success criteria for the eventual fix (agreed in advance)

Used to accept any coex change, later, in the framework PR:
- Pixel single-attempt success **≥ 95%** (n ≥ 100, using this rig);
- **zero** established-link drops across 30-minute holds on the Pixel and the P12;
- P12 + Echelon app first-time connect unchanged;
- **10/10** WiFi associations on the owner's network, with no `AUTH_EXPIRE`.

---

## 8. Rules (unchanged)

- Do **not** choose or implement a coex fix or architecture option. Escalate
  the results to the CA.
- Any fix belongs in **matthewmcneill/esp32-app-foundation** (framework PR,
  app-neutral). Echenon carries only local diagnostic overrides, never
  committed.
- Write upstream issue **drafts** for CA review, and do not file them:
  (i) the mislabelled coex default and deprecated API;
  (ii) the WiFi power-save policy (after WO3d/WO3e);
  (iii) BLE requires a STA connection (F12);
  (iv) WiFi scan timing and the auto-scan on tab open (F11).
- Correct the existing WO3b docs where they conflict with §2. In particular:
  - the "Android stack race" is unverified (F8);
  - the 160–300 ms failure time supports a link-layer failure (F5);
  - "cold-start" is retired (F3 clustering).
- Use relative links only in committed docs; never `file:///` or `/Users/`.
- **PR-03 is not blocked.** WO4 (HRMClient) may proceed in parallel. The phone
  question gates **PR-05**.

## 9. Return

After WO3d (and WO3e if it was triggered and approved):
1. Commit the batch evidence under `docs/measurements/phase2/evidence/`.
2. Update the WO3b paper's summary with the A/B/A′ result.
3. Give the owner this return prompt:
   *"WO3d complete. Chief Architect: review the A/B/A′ batch results
   (tools/ble-test output under docs/measurements/phase2/evidence/), the
   captured failure trace, and the upstream issue drafts, then decide the coex
   direction."*
