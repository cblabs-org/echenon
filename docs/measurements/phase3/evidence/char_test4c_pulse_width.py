#!/usr/bin/env python3
"""Test 4c fixed: Pulse width at 25ms, 50ms, 75ms with proper HOME."""
import subprocess, json, time, re

API = "http://192.168.1.100/api/echenon/rig"
ADB = "/Users/mcneillm/Library/Android/sdk/platform-tools/adb"

def cmd(c, timeout=15):
    r = subprocess.run(["curl","-s","-X","POST",f"{API}/command","-H","Content-Type: text/plain","-d",c], capture_output=True, text=True, timeout=timeout)
    return r.stdout.strip()

def status():
    r = subprocess.run(["curl","-s",f"{API}/status"], capture_output=True, text=True, timeout=5)
    return json.loads(r.stdout)

def adb_r():
    try:
        r = subprocess.run([ADB,"shell","uiautomator dump /sdcard/window_dump.xml && cat /sdcard/window_dump.xml"], capture_output=True, text=True, timeout=10)
        vals = [m.group(1) for m in re.finditer(r'text="([^"]+)"', r.stdout)]
        for i,v in enumerate(vals):
            if v == 'Resistance' and i+1 < len(vals): return vals[i+1]
    except: pass
    return "?"

def home():
    """HOME using chunked bursts at 50ms spacing, verify pot."""
    cmd("STOP")
    # 4 chunks of 64 edges at 50ms each = 3.2s per chunk, well within timeout
    for _ in range(4):
        cmd("STEP DOWN 64 50000", timeout=10)
    time.sleep(3)
    # Verify
    s = status()
    pot = s['pot']
    if pot > 200:
        # Not at level 1 yet, send more
        for _ in range(4):
            cmd("STEP DOWN 64 50000", timeout=10)
        time.sleep(3)
        s = status()
        pot = s['pot']
    return pot

print("=== Pulse Width Test: 25ms, 50ms, 75ms ===")
print()

delays_ms = [25, 50, 75]
target = 10
edges = (target - 1) * 8  # 72 edges

for delay_ms in delays_ms:
    delay_us = delay_ms * 1000
    burst_time_s = edges * delay_ms / 1000.0
    
    # HOME and verify
    home_pot = home()
    r_before = adb_r()
    print(f"--- {delay_ms}ms delay ({delay_us}µs) ---")
    print(f"  Home: pot={home_pot} adb_R={r_before}")
    
    # Burst UP — timeout = burst_time + 5s margin
    timeout = int(burst_time_s + 10)
    resp = cmd(f"STEP UP {edges} {delay_us}", timeout=timeout)
    print(f"  FW response: {resp}")
    
    # Wait for motor settle
    time.sleep(4)
    
    s = status()
    r_after = adb_r()
    pot_after = s['pot']
    
    ok = "✅" if r_after == str(target) else "⚠"
    print(f"  Result: pot={pot_after} adb_R={r_after} {ok}")
    print()

print("Done")
