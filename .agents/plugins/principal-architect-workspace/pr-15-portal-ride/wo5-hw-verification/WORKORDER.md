---
schema_version: 1
wo_id: WO-15.5
pr_id: PR-15
title: Hardware Verification — P12 Split-Screen
complexity: interactive
status: PENDING
allowed_files:
- docs/measurements/phase4/wo15-portal-ride/**
forbidden_files:
- src/**
- web/**
- include/**
- platformio.ini
- SPEC.md
required_skills:
- framework-expert
- pio-manager
- esp32-flash-monitor
- hardware-in-the-loop-test
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: cdd6e618f244e0acdef5b749c54ab28bfcb6fe5eb0e036c48715969906adcda2
completion: null
---

# WO-15.5: Hardware Verification — P12 Split-Screen

> **Parent PR:** [PR-15 PROJECT.md](../PROJECT.md)
> **Depends on:** WO-15.1–WO-15.4 (all code and docs complete)
> **Branch:** `pr-15-portal-ride/wo5-hw-verification` (base: `pr-15-portal-ride/wo4-spec-updates`)
> **Complexity:** `interactive` — requires physical hardware and user collaboration

---

## 1. Objective

Flash the updated firmware to the bench ESP32-S3 device and verify the portal tab rework and ride page on real hardware and real devices. Collect screenshot evidence and commit to `docs/measurements/phase4/wo15-portal-ride/`.

This is an **interactive** Work Order — the `pa-debugger` will collaborate with the user to run each verification step on physical devices.

## 2. Strict Boundary Box

### Allowed Files
| File | Operations |
|---|---|
| `docs/measurements/phase4/wo15-portal-ride/**` | Commit screenshot evidence and verification notes |

### Forbidden
- Do NOT modify any source code, web files, or build configuration
- This WO is read-only verification with evidence collection

## 3. Detailed Task Checklist

### 3.1 Build & Flash
- [ ] Ensure all WO-15.1–WO-15.4 changes are merged to the PR branch
- [ ] Tag a dev version (`v0.X.Y-dev.N+1`) per versioning policy
- [ ] Build the firmware (`pio run`)
- [ ] Flash to bench device (`/dev/cu.usbmodem101` or current port)
- [ ] Verify boot via serial monitor (clean boot, no crashes, watchdog OK)

### 3.2 Portal Tab Verification (Phone Browser)
- [ ] Open `echenon.local` on phone browser
- [ ] Verify **6 bottom-bar tabs** in correct order:
  1. WiFi
  2. Bluetooth
  3. Echenon
  4. Setup
  5. Security
  6. System
- [ ] Verify **no App tab** visible
- [ ] Screenshot: portal bottom navigation bar
- [ ] Verify Echenon tab shows device status summary + "Open Ride Page →" button
- [ ] Screenshot: Echenon tab content

### 3.3 Setup Tab Pills Verification
- [ ] Tap "Setup" tab
- [ ] Verify PillBar shows 3 pills: Apps, Level Mapping, Power Tuning
- [ ] Tap each pill and verify content renders:
  - Apps: Strava OAuth config
  - Level Mapping: Calibration wizard with pre-populated values
  - Power Tuning: Recalibration with pre-populated values
- [ ] Screenshot: Setup tab with pills visible
- [ ] Screenshot: Each pill panel

### 3.4 Ride Page — Lenovo Tab P12 Split-Screen
- [ ] Open Echelon app on P12
- [ ] Open Chrome alongside in Android split-screen mode
- [ ] Navigate to `echenon.local/ride`
- [ ] Verify page renders correctly at split-screen width (~280–350px)
- [ ] Verify dark theme
- [ ] Verify live metrics display (cadence, power, speed, HR, level)
- [ ] Verify metrics update at ~2 Hz (WebSocket)
- [ ] Verify session controls (Start → Active → Pause → Resume → Stop)
- [ ] Verify upload queue mini-status
- [ ] Verify ride history (or empty state if no previous rides)
- [ ] Verify ⚙️ gear icon links to portal
- [ ] Screenshot: P12 split-screen with Echelon + ride page

### 3.5 PWA Verification
- [ ] Test "Add to Home Screen" on P12
- [ ] Verify PWA launches in standalone mode (no browser chrome)
- [ ] Verify correct theme colour and name
- [ ] Screenshot: PWA icon on home screen
- [ ] Screenshot: PWA in standalone mode

### 3.6 Phone Full-Screen Verification
- [ ] Open `/ride` on phone (full-screen, not split)
- [ ] Verify responsive layout at ~375–430px width
- [ ] Verify all features work in full-screen mode
- [ ] Screenshot: phone full-screen ride page

### 3.7 iPad Slide Over (Optional)
- [ ] If iPad available: test `/ride` in iPad Slide Over mode alongside Echelon app
- [ ] Verify responsive layout
- [ ] Screenshot: iPad Slide Over

### 3.8 End-to-End Session Test
- [ ] From `/ride` page, start a session
- [ ] Verify metrics stream in real-time
- [ ] Pause session, verify badge changes to PAUSED
- [ ] Resume session
- [ ] Stop session
- [ ] Verify ride appears in history
- [ ] Verify TCX download link works
- [ ] Screenshot: active session with live metrics

### 3.9 Commit Evidence
- [ ] Commit all screenshots to `docs/measurements/phase4/wo15-portal-ride/`
- [ ] Create `docs/measurements/phase4/wo15-portal-ride/VERIFICATION.md` with:
  - Date and firmware version
  - Device inventory (which devices tested)
  - Pass/fail status for each test
  - Screenshot index

## 4. Verification

- [ ] All screenshots committed to `docs/measurements/phase4/wo15-portal-ride/`
- [ ] `VERIFICATION.md` written with test results
- [ ] `pio test -e native` still passes
- [ ] No code changes made (evidence-only commit)

---

## Completion Report

*(Populated by `pa-debugger` after interactive verification session)*
