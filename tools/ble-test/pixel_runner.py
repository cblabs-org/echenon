#!/usr/bin/env python3
"""
pixel_runner.py - Automated Android / nRF Connect BLE test runner over ADB.

Drives the Google Pixel over ADB using uiautomator dump to locate controls.
Monitors Android BluetoothGatt logcat for connection results (status=0 PASS, status=147/133 FAIL).
Enforces 30s connection hold, 30s attempt spacing, and automated bugreport capture on first failure.

Usage:
    python pixel_runner.py --batch-id <id> --evidence-dir <dir> [--attempts <n>] [--device-addr <addr>] [--dry-run]
"""

import sys
import os
import re
import time
import argparse
import csv
import subprocess
import shutil
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from typing import Optional, Tuple, Dict, Any, List

DEFAULT_DEVICE_ADDR = "3C:0F:02:DE:72:26"
DEFAULT_ATTEMPTS = 50
HOLD_DURATION_S = 30.0
SPACING_DURATION_S = 30.0
MAX_BUGREPORTS_PER_BATCH = 2
NRF_PACKAGE = "no.nordicsemi.android.mcp"


def find_adb_binary(custom_path: Optional[str] = None) -> str:
    """Finds adb binary from PATH, custom path, or standard Android SDK locations."""
    if custom_path and os.path.isfile(custom_path) and os.access(custom_path, os.X_OK):
        return custom_path

    in_path = shutil.which("adb")
    if in_path:
        return in_path

    # Standard macOS / Linux locations
    sdk_locations = [
        os.path.expanduser("~/Library/Android/sdk/platform-tools/adb"),
        os.path.expanduser("~/Android/Sdk/platform-tools/adb"),
        "/usr/local/bin/adb",
        "/opt/homebrew/bin/adb",
    ]
    for loc in sdk_locations:
        if os.path.isfile(loc) and os.access(loc, os.X_OK):
            return loc

    return "adb"


class AdbDriver:
    def __init__(self, adb_path: str, dry_run: bool = False):
        self.adb = adb_path
        self.dry_run = dry_run

    def run_cmd(self, args: List[str], timeout: Optional[float] = None) -> Tuple[int, str, str]:
        cmd = [self.adb] + args
        if self.dry_run:
            return 0, "", ""
        try:
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
            return res.returncode, res.stdout, res.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timed out"
        except Exception as e:
            return -1, "", str(e)

    def check_device(self) -> Tuple[bool, str]:
        if self.dry_run:
            return True, "Mock Pixel 10 Pro Fold (dry-run)"
        code, out, _ = self.run_cmd(["devices", "-l"])
        if code != 0:
            return False, "Failed to run 'adb devices'"
        lines = [line.strip() for line in out.strip().splitlines() if line.strip() and not line.startswith("*")]
        devices = [l for l in lines[1:] if "device" in l.split()]
        if not devices:
            return False, "No active ADB device detected. Ensure USB debugging is enabled."
        return True, devices[0]

    def set_phone_hygiene(self) -> Dict[str, str]:
        info = {
            "build_id": "dry-run-build",
            "android_version": "16",
            "bonded_summary": "None",
        }
        if self.dry_run:
            print("[DRY-RUN] Setting phone hygiene: screen awake, fetching build info")
            return info

        # Keep awake while plugged in
        self.run_cmd(["shell", "svc", "power", "stayon", "true"])
        self.run_cmd(["shell", "settings", "put", "global", "stay_on_while_plugged_in", "3"])

        # Fetch build
        _, build_id, _ = self.run_cmd(["shell", "getprop", "ro.build.display.id"])
        _, android_ver, _ = self.run_cmd(["shell", "getprop", "ro.build.version.release"])
        _, bt_dump, _ = self.run_cmd(["shell", "dumpsys", "bluetooth_manager"])

        info["build_id"] = build_id.strip() or "Unknown"
        info["android_version"] = android_ver.strip() or "Unknown"

        # Extract bonded devices summary from bt_dump
        bonded = []
        for line in bt_dump.splitlines():
            if "Bonded devices:" in line or "bonded:" in line.lower():
                bonded.append(line.strip())
            elif bonded and line.startswith("  "):
                bonded.append(line.strip())
            elif bonded and not line.startswith("  "):
                break
        info["bonded_summary"] = "\n".join(bonded) if bonded else "No bonded devices reported"
        return info

    def dump_hierarchy(self) -> Optional[ET.Element]:
        """Dumps window hierarchy XML using uiautomator."""
        if self.dry_run:
            return None

        # Execute dump to device /sdcard/window_dump.xml
        code, _, _ = self.run_cmd(["shell", "uiautomator", "dump", "/sdcard/window_dump.xml"], timeout=10)
        if code != 0:
            # Fallback to /data/local/tmp
            self.run_cmd(["shell", "uiautomator", "dump", "/data/local/tmp/window_dump.xml"], timeout=10)
            code, out, _ = self.run_cmd(["shell", "cat", "/data/local/tmp/window_dump.xml"], timeout=10)
        else:
            code, out, _ = self.run_cmd(["shell", "cat", "/sdcard/window_dump.xml"], timeout=10)

        if code != 0 or not out.strip().startswith("<?xml"):
            return None

        try:
            return ET.fromstring(out.strip())
        except Exception:
            return None

    def find_node(self, root: ET.Element, target_addr: str, action: str = "CONNECT") -> Optional[Tuple[int, int]]:
        """
        Locates the center coordinates (x, y) of the target button (CONNECT or DISCONNECT)
        associated with the target device address or currently open device page.
        """
        if root is None:
            return None

        norm_addr = target_addr.upper()
        norm_action = action.upper()

        # Check all nodes in hierarchy
        all_nodes = list(root.iter("node"))

        # Case 1: Look for action button (CONNECT / DISCONNECT)
        candidate_buttons = []
        for node in all_nodes:
            text = (node.attrib.get("text") or "").upper()
            res_id = node.attrib.get("resource-id") or ""
            bounds = node.attrib.get("bounds") or ""

            is_match = False
            if norm_action == "CONNECT":
                if "CONNECT" in text and "DISCONNECT" not in text:
                    is_match = True
                elif "action_connect" in res_id:
                    is_match = True
            elif norm_action == "DISCONNECT":
                if "DISCONNECT" in text:
                    is_match = True
                elif "action_disconnect" in res_id:
                    is_match = True

            if is_match and bounds:
                candidate_buttons.append((node, bounds))

        if not candidate_buttons:
            return None

        # If on device page with exactly one action button, use it
        if len(candidate_buttons) == 1:
            return self._parse_bounds(candidate_buttons[0][1])

        # If multiple buttons (e.g. scanner device list), locate the one nearest the target device address
        # Find device node
        target_device_node = None
        for node in all_nodes:
            text = (node.attrib.get("text") or "").upper()
            if norm_addr in text or "ECHENON" in text:
                target_device_node = node
                break

        if target_device_node:
            dev_bounds = self._parse_bounds(target_device_node.attrib.get("bounds") or "")
            if dev_bounds:
                dev_y = dev_bounds[1]
                # Find button with closest vertical coordinate
                best_btn = min(candidate_buttons, key=lambda b: abs(self._parse_bounds(b[1])[1] - dev_y))
                return self._parse_bounds(best_btn[1])

        # Default to first candidate
        return self._parse_bounds(candidate_buttons[0][1])

    def _parse_bounds(self, bounds_str: str) -> Optional[Tuple[int, int]]:
        m = re.match(r"\[(\d+),(\d+)\]\[(\d+),(\d+)\]", bounds_str)
        if not m:
            return None
        x1, y1, x2, y2 = map(int, m.groups())
        return ((x1 + x2) // 2, (y1 + y2) // 2)

    def tap(self, x: int, y: int):
        if self.dry_run:
            print(f"[DRY-RUN] Tapping coordinate ({x}, {y})")
            return
        self.run_cmd(["shell", "input", "tap", str(x), str(y)])

    def clear_logcat(self):
        if not self.dry_run:
            self.run_cmd(["logcat", "-c"])

    def capture_bugreport(self, evidence_dir: str, filename_prefix: str) -> Optional[str]:
        if self.dry_run:
            print(f"[DRY-RUN] Capturing bug report to {evidence_dir}")
            mock_zip = os.path.join(evidence_dir, f"{filename_prefix}-mock.zip")
            with open(mock_zip, "w") as f:
                f.write("mock bugreport")
            return mock_zip

        print(f"Capturing bug report to {evidence_dir} (may take 1-3 minutes)...")
        os.makedirs(evidence_dir, exist_ok=True)
        # adb bugreport <dir> generates a zip file directly in <dir>
        start_time = time.time()
        code, out, err = self.run_cmd(["bugreport", evidence_dir], timeout=360)
        elapsed = time.time() - start_time
        print(f"Bug report command completed in {elapsed:.1f}s (code {code})")
        if code == 0:
            # Locate newly created zip file
            zips = [os.path.join(evidence_dir, f) for f in os.listdir(evidence_dir) if f.endswith(".zip")]
            if zips:
                zips.sort(key=os.path.getmtime, reverse=True)
                return zips[0]
        return None


class PixelRunner:
    def __init__(self, adb: AdbDriver, batch_id: str, attempts: int, evidence_dir: str, device_addr: str, dry_run: bool = False):
        self.adb = adb
        self.batch_id = batch_id
        self.total_attempts = attempts
        self.evidence_dir = evidence_dir
        self.device_addr = device_addr
        self.dry_run = dry_run
        self.bugreport_count = 0

    def run_batch(self):
        os.makedirs(self.evidence_dir, exist_ok=True)
        attempts_csv = os.path.join(self.evidence_dir, "attempts.csv")
        summary_md = os.path.join(self.evidence_dir, "summary.md")

        print("=" * 80)
        print(f"Starting Batch '{self.batch_id}' ({self.total_attempts} attempts)")
        print(f"Target Peripheral: {self.device_addr}")
        print(f"Evidence Dir:      {self.evidence_dir}")
        print(f"Mode:              {'DRY-RUN' if self.dry_run else 'LIVE'}")
        print("=" * 80)

        ok, dev_desc = self.adb.check_device()
        if not ok:
            sys.exit(f"Device Check Failed: {dev_desc}")
        print(f"ADB Device: {dev_desc}")

        hygiene = self.adb.set_phone_hygiene()

        # Initialize CSV
        with open(attempts_csv, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["batch_id", "attempt_num", "wall_clock_time", "result", "android_status", "latency_ms", "held_30s", "drop_time"])

        results_list = []
        passes = 0
        consecutive_fails = 0
        max_consecutive_fails = 0
        drops = 0

        for attempt in range(1, self.total_attempts + 1):
            print(f"\n--- [Attempt {attempt}/{self.total_attempts}] ---")
            res = self._execute_attempt(attempt)
            results_list.append(res)

            # Record metrics
            if res["result"] == "PASS":
                passes += 1
                consecutive_fails = 0
                if not res["held_30s"]:
                    drops += 1
            else:
                consecutive_fails += 1
                if consecutive_fails > max_consecutive_fails:
                    max_consecutive_fails = consecutive_fails

            # Append to CSV
            with open(attempts_csv, "a", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow([
                    res["batch_id"],
                    res["attempt_num"],
                    res["wall_clock_time"],
                    res["result"],
                    res["android_status"],
                    res["latency_ms"],
                    res["held_30s"],
                    res["drop_time"] or "",
                ])

            # Failure trace capture (fixes F8): First FAIL of batch
            if res["result"] == "FAIL" and self.bugreport_count < MAX_BUGREPORTS_PER_BATCH:
                self.bugreport_count += 1
                print(f"\n[ALERT] Failure encountered on attempt {attempt}. Capturing bugreport ({self.bugreport_count}/{MAX_BUGREPORTS_PER_BATCH})...")
                bugreport_path = self.adb.capture_bugreport(self.evidence_dir, f"bugreport-fail-attempt{attempt}")
                if bugreport_path:
                    print(f"Saved failure bug report to: {bugreport_path}")

            # Spacing between attempts (30s)
            if attempt < self.total_attempts:
                print(f"Spacing: Waiting {SPACING_DURATION_S}s before next attempt...")
                if not self.dry_run:
                    time.sleep(SPACING_DURATION_S)

        # Write summary.md
        self._write_summary(summary_md, hygiene, results_list, passes, max_consecutive_fails, drops)
        print("\n" + "=" * 80)
        print(f"Batch {self.batch_id} Finished.")
        print(f"Results: {passes}/{self.total_attempts} PASS ({(passes/self.total_attempts)*100:.1f}%)")
        print(f"Summary written to: {summary_md}")
        print("=" * 80)

    def _execute_attempt(self, attempt_num: int) -> Dict[str, Any]:
        wall_time = datetime.now(timezone.utc).astimezone().isoformat()

        if self.dry_run:
            print(f"[DRY-RUN] Verifying screen hierarchy for {self.device_addr}")
            print(f"[DRY-RUN] Found Connect button. Simulating tap...")
            # Simulate 80% pass in dry run
            sim_pass = (attempt_num % 5 != 0)
            if sim_pass:
                latency = 240
                print(f"[DRY-RUN] Logcat: status=0 CONNECTED (latency={latency}ms) -> PASS")
                print(f"[DRY-RUN] Holding 30s. No drop detected.")
                print(f"[DRY-RUN] Tapping Disconnect. Link down confirmed.")
                return {
                    "batch_id": self.batch_id,
                    "attempt_num": attempt_num,
                    "wall_clock_time": wall_time,
                    "result": "PASS",
                    "android_status": 0,
                    "latency_ms": latency,
                    "held_30s": True,
                    "drop_time": None,
                }
            else:
                latency = 180
                print(f"[DRY-RUN] Logcat: status=147 GATT_CONN_TIMEOUT (latency={latency}ms) -> FAIL")
                return {
                    "batch_id": self.batch_id,
                    "attempt_num": attempt_num,
                    "wall_clock_time": wall_time,
                    "result": "FAIL",
                    "android_status": 147,
                    "latency_ms": latency,
                    "held_30s": False,
                    "drop_time": None,
                }

        # LIVE EXECUTION
        root = self.adb.dump_hierarchy()
        if root is None:
            sys.exit("Error: Failed to obtain uiautomator window dump. Is phone unlocked?")

        connect_coords = self.adb.find_node(root, self.device_addr, action="CONNECT")
        if not connect_coords:
            sys.exit(
                f"Error: Could not locate CONNECT button for device '{self.device_addr}'.\n"
                f"Ensure nRF Connect is open on the device or scanner tab."
            )

        self.adb.clear_logcat()
        tap_time = time.time()
        self.adb.tap(connect_coords[0], connect_coords[1])

        # Monitor logcat for result
        result, status, latency_ms = self._wait_for_connection_state(timeout=30.0, start_time=tap_time)

        held_30s = False
        drop_time = None

        if result == "PASS":
            print(f"Connected in {latency_ms} ms. Holding link for {HOLD_DURATION_S}s (monitoring drops)...")
            hold_start = time.time()
            dropped = False
            while (time.time() - hold_start) < HOLD_DURATION_S:
                if self._check_logcat_drop():
                    dropped = True
                    drop_time = round(time.time() - hold_start, 2)
                    print(f"Warning: Connection dropped after {drop_time}s!")
                    break
                time.sleep(1.0)

            if not dropped:
                held_30s = True
                print("30s hold completed cleanly.")

            # Tap Disconnect
            print("Locating and tapping DISCONNECT...")
            disc_root = self.adb.dump_hierarchy()
            disc_coords = self.adb.find_node(disc_root, self.device_addr, action="DISCONNECT")
            if disc_coords:
                self.adb.tap(disc_coords[0], disc_coords[1])
                # Wait for link down
                self._wait_for_disconnect_confirm(timeout=5.0)
            else:
                print("Warning: Could not find DISCONNECT button. Link may have already dropped.")

        return {
            "batch_id": self.batch_id,
            "attempt_num": attempt_num,
            "wall_clock_time": wall_time,
            "result": result,
            "android_status": status,
            "latency_ms": latency_ms,
            "held_30s": held_30s,
            "drop_time": drop_time,
        }

    def _wait_for_connection_state(self, timeout: float, start_time: float) -> Tuple[str, int, int]:
        """Polls logcat for BLE connection state result.

        Supports both pre-Android-17 (onClientConnectionState, state=2/0)
        and Android 17+ (onConnectionState, connected=true/false) log formats.
        """
        deadline = start_time + timeout
        while time.time() < deadline:
            code, out, _ = self.adb.run_cmd(["logcat", "-d", "-s", "BluetoothGatt:*"])
            for line in out.splitlines():
                if ("onClientConnectionState()" in line
                        or "onConnectionStateChange" in line
                        or "onConnectionState()" in line):
                    m = re.search(r"status=(\d+)", line)
                    if m:
                        status = int(m.group(1))
                        latency = int((time.time() - start_time) * 1000)
                        if status == 0 and ("state=2" in line or "CONNECTED" in line
                                            or "state: 2" in line or "connected=true" in line):
                            return "PASS", 0, latency
                        elif status in (147, 133) or "DISCONNECTED" in line or "connected=false" in line:
                            return "FAIL", status, latency
            time.sleep(0.05)

        latency = int((time.time() - start_time) * 1000)
        return "FAIL", 147, latency

    def _check_logcat_drop(self) -> bool:
        code, out, _ = self.adb.run_cmd(["logcat", "-d", "-s", "BluetoothGatt:*"])
        for line in out.splitlines():
            if ("onConnectionState()" in line or "onClientConnectionState()" in line
                    or "onConnectionStateChange" in line):
                if ("DISCONNECTED" in line or "state=0" in line
                        or "state: 0" in line or "connected=false" in line):
                    return True
        return False

    def _wait_for_disconnect_confirm(self, timeout: float):
        start = time.time()
        while time.time() - start < timeout:
            code, out, _ = self.adb.run_cmd(["logcat", "-d", "-s", "BluetoothGatt:*"])
            if "DISCONNECTED" in out or "state=0" in out or "connected=false" in out:
                return
            time.sleep(0.2)

    def _write_summary(self, filepath: str, hygiene: Dict[str, str], results: List[Dict[str, Any]], passes: int, max_consec_fail: int, drops: int):
        total = len(results)
        rate = (passes / total) * 100 if total > 0 else 0.0

        content = f"""# BLE Test Batch Summary: {self.batch_id}

## Environment & Configuration
- **Date / Time:** {datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S %Z')}
- **Batch ID:** `{self.batch_id}`
- **Target Peripheral:** `{self.device_addr}`
- **Test Client:** Google Pixel 10 Pro Fold (`ro.build.display.id`: `{hygiene['build_id']}`, Android {hygiene['android_version']})
- **Application:** nRF Connect for Android (`{NRF_PACKAGE}`)
- **Hold Duration:** {HOLD_DURATION_S}s
- **Attempt Spacing:** {SPACING_DURATION_S}s
- **Bug Reports Captured:** {self.bugreport_count} (max {MAX_BUGREPORTS_PER_BATCH})

### Bonded Devices Summary
```
{hygiene['bonded_summary']}
```

## Results
| Metric | Value |
| :--- | :--- |
| **Total Attempts** | {total} |
| **Passes** | {passes} ({rate:.1f}%) |
| **Failures** | {total - passes} ({100.0 - rate:.1f}%) |
| **Longest Consecutive Failures** | {max_consec_fail} |
| **Connection Drops (during 30s hold)** | {drops} |

## Evidence Files
- Attempts log: [`attempts.csv`](attempts.csv)
"""
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)


def main():
    parser = argparse.ArgumentParser(description="Pixel nRF Connect Automated BLE Test Runner.")
    parser.add_argument("--batch-id", required=True, help="Batch identifier (e.g. batch-a-01)")
    parser.add_argument("--evidence-dir", required=True, help="Directory to store evidence and CSV")
    parser.add_argument("--attempts", type=int, default=DEFAULT_ATTEMPTS, help=f"Number of attempts (default: {DEFAULT_ATTEMPTS})")
    parser.add_argument("--device-addr", default=DEFAULT_DEVICE_ADDR, help=f"Target device MAC (default: {DEFAULT_DEVICE_ADDR})")
    parser.add_argument("--adb-path", help="Custom path to adb binary")
    parser.add_argument("--dry-run", action="store_true", help="Run offline simulation without tapping or real ADB device")

    args = parser.parse_args()

    adb_bin = find_adb_binary(args.adb_path)
    adb = AdbDriver(adb_bin, dry_run=args.dry_run)

    runner = PixelRunner(
        adb=adb,
        batch_id=args.batch_id,
        attempts=args.attempts,
        evidence_dir=args.evidence_dir,
        device_addr=args.device_addr,
        dry_run=args.dry_run,
    )
    runner.run_batch()


if __name__ == "__main__":
    main()
