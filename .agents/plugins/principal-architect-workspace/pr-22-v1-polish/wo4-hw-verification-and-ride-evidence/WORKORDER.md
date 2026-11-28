---
schema_version: 1
wo_id: WO-22.4
pr_id: PR-22
title: Hardware Verification, Live Asset Check & Installed-Board Ride Evidence
complexity: interactive
status: PENDING
allowed_files:
- docs/measurements/phase3/evidence/
- CHANGELOG.md
forbidden_files:
- src/**
- web/**
- test/**
- tools/**
- framework/**
- platformio.ini
- hardware/**
- SPEC.md
- README.md
required_skills:
- framework-expert
- pio-antigravity
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 942d86dd9031bf534ac66315d5033acfdf25226e524803c248ab30d39b7d9b2c
completion: null
---

# WO-22.4 — Hardware Verification, Live Asset Check & Installed-Board Ride Evidence

## 1. Role & Objective

Verify `v0.3.0-beta.2` on hardware in two stages—**Stage 1 (Fresh Bench Board `/dev/cu.usbmodem806599A0DA702`)** for clean boot + live HTTP `200 OK` gzipped branding/PWA route verification, followed by **Stage 2 (Installed EX-3 Bike Board)** for extended operational ride validation—and author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md` closing the non-OTA half of `PR-04a`.

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only create/modify files under `docs/measurements/phase3/evidence/`, `CHANGELOG.md`, and Part II (`## Completion Report` + `completion:` frontmatter) of this `WORKORDER.md`.
2. **Rule 03 Versioning Policy (`.agents/rules/03-versioning.md`):** Use the user-approved **`v0.3.0-beta.2`** tag (open `0.3.0` beta increment), rebuild `ex3-waveshare-nano` so `inject_version.py` bakes `0.3.0-beta.2` into the firmware, and then flash.
3. **Rule 04 MCP Tooling Priority:** Use `pio-antigravity` MCP tools (`pio_test`, `pio_build`, `pio_size_report`, `pio_flash`, `pio_serial_monitor`) — NEVER raw `pio` or `esptool` CLI commands, and NEVER call `target: "filesystem"`.
4. **Hardware Safety (`SW0` Interlock):** For Stage 2 on the installed bike board, confirm with the user that `SW0` (`VIN`) is **OFF** before USB-C attachment, and confirm USB-C is unplugged before switching `SW0` to **BIKE / VIN-ON**. Never fabricate unmeasured ride or hardware telemetry.

**Forbidden:** Do NOT edit `src/**`, `web/**`, `test/**`, `tools/**`, `framework/**`, `platformio.ini`, `hardware/**`, `SPEC.md`, or `README.md`.

## 3. Line-Item Instructions

### Stage 1 — Fresh Bench Board (`/dev/cu.usbmodem806599A0DA702`)
- [ ] Rebuild `ex3-waveshare-nano` via `pio_build` after `v0.3.0-beta.2` tag is created and confirm `0.3.0-beta.2` is injected (`pio_size_report` already confirmed on `WO-22.5`).
- [ ] Flash firmware (`target: "firmware"`, `noBuild: true`) to `/dev/cu.usbmodem806599A0DA702` via `pio_flash`.
- [ ] Capture 30 s first-boot serial output via `pio_serial_monitor` (`action: "capture"`) and save a redacted log to `docs/measurements/phase3/evidence/pr22-bench-serial.log`, confirming version banner `v0.3.0-beta.2`, 8 MB PSRAM/heap health, LittleFS mounts, and `0` panics/WDT resets/`DeviceCrypto` errors.
- [ ] Verify live HTTP `200 OK` (`Content-Encoding: gzip`, `Content-Type`, `ETag`) on `/favicon.ico`, `/favicon.svg`, `/icon-192.png`, `/icon-512.png`, `/manifest.json`, `/ride`, and `/api/system` (`firmware_version: "0.3.0-beta.2"`) once reachable on LAN.

### Stage 2 — Installed EX-3 Bike Board & Ride Validation
- [ ] After user connects the installed bike board (`SW0` `VIN` **OFF**), flash `v0.3.0-beta.2` over USB-C and verify boot/console readiness.
- [ ] Unplug USB-C (`SW0` `VIN` **ON**) and perform in-situ operational ride validation on the EX-3 bike (`+9 V VIN` via `SW0` interlock, console transparency, live metrics, BLE FTMS/HR stability, cloud uploads).
- [ ] Author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md` and `docs/measurements/phase3/evidence/pr22-polish-verification.md` recording verified bench + bike results, closing the non-OTA half of `PR-04a`.
- [ ] Update `CHANGELOG.md` (`## [Unreleased]`) if needed to reference the verified evidence reports.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Do NOT run `pa wo verify WO-22.4` until both Stage 1 and Stage 2 are complete.
- Once complete, leave `WORKORDER.md` clean and run `pa wo verify WO-22.4` (executes `pio test -e native --verbose`, stamps receipt, and commits `WO-22.4: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
