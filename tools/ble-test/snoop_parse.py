#!/usr/bin/env python3
"""
snoop_parse.py - BTSnoop / BTSnooz HCI trace parser for Android BLE connection analysis.

Parses Android btsnooz_hci.log and raw btsnoop_hci.log files (datalink type 1002 / H4 format).
Converts 64-bit BTSnoop timestamps into wall-clock UTC and BST time.
Extracts LE connection commands, events, parameter updates, and accept-list operations.

Usage:
    python snoop_parse.py <file_or_zip> [--peer <addr>] [--csv <out.csv>] [--all]
"""

import sys
import os
import struct
import argparse
import csv
import base64
import zlib
import zipfile
from datetime import datetime, timezone, timedelta
from typing import Optional, List, Dict, Any, Tuple

BTSNOOP_MAGIC = b"btsnoop\x00"
BTSNOOP_EPOCH_OFFSET_US = 0x00DC_DDB3_0F2F_8000  # Microseconds from 0001-01-01 to 1970-01-01
BST_OFFSET = timedelta(hours=1)

# HCI Packet Types
HCI_COMMAND_PKT = 0x01
HCI_ACLDATA_PKT = 0x02
HCI_SCODATA_PKT = 0x03
HCI_EVENT_PKT = 0x04
HCI_ISODATA_PKT = 0x05

# HCI Event Codes
EVT_DISCONN_COMPLETE = 0x05
EVT_CMD_COMPLETE = 0x0E
EVT_CMD_STATUS = 0x0F
EVT_LE_META = 0x3E

# LE Meta Subevent Codes
LE_SUB_CONN_COMPLETE = 0x01
LE_SUB_CONN_UPDATE_COMPLETE = 0x03
LE_SUB_ENH_CONN_COMPLETE_V1 = 0x0A
LE_SUB_DIR_ADV_REPORT = 0x0B
LE_SUB_PHY_UPDATE_COMPLETE = 0x0C
LE_SUB_ENH_CONN_COMPLETE_V2 = 0x29

# HCI LE Command Opcodes
CMD_LE_SET_ADV_ENABLE = 0x200A
CMD_LE_SET_SCAN_PARAMS = 0x200B
CMD_LE_SET_SCAN_ENABLE = 0x200C
CMD_LE_CREATE_CONN = 0x200D
CMD_LE_CREATE_CONN_CANCEL = 0x200E
CMD_LE_CLEAR_ACCEPT_LIST = 0x2010
CMD_LE_ADD_ACCEPT_LIST = 0x2011
CMD_LE_REMOVE_ACCEPT_LIST = 0x2012
CMD_LE_CONN_UPDATE = 0x2013
CMD_LE_EXT_CREATE_CONN = 0x2043

DISCONNECT_REASONS = {
    0x08: "Connection Timeout",
    0x13: "Remote User Terminated Connection",
    0x16: "Connection Terminated by Local Host",
    0x22: "LMP Response Timeout / LL Response Timeout",
    0x3E: "Connection Failed to be Established",
}


def format_bd_addr(raw_bytes: bytes) -> str:
    """Formats 6 raw bytes (little-endian) into MAC string XX:XX:XX:XX:XX:XX."""
    if len(raw_bytes) < 6:
        return "00:00:00:00:00:00"
    return ":".join(f"{b:02X}" for b in reversed(raw_bytes[:6]))


def parse_btsnoop_stream(stream_bytes: bytes):
    """
    Parses binary btsnoop data into a list of packet dicts.
    """
    if len(stream_bytes) < 16:
        raise ValueError("Stream is too short to be a valid BTSnoop file")

    magic = stream_bytes[:8]
    if magic != BTSNOOP_MAGIC:
        raise ValueError(f"Invalid magic: {magic!r}")

    version, datalink = struct.unpack(">II", stream_bytes[8:16])
    if version != 1:
        raise ValueError(f"Unsupported BTSnoop version: {version}")

    offset = 16
    packets = []
    pkt_num = 0

    while offset + 24 <= len(stream_bytes):
        orig_len, incl_len, flags, drops, ts_raw = struct.unpack(">IIIIQ", stream_bytes[offset:offset+24])
        offset += 24

        if offset + incl_len > len(stream_bytes):
            break

        data = stream_bytes[offset:offset+incl_len]
        offset += incl_len
        pkt_num += 1

        # Calculate wall-clock timestamp
        ts_unix_us = ts_raw - BTSNOOP_EPOCH_OFFSET_US
        dt_utc = datetime.fromtimestamp(ts_unix_us / 1e6, tz=timezone.utc)
        dt_bst = dt_utc + BST_OFFSET

        packets.append({
            "pkt_num": pkt_num,
            "orig_len": orig_len,
            "incl_len": incl_len,
            "flags": flags,
            "drops": drops,
            "ts_raw": ts_raw,
            "dt_utc": dt_utc,
            "dt_bst": dt_bst,
            "data": data,
        })

    return packets


def extract_btsnoop_from_file(filepath: str) -> bytes:
    """
    Reads btsnoop raw data from file, zip, or base64 text.
    """
    if zipfile.is_zipfile(filepath):
        with zipfile.ZipFile(filepath, "r") as z:
            # Look for btsnooz or btsnoop files
            candidates = [n for n in z.namelist() if "btsnoo" in n.lower() or "btsnoop" in n.lower()]
            if not candidates:
                # Look for bluetooth snoop logs
                candidates = [n for n in z.namelist() if n.endswith(".log") and "bluetooth" in n.lower()]
            if not candidates:
                raise FileNotFoundError("No BTSnoop or btsnooz log found inside zip archive")
            target = candidates[0]
            with z.open(target) as f:
                content = f.read()
                if content.startswith(BTSNOOP_MAGIC):
                    return content
                # Try base64 decoding if text
                return decode_btsnooz_text(content)

    with open(filepath, "rb") as f:
        content = f.read()

    if content.startswith(BTSNOOP_MAGIC):
        return content

    return decode_btsnooz_text(content)


def decode_btsnooz_text(content: bytes) -> bytes:
    """
    Decodes btsnooz base64 text if present.
    """
    text = content.decode("utf-8", errors="ignore")
    # Check for BEGIN:BTSNOOP markers
    lines = text.splitlines()
    b64_lines = []
    in_block = False

    for line in lines:
        stripped = line.strip()
        if "BEGIN:BTSNOOP" in stripped:
            in_block = True
            continue
        if "END:BTSNOOP" in stripped:
            in_block = False
            break
        if in_block:
            b64_lines.append(stripped)
        elif not in_block and len(stripped) > 40 and not stripped.startswith("#"):
            # May be naked base64
            b64_lines.append(stripped)

    if b64_lines:
        b64_str = "".join(b64_lines)
        try:
            raw = base64.b64decode(b64_str)
            if raw.startswith(BTSNOOP_MAGIC):
                return raw
            # Standard Android btsnooz format: version (1 byte), last_time_ms (8 bytes), then zlib
            if len(raw) > 9:
                try:
                    decomp = zlib.decompress(raw[9:])
                    # Reconstruct BTSnoop file with header
                    hdr = BTSNOOP_MAGIC + struct.pack(">II", 1, 1002)
                    return hdr + decomp
                except Exception:
                    pass
            return raw
        except Exception:
            pass

    raise ValueError("Could not decode BTSnoop binary format from file")


def parse_hci_packets(packets: List[Dict[str, Any]], filter_peer: Optional[str] = None, show_all: bool = False):
    """
    Analyzes HCI packets and extracts LE connection lifecycle events.
    """
    events = []
    handle_peer_map: Dict[int, str] = {}
    first_ts: Optional[datetime] = None

    if packets:
        first_ts = packets[0]["dt_utc"]

    for pkt in packets:
        data = pkt["data"]
        if not data:
            continue

        pkt_type = data[0]
        dt_utc = pkt["dt_utc"]
        dt_bst = pkt["dt_bst"]
        rel_s = (dt_utc - first_ts).total_seconds() if first_ts else 0.0

        # HCI COMMAND (0x01)
        if pkt_type == HCI_COMMAND_PKT and len(data) >= 3:
            opcode = struct.unpack("<H", data[1:3])[0]
            param_len = data[3] if len(data) > 3 else 0
            payload = data[4:4+param_len]

            if opcode == CMD_LE_EXT_CREATE_CONN:
                filter_policy = payload[0] if len(payload) > 0 else 0
                own_addr_type = payload[1] if len(payload) > 1 else 0
                peer_addr_type = payload[2] if len(payload) > 2 else 0
                peer_addr = format_bd_addr(payload[3:9]) if len(payload) >= 9 else "00:00:00:00:00:00"
                phys = payload[9] if len(payload) > 9 else 0

                desc = f"filter_policy={filter_policy}, own_type={own_addr_type}, peer_type={peer_addr_type}, phys=0x{phys:02x}"
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_EXT_CREATE_CONNECTION",
                    "handle": None,
                    "peer_addr": peer_addr,
                    "status": "INIT",
                    "details": desc,
                })

            elif opcode == CMD_LE_CREATE_CONN:
                scan_itvl, scan_win, filter_policy, peer_addr_type = struct.unpack("<HHBB", payload[0:6])
                peer_addr = format_bd_addr(payload[6:12])
                desc = f"filter_policy={filter_policy}, peer_type={peer_addr_type}, scan_itvl={scan_itvl*0.625}ms, scan_win={scan_win*0.625}ms"
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_CREATE_CONNECTION",
                    "handle": None,
                    "peer_addr": peer_addr,
                    "status": "INIT",
                    "details": desc,
                })

            elif opcode == CMD_LE_CREATE_CONN_CANCEL:
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_CREATE_CONNECTION_CANCEL",
                    "handle": None,
                    "peer_addr": None,
                    "status": "CANCEL",
                    "details": "Initiator cancel connection attempt",
                })

            elif opcode == CMD_LE_CONN_UPDATE and len(payload) >= 14:
                handle, min_itvl, max_itvl, latency, timeout = struct.unpack("<HHHHH", payload[0:10])
                peer = handle_peer_map.get(handle, "unknown")
                desc = f"itvl={min_itvl*1.25:.2f}-{max_itvl*1.25:.2f}ms, latency={latency}, timeout={timeout*10}ms"
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_CONNECTION_UPDATE",
                    "handle": handle,
                    "peer_addr": peer,
                    "status": "UPDATE",
                    "details": desc,
                })

            elif opcode == CMD_LE_ADD_ACCEPT_LIST and len(payload) >= 7:
                addr_type = payload[0]
                addr = format_bd_addr(payload[1:7])
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_ADD_ACCEPT_LIST",
                    "handle": None,
                    "peer_addr": addr,
                    "status": "ACCEPT_LIST",
                    "details": f"type={addr_type}",
                })

            elif opcode == CMD_LE_REMOVE_ACCEPT_LIST and len(payload) >= 7:
                addr_type = payload[0]
                addr = format_bd_addr(payload[1:7])
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_REMOVE_ACCEPT_LIST",
                    "handle": None,
                    "peer_addr": addr,
                    "status": "ACCEPT_LIST",
                    "details": f"type={addr_type}",
                })

            elif opcode == CMD_LE_CLEAR_ACCEPT_LIST:
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "CMD",
                    "name": "LE_CLEAR_ACCEPT_LIST",
                    "handle": None,
                    "peer_addr": None,
                    "status": "ACCEPT_LIST",
                    "details": "Clear all entries",
                })

        # HCI EVENT (0x04)
        elif pkt_type == HCI_EVENT_PKT and len(data) >= 3:
            evt_code = data[1]
            param_len = data[2]
            payload = data[3:3+param_len]

            if evt_code == EVT_DISCONN_COMPLETE and len(payload) >= 4:
                status, handle, reason = struct.unpack("<BHB", payload[0:4])
                peer = handle_peer_map.get(handle, "unknown")
                reason_str = DISCONNECT_REASONS.get(reason, f"0x{reason:02X}")
                status_str = "SUCCESS" if status == 0 else f"FAIL(0x{status:02x})"
                desc = f"status=0x{status:02X} ({status_str}), reason=0x{reason:02X} ({reason_str})"
                events.append({
                    "pkt_num": pkt["pkt_num"],
                    "utc": dt_utc,
                    "bst": dt_bst,
                    "rel_s": rel_s,
                    "type": "EVT",
                    "name": "DISCONNECTION_COMPLETE",
                    "handle": handle,
                    "peer_addr": peer,
                    "status": f"DISCONNECTED({reason_str})",
                    "details": desc,
                })

            elif evt_code == EVT_LE_META and len(payload) >= 1:
                sub_code = payload[0]
                sub_data = payload[1:]

                if sub_code in (LE_SUB_ENH_CONN_COMPLETE_V1, LE_SUB_ENH_CONN_COMPLETE_V2) and len(sub_data) >= 15:
                    status, handle, role, addr_type = struct.unpack("<BHBB", sub_data[0:5])
                    peer_addr = format_bd_addr(sub_data[5:11])
                    local_rpa = format_bd_addr(sub_data[11:17]) if len(sub_data) >= 17 else ""
                    peer_rpa = format_bd_addr(sub_data[17:23]) if len(sub_data) >= 23 else ""
                    itvl, latency, timeout = 0, 0, 0
                    if len(sub_data) >= 29:
                        itvl, latency, timeout = struct.unpack("<HHH", sub_data[23:29])

                    status_str = "SUCCESS" if status == 0 else f"FAIL(0x{status:02x})"
                    role_str = "Central" if role == 0 else "Peripheral"
                    desc = f"role={role_str}, peer_rpa={peer_rpa}, itvl={itvl*1.25:.2f}ms, latency={latency}, timeout={timeout*10}ms"

                    if status == 0 and peer_addr != "00:00:00:00:00:00":
                        handle_peer_map[handle] = peer_addr

                    events.append({
                        "pkt_num": pkt["pkt_num"],
                        "utc": dt_utc,
                        "bst": dt_bst,
                        "rel_s": rel_s,
                        "type": "EVT",
                        "name": "LE_ENH_CONNECTION_COMPLETE",
                        "handle": handle,
                        "peer_addr": peer_addr,
                        "status": status_str,
                        "details": desc,
                    })

                elif sub_code == LE_SUB_CONN_COMPLETE and len(sub_data) >= 11:
                    status, handle, role, addr_type = struct.unpack("<BHBB", sub_data[0:5])
                    peer_addr = format_bd_addr(sub_data[5:11])
                    itvl, latency, timeout = 0, 0, 0
                    if len(sub_data) >= 17:
                        itvl, latency, timeout = struct.unpack("<HHH", sub_data[11:17])
                    status_str = "SUCCESS" if status == 0 else f"FAIL(0x{status:02x})"
                    role_str = "Central" if role == 0 else "Peripheral"
                    desc = f"role={role_str}, itvl={itvl*1.25:.2f}ms, latency={latency}, timeout={timeout*10}ms"

                    if status == 0 and peer_addr != "00:00:00:00:00:00":
                        handle_peer_map[handle] = peer_addr

                    events.append({
                        "pkt_num": pkt["pkt_num"],
                        "utc": dt_utc,
                        "bst": dt_bst,
                        "rel_s": rel_s,
                        "type": "EVT",
                        "name": "LE_CONNECTION_COMPLETE",
                        "handle": handle,
                        "peer_addr": peer_addr,
                        "status": status_str,
                        "details": desc,
                    })

                elif sub_code == LE_SUB_CONN_UPDATE_COMPLETE and len(sub_data) >= 9:
                    status, handle, itvl, latency, timeout = struct.unpack("<BHHHH", sub_data[0:9])
                    peer = handle_peer_map.get(handle, "unknown")
                    status_str = "SUCCESS" if status == 0 else f"FAIL(0x{status:02x})"
                    desc = f"itvl={itvl*1.25:.2f}ms, latency={latency}, timeout={timeout*10}ms"
                    events.append({
                        "pkt_num": pkt["pkt_num"],
                        "utc": dt_utc,
                        "bst": dt_bst,
                        "rel_s": rel_s,
                        "type": "EVT",
                        "name": "LE_CONN_UPDATE_COMPLETE",
                        "handle": handle,
                        "peer_addr": peer,
                        "status": status_str,
                        "details": desc,
                    })

    # Filtering
    if filter_peer and not show_all:
        norm_peer = filter_peer.upper()
        filtered = []
        for ev in events:
            ev_peer = (ev["peer_addr"] or "").upper()
            # Include if peer matches, or if peer is 00:00:00:00:00:00 (accept list / cancel), or peer is unknown with matching handle
            if ev_peer == norm_peer or ev_peer == "00:00:00:00:00:00" or ev["peer_addr"] is None:
                filtered.append(ev)
            elif ev_peer == "UNKNOWN":
                filtered.append(ev)
        events = filtered

    return events


def main():
    parser = argparse.ArgumentParser(description="Parse BTSnoop/BTSnooz HCI logs and extract BLE connection events.")
    parser.add_argument("file", help="Path to btsnooz_hci.log, btsnoop_hci.log, or bugreport zip")
    parser.add_argument("--peer", default="3C:0F:02:DE:72:26", help="Filter by target peer address (default: 3C:0F:02:DE:72:26)")
    parser.add_argument("--all", action="store_true", help="Show all LE events without address filtering")
    parser.add_argument("--csv", help="Optional output CSV filepath")

    args = parser.parse_args()

    if not os.path.isfile(args.file):
        sys.exit(f"Error: File '{args.file}' not found.")

    try:
        raw_data = extract_btsnoop_from_file(args.file)
    except Exception as e:
        sys.exit(f"Error reading file '{args.file}': {e}")

    try:
        packets = parse_btsnoop_stream(raw_data)
    except Exception as e:
        sys.exit(f"Error parsing BTSnoop packets: {e}")

    events = parse_hci_packets(packets, filter_peer=args.peer, show_all=args.all)

    print(f"Parsed {len(packets)} total packets from {args.file}")
    print(f"Extracted {len(events)} relevant connection events (filter peer: {args.peer if not args.all else 'NONE'})")
    print("=" * 110)
    print(f"{'Pkt#':<6} {'BST Time':<12} {'Rel(s)':<9} {'Type':<4} {'Event / Command':<28} {'Handle':<7} {'Peer Address':<18} {'Status':<10} {'Details'}")
    print("-" * 110)

    for ev in events:
        bst_str = ev["bst"].strftime("%H:%M:%S.%f")[:-3]
        handle_str = str(ev["handle"]) if ev["handle"] is not None else "-"
        peer_str = ev["peer_addr"] if ev["peer_addr"] else "-"
        print(f"{ev['pkt_num']:<6} {bst_str:<12} {ev['rel_s']:>7.3f}s  {ev['type']:<4} {ev['name']:<28} {handle_str:<7} {peer_str:<18} {ev['status']:<10} {ev['details']}")

    print("=" * 110)

    if args.csv:
        csv_dir = os.path.dirname(args.csv)
        if csv_dir:
            os.makedirs(csv_dir, exist_ok=True)
        with open(args.csv, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["packet_num", "utc_time", "bst_time", "rel_sec", "type", "name", "handle", "peer_addr", "status", "details"])
            for ev in events:
                writer.writerow([
                    ev["pkt_num"],
                    ev["utc"].isoformat(),
                    ev["bst"].isoformat(),
                    f"{ev['rel_s']:.3f}",
                    ev["type"],
                    ev["name"],
                    ev["handle"] if ev["handle"] is not None else "",
                    ev["peer_addr"] or "",
                    ev["status"],
                    ev["details"]
                ])
        print(f"Wrote {len(events)} events to {args.csv}")


if __name__ == "__main__":
    main()
