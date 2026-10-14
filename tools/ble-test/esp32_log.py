#!/usr/bin/env python3
"""
esp32_log.py - Serial monitor capture and BLE event parser for ESP32.

Captures serial output from an ESP32 board, prepending each line with host wall-clock time (ISO 8601).
Extracts BLE connection events (Central connected, Central disconnected, Reattempt advertising).
Generates timestamped raw log and parsed CSV event table.

Usage:
    python esp32_log.py --batch-id <id> --output-dir <dir> [--port <port>] [--baud <baud>]
    python esp32_log.py --batch-id <id> --output-dir <dir> --input-file <existing_log>
"""

import sys
import os
import re
import time
import argparse
import csv
import glob
from datetime import datetime, timezone
from typing import Optional, List, Dict, Any

# Regex patterns for framework BLE log events
RE_CONNECTED = re.compile(
    r"Central connected:\s*handle=(\d+),\s*addr=([0-9A-Fa-f:]{17})(?:,\s*itvl=(\d+)\s*\(([\d\.]+)ms\))?(?:,\s*latency=(\d+))?(?:,\s*timeout=(\d+)\s*\(([\d\.]+)ms\))?(?:,\s*MTU=(\d+))?"
)
RE_DISCONNECTED = re.compile(
    r"Central disconnected:\s*handle=(\d+),\s*addr=([0-9A-Fa-f:]{17}),\s*reason=(0x[0-9a-fA-F]+|\d+)"
)
RE_REATTEMPT = re.compile(
    r"Reattempt advertising;\s*reason:\s*(0x[0-9a-fA-F]+|\d+)(?:,\s*status\s*=\s*(\d+))?"
)
RE_MTU = re.compile(
    r"Central MTU changed:\s*handle=(\d+),\s*MTU=(\d+)"
)
RE_ESP_TIMESTAMP = re.compile(
    r"^[IWEVD]\s*\((\d+)\)"
)


def find_default_serial_port() -> Optional[str]:
    """Auto-detects USB serial port on macOS/Linux."""
    patterns = [
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
    ]
    candidates = []
    for pat in patterns:
        candidates.extend(glob.glob(pat))

    if len(candidates) == 1:
        return candidates[0]
    elif len(candidates) > 1:
        # Prefer usbmodem for ESP32-S3
        modems = [c for c in candidates if "usbmodem" in c]
        if modems:
            return modems[0]
        return candidates[0]
    return None


def parse_line_for_events(line: str, host_ts: Optional[str] = None) -> List[Dict[str, Any]]:
    """Checks a single log line for BLE events."""
    events = []

    # Extract ESP uptime timestamp if available
    esp_uptime = ""
    m_uptime = RE_ESP_TIMESTAMP.search(line)
    if m_uptime:
        esp_uptime = m_uptime.group(1)

    ts = host_ts or datetime.now(timezone.utc).astimezone().isoformat()

    # Central connected
    m = RE_CONNECTED.search(line)
    if m:
        handle, addr, itvl_raw, itvl_ms, latency, to_raw, to_ms, mtu = m.groups()
        details_parts = []
        if itvl_ms:
            details_parts.append(f"itvl={itvl_ms}ms")
        if latency:
            details_parts.append(f"latency={latency}")
        if to_ms:
            details_parts.append(f"timeout={to_ms}ms")
        if mtu:
            details_parts.append(f"MTU={mtu}")
        if esp_uptime:
            details_parts.append(f"uptime_ms={esp_uptime}")

        events.append({
            "timestamp": ts,
            "event_type": "CONNECTED",
            "handle": handle,
            "addr": addr,
            "reason": "",
            "details": "; ".join(details_parts),
        })
        return events

    # Central disconnected
    m = RE_DISCONNECTED.search(line)
    if m:
        handle, addr, reason = m.groups()
        details = f"uptime_ms={esp_uptime}" if esp_uptime else ""
        events.append({
            "timestamp": ts,
            "event_type": "DISCONNECTED",
            "handle": handle,
            "addr": addr,
            "reason": reason,
            "details": details,
        })
        return events

    # Reattempt advertising
    m = RE_REATTEMPT.search(line)
    if m:
        reason, status = m.groups()
        details = f"status={status}" if status else ""
        if esp_uptime:
            details = f"{details}; uptime_ms={esp_uptime}" if details else f"uptime_ms={esp_uptime}"
        events.append({
            "timestamp": ts,
            "event_type": "REATTEMPT_ADV",
            "handle": "",
            "addr": "",
            "reason": reason,
            "details": details,
        })
        return events

    # MTU changed
    m = RE_MTU.search(line)
    if m:
        handle, mtu = m.groups()
        details = f"MTU={mtu}"
        if esp_uptime:
            details += f"; uptime_ms={esp_uptime}"
        events.append({
            "timestamp": ts,
            "event_type": "MTU_CHANGED",
            "handle": handle,
            "addr": "",
            "reason": "",
            "details": details,
        })
        return events

    return events


def parse_existing_log(input_file: str, batch_id: str, output_dir: str):
    """Parses an existing serial monitor log file into raw and events CSV."""
    raw_out = os.path.join(output_dir, f"esp32_serial_{batch_id}.log")
    csv_out = os.path.join(output_dir, f"esp32_events_{batch_id}.csv")

    all_events = []
    print(f"Reading input file: {input_file}")

    with open(input_file, "r", encoding="utf-8", errors="replace") as fin, \
         open(raw_out, "w", encoding="utf-8") as fraw:

        for line in fin:
            clean_line = line.strip("\r\n")
            # If line already has ISO timestamp at start, preserve it
            m_iso = re.match(r"^(\d{4}-\d{2}-\d{2}T[^\s]+)\s+(.*)$", clean_line)
            if m_iso:
                ts = m_iso.group(1)
                text = m_iso.group(2)
            else:
                ts = datetime.now(timezone.utc).astimezone().isoformat()
                text = clean_line

            fraw.write(f"{ts} {text}\n")
            evs = parse_line_for_events(text, host_ts=ts)
            all_events.extend(evs)

    with open(csv_out, "w", newline="", encoding="utf-8") as fcsv:
        writer = csv.writer(fcsv)
        writer.writerow(["timestamp", "event_type", "handle", "addr", "reason", "details"])
        for ev in all_events:
            writer.writerow([ev["timestamp"], ev["event_type"], ev["handle"], ev["addr"], ev["reason"], ev["details"]])

    print(f"Parsed {len(all_events)} events.")
    print(f"Wrote raw log: {raw_out}")
    print(f"Wrote events CSV: {csv_out}")
    return all_events


def capture_serial(port: str, baud: int, batch_id: str, output_dir: str, duration_s: Optional[float] = None):
    """Captures live serial port output and parses BLE events."""
    try:
        import serial
    except ImportError:
        sys.exit("Error: pyserial is required. Install via 'pip install pyserial'.")

    raw_out = os.path.join(output_dir, f"esp32_serial_{batch_id}.log")
    csv_out = os.path.join(output_dir, f"esp32_events_{batch_id}.csv")

    print(f"Opening serial port {port} at {baud} baud...")
    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.5)
    except serial.SerialException as e:
        sys.exit(f"Serial port error on {port}: {e}\nCheck if the port is in use or locked by PlatformIO.")

    print(f"Capturing serial output to {raw_out}...")
    all_events = []
    start_time = time.time()

    with open(raw_out, "a", encoding="utf-8") as fraw, \
         open(csv_out, "a", newline="", encoding="utf-8") as fcsv:

        writer = csv.writer(fcsv)
        if fcsv.tell() == 0:
            writer.writerow(["timestamp", "event_type", "handle", "addr", "reason", "details"])

        try:
            while True:
                if duration_s and (time.time() - start_time) >= duration_s:
                    break

                raw_bytes = ser.readline()
                if not raw_bytes:
                    continue

                line_str = raw_bytes.decode("utf-8", errors="replace").strip("\r\n")
                if not line_str:
                    continue

                ts = datetime.now(timezone.utc).astimezone().isoformat()
                timestamped_line = f"{ts} {line_str}"
                print(timestamped_line)
                fraw.write(timestamped_line + "\n")
                fraw.flush()

                evs = parse_line_for_events(line_str, host_ts=ts)
                for ev in evs:
                    all_events.append(ev)
                    writer.writerow([ev["timestamp"], ev["event_type"], ev["handle"], ev["addr"], ev["reason"], ev["details"]])
                    fcsv.flush()
                    print(f"  --> [EVENT DETECTED] {ev['event_type']} handle={ev['handle']} addr={ev['addr']} reason={ev['reason']}")

        except KeyboardInterrupt:
            print("\nCapture stopped by user.")
        finally:
            ser.close()

    print(f"Finished capture. Total events: {len(all_events)}")
    return all_events


def main():
    parser = argparse.ArgumentParser(description="ESP32 Serial Log Capture and BLE Event Parser.")
    parser.add_argument("--batch-id", required=True, help="Batch identifier (e.g. batch-a-01)")
    parser.add_argument("--output-dir", required=True, help="Directory to store serial log and CSV")
    parser.add_argument("--port", help="Serial port device path (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--duration", type=float, help="Capture duration in seconds (indefinite if omitted)")
    parser.add_argument("--input-file", help="Parse an existing log file instead of live serial capture")

    args = parser.parse_args()
    os.makedirs(args.output_dir, exist_ok=True)

    if args.input_file:
        if not os.path.isfile(args.input_file):
            sys.exit(f"Error: Input file '{args.input_file}' not found.")
        parse_existing_log(args.input_file, args.batch_id, args.output_dir)
        return

    port = args.port or find_default_serial_port()
    if not port:
        sys.exit(
            "Error: No serial port specified and no USB serial device auto-detected.\n"
            "Connect the board or specify --port <device_path> (e.g. /dev/cu.usbmodem1101).\n"
            "Or use --input-file <path> to parse an existing log."
        )

    capture_serial(port, args.baud, args.batch_id, args.output_dir, duration_s=args.duration)


if __name__ == "__main__":
    main()
