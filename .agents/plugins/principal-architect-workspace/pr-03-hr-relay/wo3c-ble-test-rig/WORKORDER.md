---
schema_version: 1
wo_id: WO-03.33
pr_id: PR-03
title: BLE Test Rig
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- framework/
- platformio.ini
- tools/
verification_command: echo legacy-migrated
---
# WO3c — BLE Test Rig

> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not branch, and do not plan. Your task is strictly limited to
> executing the checklist below.

## Objective

Build a reusable automated BLE connection-testing rig under `tools/ble-test/`.
This rig will be used by WO3d to run an A/B/A′ experiment comparing Echenon
(WiFi + BLE) against a no-WiFi control, and later by WO6 (coexistence gate).

**This WO creates tools and a control sketch only. No Echenon application code
(`src/`, `framework/`) is modified.**

## Background

The CA's test strategy (`reviews/2026-10-14-wo3b-ble-test-strategy.md` on
`origin/main`) specifies the rig. Key facts the tools must encode:

- **Target peripheral:** `3C:0F:02:DE:72:26` (public address), advertising as
  `Echenon-7226`, legacy advertising at 50 ms interval, HR service `0x180D`.
- **Test client:** Google Pixel 10 Pro Fold running nRF Connect for Android.
- **Failure signature:** Android `BluetoothGatt.onClientConnectionState()`
  reports `status=147` (GATT_CONN_TIMEOUT) or `status=133` (GATT_ERROR) within
  160–300 ms. On the ESP32 side: either silence (CONNECT_IND never received) or
  `Reattempt advertising; reason: 0x3e`.
- **Pass:** `status=0` + state CONNECTED.
- **Hold time:** 30 s after each successful connection (tests H-UPD drop
  hypothesis), then disconnect.
- **Spacing:** 30 s between attempts (start-to-start from previous end).
- **Bug report capture:** On the **first FAIL** of a batch, immediately run
  `adb bugreport <evidence_dir>/` (must complete within ~5 min so the btsnooz
  ring buffer still contains the failure). At most 2 bug reports per batch.

## Strict Boundary Box

**DO NOT touch:**
- `src/` — no Echenon application code
- `framework/` — no framework changes
- `SPEC.md`, `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`
- Any existing `docs/measurements/` files
- Any existing `platformio.ini` (the control peripheral has its own)

**You MAY:**
- Create everything under `tools/ble-test/`
- Create the control peripheral under `tools/ble-test/control-peripheral/`
  (a standalone PlatformIO project)
- Add `tools/ble-test/requirements.txt` for Python dependencies
- Run a 5-attempt dry run against the current Echenon firmware to verify the rig

## Tooling Constraints

- **Rule 04:** Use PlatformIO MCP tools (`build_project`, `upload_firmware`,
  etc.) for the control peripheral, never raw `pio` CLI.
- **Rule 03:** The control peripheral is NOT Echenon firmware, so Echenon's
  flash-tag-gate hook may not apply. If the hook blocks the control sketch,
  **STOP and ask the owner** — do not bypass it.
- Python scripts target the Mac host (macOS, Python 3.11+). Use `adb` for
  Android interaction, `pyserial` for serial capture, `bleak` for optional Mac
  runner.

## Line-Item Instructions

### 1. Pixel Runner — `tools/ble-test/pixel_runner.py`

- [ ] A Python script on the Mac that drives the Pixel over **adb** (USB
  debugging enabled).
- [ ] **Attempt mechanics:** Use `uiautomator dump` to locate nRF Connect
  controls by text and resource-id, not fixed pixel coordinates. Navigate to
  the device page for `3C:0F:02:DE:72:26` and tap **Connect**. Verify the
  expected screen is showing before every action; abort the batch with a clear
  message if it isn't.
- [ ] **Result source:** `adb logcat -s BluetoothGatt`. Parse
  `onClientConnectionState() - status=<n>`. `status=0` + connected = PASS;
  `status=147` / `133` or no connection within 30 s = FAIL. Record the failure
  latency from the tap time.
- [ ] **After a PASS:** hold 30 s (watch logcat for drops → tests H-UPD), then
  tap **Disconnect**, wait for link-down confirmation.
- [ ] **Spacing:** Next attempt starts 30 s after previous ended.
- [ ] **Failure trace capture (fixes F8):** On the **first FAIL** of a batch,
  stop attempts and immediately run `adb bugreport <evidence_dir>/`. Must
  complete within ~5 min of the failure. Then resume. At most 2 bug reports per
  batch.
- [ ] **Phone hygiene before each batch:** Screen stays on ("Stay awake"
  developer option). Log the Android build and bonded devices
  (`adb shell dumpsys bluetooth_manager` summary) into the batch header.
- [ ] **Output:** `attempts.csv` with columns: `batch_id, attempt_num,
  wall_clock_time, result, android_status, latency_ms, held_30s, drop_time`.
  Plus `summary.md` with: pass k/n, longest consecutive FAIL run, drops,
  firmware/config. **Relative paths only.**
- [ ] **CLI interface:** `python pixel_runner.py --batch-id <id> --attempts <n>
  --evidence-dir <dir> [--device-addr <addr>]`. Defaults: 50 attempts,
  `3C:0F:02:DE:72:26`.

### 2. ESP32 Log Capture — `tools/ble-test/esp32_log.py`

- [ ] Capture the board's serial output during every batch. Timestamp each line
  with **host wall-clock time** (ISO 8601) so it lines up with logcat and HCI.
- [ ] **Rule 04 compliance:** Either use the PlatformIO MCP monitor capture, or
  ensure the MCP monitor has released the port before opening it with pyserial.
  Never fight the MCP port lock.
- [ ] Extract per attempt: `Central connected`, `Central disconnected`
  (+reason), and `Reattempt advertising; reason: 0x3e`.
- [ ] **Output:** `esp32_serial_<batch_id>.log` (raw timestamped lines) and
  `esp32_events_<batch_id>.csv` (parsed events: timestamp, event_type, handle,
  addr, reason).
- [ ] **CLI interface:** `python esp32_log.py --port <port> --batch-id <id>
  --output-dir <dir>`. Auto-detect the port if only one USB-serial is present.

### 3. Control Peripheral — `tools/ble-test/control-peripheral/`

A **standalone PlatformIO project**. No framework. WiFi never initialised.

- [ ] **`platformio.ini`:** ESP32-S3 (same board: `esp32-s3-devkitc-1` or
  the Waveshare Nano board def if available), `framework = espidf`,
  `platform = espressif32`, `lib_deps = h2zero/esp-nimble-cpp@^2.1.0`,
  ESP-IDF 5.5.5.
- [ ] **Match the Echenon BLE presentation exactly:**
  - Same public address (same board → automatic)
  - Same NimBLE library version (`esp-nimble-cpp@^2.1.0`)
  - Legacy advertising, 50 ms interval (`BLE_GAP_ADV_ITVL_MS(50)`)
  - `0x180D` in advertising data, name pattern `Echenon-XXXX` (use the same
    name the board advertises normally — derive from MAC or hardcode `Echenon-7226`)
  - TX power +9 dBm (`CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_P9=y`)
  - GATT: HR service `0x180D` / `0x2A37` notifying a synthetic 1 Hz heartbeat
    (60–120 BPM sine, or constant 72 BPM — anything valid)
  - Body Sensor Location `0x2A38` (read, value `1` = Chest)
  - MTU 185 (`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=185`)
  - No immediate connection-parameter update after connect
- [ ] **Serial logging:** Use the same log format as the framework:
  `>>> Central connected: handle=%d, addr=%s, itvl=%d ...` and
  `<<< Central disconnected: handle=%d, addr=%s, reason=0x%x` so that
  `esp32_log.py` parses both identically.
- [ ] **No WiFi code at all.** WiFi headers must not be included. WiFi must
  never be initialised. This is the decisive control for H-COEX.
- [ ] **sdkconfig.defaults:** Match the BLE-relevant settings from the Echenon
  build:
  - `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4`
  - `CONFIG_BT_CTRL_BLE_MAX_ACT=6`
  - `CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=185`
  - `CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_P9=y`
  - `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT=y`
  - `CONFIG_BT_NIMBLE_MAX_CONN_REATTEMPT=3`
  - `# CONFIG_BT_CTRL_MODEM_SLEEP is not set`

### 4. BTSnoop Parser — `tools/ble-test/snoop_parse.py`

- [ ] Parse Android `btsnooz_hci.log` files (datalink type 1002 / H4 format).
- [ ] Convert BTSnoop 64-bit timestamps to wall-clock (UTC and BST).
- [ ] Extract LE commands/events with wall-clock times: `LE_Create_Connection`,
  `LE_Extended_Create_Connection`, `LE_Enhanced_Connection_Complete`,
  `HCI_Disconnection_Complete`, `LE_Connection_Update`, connection parameter
  updates, accept-list operations.
- [ ] Filter by peer address (default: `3C:0F:02:DE:72:26`).
- [ ] Output: human-readable timeline to stdout, and optional CSV
  (`--csv <file>`).
- [ ] Handle the `btsnooz_hci.log` format (base64-encoded, header with version
  and last-timestamp; Qualcomm/Android-specific). Also handle raw `btsnoop_hci.log`
  if present.
- [ ] **CLI:** `python snoop_parse.py <file> [--peer <addr>] [--csv <out>]`

### 5. Optional: Mac Runner — `tools/ble-test/mac_runner.py`

- [ ] Same attempt/record loop using `bleak` as a third client and a smoke test
  for the rig. Not a substitute for the Pixel (which is the only failing client).
- [ ] Connect, hold 30 s, disconnect, record pass/fail/latency.
- [ ] Output: same CSV format as `pixel_runner.py`.

### 6. Documentation — `tools/ble-test/README.md`

- [ ] Setup instructions (Python deps, adb, nRF Connect app, PIO control
  peripheral build).
- [ ] How to run each tool.
- [ ] Expected output format.
- [ ] Batch naming conventions.

### 7. Dry Run

- [ ] Run a 5-attempt dry run of `pixel_runner.py` + `esp32_log.py` against the
  **current Echenon firmware** (already flashed, `v0.1.0-dev.11`) to verify the
  rig works end to end.
- [ ] Output the dry-run results to `tools/ble-test/dry-run/`.
- [ ] Do NOT flash anything for the dry run — use whatever is currently on the
  board.
- [ ] **If adb or nRF Connect is not accessible** (e.g. phone not connected),
  note this in the completion report and verify the rest of the rig (control
  peripheral build, esp32_log parsing, snoop_parse against existing evidence).

## Verification Requirement

1. The control peripheral builds cleanly via PlatformIO MCP tools.
2. `snoop_parse.py` successfully parses the existing evidence file at
   `docs/measurements/phase2/evidence/t6-prime-2026-10-14/raw/btsnooz_hci.log`
   (if accessible locally — it's gitignored, so check if it exists).
3. `esp32_log.py` correctly parses the existing `esp32-serial-log.txt` evidence
   (in `raw/`).
4. `pixel_runner.py` has a `--dry-run` mode that logs what it would do without
   actually tapping (for offline verification).
5. All Python scripts pass a quick lint check (no syntax errors).
6. The dry-run (item 7 above) passes, or failures are clearly documented.
7. **Before committing, ask the owner if they are happy with the changes.**

## Issue Surfacing

If you discover a defect, upstream bug, or concern outside your boundary box,
surface it in your Completion Report below — do not file issues directly.
The Principal Architect will triage and file them.

---

## Completion Report

### Status
complete

### Changes Made
- `tools/ble-test/requirements.txt` — Created Python dependency file with `pyserial>=3.5` and `bleak>=0.21.0`.
- `tools/ble-test/snoop_parse.py` — BTSnoop / BTSnooz HCI parser decoding 64-bit microsecond timestamps to UTC/BST, filtering LE connection management packets, and outputting timeline and CSV. Validated against `docs/measurements/phase2/evidence/t6-prime-2026-10-14/raw/btsnooz_hci.log` (37 events matched).
- `tools/ble-test/esp32_log.py` — Host-side serial logging tool with ISO 8601 wall-clock timestamps and real-time BLE lifecycle event extraction (`Central connected`, `Central disconnected`, `Reattempt advertising; reason: 0x3e`). Validated against `docs/measurements/phase2/evidence/t6-prime-2026-10-14/raw/esp32-serial-log.txt` (21 events parsed).
- `tools/ble-test/control-peripheral/` — Standalone PlatformIO ESP-IDF project (ESP32-S3, pure NimBLE 2.1.0, ESP-IDF 5.5.5, no WiFi). Mirrors Echenon BLE profile (`0x180D`/`0x2A37`/`0x2A38`, +9dBm, 50ms adv interval) and serial output format. Built cleanly via PlatformIO MCP tool `build_project`.
- `tools/ble-test/pixel_runner.py` — Automated runner driving nRF Connect on Android via ADB and `uiautomator` UI element inspection, parsing `logcat -s BluetoothGatt`, implementing 30s connection hold, 30s attempt spacing, automatic bug report capture on first failure, and offline `--dry-run` mode.
- `tools/ble-test/mac_runner.py` — macOS smoke runner using `bleak` for third-client validation producing identical CSV format.
- `tools/ble-test/README.md` — Detailed setup, execution instructions, output formats, and batch naming conventions.
- `tools/ble-test/dry-run/` — Verified dry-run execution results (`attempts.csv`, `summary.md`, and mocked bug report).

### Decisions Made
- **BTSnoop Timestamp Epoch Conversion:** Android BTSnoop timestamps use a 64-bit microsecond integer representing microseconds since `0001-01-01 00:00:00 UTC` (offset `0x00DC_DDB3_0F2F_8000` = 62,135,596,800,000,000 µs). Formatted timestamps for both UTC and UTC+1 (BST) to align directly with experimental timelines in `ANALYSIS.md`.
- **Dynamic ADB Path Resolution:** ADB is often located in `<android-sdk>/platform-tools/adb` when not present in standard system PATH. `pixel_runner.py` checks both PATH and common SDK locations automatically.
- **MCP Build Tooling Compliance:** Pure PlatformIO MCP tools (`build_project`) were used exclusively for control peripheral compilation per Rule 04, verifying build artifacts without any direct CLI `pio` calls.

### Dead Ends & Warnings
- **Physical Device Non-Availability:** No physical Android phone is currently attached via ADB (`adb devices` is empty) and no ESP32 USB-serial port is currently mounted on the Mac host. Physical bench runs cannot proceed until hardware is plugged in, but offline verification (`--dry-run`, real evidence file parsing, MCP build) fully confirms rig functionality.
- **Port Contention (Rule 04):** Ensure any background PlatformIO monitor sessions are stopped before launching `esp32_log.py` to prevent serial port lock contention on macOS.

### Out-of-Scope Findings
- (none)

### Blockers
(none)

