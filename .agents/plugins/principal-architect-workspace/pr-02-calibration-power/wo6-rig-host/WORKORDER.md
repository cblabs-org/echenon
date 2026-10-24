---
wo_id: WO-02.6
pr_id: PR-02
programme_id: prog-echenon
title: "Host Script (software-only pre-work)"
slug: wo6-rig-host
branch: "calibration-power/wo6-rig-host"
base_branch: "calibration-power/wo5-rig-firmware"
complexity: interactive
status: IN_PROGRESS
attempts: 1
spec_hash: null
allowed_files:
  - tools/calibration-rig/calibration_sweep.py
  - tools/calibration-rig/requirements.txt
  - tools/calibration-rig/README.md
verification_command: "python3 -c 'import ast; ast.parse(open(\"tools/calibration-rig/calibration_sweep.py\").read()); print(\"syntax ok\")'"
required_skills: []
---

# WO-02.6 — Host Script (Software-Only Pre-Work)

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo6-rig-host` from `calibration-power/wo5-rig-firmware`
> **Charter sections:** §8 (host + preflight) of the PR-02 Work Order Ledger
> **Note:** This execution covers ONLY the software deliverables. Physical wiring,
>   ADB preflight, and the dry-run bench session are deferred to the owner.

## 1. Objective

Write the Python host-side calibration sweep script that communicates with the
ESP32 CalibrationRig firmware module over serial, captures power readings from
the Echelon Fit Android app via ADB, and produces a CSV suitable for loading
into the PowerTable.

## 2. Strict Boundary Box

### Allowed files (create)
- `tools/calibration-rig/calibration_sweep.py` — main sweep script
- `tools/calibration-rig/requirements.txt` — Python dependencies
- `tools/calibration-rig/README.md` — usage documentation

### Out of scope
- Physical wiring, ADB preflight, dry-run testing (owner bench work)
- `docs/measurements/phase1/` evidence (WO-02.7)
- Any firmware files — do NOT modify src/, hardware/, platformio.ini, etc.
- Flashing or building — PA handles this separately

## 3. Specification

### 3.1 Serial Protocol (CalibrationRig firmware)

The ESP32 firmware implements this serial command protocol at 115200 baud:

**Commands (host → ESP32):**
- `SET level=<1-32> cadence=<0-200>` — seek to a target level and set cadence RPM
- `HOME` — return to level 1 (31 levels of reverse encoder pulses)
- `STATUS` — report current state
- `STOP` — stop cadence simulation, hold encoder position

**Responses (ESP32 → host):**
- `SETTLED level=N pot=ADC cadence=RPM` — motor has settled at the target level
- `SEEKING level=N pot=ADC` — still seeking (pot not yet settled)
- `ERROR <message>` — an error occurred

**Settle behaviour:** After a `SET` command, the rig drives encoder quadrature
pulses to the target level. The rig monitors the pot ADC and reports `SETTLED`
when the pot reading matches the expected B03 fit value (±15 counts) for 500 ms.

### 3.2 ADB Screen Reading

The script reads the power value from the Echelon Fit app running on an
Android phone connected via USB:

**Primary method: `uiautomator dump`**
- `adb shell uiautomator dump /sdcard/window_dump.xml`
- `adb pull /sdcard/window_dump.xml`
- Parse the XML for the watts/power field
- Look for text nodes containing the power value (likely near "Output (watts)")

**Fallback method: screencap + OCR**
- `adb shell screencap -p /sdcard/screenshot.png`
- `adb pull /sdcard/screenshot.png`
- Use pytesseract OCR to extract the power value from a known screen region
- This is fallback only — `uiautomator` is strongly preferred

### 3.3 Sweep Logic

**Matrix:** 32 levels × 10 cadence steps = 320 data points
- **Cadence steps:** 40, 50, 60, 70, 80, 90, 100, 110, 120, 130 RPM
- **Levels:** 1 through 32
- **Outer loop:** cadence (hold cadence constant, sweep all levels)
- **Inner loop:** resistance level (ascending 1→32)

**Per-point procedure:**
1. Send `SET level=L cadence=C`
2. Wait for `SETTLED` response (poll `STATUS` every 500 ms if needed)
3. Wait an additional 3–5 seconds for the Echelon app to stabilise its reading
4. Capture the power value from the app via ADB
5. Record: `level, cadence_rpm, power_watts, pot_adc, timestamp`
6. If timeout (60 s without SETTLED): retry once with HOME + re-seek
7. If retry fails: record ERROR, continue to next point

**Between cadence changes:** Send `HOME` to return to level 1, then set new
cadence before starting the next inner loop.

### 3.4 CSV Output

**File:** `tools/calibration-rig/sweep_output.csv` (default, configurable via `--output`)

**Columns:**
```
level,cadence_rpm,power_watts,pot_adc,timestamp
```

**Resume capability:** On startup, if the output CSV exists, read it, skip
completed points, and continue from where it left off. This allows restarting
after failures without losing data.

### 3.5 CLI Arguments

```
usage: calibration_sweep.py [-h] [--port PORT] [--baud BAUD] [--output OUTPUT]
                            [--dry-run] [--settle-wait SECONDS]
                            [--read-delay SECONDS] [--timeout SECONDS]
                            [--adb-method {uiautomator,screencap}]

Options:
  --port PORT          Serial port (default: auto-detect or /dev/ttyACM0)
  --baud BAUD          Baud rate (default: 115200)
  --output OUTPUT      Output CSV path (default: sweep_output.csv)
  --dry-run            Serial commands only, no ADB. Print "DRY_RUN" for power.
  --settle-wait SECS   Extra wait after SETTLED before reading (default: 4.0)
  --read-delay SECS    Delay between ADB reads (default: 1.0)
  --timeout SECS       Per-point timeout before retry (default: 60)
  --adb-method METHOD  uiautomator or screencap (default: uiautomator)
```

### 3.6 Error Handling

- Serial timeout → retry with HOME + re-seek
- ADB failure → retry 3 times with 2s backoff, then record "ADB_ERROR"
- Missing power value in XML/OCR → record "PARSE_ERROR"
- KeyboardInterrupt → flush CSV, print summary, exit cleanly
- All errors logged to stderr with timestamps

## 4. Quality Requirements

- Python 3.8+ compatible
- No runtime imports beyond: `serial` (pyserial), `subprocess`, `argparse`,
  `csv`, `time`, `datetime`, `os`, `sys`, `re`, `xml.etree.ElementTree`,
  `pathlib`, `logging`
- `pytesseract` imported conditionally only when `--adb-method screencap`
- Script is executable (`#!/usr/bin/env python3`)
- Well-structured with functions for: serial communication, ADB reading,
  sweep orchestration, CSV I/O
- Clear logging throughout (INFO for progress, WARNING for retries, ERROR for
  failures)

## 5. Verification

Since this is a host script (not firmware), verification is:
1. Python syntax check: `python3 -c "import ast; ast.parse(open('tools/calibration-rig/calibration_sweep.py').read()); print('syntax ok')"`
2. `--help` runs without error
3. Code review of the serial protocol commands matches §3.1

## 6. Commit

Message: `WO-02.6: Calibration sweep host script`
Stage ONLY: `tools/calibration-rig/calibration_sweep.py`,
`tools/calibration-rig/requirements.txt`, `tools/calibration-rig/README.md`
Do NOT commit: framework submodule, any firmware files
