---
schema_version: 1
wo_id: WO-20.3
pr_id: PR-20
title: Hardware Verification — Tag, Build, Flash & Live Test
complexity: interactive
required_skills:
  - framework-expert
  - pio-manager
  - esp32-flash-monitor
allowed_files:
  - docs/**
forbidden_files:
  - src/**
  - web/**
  - include/**
  - hardware/**
verification_command: "manual"
max_attempts: 5
max_structural_failures: 2
status: PENDING
spec_hash: null
completion: null
---

# WO-20.3 — Hardware Verification: Tag, Build, Flash & Live Test

> **PR:** PR-20 (Echelon Tab Live Diagnostics Drawer)
> **Depends on:** WO-20.1, WO-20.2 (firmware + web changes merged)
> **Branch:** `pr-20-diagnostics-drawer/wo3-hw-verification`

---

## 1. Objective

Tag a new dev version, build with PROGMEM assets, flash to hardware, and
verify that the diagnostics drawer functions correctly with live sensor data.

---

## 2. Strict Boundary Box

### 2.1 Allowed Actions

- Git tagging (`v0.X.Y-dev.N+1`)
- PlatformIO MCP build (`build_project`)
- PlatformIO MCP flash (`upload_firmware`)
- Serial monitor observation
- Browser portal verification
- Document test results in `docs/`

### 2.2 Forbidden

- Do NOT modify source code or web files
- If bugs are found, report them for a remediation WO — do NOT fix inline

---

## 3. Test Procedure

### Step 1: Tag

```bash
git describe --tags --abbrev=0  # check current tag
git tag v0.X.Y-dev.N+1         # increment
git push origin v0.X.Y-dev.N+1
```

### Step 2: Build

Use PlatformIO MCP `build_project` for the `esp32s3-nano` environment.
The `portalBuilder.py` pre-build script will auto-gzip `web/echenon-dashboard.js`
into `include/portal_assets/echenon-dashboard_js.hpp`.

### Step 3: Flash

Use PlatformIO MCP `upload_firmware`. Follow the versioning policy:
Tag → Build → Flash.

### Step 4: Verify Dashboard

1. Open `http://echenon.local` → Echelon tab
2. Verify Live Metrics card shows cadence/power/level/HR updating at ~2 Hz
3. Verify "Echenon Status" static card is GONE
4. Verify info-drawer appears below the Ride Page button

### Step 5: Verify Drawer Lid

1. Check dot colour matches BLE connection state:
   - HRM connected → green dot, "Bike Connected"
   - HRM disconnected → grey dot, "Not Connected"

### Step 6: Verify Expanded Drawer

1. Expand the drawer
2. Verify all sections present: Status, Resistance Knob, Cadence, Motor Sense
3. Verify values update at ~2 Hz (matching WS broadcast rate)
4. Check specific field sanity:
   - `pot_raw` / `pot_filt` should be ~1500-3500 range for connected encoder
   - `p_tilde` should be 0.0-1.0
   - `enc_vcc` should be ~3000-3300 (3.3V ADC reading) when encoder plugged in
   - `cad_us` should show pulse intervals when pedalling
   - `mot_p` / `mot_n` should show motor sense readings

### Step 7: Disconnect Test

1. Unplug the encoder cable
2. Verify `enc_vcc` drops to ~0 and shows red indicator
3. Verify `pot_raw` / `pot_filt` go to 0 or noise
4. Verify drawer lid still works

### Step 8: Serial Log Verification

Monitor serial output and verify the extended WS payload includes new fields:
`pot_raw`, `pot_filt`, `p_tilde`, `cad_us`, `cad_pin`, `enc_vcc`, `mot_p`,
`mot_n`, `ble_conn`, `ses`, `ses_s`, `hr`.

---

## 4. Verification

This is an interactive WO — results are reported to the PA for disposition.
Pass criteria:
- All 8 steps pass
- No console errors in browser
- No crash/panic on ESP32
- WS payload under 600 bytes total


## Completion Report

_No completion report recorded._
