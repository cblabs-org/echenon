---
schema_version: 1
wo_id: WO-09.4
pr_id: PR-09
title: Portal CSS Design Tokens, Reset Wiring, Contrast & Accessibility Polish
complexity: standard
status: PENDING
allowed_files:
- web/calibration-wizard.js
- web/diagnostics-events.html
- web/diagnostics.js
- web/echenon-dashboard.js
- web/intervals_settings.js
- web/recalibration.js
- web/ride.html
- web/ride.js
- web/session.js
- web/stored_rides.js
- web/upload_settings.js
- web/wahoo_settings.js
forbidden_files:
- framework/**
- src/**
- docs/**
required_skills:
- framework-expert
- framework-design-system
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: ffeb21990a12c467255b20c6e142a456eca5863b6895d569ab98f04168b7d9a0
completion: null
---

# WO-09.4 — Portal CSS Design Tokens, Reset Wiring, Contrast & Accessibility Polish

## 1. Objective

Remediate Dimension 6 (Portal Design-System Tokens, WCAG AA Contrast & ARIA Accessibility) across `web/` and align two frontend API details with `WO-09.1`:

1. Wrap all bare `#hex` colour literals across `web/*.js` and `web/ride.html` inside `var(--token, #fallback)` expressions matching the Framework Design System (`framework-design-system` skill + portal base stylesheet).
2. Fix the Level Mapping calibration reset bug in `web/calibration-wizard.js` (`resetCalibration()` calling `/api/echenon/calibration/stop` -> update to `POST /api/echenon/calibration/reset`), and refresh the active power table preview in `web/recalibration.js` (`resetPoints()`).
3. Ensure all portal `fetch` calls use `window.apiFetch` (or `credentials: 'same-origin'` fallback) so authenticated sessions succeed cleanly against `WO-09.1`'s `AuthManager`-protected `/api/echenon/*` routes.

## 2. Strict Boundary Box

- **Allowed files:** All 12 UI files under `web/` listed in `allowed_files` (do not edit `web/manifest.json` unless needed — it is not in `allowed_files`).
- **Forbidden files:** `framework/**`, `src/**`, `docs/**`.
- **Mandatory skill:** Read `.agents/skills/framework-design-system/SKILL.md` (and check `framework/web/portal/` CSS variable names if helpful) before editing CSS/colours.
- **Shell hygiene:** Never use bash `$VAR` loops or `/tmp` in `run_command`. Use `call_mcp_tool` (`platformio-mcp_platformio` `build_project` on `ex3-waveshare-nano`) to verify asset gzip bundling (`embed_assets.py`).

## 3. Technical Specification

### 3.1 Tokenise Bare Hex Colours (`var(--token, #fallback)`)
- Inspect every `#hex` (`#[0-9a-fA-F]{3,8}`) match across all 12 `web/` files.
- **Standard portal pills (`calibration-wizard.js`, `diagnostics.js`, `echenon-dashboard.js`, `intervals_settings.js`, `recalibration.js`, `session.js`, `stored_rides.js`, `upload_settings.js`, `wahoo_settings.js`):**
  - Replace bare `#fff` / `#ffffff` button foregrounds with `var(--text-inverse, #ffffff)`.
  - Replace bare warning colours (`#f9a825`, `#ef4444`, `#dc262622`, `#f59e0b`, `#2563eb`, `#10b981`, etc.) with semantic tokens + fallbacks (`var(--warning, #d97706)`, `var(--danger, #dc2626)`, `var(--success, #16a34a)`, `var(--primary, #2196F3)`, `var(--info, #0284c7)`, `var(--danger-bg, rgba(220, 38, 38, 0.13))`, etc.) — using `rgba(...)` rather than 8-digit `#RRGGBBAA` when paired with tokens, or semantic badge classes.
  - Replace bare `rgba(...)` border/surface tints where portal CSS tokens apply cleanly, preserving existing `var(--card-bg)`, `var(--border)`, `var(--text)`, `var(--text-muted)` conventions.
- **Standalone `/ride` page (`web/ride.html` & `web/ride.js`):**
  - Since `/ride` is standalone HTML (dark-by-default cockpit), define a `:root { --bg-page: #121212; --card-bg: #1e1e1e; --border: #333333; --text: #ffffff; --text-muted: rgba(255, 255, 255, 0.7); --primary: #2196F3; --success: #4CAF50; --warning: #FF9800; --danger: #f44336; --text-inverse: #ffffff; --text-on-warning: #000000; ... }` block at the top of `<style>` in `web/ride.html` (supporting `@media (prefers-color-scheme: light)` optional overrides or clean token mapping) and reference `var(--...)` throughout CSS rules and `web/ride.js` dynamic status colours (except `<meta name="theme-color" content="#121212">` which HTML spec requires to be a concrete colour).

### 3.2 WCAG AA Contrast & SVG / ARIA Check
- Audit all warning/amber badges (`#f9a825` text on translucent gray had borderline light-mode contrast!) — use accessible amber (`var(--warning, #b45309)` or `var(--warning, #d97706)` text on light/card background, or dark text on solid amber pill) for `>= 4.5:1` contrast in both light & dark themes.
- Ensure all inline SVGs use `fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"` and icon buttons / drawers have proper `aria-label` / `aria-expanded` / `role="dialog"`.

### 3.3 Endpoint Wiring Fixes (`calibration-wizard.js`, `recalibration.js`, Auth Fetch Consistency)
1. In `web/calibration-wizard.js`:
   - `resetCalibration()`: change `/api/echenon/calibration/stop` to `/api/echenon/calibration/reset` (`method: 'POST'`).
2. In `web/recalibration.js`:
   - In `resetPoints()`, after `if (resp.ok) { await fetchPoints(); await fetchActiveTable(); }` so the active table preview updates immediately on reset.
3. Verify every `fetch` across all `web/*.js` files (`calibration-wizard.js`, `recalibration.js`, `echenon-dashboard.js`, `diagnostics.js`, `session.js`, `stored_rides.js`, `intervals_settings.js`, `upload_settings.js`, `wahoo_settings.js`, `ride.js`):
   - Prefer `var fetchFn = window.apiFetch || function(u, o) { return fetch(u, Object.assign({ credentials: 'same-origin' }, o)); };` consistently so cookie/session/CSRF headers work identically with `WO-09.1`'s `AuthManager`-protected endpoints.

## 4. Verification

1. Run `grep -rnE "#[0-9a-fA-F]{3,8}" web/` to confirm every remaining `#hex` literal in `web/` lives inside a `:root` CSS custom property definition, a `var(--token, #hex)` fallback, or `<meta name="theme-color">`.
2. Verify `web/calibration-wizard.js` calls `/api/echenon/calibration/reset` inside `resetCalibration()`.
3. Via `call_mcp_tool` (`platformio-mcp_platformio`), run `build_project` (`environment: "ex3-waveshare-nano"`) so `portalBuilder.py` bundles all updated `web/` assets cleanly into firmware without overflow or errors.
4. Run `~/.local/bin/pa wo verify WO-09.4` and post completion summary to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
