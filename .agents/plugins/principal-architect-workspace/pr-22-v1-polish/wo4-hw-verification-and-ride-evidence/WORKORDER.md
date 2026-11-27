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
spec_hash: ac5f36aef1a91b01341164ee19c94733f8f894f12679c70a7a1acb5d4cabc738
completion: null
---

# WO-22.4 — Hardware Verification, Live Asset Check & Installed-Board Ride Evidence

## 1. Role & Objective

Verify all 3 ESP32-S3 firmware builds (`< 25 KB` flash delta, `0 B` internal DRAM increase), flash and verify live HTTP `200 OK` gzipped branding routes on ESP32-S3 hardware, and author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md` closing the non-OTA half of `PR-04a`.

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only create/modify files under `docs/measurements/phase3/evidence/`, `CHANGELOG.md`, and Part II (`## Completion Report` + `completion:` frontmatter) of this `WORKORDER.md`.
2. **Rule 03 Versioning Policy (`.agents/rules/03-versioning.md`):** Before flashing hardware, confirm user-approved `v0.3.1-dev.1` tag (`git tag v0.3.1-dev.1 && git push origin v0.3.1-dev.1`), rebuild so `inject_version.py` bakes the tag in, and then flash.
3. **Rule 04 MCP Tooling Priority:** Use `pio-antigravity` MCP tools (`pio_test`, `pio_build`, `pio_size_report`, `pio_flash`, `pio_serial_monitor`) — NEVER raw `pio` or `esptool` CLI commands, and NEVER upload filesystem.
4. **Hardware Safety (`SW0` Interlock):** Confirm with the user whether testing on the bench ESP32-S3 or in situ on the installed bike board (`SW0` `VIN` **OFF** before USB-C attachment). Never fabricate unmeasured ride or hardware telemetry.

**Forbidden:** Do NOT edit `src/**`, `web/**`, `test/**`, `tools/**`, `framework/**`, `platformio.ini`, `hardware/**`, `SPEC.md`, or `README.md`.

## 3. Line-Item Instructions

- [ ] Run `pio_test` (`native`) and build all 3 ESP32-S3 targets (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`) via `pio_build` + `pio_size_report`, confirming `< 25 KB` flash delta and `0 B` internal DRAM increase relative to `v0.3.0-dev.51` (`59,516 B` DRAM, `1,798,083 B` flash).
- [ ] Tag `v0.3.1-dev.1` (after confirming with user per Rule 03), rebuild `ex3-waveshare-nano`, flash via `pio_flash`, verify serial boot banner (`v0.3.1-dev.1`), and verify live HTTP `200 OK` (`Content-Encoding: gzip`, `Content-Type`, `ETag`) on `/favicon.ico`, `/favicon.svg`, `/icon-192.png`, `/icon-512.png`, `/manifest.json`, and `/ride`.
- [ ] Author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md` (and `pr22-polish-verification.md` + serial log) documenting the soldered v1 Passive Tap Board extended operational ride validation on the EX-3 bike (`+9 V VIN` via `SW0` interlock, console transparency, live metrics, BLE FTMS/HR stability, cloud uploads) using verified user/hardware observations, closing the non-OTA half of `PR-04a`.
- [ ] Update `CHANGELOG.md` (`## [Unreleased]`) if needed to reference the verified evidence report and size/route checks.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pa wo verify WO-22.4` (executes `pio test -e native --verbose`, stamps receipt, updates `## Completion Report`, and commits `WO-22.4: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
