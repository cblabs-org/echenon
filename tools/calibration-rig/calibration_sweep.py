#!/usr/bin/env python3
"""
Echelon Calibration Sweep — Host Script
========================================

Drives the ESP32 CalibrationRig firmware over HTTP to sweep all 32 resistance
levels at each of 10 cadence steps (40–130 RPM), capturing the power reading
from the Echelon Fit Android app via ADB.

Produces a CSV:  level, cadence_rpm, power_watts, pot_adc, timestamp

Usage:
    python3 calibration_sweep.py --host 192.168.1.100
    python3 calibration_sweep.py --dry-run          # HTTP only, no ADB
    python3 calibration_sweep.py --help
"""

from __future__ import annotations

import argparse
import csv
import json
import logging
import os
import re
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from xml.etree import ElementTree

try:
    import requests
except ImportError:
    # Fallback to urllib if requests not installed
    requests = None
    import urllib.request
    import urllib.error

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

LEVELS = list(range(1, 33))                          # 1 … 32
CADENCE_STEPS = list(range(40, 131, 10))             # 40, 50, … 130
CSV_HEADER = ["level", "cadence_rpm", "power_watts", "pot_adc", "timestamp"]

DEFAULT_HOST = "192.168.1.100"
DEFAULT_OUTPUT = "sweep_output.csv"
DEFAULT_SETTLE_WAIT = 4.0   # seconds after SETTLED before ADB read
DEFAULT_READ_DELAY = 1.0    # seconds between consecutive ADB reads
DEFAULT_TIMEOUT = 60        # seconds to wait for SETTLED before retry
STATUS_POLL_INTERVAL = 0.5  # seconds between status polls

ADB_RETRY_COUNT = 3
ADB_RETRY_BACKOFF = 2.0     # seconds

ADB_PATH = os.environ.get("ADB_PATH", "adb")

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
# HTTP helpers
# ---------------------------------------------------------------------------

class RigHTTP:
    """MCP JSON-RPC 2.0 client for the CalibrationRig firmware tools."""

    def __init__(self, host: str, timeout: float = 10.0, mcp_token: str | None = None):
        self._base = f"http://{host}"
        self._mcp_url = f"{self._base}/api/mcp"
        self._timeout = timeout
        self._mcp_token = mcp_token or os.environ.get("ECHENON_MCP_TOKEN")
        self._seq = 0
        log.info("Rig MCP endpoint: %s", self._mcp_url)

    def _call_tool(self, name: str, arguments: dict) -> dict:
        """Invoke an MCP tool via JSON-RPC 2.0 POST /api/mcp and return parsed content dict."""
        self._seq += 1
        payload = {
            "jsonrpc": "2.0",
            "id": self._seq,
            "method": "tools/call",
            "params": {
                "name": name,
                "arguments": arguments,
            },
        }
        headers = {"Content-Type": "application/json"}
        if self._mcp_token:
            headers["Authorization"] = f"Bearer {self._mcp_token}"

        body_bytes = json.dumps(payload).encode("utf-8")
        if requests:
            resp = requests.post(
                self._mcp_url,
                data=body_bytes,
                headers=headers,
                timeout=self._timeout,
            )
            resp.raise_for_status()
            rpc_resp = resp.json()
        else:
            req = urllib.request.Request(
                self._mcp_url,
                data=body_bytes,
                headers=headers,
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=self._timeout) as resp:
                rpc_resp = json.loads(resp.read().decode("utf-8"))

        if "error" in rpc_resp and rpc_resp["error"]:
            err = rpc_resp["error"]
            if (
                (isinstance(err, dict) and (err.get("code") == -32000 or "read-only" in str(err.get("message", "")).lower()))
                or "read-only" in str(err).lower()
            ):
                raise RuntimeError(
                    f"MCP tool '{name}' rejected because MCPServer defaults to readOnly: true ({err}). "
                    f"To enable mutating tools: "
                    f"(1) run: curl -X POST {self._base}/api/mcp/config -H \"Content-Type: application/json\" -d '{{\"readOnly\":false}}' "
                    f"(add -H \"Authorization: Bearer <token>\" if Portal Auth is enabled), or "
                    f"(2) disable Read-Only mode in the Web Portal under System -> MCP."
                )
            raise RuntimeError(f"MCP JSON-RPC error calling {name}: {err}")

        result = rpc_resp.get("result") or {}
        content = result.get("content") or []
        text = content[0].get("text", "{}") if content else "{}"
        parsed = json.loads(text) if text else {}

        if result.get("isError"):
            err_msg = parsed.get("error", text) if isinstance(parsed, dict) else text
            raise RuntimeError(f"MCP tool {name} returned error: {err_msg}")

        return parsed

    def send_command(self, command: str) -> str:
        """Send a command to the rig via echenon_rig_command and return the status line."""
        cmd = command.strip()
        log.info("TX → %s", cmd)
        res = self._call_tool("echenon_rig_command", {"command": cmd})
        return str(res.get("status_line", "")).strip()

    def get_status_json(self) -> dict:
        """Read full diagnostic status from the rig via echenon_rig_status."""
        return self._call_tool("echenon_rig_status", {})

    def get_status_line(self) -> str:
        """Read the text status line via read-only echenon_rig_status."""
        res = self._call_tool("echenon_rig_status", {})
        return str(res.get("status_line", "")).strip()


def _wait_for_settled(
    rig: RigHTTP,
    timeout: float,
) -> tuple[bool, int, str]:
    """
    Poll until rig reports SETTLED or *timeout* seconds elapse.

    Returns:
        (settled, pot_adc, raw_line)
        settled is False on timeout or ERROR.
    """
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        try:
            line = rig.get_status_line()
        except Exception as exc:
            log.warning("Status poll failed: %s", exc)
            time.sleep(STATUS_POLL_INTERVAL)
            continue

        if not line:
            time.sleep(STATUS_POLL_INTERVAL)
            continue

        if "state=SETTLED" in line:
            pot = _parse_pot(line)
            log.debug("  SETTLED: %s", line)
            return True, pot, line

        if "state=ERROR" in line:
            log.error("Rig error: %s", line)
            return False, 0, line

        if "state=SEEKING" in line:
            log.debug("  SEEKING: %s", line)

        time.sleep(STATUS_POLL_INTERVAL)

    log.warning("Timeout waiting for SETTLED (%ds)", timeout)
    return False, 0, ""


def _parse_pot(line: str) -> int:
    """Extract pot=NNN from a status line."""
    m = re.search(r"pot=(\d+)", line)
    return int(m.group(1)) if m else 0


# ---------------------------------------------------------------------------
# ADB helpers
# ---------------------------------------------------------------------------

def _find_adb() -> str:
    """Find the adb binary, checking common macOS SDK locations."""
    # Check if adb is on PATH
    try:
        result = subprocess.run(
            ["which", "adb"], capture_output=True, text=True, timeout=5,
        )
        if result.returncode == 0:
            return "adb"
    except Exception:
        pass

    # Check common macOS SDK locations
    sdk_paths = [
        os.path.expanduser("~/Library/Android/sdk/platform-tools/adb"),
        "/usr/local/bin/adb",
    ]
    for p in sdk_paths:
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return p

    return ADB_PATH  # Fall back to env var or default


_adb_binary: str | None = None


def _adb_cmd() -> str:
    global _adb_binary
    if _adb_binary is None:
        _adb_binary = _find_adb()
        log.info("ADB binary: %s", _adb_binary)
    return _adb_binary


def _adb_read_power_uiautomator() -> str:
    """
    Use ``adb shell uiautomator dump`` to capture the Echelon Fit UI tree,
    then parse the XML for the power / watts value.

    Returns the power string (e.g. ``"145"``) or a sentinel on failure.
    """
    adb = _adb_cmd()
    host_dump = os.path.join(tempfile.gettempdir(), "window_dump.xml")
    try:
        subprocess.run(
            [adb, "shell", "uiautomator", "dump", "/sdcard/window_dump.xml"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        subprocess.run(
            [adb, "pull", "/sdcard/window_dump.xml", host_dump],
            capture_output=True, text=True, timeout=15, check=True,
        )
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        log.warning("ADB uiautomator command failed: %s", exc)
        raise

    try:
        tree = ElementTree.parse(host_dump)
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

    adb = _adb_cmd()
    host_screencap = os.path.join(tempfile.gettempdir(), "screenshot.png")
    try:
        subprocess.run(
            [adb, "shell", "screencap", "-p", "/sdcard/screenshot.png"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        subprocess.run(
            [adb, "pull", "/sdcard/screenshot.png", host_screencap],
            capture_output=True, text=True, timeout=15, check=True,
        )
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        log.warning("ADB screencap command failed: %s", exc)
        raise

    img = Image.open(host_screencap)
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
# ADB helpers — read resistance and cadence
# ---------------------------------------------------------------------------

def _adb_read_resistance() -> str:
    """Read the resistance value from the Echelon Fit app via uiautomator."""
    adb = _adb_cmd()
    host_dump = os.path.join(tempfile.gettempdir(), "window_dump.xml")
    try:
        subprocess.run(
            [adb, "shell", "uiautomator", "dump", "/sdcard/window_dump.xml"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        subprocess.run(
            [adb, "pull", "/sdcard/window_dump.xml", host_dump],
            capture_output=True, text=True, timeout=15, check=True,
        )
    except Exception as exc:
        log.warning("ADB resistance read failed: %s", exc)
        return "ADB_ERROR"

    try:
        tree = ElementTree.parse(host_dump)
    except ElementTree.ParseError:
        return "PARSE_ERROR"

    root = tree.getroot()
    all_texts: list[str] = []
    for node in root.iter("node"):
        text = node.get("text", "").strip()
        if text:
            all_texts.append(text)

    # Look for "Resistance" label followed by a number
    for i, text in enumerate(all_texts):
        if "resist" in text.lower():
            for j in range(i + 1, min(i + 4, len(all_texts))):
                candidate = all_texts[j].strip()
                if re.fullmatch(r"\d+", candidate):
                    return candidate

    return "PARSE_ERROR"


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
    rig: RigHTTP,
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
            rig.send_command("HOME")
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
    rig: RigHTTP,
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
    rig.send_command(f"SET level={level} cadence={cadence}")
    settled, pot_adc, raw = _wait_for_settled(rig, timeout)

    if not settled:
        # Retry once: HOME → re-seek
        log.warning("  Retrying level=%d cadence=%d (HOME + re-seek)", level, cadence)
        rig.send_command("HOME")
        _wait_for_settled(rig, timeout=20)
        time.sleep(1.0)

        rig.send_command(f"SET level={level} cadence={cadence}")
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
            "Echelon Calibration Sweep — drive the CalibrationRig over HTTP "
            "and capture power readings from the Echelon Fit app via ADB."
        ),
    )
    parser.add_argument(
        "--host", default=DEFAULT_HOST,
        help=f"ESP32 hostname or IP (default: {DEFAULT_HOST})",
    )
    parser.add_argument(
        "--output", default=DEFAULT_OUTPUT,
        help=f"Output CSV path (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help='HTTP commands only, no ADB. Records "DRY_RUN" for power.',
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
    parser.add_argument(
        "--http-timeout", type=float, default=10.0,
        metavar="SECONDS",
        help="HTTP request timeout (default: 10.0)",
    )
    parser.add_argument(
        "--mcp-token", default=os.environ.get("ECHENON_MCP_TOKEN"),
        metavar="TOKEN",
        help="Optional Bearer token for /api/mcp (default: $ECHENON_MCP_TOKEN)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    _setup_logging()
    args = parse_args(argv)

    output_path = Path(args.output)

    log.info("Echelon Calibration Sweep")
    log.info("  Host:         %s", args.host)
    log.info("  Output:       %s", output_path)
    log.info("  Dry-run:      %s", args.dry_run)
    log.info("  ADB method:   %s", args.adb_method)
    log.info("  Settle wait:  %.1fs", args.settle_wait)
    log.info("  Read delay:   %.1fs", args.read_delay)
    log.info("  Timeout:      %.0fs", args.timeout)
    log.info("  HTTP timeout: %.1fs", args.http_timeout)
    log.info("  Matrix:       %d levels × %d cadences = %d points",
             len(LEVELS), len(CADENCE_STEPS), len(LEVELS) * len(CADENCE_STEPS))

    rig = RigHTTP(args.host, timeout=args.http_timeout, mcp_token=args.mcp_token)

    # Pre-flight: verify connectivity
    try:
        status = rig.get_status_line()
        log.info("Pre-flight OK: %s", status)
    except Exception as exc:
        log.error("Cannot reach rig at %s: %s", args.host, exc)
        return 1

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
        try:
            rig.send_command("STOP")
        except Exception:
            pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
