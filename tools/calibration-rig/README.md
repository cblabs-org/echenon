# Echelon Calibration Sweep — Host Script

Drives the ESP32 **CalibrationRig** firmware module over serial to sweep all
32 resistance levels at each of 10 cadence steps (40–130 RPM), capturing the
power reading displayed by the Echelon Fit Android app via ADB.

Produces a CSV:

```
level,cadence_rpm,power_watts,pot_adc,timestamp
```

## Prerequisites

| Requirement | Notes |
|---|---|
| Python 3.8+ | Standard CPython |
| pyserial | `pip install -r requirements.txt` |
| adb | Android SDK Platform Tools — must be on `PATH` |
| Echelon Fit app | Running on an Android phone connected via USB |
| ESP32 CalibrationRig | Flashed with CalibrationRig firmware, connected via USB serial |
| pytesseract + Pillow | **Only** if using `--adb-method screencap` fallback |

## Quick Start

```bash
# 1. Install Python dependencies
pip install -r requirements.txt

# 2. Verify ADB connection to phone
adb devices          # should list one device

# 3. Dry-run (serial only, no ADB) to validate rig communication
python3 calibration_sweep.py --port /dev/ttyACM0 --dry-run

# 4. Full sweep
python3 calibration_sweep.py --port /dev/ttyACM0 --output sweep_data.csv
```

## CLI Reference

```
usage: calibration_sweep.py [-h] [--port PORT] [--baud BAUD] [--output OUTPUT]
                            [--dry-run] [--settle-wait SECONDS]
                            [--read-delay SECONDS] [--timeout SECONDS]
                            [--adb-method {uiautomator,screencap}]

Options:
  --port PORT          Serial port (default: /dev/ttyACM0)
  --baud BAUD          Baud rate (default: 115200)
  --output OUTPUT      Output CSV path (default: sweep_output.csv)
  --dry-run            Serial commands only, no ADB. Records "DRY_RUN" for power.
  --settle-wait SECS   Extra wait after SETTLED before reading (default: 4.0)
  --read-delay SECS    Delay between ADB reads (default: 1.0)
  --timeout SECS       Per-point timeout before retry (default: 60)
  --adb-method METHOD  uiautomator or screencap (default: uiautomator)
```

## Sweep Matrix

- **Outer loop:** cadence — 40, 50, 60, 70, 80, 90, 100, 110, 120, 130 RPM
- **Inner loop:** resistance level — 1 through 32 (ascending)
- **Total:** 320 data points

Between cadence changes, the script sends `HOME` to return the rig to level 1
before setting the new cadence.

## Resume Capability

If the output CSV already exists, the script reads it on startup, identifies
completed data points, and skips them. This allows you to restart after a
failure without losing captured data.

Points recorded with `ERROR`, `ADB_ERROR`, or `PARSE_ERROR` are considered
incomplete and will be re-attempted.

## Serial Protocol

The script speaks the CalibrationRig serial protocol at 115200 baud:

| Direction | Command | Description |
|---|---|---|
| Host → ESP32 | `SET level=<1-32> cadence=<0-200>` | Seek to level, set cadence |
| Host → ESP32 | `HOME` | Return to level 1 |
| Host → ESP32 | `STATUS` | Query current state |
| Host → ESP32 | `STOP` | Stop cadence, hold position |
| ESP32 → Host | `SETTLED level=N pot=ADC cadence=RPM` | Motor settled at target |
| ESP32 → Host | `SEEKING level=N pot=ADC` | Still seeking target |
| ESP32 → Host | `ERROR <msg>` | Error occurred |

## Error Handling

- **Serial timeout** — retries once with `HOME` + re-seek; records `ERROR` if
  the retry also fails
- **ADB failure** — retries 3 times with 2s backoff; records `ADB_ERROR`
- **Missing power in XML/OCR** — records `PARSE_ERROR`
- **KeyboardInterrupt** — flushes CSV, prints summary, exits cleanly
- All errors logged to stderr with timestamps

## ADB Methods

### `uiautomator` (default, preferred)

Uses `adb shell uiautomator dump` to capture the Echelon Fit app's UI
hierarchy as XML, then parses it for the power/watts value.

### `screencap` (fallback)

Captures a screenshot via ADB and uses pytesseract OCR to extract the power
value. Requires `pytesseract` and `Pillow`:

```bash
pip install pytesseract Pillow
# Also requires Tesseract OCR engine installed on the host
```
