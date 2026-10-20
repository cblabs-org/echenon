# Automated BLE Test Rig

Automated BLE connection testing rig for Echenon and ESP32 NimBLE peripherals.
Designed to execute controlled A/B/A′ experiments (WiFi + BLE vs. No-WiFi Control) and coexistence gating tests.

---

## Architecture & Components

```
tools/ble-test/
├── control-peripheral/       # Standalone ESP-IDF PlatformIO project (No-WiFi control)
│   ├── platformio.ini
│   ├── sdkconfig.defaults
│   └── src/main.cpp
├── dry-run/                  # Dry-run validation artifacts
├── esp32_log.py              # Host-side ESP32 serial logger with ISO 8601 timestamps
├── mac_runner.py             # macOS smoke runner using Bleak (third-client baseline)
├── pixel_runner.py           # Android nRF Connect test runner using ADB & uiautomator
├── requirements.txt          # Python host dependencies
├── snoop_parse.py            # BTSnoop / BTSnooz HCI log decoder (UTC/BST wall-clock)
└── README.md                 # This documentation
```

### Components Summary

1. **`pixel_runner.py`**:
   - Drives nRF Connect on Android via `adb` and `uiautomator` (dynamic view inspection, no hardcoded coordinates).
   - Monitors `adb logcat -s BluetoothGatt` for `onClientConnectionState()` (`status=0` = PASS, `147`/`133` = FAIL).
   - Holds active connection for 30 seconds after each PASS to evaluate connection stability and drop events (H-UPD).
   - Enforces a 30-second spacing interval between attempts.
   - Automatically triggers `adb bugreport` on the **first FAIL** of a batch (up to 2 bugreports per batch) to capture BTSnooz HCI ring buffers within ~5 minutes of failure.
   - Produces `attempts.csv` and Markdown `summary.md`.
   - Includes `--dry-run` mode for host-only verification without physical device actions.

2. **`esp32_log.py`**:
   - Captures ESP32 USB-serial output, prefixing every line with ISO 8601 host wall-clock timestamps for 1:1 cross-correlation with Android logcat and HCI traces.
   - Real-time extraction of BLE lifecycle events: `Central connected`, `Central disconnected` (with reason), and `Reattempt advertising; reason: 0x3e`.
   - Produces raw `esp32_serial_<batch_id>.log` and parsed `esp32_events_<batch_id>.csv`.

3. **`snoop_parse.py`**:
   - Parses BTSnooz base64 logs (from Android bug reports) and raw BTSnoop files (datalink type 1002 / H4).
   - Converts 64-bit microsecond timestamps into UTC and BST wall-clock times.
   - Filters and decodes LE connection commands and events (`LE_Create_Connection`, `LE_Enhanced_Connection_Complete`, `HCI_Disconnection_Complete`, `LE_Connection_Update`, etc.).
   - Outputs formatted chronological CLI timeline and `--csv` event export.

4. **`control-peripheral/`**:
   - Standalone PlatformIO ESP-IDF project (ESP32-S3, NimBLE 2.1.0, ESP-IDF 5.5.5).
   - **Zero WiFi headers or initialization** — serves as the isolated control for BLE coexistence hypothesis testing.
   - Identical BLE profile to Echenon: `Echenon-XXXX` device name, 50 ms legacy advertising interval, HR service `0x180D`, Heart Rate Measurement `0x2A37` (notifying synthetic 1 Hz heartbeat), Body Sensor Location `0x2A38` (Chest), MTU 185, +9 dBm TX power.
   - Identical serial log output format for seamless consumption by `esp32_log.py`.

5. **`mac_runner.py`**:
   - macOS host runner using `bleak`.
   - Validates peripheral connectivity and functions as a third-client smoke test.
   - Produces output in the same CSV format as `pixel_runner.py`.

---

## Prerequisites & Setup

### 1. Python Environment

Install host dependencies:
```bash
pip install -r tools/ble-test/requirements.txt
```
Dependencies:
- `pyserial>=3.5`
- `bleak>=0.21.0`

### 2. Android Device Setup (Pixel Runner)

- **Device**: Android phone (e.g. Google Pixel) connected via USB.
- **Developer Options**:
  - Enable **USB Debugging**.
  - Enable **Stay awake** (screen stays on while charging).
  - Enable **Bluetooth HCI snoop log** in Developer Options.
- **App**: Install **nRF Connect for Mobile** (Nordic Semiconductor).
- **Verify ADB**:
  ```bash
  adb devices
  ```
  Ensure the device is listed as `device` (authorized).

---

## Usage Guide

### 1. Pixel Test Runner (`pixel_runner.py`)

Run an automated test batch against the target peripheral:
```bash
python3 tools/ble-test/pixel_runner.py \
  --batch-id batch-a-wifi-01 \
  --attempts 50 \
  --evidence-dir docs/measurements/phase2/evidence/batch-a-wifi-01
```

**Options**:
- `--batch-id <id>`: Unique batch identifier (required).
- `--attempts <n>`: Number of connection attempts (default: 50).
- `--evidence-dir <dir>`: Directory where CSV, summary, and bug reports are stored (default: `.`).
- `--device-addr <addr>`: MAC address of peripheral (default: `3C:0F:02:DE:72:26`).
- `--hold-time <sec>`: Connection hold duration after connect in seconds (default: 30).
- `--spacing <sec>`: Inter-attempt spacing in seconds (default: 30).
- `--dry-run`: Simulate runner logic and generate mock reports without issuing ADB commands.

### 2. ESP32 Serial Logger (`esp32_log.py`)

Run in parallel with the test runner to capture ESP32 console events:
```bash
python3 tools/ble-test/esp32_log.py \
  --port /dev/cu.usbmodem1101 \
  --batch-id batch-a-wifi-01 \
  --output-dir docs/measurements/phase2/evidence/batch-a-wifi-01
```

**Options**:
- `--port <device>`: Serial port path (auto-detected if only one serial port is present).
- `--baud <rate>`: Baud rate (default: 115200).
- `--batch-id <id>`: Batch ID matching the runner.
- `--output-dir <dir>`: Directory to save `.log` and `.csv` files.

### 3. BTSnoop / BTSnooz Parser (`snoop_parse.py`)

Extract connection timelines directly from an Android bug report zip or raw btsnooz log:
```bash
# Parse directly from bug report zip
python3 tools/ble-test/snoop_parse.py \
  evidence/bugreport.zip \
  --peer 3C:0F:02:DE:72:26 \
  --csv evidence/hci_events.csv

# Or parse standalone btsnooz_hci.log
python3 tools/ble-test/snoop_parse.py \
  evidence/btsnooz_hci.log \
  --peer 3C:0F:02:DE:72:26
```

### 4. Control Peripheral Build (`control-peripheral/`)

The control peripheral must be built using PlatformIO MCP tools (per Rule 04):
- Use PlatformIO MCP tool `build_project` with `project_dir="tools/ble-test/control-peripheral"`.
- Flash via PlatformIO MCP tool `upload_firmware` with `project_dir="tools/ble-test/control-peripheral"`.

### 5. Mac Smoke Runner (`mac_runner.py`)

Perform a quick sanity check from macOS using Bleak:
```bash
python3 tools/ble-test/mac_runner.py \
  --batch-id mac-smoke-01 \
  --attempts 5 \
  --evidence-dir tmp/mac_smoke
```

---

## Output Formats

### `attempts.csv`
Generated by `pixel_runner.py` and `mac_runner.py`:
| Column | Description | Example |
|---|---|---|
| `batch_id` | Identifier for the experiment run | `batch-a-wifi-01` |
| `attempt_num` | 1-based attempt sequence number | `1` |
| `wall_clock_time` | ISO 8601 UTC timestamp of attempt start | `2026-10-14T22:30:00.123456Z` |
| `result` | Outcome | `PASS` or `FAIL` |
| `android_status` | GATT status code (`0` on pass, `147`/`133` on fail) | `0` |
| `latency_ms` | Latency from Connect trigger to result | `240.5` |
| `held_30s` | Boolean: was connection held for full 30s | `True` |
| `drop_time` | Timestamp of drop if dropped before 30s | `""` |

### `esp32_events_<batch_id>.csv`
Generated by `esp32_log.py`:
| Column | Description |
|---|---|
| `timestamp` | Host wall-clock ISO 8601 timestamp |
| `event_type` | `CENTRAL_CONNECTED`, `CENTRAL_DISCONNECTED`, or `REATTEMPT_ADV` |
| `handle` | NimBLE connection handle |
| `addr` | Client Bluetooth address |
| `reason` | Disconnect or reattempt reason code (e.g. `0x13`, `0x3e`) |
| `itvl` | Connection interval (slots) |
| `latency` | Connection latency |
| `timeout` | Supervision timeout |

---

## Batch Naming Conventions

To ensure consistency across A/B/A′ testing and coexistence gating, batches must use the following standard naming pattern:

- **Condition A (Echenon baseline — WiFi + BLE active)**:
  - `batch-a-wifi-01`, `batch-a-wifi-02`, ...
- **Condition B (Control peripheral — pure NimBLE, No WiFi)**:
  - `batch-b-nowifi-01`, `batch-b-nowifi-02`, ...
- **Condition A′ (Echenon re-test — return to baseline)**:
  - `batch-aprime-wifi-01`, `batch-aprime-wifi-02`, ...
- **Ad-hoc / Smoke / Dry-run batches**:
  - `dry-run-<timestamp>`
  - `mac-smoke-<id>`
