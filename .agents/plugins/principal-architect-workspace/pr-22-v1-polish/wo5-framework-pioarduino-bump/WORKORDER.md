---
schema_version: 1
wo_id: WO-22.5
pr_id: PR-22
title: Framework v1.0.0-beta.4 & pioarduino 55.03.313 Toolchain Bump
complexity: standard
status: COMPLETED
allowed_files:
- platformio.ini
- framework
- CHANGELOG.md
forbidden_files:
- src/**
- web/**
- docs/**
- test/**
- tools/**
- hardware/**
- SPEC.md
- README.md
required_skills:
- framework-expert
- pio-antigravity
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: cf985a4675b9c6ab4916d386c0f8226069bb2c7d15c4a58efd94cb16a1dc2ba8
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_3cf7e25b5c34
  commit_sha: 2a2b64e7276d38b07b1e0e15bef251ffdd31de83
  files_modified:
  - CHANGELOG.md
  - framework
  - platformio.ini
  verification_summary: 'pio test -e native --verbose: ================ 271 test cases:
    271 succeeded in 00:00:00.438 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-22.5 — Framework v1.0.0-beta.4 & pioarduino 55.03.313 Toolchain Bump

## 1. Role & Objective

Align the `framework` submodule (`cblabs-org/esp32-app-foundation` `v1.0.0-beta.2` → `v1.0.0-beta.4` / `cc05a422`) and `platformio.ini` (`pioarduino/platform-espressif32` `55.03.311` → `55.03.313`) with the host `pioarduino 6.2.0` / `tool-scons 4.41101.0` toolchain, then compile and size-check all 3 ESP32-S3 targets (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`).

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only modify `platformio.ini`, `framework` (gitlink bump to `v1.0.0-beta.4`), `CHANGELOG.md`, and Part II (`status`, `completion`, `## Completion Report`) of this `WORKORDER.md` (via `pa wo verify WO-22.5`).
2. **Submodule Checkout Command (Rule R4):** Always use `git -C framework fetch --tags origin && git -C framework checkout v1.0.0-beta.4`. NEVER run `git checkout` on the superproject or `cd framework && git checkout` (which triggers Rule R4). Do NOT modify any files inside `framework/`.
3. **Rule 04 MCP Tooling Priority:** Use `pio-antigravity` MCP tools (`pio_build`, `pio_size_report`, `pio_test`) — NEVER raw `pio run` or `pio test` CLI commands. NEVER flash hardware or upload filesystem in this Work Order.
4. **`pa wo verify` Hygiene:** Leave `WORKORDER.md` unmodified before calling `pa wo verify WO-22.5` (because `_parse_porcelain_paths` in `pa_cli.py` strips leading `.` on unstaged `.agents/...` paths).

**Forbidden:** Do NOT edit `src/**`, `web/**`, `docs/**`, `test/**`, `tools/**`, `hardware/**`, `SPEC.md`, or `README.md`.

## 3. Line-Item Instructions

- [ ] **Bump `framework` submodule to `v1.0.0-beta.4`:**
  - Run `git -C framework fetch --tags origin && git -C framework checkout v1.0.0-beta.4`.
  - Confirm `git -C framework rev-parse HEAD` equals `cc05a422b6dbc55d7cda964e2887efab1a0bb3b3` and `git -C framework status --short` is clean.
- [ ] **Update `platformio.ini` (line 6):**
  - Change `https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip` to `https://github.com/pioarduino/platform-espressif32/releases/download/55.03.313/platform-espressif32.zip`.
- [ ] **Clear gitignored ESP-IDF/PlatformIO build caches:**
  - Remove stale `.pio/` and `sdkconfig.ex3-waveshare-nano*` so ESP-IDF `5.5.6` / Arduino `3.3.13` configures cleanly from `SDKCONFIG_DEFAULTS`.
- [ ] **Build all 3 ESP32-S3 targets & measure footprint (`pio-antigravity` MCP):**
  - Call `pio_build` on `ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, and `ex3-waveshare-nano-rig` — confirm all 3 compile and link (`success: true`) and `portalBuilder.py` generates `include/portal_assets/{favicon_svg,favicon_ico,icon-192_png,icon-512_png}.hpp` cleanly.
  - Call `pio_size_report` on `ex3-waveshare-nano` and compare static RAM (`59,516 B` baseline) and flash (`1,798,083 B` baseline) against `v0.3.0-dev.51`.
- [ ] **Update `CHANGELOG.md` (`## [Unreleased]`):**
  - Add a concise bullet under `### Changed` noting the `framework` submodule bump to `v1.0.0-beta.4` (`cc05a422`) and `pioarduino/platform-espressif32` `55.03.313` (`ESP-IDF 5.5.6`, `Arduino 3.3.13`) alignment.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pa wo verify WO-22.5` (executes `pio test -e native --verbose`, stamps receipt, updates `## Completion Report`, and commits `WO-22.5: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
