---
schema_version: 1
wo_id: WO-09.6
pr_id: PR-09
title: LICENSE, Beta Recruitment README, CHANGELOG, CONTRIBUTING & SPEC Sync
complexity: standard
status: PENDING
allowed_files:
- LICENSE
- README.md
- CHANGELOG.md
- CONTRIBUTING.md
- SPEC.md
forbidden_files:
- framework/**
- src/**
- web/**
- docs/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 1cc2b6009fded0c51acea09534ba47e4eb4814b3f4f9867f826b1fcb27a1336a
completion: null
---

# WO-09.6 — LICENSE, Beta Recruitment README, CHANGELOG, CONTRIBUTING & SPEC Sync

## 1. Objective

Create the missing public-repository root files (`LICENSE`, beta-focused `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`) tailored to recruit capable DIY beta builders for **`v0.3.0-beta.1`**, and align `SPEC.md` with the beta OTA deferral and MCP CalibrationRig migration.

## 2. Strict Boundary Box

- **Allowed files:** `LICENSE`, `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SPEC.md`.
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, `docs/**`, or any code/test file.
- **Factuality rule:** Quote only verified hardware facts (`docs/beta-build-guide.md`, `docs/hardware_specification.md`), verified build/test flags (`platformio.ini`), and actual implemented endpoints/MCP tools (`EchenonModule.cpp`). Do NOT invent claims or advertise OTA as active in `v0.3.0-beta.1`.
- **Shell hygiene:** Never use bash `$VAR` loops, `/tmp`, or raw `pio` CLI in `run_command`.

## 3. Technical Specification

### 3.1 `LICENSE`
- Write the full canonical **Apache License, Version 2.0** text (matching all SPDX `Apache-2.0` file headers added in `WO-09.2` / `WO-09.3` and `SPEC.md`). Include copyright notice: `Copyright 2026 Echenon Contributors`.

### 3.2 `README.md` (First-Load Beta Recruitment Landing Page)
Make `README.md` crisp, engaging, visual-scannable, and practical for recruiting a handful of capable DIY makers to build, install, and test **`v0.3.0-beta.1`** on an Echelon EX-3:
1. **Header & Beta Callout:**
   - Clear badge/subtitle: `v0.3.0-beta.1` — **Open-Source Smart Bike Telemetry & FTMS Bridge for the Echelon EX-3 (Passive Tap v1)**.
   - Immediate 2-sentence hook: Turns your Echelon EX-3 into an open Bluetooth FTMS indoor trainer/power meter that works natively with Zwift, MyWhoosh, TrainerRoad, Garmin, and Wahoo — plus physics-calibrated power modeling, standalone `.fit` ride recording, Strava & Intervals.icu sync, and a live local web cockpit (`/overlay`).
   - Clear **"Looking for Early Beta Builders (v0.3.0-beta.1)"** callout explaining what kind of tester we want (comfortable soldering a 70×50 mm perfboard harness, crimping JST connectors, running PlatformIO over USB-C, and capturing diagnostic feedback).
2. **Hardware Power Safety `[!CAUTION]`:**
   - Highlight the **`SW0` Mechanical Power Interlock**: power the Waveshare ESP32-S3-Nano via **bike `+9 V VIN` OR `USB-C` — NEVER BOTH simultaneously**. Link directly to [docs/beta-build-guide.md](docs/beta-build-guide.md).
3. **Key Capabilities (Implemented in v1):**
   - Non-destructive inline passive tap harness (no cut bike wires; stock EX-3 console keeps working normally).
   - Physics-calibrated virtual power & 4-point per-bike Calibration Wizard + live Recalibration UI.
   - Standard BLE FTMS Indoor Bike Data + BLE Heart Rate sink/forwarding + optional Wahoo Kickr mode.
   - LittleFS-buffered `.fit` ride files with automated Strava & Intervals.icu upload + Portal (`/`, `/calibration`, `/overlay`, `/ride`, `/diagnostics`) & JSON-RPC MCP tools (`/api/mcp`).
4. **Shopping List & Quick Start (5 Steps):**
   - Short bulleted hardware summary (Waveshare ESP32-S3-Nano, perfboard, resistors/caps, JST-SM 9-pin + JST 3-pin pass-throughs, mechanical `SW0` switch — total ~\$25–\$35 USD) pointing to the full BOM & wiring guide in [docs/beta-build-guide.md](docs/beta-build-guide.md).
   - Submodule clone note (`git clone --recurse-submodules https://github.com/cblabs-org/echenon.git` — note that `framework` submodule access accompanies the public beta launch), build & USB-C flash (`ex3-waveshare-nano` firmware + LittleFS), WiFi captive-portal onboarding, Calibration Wizard, and first ride.
5. **Known Beta Limitations & Roadmap (v1 -> v2):**
   - Passive read-only tap in v1 (ERG / active motor control is planned for v2 MITM hardware).
   - **`WITH_OTA` disabled in `v0.3.0-beta.1`:** firmware updates during beta are via USB-C only (with `VIN` isolated via `SW0`) while framework OTA is hardened.
   - Verified on **Echelon EX-3** (other Echelon models untested — contributions welcome!).
6. **Documentation Index:**
   - Links to `docs/beta-build-guide.md`, `SPEC.md`, `docs/hardware_specification.md`, `docs/power-physics-model.md`, `docs/data-sinks.md`, `docs/diagnostics.md`, `CONTRIBUTING.md`, and `LICENSE`.

### 3.3 `CHANGELOG.md`
- Follow Keep a Changelog (`[Unreleased]` and `[0.3.0-beta.1]` sections):
  - List core v1 capabilities (ESP32-S3-Nano sensor acquisition, exponential physics power model, BLE FTMS + HR + Wahoo sink, `.fit` persistence + Strava/Intervals.icu upload, `SessionOverlay`, `DiagnosticsRoutes`, `IMCPContributor` tools + `AuthManager` portal security, CalibrationRig MCP migration, standalone beta hardware guide).
  - Explicitly note `WITH_OTA` disabled for beta safety and passive-only EX-3 support.

### 3.4 `CONTRIBUTING.md`
- Short, welcoming guide for beta testers and code contributors:
  - How to report beta issues cleanly (include firmware version, EX-3 hardware revision, `/api/echenon/diagnostics` summary / serial logs — scrub WiFi SSIDs/passwords/tokens before pasting).
  - Hardware safety reminder (`SW0` mechanical `VIN`/`USB-C` interlock — never attach USB-C while `VIN` is live).
  - Developer build & verification commands (`pio test -e native`, `pio run -e ex3-waveshare-nano`, `-diag`, `-rig`).
  - Apache-2.0 contribution terms & repository layout (`src/`, `web/`, `docs/`, `test/`).

### 3.5 `SPEC.md`
- Read lines ~120–180, ~425–475, and ~765–785 of `SPEC.md` first so edits are exact.
- Add concise, non-disruptive notes in those sections clarifying:
  1. For **`v0.3.0-beta.1`**, `WITH_OTA` is intentionally omitted/disabled in `platformio.ini` while framework OTA undergoes stabilisation; beta firmware updates are performed over **USB-C** with the mechanical `SW0` `VIN`/`USB-C` interlock set to USB.
  2. CalibrationRig control uses `IMCPContributor` (`POST /api/mcp`: `echenon_rig_status`, `echenon_rig_command`) rather than raw HTTP endpoints.
  3. Reference [docs/beta-build-guide.md](docs/beta-build-guide.md) alongside `docs/hardware_specification.md` for DIY beta board construction.

## 4. Verification

1. Verify `LICENSE` is full Apache-2.0, markdown links in `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, and `SPEC.md` resolve to real files in the repo, and zero personal WiFi/URL details exist in the edited files.
2. Run `~/.local/bin/pa wo verify WO-09.6` (executes `pio test -e native --verbose`, stamps receipt, commits on `pr-09-integration/wo6-public-repo-docs`).
3. Post completion summary to `@coordinator` via `post_message`.

## Completion Report

_Pending execution._
