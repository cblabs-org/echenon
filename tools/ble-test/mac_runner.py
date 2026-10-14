#!/usr/bin/env python3
"""
mac_runner.py - Automated Mac CoreBluetooth test runner using Bleak.

Acts as a third BLE client and smoke test for the testing rig.
Attempts connection to Echenon-XXXX / target peripheral, holds for 30s, and disconnects.
Outputs matching attempts.csv and summary.md.

Usage:
    python mac_runner.py --batch-id <id> --evidence-dir <dir> [--attempts <n>] [--device-name <name>]
"""

import sys
import os
import time
import argparse
import csv
import asyncio
from datetime import datetime, timezone
from typing import Optional, List, Dict, Any

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    sys.exit("Error: bleak library is required. Install via 'pip install bleak'.")

DEFAULT_DEVICE_NAME = "Echenon-7226"
DEFAULT_ATTEMPTS = 5
HOLD_DURATION_S = 30.0
SPACING_DURATION_S = 30.0


async def find_device(target_name: str, target_addr: Optional[str] = None, timeout: float = 10.0):
    """Finds BLE device matching target name or address."""
    print(f"Scanning for BLE peripheral (name='{target_name}', timeout={timeout}s)...")
    devices = await BleakScanner.discover(timeout=timeout)
    for d in devices:
        name = d.name or ""
        addr = d.address or ""
        if target_name.lower() in name.lower():
            return d
        if target_addr and target_addr.lower() == addr.lower():
            return d
    return None


async def run_single_attempt(device, attempt_num: int, batch_id: str) -> Dict[str, Any]:
    wall_time = datetime.now(timezone.utc).astimezone().isoformat()
    start_time = time.time()
    dropped = False
    drop_time = None

    def on_disconnect(client):
        nonlocal dropped, drop_time
        dropped = True
        drop_time = round(time.time() - connect_time, 2)
        print(f"Warning: Disconnected unexpectedly after {drop_time}s!")

    connect_time = 0.0
    try:
        print(f"Attempting connection to {device.name} ({device.address})...")
        async with BleakClient(device, disconnected_callback=on_disconnect, timeout=20.0) as client:
            latency_ms = int((time.time() - start_time) * 1000)
            connect_time = time.time()
            if not client.is_connected:
                return {
                    "batch_id": batch_id,
                    "attempt_num": attempt_num,
                    "wall_clock_time": wall_time,
                    "result": "FAIL",
                    "android_status": -1,
                    "latency_ms": latency_ms,
                    "held_30s": False,
                    "drop_time": None,
                }

            print(f"Connected in {latency_ms} ms. Holding link for {HOLD_DURATION_S}s...")
            # Hold loop
            while (time.time() - connect_time) < HOLD_DURATION_S:
                if dropped or not client.is_connected:
                    break
                await asyncio.sleep(1.0)

            held_30s = not dropped and client.is_connected
            print(f"Hold complete (held_30s={held_30s}). Initiating disconnect...")

            return {
                "batch_id": batch_id,
                "attempt_num": attempt_num,
                "wall_clock_time": wall_time,
                "result": "PASS",
                "android_status": 0,
                "latency_ms": latency_ms,
                "held_30s": held_30s,
                "drop_time": drop_time,
            }

    except Exception as e:
        latency_ms = int((time.time() - start_time) * 1000)
        print(f"Connection failed: {e}")
        return {
            "batch_id": batch_id,
            "attempt_num": attempt_num,
            "wall_clock_time": wall_time,
            "result": "FAIL",
            "android_status": 133,
            "latency_ms": latency_ms,
            "held_30s": False,
            "drop_time": None,
        }


async def run_mac_batch(batch_id: str, attempts: int, evidence_dir: str, device_name: str, device_addr: Optional[str]):
    os.makedirs(evidence_dir, exist_ok=True)
    attempts_csv = os.path.join(evidence_dir, "attempts.csv")
    summary_md = os.path.join(evidence_dir, "summary.md")

    print("=" * 80)
    print(f"Starting Mac BLE Runner Batch '{batch_id}' ({attempts} attempts)")
    print(f"Target Name: {device_name}")
    print(f"Evidence Dir: {evidence_dir}")
    print("=" * 80)

    device = await find_device(device_name, device_addr)
    if not device:
        sys.exit(f"Error: Could not discover device '{device_name}'. Is peripheral advertising?")

    print(f"Discovered Target: {device.name} [{device.address}]")

    with open(attempts_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["batch_id", "attempt_num", "wall_clock_time", "result", "android_status", "latency_ms", "held_30s", "drop_time"])

    results = []
    passes = 0
    consecutive_fails = 0
    max_consecutive_fails = 0
    drops = 0

    for attempt in range(1, attempts + 1):
        print(f"\n--- [Attempt {attempt}/{attempts}] ---")
        res = await run_single_attempt(device, attempt, batch_id)
        results.append(res)

        if res["result"] == "PASS":
            passes += 1
            consecutive_fails = 0
            if not res["held_30s"]:
                drops += 1
        else:
            consecutive_fails += 1
            if consecutive_fails > max_consecutive_fails:
                max_consecutive_fails = consecutive_fails

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

        if attempt < attempts:
            print(f"Spacing: Waiting {SPACING_DURATION_S}s before next attempt...")
            await asyncio.sleep(SPACING_DURATION_S)

    rate = (passes / attempts) * 100 if attempts > 0 else 0.0
    summary_content = f"""# BLE Test Batch Summary: {batch_id} (Mac Runner)

## Environment & Configuration
- **Date / Time:** {datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S %Z')}
- **Batch ID:** `{batch_id}`
- **Target Peripheral:** `{device_name}` (`{device.address}`)
- **Test Client:** macOS Bleak CoreBluetooth
- **Hold Duration:** {HOLD_DURATION_S}s
- **Attempt Spacing:** {SPACING_DURATION_S}s

## Results
| Metric | Value |
| :--- | :--- |
| **Total Attempts** | {attempts} |
| **Passes** | {passes} ({rate:.1f}%) |
| **Failures** | {attempts - passes} ({100.0 - rate:.1f}%) |
| **Longest Consecutive Failures** | {max_consecutive_fails} |
| **Connection Drops (during 30s hold)** | {drops} |

## Evidence Files
- Attempts log: [`attempts.csv`](attempts.csv)
"""
    with open(summary_md, "w", encoding="utf-8") as f:
        f.write(summary_content)

    print("\n" + "=" * 80)
    print(f"Mac Runner Finished: {passes}/{attempts} PASS ({rate:.1f}%)")
    print(f"Summary written to: {summary_md}")
    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(description="Mac CoreBluetooth BLE Test Runner using Bleak.")
    parser.add_argument("--batch-id", required=True, help="Batch identifier (e.g. batch-mac-01)")
    parser.add_argument("--evidence-dir", required=True, help="Directory to store evidence and CSV")
    parser.add_argument("--attempts", type=int, default=DEFAULT_ATTEMPTS, help=f"Number of attempts (default: {DEFAULT_ATTEMPTS})")
    parser.add_argument("--device-name", default=DEFAULT_DEVICE_NAME, help=f"Target device name (default: {DEFAULT_DEVICE_NAME})")
    parser.add_argument("--device-addr", help="Optional target address / UUID")

    args = parser.parse_args()

    asyncio.run(run_mac_batch(
        batch_id=args.batch_id,
        attempts=args.attempts,
        evidence_dir=args.evidence_dir,
        device_name=args.device_name,
        device_addr=args.device_addr,
    ))


if __name__ == "__main__":
    main()
