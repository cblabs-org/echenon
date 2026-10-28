---
schema_version: 1
wo_id: WO-19.8
pr_id: PR-19
title: Hardware Verification & Soak
complexity: interactive
status: PENDING
allowed_files:
- docs/measurements/phase3/evidence/
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: da3d626213495e68ed34ef9e0f64cf18efaf16a679462310778358a22b4b302b
completion: null
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
Firmware build for `ex3-waveshare-nano` fails with 2 compilation errors. This WO is forbidden from editing source code, so these must be fixed in a prior WO.

**Error 1:** `src/main.cpp:148` — `cannot convert 'KeyManager*' to 'echenon::ISecretStore*'`
- `IntervalsIcuUploader::setSecretStore()` expects `ISecretStore*` but `KeyManager` does not implement `ISecretStore`.
- Fix: Make `KeyManager` inherit from `echenon::ISecretStore`, or cast appropriately.

**Error 2:** `src/echenon/SessionOverlay.cpp` — `PsychicHttpServer::on()` lambda mismatch
- Route registration lambda has wrong signature for `PsychicHttpServer::on()`.
- Fix: Update lambda signature to match `PsychicHttpHandler` typedef.

**Pre-conditions met:**
- ✅ Branch `pr-19-data-sinks/wo8-hardware-verification` checked out
- ✅ PROJECT.md status: APPROVED
- ✅ Hardware connected at `/dev/cu.usbmodem101`
- ✅ PlatformIO CLI available (v6.1.19)
- ❌ Firmware does not compile — blocked on source fixes
