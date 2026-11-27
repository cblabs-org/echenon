---
schema_version: 1
wo_id: WO-22.3
pr_id: PR-22
title: BLE-Without-WiFi Upstream Assessment & Core Doc Sync
complexity: standard
status: COMPLETED
allowed_files:
- docs/
- SPEC.md
- README.md
- CHANGELOG.md
forbidden_files:
- src/**
- web/**
- test/**
- tools/**
- framework/**
- platformio.ini
- hardware/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 08ba4fcf1ae469f26e8379a4353aff6ee299c749371f9dd4d69f6a7905ba744e
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_fed6041ead78
  commit_sha: 3096c78313eaec1b4f5976d25714e320b00bc74b
  files_modified:
  - CHANGELOG.md
  - README.md
  - SPEC.md
  - docs/upstream-ble-without-wifi-assessment.md
  verification_summary: 'pio test -e native --verbose: ================ 271 test cases:
    271 succeeded in 00:00:00.505 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-22.3 — BLE-Without-WiFi Upstream Assessment & Core Doc Sync

## 1. Role & Objective

Document the verified framework `AppState::Running` gate that blocks BLE operation without WiFi STA and prepare an upstream issue draft for `cblabs-org/esp32-app-foundation` (without modifying `framework/`), and synchronize `SPEC.md`, `README.md`, and `CHANGELOG.md` for `PR-22` deliverables.

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only create/modify files in `docs/`, `SPEC.md`, `README.md`, `CHANGELOG.md`, and Part II (`## Completion Report` + `completion:` frontmatter) of this `WORKORDER.md`.
2. **No Framework Edits:** Do NOT modify any file inside `framework/` or file external GitHub issues; author the upstream issue draft in `docs/`.
3. **Accuracy & Relative Links:** Cite only verified line numbers and symbol names observed directly in `framework/modules/BLEManager/BLEManager.cpp`, `framework/modules/BLEManager/BLEManager.hpp`, `framework/modules/Application/Application.cpp`, and `framework/modules/WifiManager/WifiPreconditionContributor.hpp`. Use relative links inside `docs/`.

**Forbidden:** Do NOT edit `src/**`, `web/**`, `test/**`, `tools/**`, `framework/**`, `platformio.ini`, or `hardware/**`.

## 3. Line-Item Instructions

- [ ] **Author `docs/upstream-ble-without-wifi-assessment.md`:**
  - Document the source-verified `AppState::Running` dependency in `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2` / `02d0b2de`):
    - `framework/modules/BLEManager/BLEManager.cpp:759-762` (`Idle -> Starting` requires `appState == AppState::Running` or `appState == AppState::Configuring && _config.advertiseInConfiguring`, where `advertiseInConfiguring` defaults to `false` in `BLEManager.hpp:114`).
    - `framework/modules/BLEManager/BLEManager.cpp:397-399` (`startDiscovery()` rejected unless `getEffectiveAppState() == AppState::Running`).
    - `framework/modules/BLEManager/BLEManager.cpp:854-892` (HR strap / `IBLEClientProvider` reconnect policy initialization and tick execution strictly gated on `getEffectiveAppState() == AppState::Running`).
    - `framework/modules/Application/Application.cpp:471-473` (regresses `AppState::Running -> AppState::Configuring` whenever a `PreconditionRole::Startup` or `Both` precondition becomes unsatisfied) + `framework/modules/WifiManager/WifiPreconditionContributor.hpp` (`WiFi Station` Startup precondition).
  - Cross-reference the Phase 2 observation in `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md` (§ "v1 Finding: BLE Depends on WiFi STA").
  - Include a complete, ready-to-file GitHub issue draft targeting `cblabs-org/esp32-app-foundation` covering Problem Statement, Verified Code Paths, Consumer Impact (Echenon v1 FTMS + HR relay offline / router outage), Proposed Framework Changes, and Acceptance Criteria.
- [ ] **Update `SPEC.md` (§11.1, §11.3, §10.4):**
  - Document the 4 new PROGMEM gzipped branding asset routes (`/favicon.svg`, `/favicon.ico`, `/icon-192.png`, `/icon-512.png`) in `SessionOverlay::registerRoutes()`, PWA `manifest.json` icons (`192x192`, `512x512`, `any maskable`), `/ride` `<head>` icon links, and the `/icon-512.png` Strava BYOA download pointer.
- [ ] **Update `README.md`:**
  - Add a pointer to the Concept A app icon (`docs/assets/echenon-icon-512.png` and `/icon-512.png` on-device download) for Strava BYOA app registration and PWA home-screen installation, and align multi-sink wording where appropriate without altering hardware safety or beta scope facts.
- [ ] **Update `CHANGELOG.md` (`## [Unreleased]`):**
  - Add concise entries under `### Added` / `### Changed` covering `PR-22` Concept A branding/PWA assets & routes (Issue #2), `/ride` multi-sink UI copy alignment, D1 Apache-2.0 SPDX headers on `test/test_native/*.cpp` and `tools/ble-test/control-peripheral/src/main.cpp`, and the upstream BLE-without-WiFi assessment.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pa wo verify WO-22.3` (executes `pio test -e native --verbose`, stamps receipt, updates `## Completion Report`, and commits `WO-22.3: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
