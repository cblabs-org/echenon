#!/usr/bin/env python3
"""Test 4b: True burst — single STEP UP N command, all edges at GPIO speed.

Tests the console's encoder buffer capacity at maximum edge rate.
"""
import subprocess, json, time, re

API = "http://192.168.1.100/api/echenon/rig"
ADB = "/Users/mcneillm/Library/Android/sdk/platform-tools/adb"
EDGES_PER_LEVEL = 8

def cmd(c):
    r = subprocess.run(["curl","-s","-X","POST",f"{API}/command","-H","Content-Type: text/plain","-d",c], capture_output=True, text=True, timeout=5)
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

print("=== Test 4b: True Burst (single STEP UP N command) ===")
print(f"Firmware: {status().get('state','?')}")
print()

# HOME first — blast 256 DOWN edges in one command
print("=== HOME ===")
resp = cmd("STEP DOWN 256")
print(f"  STEP DOWN 256 response: {resp}")
print("  Waiting 5s for motor...")
time.sleep(5)
s = status()
r = adb_r()
print(f"  After HOME: pot={s['pot']} derived={s['derived_level']} adb_R={r}")

# Test burst to each target level
for target in [5, 15, 25]:
    print(f"\n=== BURST to level {target} ===")
    
    # Home
    cmd("STEP DOWN 256")
    time.sleep(5)
    s = status()
    print(f"  Baseline: pot={s['pot']} derived={s['derived_level']}")
    
    # Single burst command
    edges = (target - 1) * EDGES_PER_LEVEL
    t0 = time.time()
    resp = cmd(f"STEP UP {edges}")
    burst_ms = int((time.time()-t0)*1000)
    print(f"  STEP UP {edges}: {burst_ms}ms (includes HTTP round-trip)")
    print(f"  Firmware response: {resp}")
    
    # Immediately check ADB — does screen update instantly?
    t_adb = time.time()
    adb_imm = adb_r()
    adb_ms = int((time.time()-t_adb)*1000)
    print(f"  ADB immediately: resistance={adb_imm} (read took {adb_ms}ms)")
    
    # Check pot immediately
    s = status()
    print(f"  Pot immediately: pot={s['pot']} derived={s['derived_level']}")
    
    # Poll pot for motor settle
    print(f"  Polling motor settle...")
    settle_start = time.time()
    trace = []
    while time.time() - settle_start < 15:
        s = status()
        elapsed = int((time.time() - settle_start) * 1000)
        trace.append((elapsed, s['pot']))
        
        if len(trace) >= 3:
            recent = [p for _,p in trace[-3:]]
            if max(recent) - min(recent) <= 5:
                break
        time.sleep(0.2)
    
    settle_ms = int((time.time() - settle_start) * 1000)
    adb_final = adb_r()
    final_pot = trace[-1][1] if trace else 0
    
    pots = [p for _,p in trace]
    print(f"  Motor settled: {settle_ms}ms  pot={final_pot}  adb_R={adb_final}")
    print(f"  Trace ({len(trace)} pts): {' -> '.join(str(p) for _,p in trace[:20])}")
    if pots:
        print(f"  Peak={max(pots)} Trough={min(pots)} Final={pots[-1]}")
        # Did the console register all edges?
        expected_adb = str(target)
        if adb_final == expected_adb:
            print(f"  ✅ Console registered all {edges} edges correctly (reached level {target})")
        else:
            print(f"  ⚠ Console reached level {adb_final}, expected {target} — missed edges!")

print(f"\nDone: {time.strftime('%H:%M:%S')}")
