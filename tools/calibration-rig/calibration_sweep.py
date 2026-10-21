#!/usr/bin/env python3
"""
Echelon Calibration Sweep — Host Script
========================================

Drives the ESP32 CalibrationRig firmware over serial to sweep all 32 resistance
levels at each of 10 cadence steps (40–130 RPM), capturing the power reading
from the Echelon Fit Android app via ADB.

Produces a CSV:  level, cadence_rpm, power_watts, pot_adc, timestamp

Usage:
    python3 calibration_sweep.py --port /dev/ttyACM0
    python3 calibration_sweep.py --dry-run          # serial only, no ADB
    python3 calibration_sweep.py --help
"""

from __future__ import annotations

import argparse
import csv
import logging
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from xml.etree import ElementTree

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

LEVELS = list(range(1, 33))                          # 1 … 32
CADENCE_STEPS = list(range(40, 131, 10))             # 40, 50, … 130
CSV_HEADER = ["level", "cadence_rpm", "power_watts", "pot_adc", "timestamp"]

DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 115200
DEFAULT_OUTPUT = "sweep_output.csv"
DEFAULT_SETTLE_WAIT = 4.0   # seconds after SETTLED before ADB read
DEFAULT_READ_DELAY = 1.0    # seconds between consecutive ADB reads
DEFAULT_TIMEOUT = 60        # seconds to wait for SETTLED before retry
STATUS_POLL_INTERVAL = 0.5  # seconds between STATUS polls

ADB_RETRY_COUNT = 3
ADB_RETRY_BACKOFF = 2.0     # seconds

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

log = logging.getLogger("calibration_sweep")


def _setup_logging() -> None:
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(
        logging.Formatter("%(asctime)s [%(levelname)s] %(message)s",
                          datefmt="%Y-%m-%dT%H:%M:%S")
    )
    log.addHandler(handler)
    log.setLevel(logging.INFO)


# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------

class RigSerial:
    """Thin wrapper around pyserial for the CalibrationRig protocol."""

    def __init__(self, port: str, baud: int, timeout: float = 2.0):
        import serial  # pyserial
        self._ser = serial.Serial(port, baud, timeout=timeout)
        # Drain any boot messages
        time.sleep(0.5)
        self._ser.reset_input_buffer()
        log.info("Serial opened: %s @ %d", port, baud)

    def send(self, command: str) -> None:
        """Send a command string (newline-terminated) to the rig."""
        line = command.strip() + "\n"
        self._ser.write(line.encode("ascii"))
        self._ser.flush()
        log.info("TX → %s", command.strip())

    def readline(self, timeout: float | None = None) -> str:
        """Read one line from the rig.  Returns stripped text or ''."""
        old_timeout = self._ser.timeout
        if timeout is not None:
            self._ser.timeout = timeout
        try:
            raw = self._ser.readline()
        finally:
            self._ser.timeout = old_timeout
        text = raw.decode("ascii", errors="replace").strip()
        if text:
            log.debug("RX ← %s", text)
        return text

    def close(self) -> None:
        self._ser.close()
        log.info("Serial closed.")


def _wait_for_settled(
    rig: RigSerial,
    timeout: float,
) -> tuple[bool, int, str]:
    """
    Poll until a SETTLED response or *timeout* seconds elapse.

    Returns:
        (settled, pot_adc, raw_line)
        settled is False on timeout or ERROR.
    """
    deadline = time.monotonic() + timeout
    last_status_time = 0.0

    while time.monotonic() < deadline:
        line = rig.readline(timeout=STATUS_POLL_INTERVAL)
        if not line:
            # Nothing received — send a STATUS query periodically
            now = time.monotonic()
            if now - last_status_time >= STATUS_POLL_INTERVAL:
                rig.send("STATUS")
                last_status_time = now
            continue

        if line.startswith("SETTLED"):
            pot = _parse_pot(line)
            return True, pot, line

        if line.startswith("ERROR"):
            log.error("Rig error: %s", line)
            return False, 0, line

        # SEEKING or other informational lines — keep waiting
        if line.startswith("SEEKING"):
            log.info("  %s", line)

    log.warning("Timeout waiting for SETTLED (%ds)", timeout)
    return False, 0, ""


def _parse_pot(line: str) -> int:
    """Extract pot=NNN from a SETTLED or SEEKING line."""
    m = re.search(r"pot=(\d+)", line)
    return int(m.group(1)) if m else 0


# ---------------------------------------------------------------------------
# ADB helpers
# ---------------------------------------------------------------------------

def _adb_read_power_uiautomator() -> str:
    """
    Use ``adb shell uiautomator dump`` to capture the Echelon Fit UI tree,
    then parse the XML for the power / watts value.

    Returns the power string (e.g. ``"145"``) or a sentinel on failure.
    """
    try:
        subprocess.run(
            ["adb", "shell", "uiautomator", "dump", "/sdcard/window_dump.xml"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        result = subprocess.run(
            ["adb", "pull", "/sdcard/window_dump.xml", "/tmp/window_dump.xml"],
            capture_output=True, text=True, timeout=15, check=True,
        )
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        log.warning("ADB uiautomator command failed: %s", exc)
        raise

    try:
        tree = ElementTree.parse("/tmp/window_dump.xml")
    except ElementTree.ParseError as exc:
        log.warning("XML parse error: %s", exc)
        raise

    root = tree.getroot()
    return _extract_watts_from_xml(root)


def _extract_watts_from_xml(root: ElementTree.Element) -> str:
    """
    Walk the uiautomator XML tree looking for the watts / power field.

    The Echelon Fit app typically shows a label like "Output (watts)" near
    the numeric power value.  We search for text nodes matching common
    patterns.

    Returns the power string or raises ValueError.
    """
    all_texts: list[str] = []
    for node in root.iter("node"):
        text = node.get("text", "").strip()
        if text:
            all_texts.append(text)

    # Strategy 1: look for a pure numeric node immediately after
    #             a label containing "watts" or "output"
    for i, text in enumerate(all_texts):
        lower = text.lower()
        if "watt" in lower or "output" in lower:
            # Check the next few text nodes for a number
            for j in range(i + 1, min(i + 4, len(all_texts))):
                candidate = all_texts[j].strip()
                if re.fullmatch(r"\d+", candidate):
                    return candidate

    # Strategy 2: look for any text node that is purely numeric and
    #             plausibly a watts reading (1–2000)
    for text in all_texts:
        if re.fullmatch(r"\d+", text):
            val = int(text)
            if 0 <= val <= 2000:
                return text

    raise ValueError(
        f"Could not locate watts value in UI dump. "
        f"Captured texts: {all_texts[:30]}"
    )


def _adb_read_power_screencap() -> str:
    """
    Fallback: capture a screenshot via ADB and OCR it with pytesseract.

    Returns the power string or raises on failure.
    """
    try:
        import pytesseract
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError(
            "pytesseract and Pillow are required for --adb-method screencap. "
            "Install with:  pip install pytesseract Pillow"
        ) from exc

    try:
        subprocess.run(
            ["adb", "shell", "screencap", "-p", "/sdcard/screenshot.png"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        subprocess.run(
            ["adb", "pull", "/sdcard/screenshot.png", "/tmp/screenshot.png"],
            capture_output=True, text=True, timeout=15, check=True,
        )
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        log.warning("ADB screencap command failed: %s", exc)
        raise

    img = Image.open("/tmp/screenshot.png")
    ocr_text: str = pytesseract.image_to_string(img)

    # Look for a number near "watts" or "output"
    lines = ocr_text.splitlines()
    for i, line in enumerate(lines):
        lower = line.lower()
        if "watt" in lower or "output" in lower:
            numbers = re.findall(r"\d+", line)
            if numbers:
                return numbers[-1]
            # Check adjacent lines
            for j in range(max(0, i - 1), min(len(lines), i + 3)):
                nums = re.findall(r"\d+", lines[j])
                if nums:
                    return nums[-1]

    # Broad fallback
    all_nums = re.findall(r"\d+", ocr_text)
    for n in all_nums:
        val = int(n)
        if 1 <= val <= 2000:
            return n

    raise ValueError(f"OCR could not extract watts from screenshot.")


def read_power(adb_method: str, read_delay: float, dry_run: bool) -> str:
    """
    Read the power value from the Echelon Fit app.

    Returns the power as a string, or a sentinel value on failure.
    """
    if dry_run:
        return "DRY_RUN"

    time.sleep(read_delay)

    reader = (
        _adb_read_power_uiautomator
        if adb_method == "uiautomator"
        else _adb_read_power_screencap
    )

    last_exc: Exception | None = None
    for attempt in range(1, ADB_RETRY_COUNT + 1):
        try:
            return reader()
        except Exception as exc:
            last_exc = exc
            log.warning(
                "ADB read attempt %d/%d failed: %s",
                attempt, ADB_RETRY_COUNT, exc,
            )
            if attempt < ADB_RETRY_COUNT:
                time.sleep(ADB_RETRY_BACKOFF * attempt)

    log.error("All ADB read attempts exhausted: %s", last_exc)

    # Distinguish parse vs ADB errors
    if isinstance(last_exc, ValueError):
        return "PARSE_ERROR"
    return "ADB_ERROR"


# ---------------------------------------------------------------------------
# CSV I/O
# ---------------------------------------------------------------------------

def load_completed_points(path: Path) -> set[tuple[int, int]]:
    """
    Read an existing CSV and return the set of (level, cadence) pairs
    already captured successfully (i.e. not ERROR / ADB_ERROR / PARSE_ERROR).
    """
    completed: set[tuple[int, int]] = set()
    if not path.exists():
        return completed

    with open(path, "r", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            try:
                level = int(row["level"])
                cadence = int(row["cadence_rpm"])
                power = row.get("power_watts", "")
                # Only count as complete if we got a real reading
                if power and power not in ("ERROR", "ADB_ERROR", "PARSE_ERROR", "DRY_RUN"):
                    completed.add((level, cadence))
            except (KeyError, ValueError):
                continue

    log.info("Loaded %d completed data points from %s", len(completed), path)
    return completed


def open_csv_writer(
    path: Path,
    resume: bool,
) -> tuple:
    """
    Open (or create) the CSV file for appending.

    Returns (file_handle, csv_writer).
    """
    write_header = not path.exists() or not resume
    fh = open(path, "a", newline="")
    writer = csv.writer(fh)
    if write_header:
        writer.writerow(CSV_HEADER)
        fh.flush()
    return fh, writer


# ---------------------------------------------------------------------------
# Sweep orchestration
# ---------------------------------------------------------------------------

def run_sweep(
    rig: RigSerial,
    output_path: Path,
    dry_run: bool,
    settle_wait: float,
    read_delay: float,
    timeout: float,
    adb_method: str,
) -> None:
    """Execute the full 32×10 calibration sweep."""

    completed = load_completed_points(output_path)
    resume = len(completed) > 0
    total_points = len(LEVELS) * len(CADENCE_STEPS)
    done_count = len(completed)

    fh, writer = open_csv_writer(output_path, resume)
    try:
        for cadence in CADENCE_STEPS:
            log.info(
                "═══ Cadence %d RPM ═══  (%d/%d points done)",
                cadence, done_count, total_points,
            )

            # HOME between cadence changes
            rig.send("HOME")
            _wait_for_settled(rig, timeout=30)
            time.sleep(1.0)

            for level in LEVELS:
                if (level, cadence) in completed:
                    log.info(
                        "  Skip level=%d cadence=%d (already captured)",
                        level, cadence,
                    )
                    continue

                power, pot_adc = _run_single_point(
                    rig, level, cadence, timeout, settle_wait,
                    read_delay, adb_method, dry_run,
                )

                ts = datetime.now(timezone.utc).isoformat(timespec="seconds")
                writer.writerow([level, cadence, power, pot_adc, ts])
                fh.flush()
                done_count += 1

                log.info(
                    "  ✓ L=%02d C=%03d → %s W  pot=%d  [%d/%d]",
                    level, cadence, power, pot_adc, done_count, total_points,
                )

    except KeyboardInterrupt:
        log.warning("Interrupted — flushing CSV and exiting.")
    finally:
        fh.close()

    log.info("Sweep complete.  %d/%d points captured → %s",
             done_count, total_points, output_path)


def _run_single_point(
    rig: RigSerial,
    level: int,
    cadence: int,
    timeout: float,
    settle_wait: float,
    read_delay: float,
    adb_method: str,
    dry_run: bool,
) -> tuple[str, int]:
    """
    Execute a single measurement point.

    Returns (power_str, pot_adc).
    """
    rig.send(f"SET level={level} cadence={cadence}")
    settled, pot_adc, raw = _wait_for_settled(rig, timeout)

    if not settled:
        # Retry once: HOME → re-seek
        log.warning("  Retrying level=%d cadence=%d (HOME + re-seek)", level, cadence)
        rig.send("HOME")
        _wait_for_settled(rig, timeout=20)
        time.sleep(1.0)

        rig.send(f"SET level={level} cadence={cadence}")
        settled, pot_adc, raw = _wait_for_settled(rig, timeout)

        if not settled:
            log.error("  Failed to settle at level=%d cadence=%d", level, cadence)
            return "ERROR", pot_adc

    # Extra wait for the Echelon app to stabilise its display
    time.sleep(settle_wait)

    # Capture power reading from the app
    power = read_power(adb_method, read_delay, dry_run)
    return power, pot_adc


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Echelon Calibration Sweep — drive the CalibrationRig over serial "
            "and capture power readings from the Echelon Fit app via ADB."
        ),
    )
    parser.add_argument(
        "--port", default=DEFAULT_PORT,
        help=f"Serial port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--baud", type=int, default=DEFAULT_BAUD,
        help=f"Baud rate (default: {DEFAULT_BAUD})",
    )
    parser.add_argument(
        "--output", default=DEFAULT_OUTPUT,
        help=f"Output CSV path (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help='Serial commands only, no ADB. Records "DRY_RUN" for power.',
    )
    parser.add_argument(
        "--settle-wait", type=float, default=DEFAULT_SETTLE_WAIT,
        metavar="SECONDS",
        help=f"Extra wait after SETTLED before reading power (default: {DEFAULT_SETTLE_WAIT})",
    )
    parser.add_argument(
        "--read-delay", type=float, default=DEFAULT_READ_DELAY,
        metavar="SECONDS",
        help=f"Delay between ADB reads (default: {DEFAULT_READ_DELAY})",
    )
    parser.add_argument(
        "--timeout", type=float, default=DEFAULT_TIMEOUT,
        metavar="SECONDS",
        help=f"Per-point timeout before retry (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--adb-method", choices=["uiautomator", "screencap"],
        default="uiautomator",
        help="ADB screen reading method (default: uiautomator)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    _setup_logging()
    args = parse_args(argv)

    output_path = Path(args.output)

    log.info("Echelon Calibration Sweep")
    log.info("  Port:         %s", args.port)
    log.info("  Baud:         %d", args.baud)
    log.info("  Output:       %s", output_path)
    log.info("  Dry-run:      %s", args.dry_run)
    log.info("  ADB method:   %s", args.adb_method)
    log.info("  Settle wait:  %.1fs", args.settle_wait)
    log.info("  Read delay:   %.1fs", args.read_delay)
    log.info("  Timeout:      %.0fs", args.timeout)
    log.info("  Matrix:       %d levels × %d cadences = %d points",
             len(LEVELS), len(CADENCE_STEPS), len(LEVELS) * len(CADENCE_STEPS))

    rig = RigSerial(args.port, args.baud)
    try:
        run_sweep(
            rig=rig,
            output_path=output_path,
            dry_run=args.dry_run,
            settle_wait=args.settle_wait,
            read_delay=args.read_delay,
            timeout=args.timeout,
            adb_method=args.adb_method,
        )
    except KeyboardInterrupt:
        log.warning("Keyboard interrupt — stopping rig.")
        rig.send("STOP")
    finally:
        rig.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
