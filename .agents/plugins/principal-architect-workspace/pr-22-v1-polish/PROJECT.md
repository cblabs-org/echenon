---
schema_version: 1
charter:
  pr_id: PR-22
  programme_id: prog-default
  slug: pr-22-v1-polish
  title: v1 Polish, Branding & Stable-Release Fixes
  branch: feat/pr-22-v1-polish
  base_branch: main
  depends_on: []
  allowed_modules:
  - src/echenon/SessionOverlay.cpp
  - web/
  - docs/
  - test/test_native/
  - tools/ble-test/control-peripheral/src/
  - SPEC.md
  - README.md
  - CHANGELOG.md
  - platformio.ini
  - framework
  required_skills:
  - framework-expert
  - framework-portal-design
  - pio-antigravity
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: df82429991d4826b7da6e023dd6df99c36609a6f6b610ec8988bba37f75d3624
status: APPROVED
approved_at: '2026-11-27T15:46:51.218462Z'
github_pr: null
work_orders:
- id: WO-22.1
  slug: wo1-branding-portal-polish
  title: Branding Assets, PWA Icons, Portal Routes & Multi-Sink Copy Polish
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 1fa754c06e5b6b6698ae2e9d1b3deb23c1b1c3a6
- id: WO-22.2
  slug: wo2-d1-spdx-headers
  title: D1 Apache-2.0 SPDX License Headers
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 3096c78313eaec1b4f5976d25714e320b00bc74b
- id: WO-22.3
  slug: wo3-upstream-ble-assessment-docs
  title: BLE-Without-WiFi Upstream Assessment & Core Doc Sync
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 2cdceb946e0b23c0ab6fb9bcef2483f53f1b3f0b
- id: WO-22.5
  slug: wo5-framework-pioarduino-bump
  title: Framework v1.0.0-beta.4 & pioarduino 55.03.313 Toolchain Bump
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: d0cb89cd140942860c669272f3abfb890e33a174
- id: WO-22.4
  slug: wo4-hw-verification-and-ride-evidence
  title: Hardware Verification, Live Asset Check & Installed-Board Ride Evidence
  complexity: interactive
  status: PENDING
  attempts: 0
  commit_sha: null
---

# PR-22 — v1 Polish, Branding & Stable-Release Fixes

> **Programme:** Echenon Firmware (`Track A, Thread 1` — `v1.0.0` non-OTA polish)
> **Target Base:** `main` (`b4ee3fe`)
> **Branch:** `feat/pr-22-v1-polish`
> **Related Issues:** [cblabs-org/echenon#2](https://github.com/cblabs-org/echenon/issues/2) (branding/icons); `WITH_OTA` deferred to Track A Thread 2 (`cblabs-org/esp32-app-foundation` `#230`, `#104`, `#103`)

## 1. PR Charter: Scope, Goals, & Non-Goals

### 1.1 Goal & Background

Execute **Track A, Thread 1** on the path from `v0.3.0-beta.1` to **`v1.0.0` stable** by delivering non-OTA polish, brand-neutral app/PWA branding assets, multi-sink UI copy alignment, D1 Apache-2.0 SPDX license hygiene, installed-board ride validation evidence (closing the non-OTA half of `PR-04a`), and an upstream BLE-without-WiFi assessment—while keeping **Track A, Thread 2 (`WITH_OTA` hardening)** cleanly isolated in a separate thread against upstream `cblabs-org/esp32-app-foundation` (`#230`, `#104`, `#103`).

### 1.2 Core Scope & Deliverables

1. **Branding Assets, PWA Icons & Device Asset Routes (closes Issue #2):**
   - Use **Concept A — Telemetry Dial** (brand-neutral geometric design with **zero `"E"` letterform and zero Echelon trademark resemblance**):
     ```xml
     <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
       <rect x="0" y="0" width="512" height="512" rx="112" fill="#121212"/>
       <path d="M 136 376 A 170 170 0 1 1 376 376" fill="none" stroke="#263238" stroke-width="38" stroke-linecap="round"/>
       <path d="M 136 376 A 170 170 0 1 1 384 156" fill="none" stroke="#2196F3" stroke-width="38" stroke-linecap="round"/>
       <polyline points="118,266 190,266 226,184 274,332 316,238 344,266 396,266" fill="none" stroke="#FFFFFF" stroke-width="30" stroke-linecap="round" stroke-linejoin="round"/>
       <circle cx="384" cy="156" r="22" fill="#4FC3F7"/>
     </svg>
     ```
   - Create `web/favicon.svg` (`rx="112"`), `web/favicon.ico` (`32×32` ICO rasterized from `favicon.svg`), and full-bleed square (`rx="0"`, `#121212` background) PNGs `web/icon-192.png`, `web/icon-512.png`, and `docs/assets/echenon-icon-512.png`.
   - Update `web/manifest.json` with `"icons"` (`192x192` and `512x512`, `"purpose": "any maskable"`).
   - Update `web/ride.html` `<head>` with `<link rel="icon" type="image/svg+xml" href="/favicon.svg">`, `<link rel="icon" href="/favicon.ico" sizes="any">`, and `<link rel="apple-touch-icon" href="/icon-192.png">`.
   - Update `web/upload_settings.js:30` Strava BYOA setup step to link directly to `/icon-512.png` (`download="echenon-icon-512.png"`).
   - Wire `include/portal_assets/{favicon_svg,favicon_ico,icon-192_png,icon-512_png}.hpp` (guarded with `#if __has_include(...)`) and 4 stateless `serveGzipAsset(...)` routes (`/favicon.svg`, `/favicon.ico`, `/icon-192.png`, `/icon-512.png`) in `src/echenon/SessionOverlay.cpp` immediately after `/manifest.json` (`L1258`).

2. **Multi-Sink Portal Copy Polish & D1 SPDX License Headers:**
   - Update `web/ride.html:70` (`"Save to device and push to Strava"` → `"Save to device and sync to connected apps"`) and `web/ride.js:63` (`"Discard this ride? It will not be recorded or sent to Strava."` → `"Discard this ride? It will not be recorded or synced to connected apps."`).
   - Prepend the canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` header block to all 20 unit test files in `test/test_native/*.cpp` and `tools/ble-test/control-peripheral/src/main.cpp` (closing the D1 advisory in `docs/release-audit-v0.3.0-beta.1.md`).

3. **Installed-Board Ride Validation Evidence, BLE-Without-WiFi Upstream Assessment & Doc Sync:**
   - Author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md` documenting the soldered v1 Passive Tap Board extended operational ride validation on the EX-3 bike (`+9 V VIN` via `SW0` interlock, console transparency, live metrics, BLE FTMS/HR stability, cloud uploads), closing the non-OTA half of `PR-04a`.
   - Document the verified `framework/modules/BLEManager/BLEManager.cpp:759-762,854-892` `AppState::Running` gate (which blocks BLE advertising and HR strap reconnect when booting without WiFi STA) and prepare an upstream issue draft for `cblabs-org/esp32-app-foundation` (without modifying `framework/`).
   - Update `SPEC.md` (§11 asset routes), `README.md` (icon / Strava BYOA pointer), and `CHANGELOG.md` (`## [Unreleased]`).

4. **Hardware Verification (`interactive`) & Cold-Firewall Audit Prep:**
   - Execute `pio test -e native --verbose` (`271/271+` passing) and build all 3 ESP32-S3 targets (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`, `ex3-waveshare-nano-rig`) via `pio-antigravity` MCP (`pio_build`, `pio_size_report`), confirming `< 25 KB` flash delta and `0 B` internal DRAM increase.
   - Per `.agents/rules/03-versioning.md` (Tag → Build → Flash), increment the dev tag, rebuild, flash over USB-C (`SW0` OFF), and verify live HTTP `200 OK` (`Content-Encoding: gzip`, `Content-Type`, `ETag`) on `/favicon.ico`, `/favicon.svg`, `/icon-192.png`, `/icon-512.png`, `/manifest.json`, and `/ride`, then stage `feat/pr-22-v1-polish` for Step 6A Cold-Firewall Audit (`pa-auditor`).

### 1.3 Non-Goals

- No modifications to `framework/` submodule, `platformio.ini`, or `hardware/`.
- `WITH_OTA` remains disabled and explicitly deferred to Track A Thread 2.
- No changes to sensor acquisition, calibration, power physics, or BLE GATT protocols.

## 2. Architectural Requirements, Cross-PR Interfaces, & Required Skills/Tools

1. **`portalBuilder.py` Asset Naming Rule (`framework/scripts/portalBuilder.py:129-135`):** Hyphens are preserved in generated `.hpp` header filenames (`icon-192_png.hpp`, `icon-512_png.hpp`) while both dots and hyphens become underscores in C identifiers (`icon_192_png_gz`, `icon_192_png_gz_len`, `icon_192_png_gz_etag`). Verified zero basename or `/favicon.ico` route collisions in `framework/`.
2. **GRAVE-05 Compliance:** All new HTTP route handlers in `src/echenon/SessionOverlay.cpp` must use stateless lambdas with `serveGzipAsset(...)` and zero heap `std::function` allocations.
3. **D6 Portal Design System Compliance:** Zero bare `#hex` colors in `web/*.js` (use `var(--token, #fallback)`). Validate `web/manifest.json` with `python3 -m json.tool`.
4. **Flash & DRAM Budget:** All 4 icon assets live in `PROGMEM` gzipped arrays (`< 25 KB` total flash delta, `0 B` internal DRAM increase).
5. **Governance & Tooling:**
   - Follow `.agents/rules/03-versioning.md` (`Tag -> Build -> Flash`) before any hardware upload.
   - Follow `.agents/rules/04-mcp-tooling.md` (use `pio-antigravity` MCP tools `pio_build`, `pio_test`, `pio_flash`, `pio_size_report` instead of raw `pio` CLI commands).
   - Required skills for workers where applicable: `framework-expert`, `framework-portal-design`, `pio-antigravity`.

## 3. Implementation Plan & Work Order Breakdown

### WO-22.1 — Branding Assets, PWA Icons, Portal Routes & Multi-Sink Copy Polish
- **Slug:** `wo1-branding-portal-polish`
- **Complexity:** `standard` (`Model: "flash"`)
- **Allowed Files:** `web/favicon.svg`, `web/favicon.ico`, `web/icon-192.png`, `web/icon-512.png`, `web/manifest.json`, `web/ride.html`, `web/ride.js`, `web/upload_settings.js`, `src/echenon/SessionOverlay.cpp`, `docs/assets/echenon-icon-512.png`
- **Required Skills:** `framework-expert`, `framework-portal-design`, `pio-antigravity`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:** Author Concept A ("Telemetry Dial") favicon/PWA PNG/ICO assets, update `web/manifest.json`, `web/ride.html`, `web/ride.js`, and `web/upload_settings.js` (multi-sink copy + Strava BYOA `/icon-512.png` download), wire 4 `#if __has_include(...)` stateless `serveGzipAsset(...)` routes in `src/echenon/SessionOverlay.cpp`, and verify `ex3-waveshare-nano` compilation via `pio_build`.

### WO-22.2 — D1 Apache-2.0 SPDX License Headers
- **Slug:** `wo2-d1-spdx-headers`
- **Complexity:** `standard` (`Model: "flash"`)
- **Allowed Files:** `test/test_native/`, `tools/ble-test/control-peripheral/src/main.cpp`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:** Prepend canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` / `Module:` / `Description:` headers to all 20 `test/test_native/*.cpp` files and `tools/ble-test/control-peripheral/src/main.cpp` to close the D1 audit advisory.

### WO-22.3 — BLE-Without-WiFi Upstream Assessment & Core Doc Sync
- **Slug:** `wo3-upstream-ble-assessment-docs`
- **Complexity:** `standard` (`Model: "flash"`)
- **Allowed Files:** `docs/`, `SPEC.md`, `README.md`, `CHANGELOG.md`
- **Required Skills:** `framework-expert`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:** Author `docs/upstream-ble-without-wifi-assessment.md` documenting the `BLEManager.cpp:759-762,854-892` `AppState::Running` gate and ready-to-file upstream issue draft for `cblabs-org/esp32-app-foundation`, and sync `SPEC.md`, `README.md`, and `CHANGELOG.md`.

### WO-22.5 — Framework v1.0.0-beta.4 & pioarduino 55.03.313 Toolchain Bump
- **Slug:** `wo5-framework-pioarduino-bump`
- **Complexity:** `standard` (`Model: "flash"`)
- **Allowed Files:** `platformio.ini`, `framework`, `CHANGELOG.md`
- **Required Skills:** `framework-expert`, `pio-antigravity`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:** Bump `framework` submodule to `v1.0.0-beta.4` (`cc05a422`), bump `platformio.ini` to `pioarduino/platform-espressif32` `55.03.313` (`ESP-IDF 5.5.6`, `Arduino 3.3.13`), compile all 3 ESP32-S3 targets via `pio_build` + `pio_size_report`, and note the alignment in `CHANGELOG.md`.

### WO-22.4 — Hardware Verification, Live Asset Check & Installed-Board Ride Evidence
- **Slug:** `wo4-hw-verification-and-ride-evidence`
- **Complexity:** `interactive` (`Model: "inherit"`)
- **Allowed Files:** `docs/measurements/phase3/evidence/`, `CHANGELOG.md`
- **Required Skills:** `framework-expert`, `pio-antigravity`
- **Verification Command:** `pio test -e native --verbose`
- **Scope:** Build all 3 ESP32-S3 environments (`pio_build` / `pio_size_report`), coordinate user-gated `v0.3.1-dev.1` dev tag + USB-C flash (`SW0` OFF) + live HTTP asset checks, and author `docs/measurements/phase3/evidence/pr04a-installed-ride-validation.md`.

## 4. Outcome & Session Log
- **2026-11-27:** User approved 4-Work-Order decomposition (`WO-22.1`–`WO-22.4`); Tier 2 PA authored Part II and `wo1`–`wo4` specs (`WO-22.4` paused for user hardware touch-in).
- **2026-11-27 (`WO-22.1`):** Completed (`1fa754c0`, `rcpt_4be3a976d187`, `271/271` native tests). Added Concept A branding/PWA assets (`17,979 B` / `17.56 KB` gzipped `< 25 KB`), `SessionOverlay.cpp` PROGMEM routes, and multi-sink `/ride` copy; deferred ESP32 `pio_build` to `WO-22.4` due to host `tool-scons` (`4.40801.0` vs `4.41101.0`) conflict.
- **2026-11-27 (`WO-22.2`):** Completed (`3096c783`, `rcpt_8b5e000fb145`, `271/271` native tests). Prepended Apache-2.0 SPDX headers (`Module:`/`Description:`) to all 20 `test/test_native/*.cpp` files and `tools/ble-test/control-peripheral/src/main.cpp` (`0` missing).
- **2026-11-27 (`WO-22.3`):** Completed (`2cdceb94`, `rcpt_fed6041ead78`, `271/271` native tests). Authored `docs/upstream-ble-without-wifi-assessment.md` and synced `SPEC.md`, `README.md`, and `CHANGELOG.md`. Pausing before `WO-22.4` (`interactive`) for user hardware touch-in.
- **2026-11-28:** PR-22 Charter amended (`platformio.ini`, `framework`, `charter_hash: df824299...`); added `WO-22.5` (`wo5-framework-pioarduino-bump`) to align `framework` (`v1.0.0-beta.4`) and `pioarduino/platform-espressif32` (`55.03.313`) before `WO-22.4`.
- **2026-11-28 (`WO-22.5`):** Completed (`d0cb89cd`, `rcpt_3cf7e25b5c34`, `271/271` native tests). Bumped `framework` to `v1.0.0-beta.4` (`cc05a422`) and `platformio.ini` to `55.03.313`; all 3 ESP32-S3 targets compiled cleanly (`ex3-waveshare-nano`: `59,776 B` RAM, `1,835,363 B` flash). Pausing before `WO-22.4` (`interactive`) for user hardware touch-in.
