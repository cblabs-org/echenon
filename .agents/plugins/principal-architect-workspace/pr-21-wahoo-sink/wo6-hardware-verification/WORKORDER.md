---
schema_version: 1
wo_id: WO-21.6
pr_id: PR-21
title: Hardware Verification & Soak
complexity: interactive
status: PENDING
allowed_files:
- docs/
forbidden_files:
- src/**
- web/**
- test/**
- SPEC.md
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: ca8e425c78184d6b37f73f8d2ba1a5d6055fe9c244137ae415c75e93bd30804e
completion: null
---

# WO-21.6 — Hardware Verification & Soak

## 1. Role & Objective

Verify `WahooUploader` and the multi-sink system on real hardware (`ex3-waveshare-nano` on `/dev/cu.usbmodem1101`).
Confirm clean boot, module registration, portal asset serving, and heap health.

**Deliverables:**
- Tag SemVer dev increment per Rule 03 (`v0.X.Y-dev.N`).
- Build firmware for `ex3-waveshare-nano`.
- Flash to connected device via PlatformIO MCP tools.
- Capture serial log and verify:
  - Clean boot with version banner.
  - `WahooUploader` initialized and registered with `UploadQueue` (Sink #3).
  - Web portal serves `/wahoo_settings.js` without errors.
  - Free heap and PSRAM baselines remain healthy.
- Record verification evidence in `docs/measurements/`.

## 2. Strict Boundary Box & Constraints

1. **Rule 03 Versioning Policy:** Tagdev increment BEFORE flashing (`git tag v0.X.Y-dev.N && git push origin v0.X.Y-dev.N`).
2. **Never Call `upload_filesystem`:** Web assets are compiled PROGMEM gzip.
3. **MCP Tooling Priority (Rule 04):** Use PlatformIO MCP tools (`build_project`, `upload_firmware`, `start_monitor`) instead of direct CLI commands where possible.

**Forbidden:** No code modifications to `src/**`, `web/**`, `test/**`.

## 3. Line-Item Instructions

- [ ] Check latest git tag (`git describe --tags --abbrev=0`).
- [ ] Create and push next dev tag (`git tag v0.X.Y-dev.(N+1)`).
- [ ] Build firmware for `ex3-waveshare-nano` via `build_project` MCP tool.
- [ ] Flash firmware via `upload_firmware` MCP tool.
- [ ] Capture boot logs:
  - Confirm `EchenonModule` and `UploadQueue` report 3 sinks registered.
  - Confirm `/api/wahoo/*` routes registered.
  - Check heap stability.
- [ ] Document test findings in completion report.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose`.
- Run `pa wo verify WO-21.6`.

## Completion Report

_Pending execution._
