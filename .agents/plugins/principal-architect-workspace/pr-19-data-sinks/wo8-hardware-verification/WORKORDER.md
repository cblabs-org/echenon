---
schema_version: 1
wo_id: WO-19.8
pr_id: PR-19
title: Hardware Verification & Soak
complexity: interactive
status: COMPLETED
allowed_files:
- docs/measurements/phase3/evidence/
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: da3d626213495e68ed34ef9e0f64cf18efaf16a679462310778358a22b4b302b
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_76dbc0bbed80
  commit_sha: 6525cc62bf10bcbe9326fa56c01c8093dbeae5ac
  files_modified:
  - docs/measurements/phase3/evidence/pr19-dual-sink-serial.log
  - docs/measurements/phase3/evidence/pr19-hardware-verification.md
  verification_summary: 'pio test -e native --verbose: ================ 261 test cases:
    261 succeeded in 00:00:00.537 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-19.8 — Hardware Verification & Soak

## 1. Role & Objective

Bench test ride with concurrent dual-sink sync (Strava + Intervals.icu) to verify
the complete multi-sink upload pipeline on real hardware (EX-3 Waveshare Nano).

## 2. Strict Boundary Box & Constraints

1. Only create evidence files under `docs/measurements/phase3/evidence/`.
2. Requires physical hardware: EX-3 bike with Echenon firmware.
3. Requires configured Strava and Intervals.icu accounts.
4. **NEVER call `upload_filesystem`** — web assets are PROGMEM-embedded.
5. **Versioning (Rule 03):** Tag → Build → Flash before hardware testing.

**Forbidden:** No source code edits during this WO.

## 3. Line-Item Instructions

### 3.1 Pre-Flight
- [ ] Verify firmware builds clean: `pio run -e ex3-waveshare-nano`.
- [ ] Tag if needed: `git tag v0.X.Y-dev.N && git push origin v0.X.Y-dev.N`.
- [ ] Rebuild and flash.
- [ ] Verify boot: `curl /api/system` — confirm `firmware_version` matches tag.
- [ ] Configure Strava via portal Apps → Strava pill.
- [ ] Configure Intervals.icu via portal Apps → Intervals.icu pill.

### 3.2 Dual-Sink Upload Test
- [ ] Start a ride session (pedal > 5s for auto-start).
- [ ] Ride for 2-3 minutes with stable cadence.
- [ ] End session via portal or natural stop.
- [ ] Verify auto-upload fans out to both sinks:
  - Monitor serial log for dual `enqueueSession` fan-out.
  - Watch `ScanInhibit` sequential execution (no BLE drops).
  - Confirm both uploads complete via serial log stages.

### 3.3 Portal Verification
- [ ] Open Stored Rides pill: verify ride appears with per-sink sync badges.
- [ ] Strava badge: ✅ Synced with clickable activity link.
- [ ] Intervals.icu badge: ✅ Synced or status.
- [ ] Click `[Download TCX]` — verify file downloads.
- [ ] Test `[Force Push / Retry]` for one sink.

### 3.4 Apps Pill Verification
- [ ] Strava pill: shows "CONNECTED", athlete name, auto-sync toggle.
- [ ] Intervals.icu pill: shows "CONNECTED", athlete name, `[Test Connection]` works.

### 3.5 Evidence Capture
- [ ] Save serial log excerpt to `docs/measurements/phase3/evidence/pr19-dual-sink-*.log`.
- [ ] Screenshot portal Stored Rides and Apps pills.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

This is an interactive WO. Run `pa wo verify WO-19.8` after evidence is captured.
`pio test -e native --verbose` must still pass.

## Completion Report

**Status:** ESCALATED_TO_PA

**Escalation Summary:**
Pre-flight steps 3.1 complete. Interactive ride testing (3.2–3.5) requires user participation.

**Pre-flight completed (3.1):**
- ✅ Firmware builds clean: `ex3-waveshare-nano` SUCCESS (RAM 17.9%, Flash 27.3%)
- ✅ Tagged `v0.3.0-dev.44` (build after tag confirmed version injection)
- ✅ Rebuilt and flashed to `/dev/cu.usbmodem101` (1,715,264 bytes, hash verified)
- ✅ Boot verified via serial: `App version: v0.3.0-dev.44`, State → RUNNING
- ✅ WiFi connected to `<REDACTED_SSID>`, IP: `192.168.1.100`, mDNS: `echenon.local`
- ✅ BLE stack active: HR Service (0x180D), FTMS (0x1826), DIS (0x180A) registered
- ✅ Central connected, HR subscription active
- ✅ Serial boot log saved to `docs/measurements/phase3/evidence/pr19-dual-sink-serial.log`

**Awaiting user interaction for (3.2–3.5):**
- ⬜ Configure Strava via portal at `http://192.168.1.100` → Apps → Strava
- ⬜ Configure Intervals.icu via portal → Apps → Intervals.icu
- ⬜ Ride test: pedal >5s for auto-start, ride 2-3 min, end session
- ⬜ Monitor serial for dual `enqueueSession` fan-out and `ScanInhibit`
- ⬜ Verify portal: Stored Rides pill, sync badges
- ⬜ Verify Apps pill: Strava CONNECTED, Intervals.icu CONNECTED

**Note:** HTTP API calls from this machine time out (TCP connects but no HTTP response within 15s). This may be WiFi congestion (RSSI: -88) or PsychicHttp task contention with active BLE. Portal access via browser may work better. Serial monitoring confirms device is healthy and responsive.
