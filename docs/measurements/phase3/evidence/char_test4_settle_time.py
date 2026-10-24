#!/usr/bin/env python3
"""Characterization Test 4: Settle Time Profiling.

Measures how long the console's bang-bang motor controller takes to settle
after multi-level jumps from level 1 to levels 5, 15, 25.

Polls pot ADC every 200ms after stepping, records when pot stabilizes
(3 consecutive readings within 5 ADC counts).
"""
import subprocess
import json
import time
import csv
import sys

API = "http://192.168.1.100/api/echenon/rig"
ADB = "/Users/mcneillm/Library/Android/sdk/platform-tools/adb"
CSV_PATH = "/Users/mcneillm/Documents/Projects/Echenon/tmp/chartest4_settle_time.csv"
EDGES_PER_LEVEL = 8
STEP_DELAY = 0.15  # seconds between edges
POLL_INTERVAL = 0.2  # seconds between pot readings
STABLE_THRESHOLD = 5  # ADC counts — 3 consecutive within this = settled
STABLE_COUNT = 3  # consecutive stable readings required
MAX_SETTLE_TIME = 15.0  # seconds max wait for settle
TARGET_LEVELS = [5, 15, 25]


def api_get(endpoint):
    """GET from rig API, return parsed JSON."""
    result = subprocess.run(
        ["curl", "-s", f"{API}/{endpoint}"],
        capture_output=True, text=True, timeout=5
    )
    return json.loads(result.stdout)


def api_command(cmd):
    """POST command to rig."""
    subprocess.run(
        ["curl", "-s", "-X", "POST", f"{API}/command",
         "-H", "Content-Type: text/plain", "-d", cmd],
        capture_output=True, text=True, timeout=5
    )


def get_adb_resistance():
    """Read resistance from Echelon app UI via ADB uiautomator dump."""
    try:
        result = subprocess.run(
            [ADB, "shell",
             "uiautomator dump /sdcard/window_dump.xml && cat /sdcard/window_dump.xml"],
            capture_output=True, text=True, timeout=10
        )
        import re
        vals = [m.group(1) for m in re.finditer(r'text="([^"]+)"', result.stdout)]
        for i, v in enumerate(vals):
            if v == 'Resistance' and i + 1 < len(vals):
                return vals[i + 1]
    except Exception:
        pass
    return "?"


def send_steps(direction, count):
    """Send `count` STEP commands with STEP_DELAY between them."""
    for i in range(count):
        api_command(f"STEP {direction}")
        time.sleep(STEP_DELAY)


def measure_settle(target_level):
    """Poll pot after stepping, measure time to stabilize. Returns (settle_ms, pot_trace)."""
    start = time.time()
    readings = []
    
    while True:
        elapsed = time.time() - start
        if elapsed > MAX_SETTLE_TIME:
            break
        
        status = api_get("status")
        pot = status["pot"]
        readings.append((elapsed, pot))
        
        # Check stability: last STABLE_COUNT readings within STABLE_THRESHOLD
        if len(readings) >= STABLE_COUNT:
            recent_pots = [r[1] for r in readings[-STABLE_COUNT:]]
            spread = max(recent_pots) - min(recent_pots)
            if spread <= STABLE_THRESHOLD:
                return int(elapsed * 1000), readings
        
        time.sleep(POLL_INTERVAL)
    
    return int((time.time() - start) * 1000), readings


def home_to_level1():
    """Send enough DOWN steps to guarantee level 1."""
    print("  Homing to level 1...")
    api_command("STOP")
    time.sleep(0.5)
    send_steps("DOWN", 32 * EDGES_PER_LEVEL)
    time.sleep(3.0)  # settle
    status = api_get("status")
    print(f"  Home complete: pot={status['pot']} derived={status['derived_level']}")
    return status["pot"]


# ─── Main ───
print("=== Characterization Test 4: Settle Time Profiling ===")
print(f"Start: {time.strftime('%Y-%m-%d %H:%M:%S')}")
print(f"Target levels: {TARGET_LEVELS}")
print(f"Settle criteria: {STABLE_COUNT} readings within {STABLE_THRESHOLD} ADC")
print()

# Version check
sys_info = json.loads(
    subprocess.run(
        ["curl", "-s", "http://192.168.1.100/api/system"],
        capture_output=True, text=True, timeout=5
    ).stdout
)
print(f"Firmware: {sys_info.get('firmware_version', '?')}")
print()

# CSV output
csv_file = open(CSV_PATH, "w", newline="")
writer = csv.writer(csv_file)
writer.writerow(["target_level", "settle_ms", "final_pot", "adb_resistance",
                 "pot_trace_times_ms", "pot_trace_values"])

results = []

for target in TARGET_LEVELS:
    print(f"--- Target Level {target} ---")
    
    # Home first
    home_pot = home_to_level1()
    
    # Step UP to target
    steps_needed = (target - 1) * EDGES_PER_LEVEL
    print(f"  Stepping UP {steps_needed} edges ({target - 1} levels)...")
    send_steps("UP", steps_needed)
    
    # Measure settle
    print(f"  Measuring settle time...")
    settle_ms, trace = measure_settle(target)
    
    # Get final readings
    final_status = api_get("status")
    adb_r = get_adb_resistance()
    final_pot = final_status["pot"]
    
    # Trace for CSV
    trace_times = ",".join(f"{int(t*1000)}" for t, _ in trace)
    trace_values = ",".join(f"{p}" for _, p in trace)
    
    writer.writerow([target, settle_ms, final_pot, adb_r, trace_times, trace_values])
    
    print(f"  SETTLED: {settle_ms}ms  pot={final_pot}  adb_R={adb_r}")
    print(f"  Trace ({len(trace)} samples): {' → '.join(f'{p}' for _, p in trace[:10])}"
          + (f" ... {trace[-1][1]}" if len(trace) > 10 else ""))
    print()
    
    results.append({
        "level": target,
        "settle_ms": settle_ms,
        "pot": final_pot,
        "adb": adb_r,
        "trace_len": len(trace),
    })

csv_file.close()

# Summary
print("=== RESULTS ===")
print(f"{'Level':>5} {'Settle':>8} {'Pot':>5} {'ADB':>4}")
for r in results:
    print(f"{r['level']:5d} {r['settle_ms']:7d}ms {r['pot']:5d} {r['adb']:>4}")

print()
print(f"CSV: {CSV_PATH}")
print(f"End: {time.strftime('%Y-%m-%d %H:%M:%S')}")
