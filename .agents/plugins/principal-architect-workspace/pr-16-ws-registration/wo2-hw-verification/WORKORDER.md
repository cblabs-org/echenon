---
schema_version: 1
wo_id: WO-16.2
pr_id: PR-16
title: Hardware Verification — Tag, Build, Flash & Live Test
complexity: interactive
status: COMPLETED
allowed_files:
- docs/measurements/phase3/evidence/**
forbidden_files:
- src/**
- include/**
- framework/**
- web/**
required_skills:
- pio-manager
- framework-expert
verification_command: manual
max_attempts: 3
max_structural_failures: 2
spec_hash: null
completion:
  status: COMPLETED
  verification_summary: Hardware verified — v0.3.0-dev.33 flashed, WebSocket client registration confirmed via Python probe (11 telemetry frames at 2 Hz)
---

# WO-16.2 — Hardware Verification: Tag, Build, Flash & Live Telemetry Test

> **PR:** PR-16 (WebSocket Client Registration Fix)
> **Complexity:** interactive (requires hardware + user)
> **Verification:** Manual — serial log + browser observation

---

## 1. Objective

Verify the WebSocket registration fix on real hardware. Tag a new dev
version, build firmware + filesystem, flash to the ESP32-S3, and confirm
2 Hz live telemetry flows to both `/ride` and the Echenon dashboard tab.

---

## 2. Strict Boundary Box

### 2.1 Allowed Files

| File | Purpose |
|---|---|
| `docs/measurements/phase3/evidence/**` | Capture serial log evidence |

### 2.2 Forbidden

- **All source files** — code changes are complete in WO-16.1.
- **web/** — already modified in WO-16.1.

---

## 3. Verification Steps

### 3.1 Pre-Flash

1. Confirm `pio test -e native --verbose` passes (233/233).
2. Check current tag: `git describe --tags --abbrev=0`.
3. Create dev tag: `git tag v0.3.0-dev.33 && git push origin v0.3.0-dev.33`.
   - *(Adjust tag number if dev.33 already exists.)*

### 3.2 Build & Flash

4. Build: `build_project` (MCP) — verify `portalBuilder.py` gzips the
   updated JS files.
5. Flash firmware: `upload_firmware` (MCP).
6. Flash filesystem: `upload_filesystem` (MCP).
7. Monitor serial output for boot sequence.

### 3.3 Live Telemetry Verification

8. Open `http://<device-ip>/ride` in a browser.
9. Start pedalling (or trigger auto-start).
10. **Verify in serial log:** `checkForNewClient` or client registration
    evidence appears within seconds of page load.
11. **Verify in browser:** cadence, power, speed, HR update at ~2 Hz
    (no static `--` values).
12. **Verify dashboard tab:** Switch to Echenon tab on the portal —
    live metrics update in real time.

### 3.4 Fallback Verification (Optional)

13. Kill the WebSocket connection (e.g., toggle WiFi momentarily).
14. Confirm the HTTP fallback polling kicks in within 2.5s.
15. Confirm reconnection restores 2 Hz WebSocket updates.

### 3.5 Strava Link Verification

16. If any ride in history has `activity_id`, confirm a clickable
    `Strava ↗` link appears next to the TCX download link.

---

## 4. Success Criteria

- Serial log shows WebSocket client registration on page load.
- Browser displays live-updating metrics (not static `--`).
- `firmware_version` in `/api/system` matches the new dev tag.
- Native tests still pass 233/233.
## Completion Report

_No completion report recorded._
