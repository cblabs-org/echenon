---
schema_version: 1
wo_id: WO-19.5
pr_id: PR-19
title: Strava Pill Refactor & Connected Apps Polish
complexity: standard
status: PENDING
allowed_files:
- web/upload_settings.js
- web/intervals_settings.js
- src/echenon/SessionOverlay.cpp
- src/echenon/SessionOverlay.h
- src/echenon/EchenonModule.cpp
- src/echenon/EchenonModule.h
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 42447e1950f89412ab400fd00fd615b6f3219454843f985663b7952c2a12fb19
completion: null
---

# WO-19.5 — Strava Pill Refactor & Connected Apps Polish

## 1. Role & Objective

Refactor the Strava-only Apps tab into a modular per-sink pill pattern, create the
Intervals.icu settings pill, and wire `IntervalsIcuUploader` into `EchenonModule`.

**Key Deliverables:**
- Refactor `upload_settings.js` to use consistent card layout conventions.
- Create `intervals_settings.js` with athlete ID/API key form, connection test, auto-sync toggle.
- Register both pills under Apps tab: Strava (order 10), Intervals.icu (order 20).
- Wire `IntervalsIcuUploader` in `EchenonModule`: construct, `registerModule()`, `registerSink()`.
- PROGMEM gzip embedding for `intervals_settings.js`.

## 2. Strict Boundary Box & Constraints

1. **Design System:** Both pills must use the same card/form layout conventions — `--card-bg`, `--border`, `--primary`, `--success`, `--error` tokens. Match the existing Strava pill visual structure.
2. **Tab Registration:** `registry.addTab("Intervals.icu", "Apps", 20, nullptr, "/intervals_settings.js")`.
3. **PROGMEM Embedding:** Generate `intervals_settings_js.hpp` via gzip + xxd.
4. **EchenonModule Wiring:** Construct `IntervalsIcuUploader`, wire DI (SecretStore, HttpClient, FileSystem), call `app.registerModule()` and `uploadQueue.registerSink()`.

**Forbidden:** No `UploadQueue.*` core logic edits. No `IntervalsIcuUploader.*` edits (WO-19.2 scope).

## 3. Line-Item Instructions

### 3.1 upload_settings.js Polish
- [ ] Ensure consistent card layout with design token usage.
- [ ] Add auto-sync toggle label text: "Auto-upload completed rides to Strava".
- [ ] Ensure connection status badge follows standard dot pattern.

### 3.2 intervals_settings.js (new)
- [ ] IIFE pattern with `TAB_ID = 'tab-intervals-icu'`.
- [ ] Card 1: "API Credentials" — athlete ID input, API key password input, Save button.
- [ ] Card 2: "Intervals.icu Integration" — connection badge, athlete name, `[Test Connection]`, `[Disconnect]`, `[Refresh Status]` buttons.
- [ ] Auto-sync toggle: "Auto-upload completed rides to Intervals.icu".
- [ ] Consume endpoints: `GET /api/intervals/status`, `POST /api/intervals/credentials`, `POST /api/intervals/test`, `POST /api/intervals/disconnect`.
- [ ] Lazy init on `tab-changed` event.

### 3.3 EchenonModule Wiring
- [ ] Add `IntervalsIcuUploader _intervalsUploader;` member to `EchenonModule.h`.
- [ ] In `EchenonModule` setup/begin: wire DI, call `_intervalsUploader.begin()`.
- [ ] Call `_uploadQueue.registerSink(&_intervalsUploader)`.
- [ ] Call `app.registerModule(&_intervalsUploader)` for IConfigurable.
- [ ] Pass `IntervalsIcuUploader*` to `SessionOverlay` for Intervals.icu routes.
- [ ] Register tab: `registry.addTab("Intervals.icu", "Apps", 20, nullptr, "/intervals_settings.js")`.

### 3.4 PROGMEM Asset
- [ ] Generate `include/portal_assets/intervals_settings_js.hpp` via gzip + xxd.
- [ ] Register asset route in `SessionOverlay.cpp`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.5` → `pio test -e native --verbose`.

## Completion Report

_Pending execution._
