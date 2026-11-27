---
schema_version: 1
wo_id: WO-22.1
pr_id: PR-22
title: Branding Assets, PWA Icons, Portal Routes & Multi-Sink Copy Polish
complexity: standard
status: PENDING
allowed_files:
- web/favicon.svg
- web/favicon.ico
- web/icon-192.png
- web/icon-512.png
- web/manifest.json
- web/ride.html
- web/ride.js
- web/upload_settings.js
- src/echenon/SessionOverlay.cpp
- docs/assets/echenon-icon-512.png
forbidden_files:
- framework/**
- platformio.ini
- hardware/**
- test/**
- tools/**
- SPEC.md
- README.md
- CHANGELOG.md
required_skills:
- framework-expert
- framework-portal-design
- pio-antigravity
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 9ba326d024f32ec510b497d95d81fda662b48deba6304d778f45d085a4ef5928
completion: null
---

# WO-22.1 — Branding Assets, PWA Icons, Portal Routes & Multi-Sink Copy Polish

## 1. Role & Objective

Create brand-neutral Concept A ("Telemetry Dial") favicon and PWA icon assets, wire their PROGMEM gzipped HTTP routes in `src/echenon/SessionOverlay.cpp`, update the `/ride` PWA manifest/HTML and Strava BYOA icon pointer, and align `/ride` modal copy for multi-sink cloud sync.

## 2. Strict Boundary Box & Constraints

1. **Allowed Files Only:** Only create/modify `web/favicon.svg`, `web/favicon.ico`, `web/icon-192.png`, `web/icon-512.png`, `web/manifest.json`, `web/ride.html`, `web/ride.js`, `web/upload_settings.js`, `src/echenon/SessionOverlay.cpp`, `docs/assets/echenon-icon-512.png`, and Part II (`## Completion Report` + `completion:` frontmatter) of this `WORKORDER.md`.
2. **`portalBuilder.py` Naming Rule (`framework/scripts/portalBuilder.py:129-135`):** Hyphens are preserved in generated `.hpp` header filenames (`icon-192_png.hpp`, `icon-512_png.hpp`), while dots and hyphens both become underscores in C identifiers (`favicon_svg_gz`, `favicon_ico_gz`, `icon_192_png_gz`, `icon_512_png_gz`, plus `_len` and `_etag`).
3. **GRAVE-05 Compliance:** All 4 new `server.on(...)` route handlers in `src/echenon/SessionOverlay.cpp` MUST be stateless lambdas calling `serveGzipAsset(...)` with zero heap `std::function` allocations.
4. **D6 Portal Design System Compliance:** Zero bare `#hex` colors in `web/*.js` (only `var(--token, #fallback)`). Validate `web/manifest.json` with `python3 -m json.tool`.
5. **Flash Budget & Temp Files:** Total gzipped size of `favicon.svg` + `favicon.ico` + `icon-192.png` + `icon-512.png` must remain `< 25 KB`. Any temporary scripts or intermediate files used during SVG rasterization MUST live in repo-local `tmp/` (per `.agents/rules/02-tmp_workspace.md`) and be cleaned up before `pa wo verify WO-22.1`.
6. **ESP32-S3 Build Deferred to `WO-22.4`:** Full 3-target ESP32-S3 `pio_build` + `pio_size_report` verification is handled in `WO-22.4` (after resolving the host PlatformIO `tool-scons` mismatch). Verify `portalBuilder.py` generated headers (`include/portal_assets/{favicon_svg,favicon_ico,icon-192_png,icon-512_png}.hpp`) and native unit tests (`pa wo verify WO-22.1`). Never flash or upload filesystem.

**Forbidden:** Do NOT edit `framework/**`, `platformio.ini`, `hardware/**`, `test/**`, `tools/**`, `SPEC.md`, `README.md`, or `CHANGELOG.md`.

## 3. Line-Item Instructions

- [ ] **Create `web/favicon.svg`** using the exact **Concept A — Telemetry Dial** SVG (`rx="112"`, zero `"E"` letterform and zero Echelon trademark resemblance):
  ```xml
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
    <rect x="0" y="0" width="512" height="512" rx="112" fill="#121212"/>
    <path d="M 136 376 A 170 170 0 1 1 376 376" fill="none" stroke="#263238" stroke-width="38" stroke-linecap="round"/>
    <path d="M 136 376 A 170 170 0 1 1 384 156" fill="none" stroke="#2196F3" stroke-width="38" stroke-linecap="round"/>
    <polyline points="118,266 190,266 226,184 274,332 316,238 344,266 396,266" fill="none" stroke="#FFFFFF" stroke-width="30" stroke-linecap="round" stroke-linejoin="round"/>
    <circle cx="384" cy="156" r="22" fill="#4FC3F7"/>
  </svg>
  ```
- [ ] **Rasterize `web/favicon.ico`, `web/icon-192.png`, `web/icon-512.png`, and `docs/assets/echenon-icon-512.png`:**
  - Rasterize `web/favicon.ico` (`32×32` ICO) from `web/favicon.svg` (`rx="112"`).
  - Using a full-bleed square variant (`rx="0"`, `#121212` background, identical arc/waveform/dot geometry), rasterize `web/icon-512.png` (`512×512` PNG), copy byte-for-byte to `docs/assets/echenon-icon-512.png`, and produce `web/icon-192.png` (`192×192` PNG via high-quality Lanczos resampling or direct rasterization).
  - Verify image dimensions/modes using `/usr/bin/python3` (`PIL.Image`) and confirm total gzipped size of the 4 `web/` icon files is `< 25 KB`. Clean up any `tmp/` scratch files.
- [ ] **Update `web/manifest.json`:** Add `"icons"` array with `/icon-192.png` (`"192x192"`, `"image/png"`, `"purpose":"any maskable"`) and `/icon-512.png` (`"512x512"`, `"image/png"`, `"purpose":"any maskable"`). Validate with `python3 -m json.tool web/manifest.json`.
- [ ] **Update `web/ride.html`:**
  - In `<head>` next to `<link rel="manifest" href="/manifest.json">`, add `<link rel="icon" type="image/svg+xml" href="/favicon.svg">`, `<link rel="icon" href="/favicon.ico" sizes="any">`, and `<link rel="apple-touch-icon" href="/icon-192.png">`.
  - At line `70`, update `"Save to device and push to Strava"` → `"Save to device and sync to connected apps"`.
- [ ] **Update `web/ride.js`:** At line `63`, update `'Discard this ride? It will not be recorded or sent to Strava.'` → `'Discard this ride? It will not be recorded or synced to connected apps.'`.
- [ ] **Update `web/upload_settings.js`:** At line `30` (Strava BYOA setup step), link directly to `/icon-512.png` with `download="echenon-icon-512.png"` using `var(--primary, #2196F3)`. Verify zero bare `#hex` colors across `web/*.js`: `grep -rn -E "#[0-9a-fA-F]{3,8}" web/*.js | grep -v "var(--"`.
- [ ] **Update `src/echenon/SessionOverlay.cpp`:**
  - Inside `#ifndef UNIT_TEST` (after `#include "include/portal_assets/manifest_json.hpp"`), add `#if __has_include(...)` guarded `#include` directives for `favicon_svg.hpp`, `favicon_ico.hpp`, `icon-192_png.hpp`, and `icon-512_png.hpp`.
  - Immediately after `/manifest.json` (`L1258`), add 4 `#if __has_include(...)` guarded stateless `serveGzipAsset(...)` routes for `/favicon.svg` (`"image/svg+xml"`), `/favicon.ico` (`"image/x-icon"`), `/icon-192.png` (`"image/png"`), and `/icon-512.png` (`"image/png"`).
- [ ] **Verify `portalBuilder.py` Generated Headers:** Confirm `include/portal_assets/{favicon_svg,favicon_ico,icon-192_png,icon-512_png}.hpp` exist with matching C identifiers (`favicon_svg_gz`, `favicon_ico_gz`, `icon_192_png_gz`, `icon_512_png_gz`) and zero basename collisions (full 3-target ESP32-S3 `pio_build` deferred to `WO-22.4`).

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pa wo verify WO-22.1` (executes `pio test -e native --verbose`, stamps receipt, updates `## Completion Report`, and commits `WO-22.1: ...`).

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

_Pending execution._
