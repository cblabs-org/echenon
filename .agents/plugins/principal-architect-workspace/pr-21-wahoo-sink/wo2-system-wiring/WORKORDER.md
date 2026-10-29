---
schema_version: 1
wo_id: WO-21.2
pr_id: PR-21
title: System Wiring & REST Endpoints
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/echenon/SessionOverlay.h
- src/echenon/SessionOverlay.cpp
- src/main.cpp
forbidden_files:
- src/echenon/WahooUploader.*
- web/**
- test/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: e8f3ec5630621dff121fdf022691fc6c1f3d1a884b68cfa320a5d02e1ba92261
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_bac5b101d302
  commit_sha: 3e9b495da64eeea315f389aa0fc289e092f66a58
  files_modified:
  - src/echenon/EchenonModule.cpp
  - src/echenon/EchenonModule.h
  - src/echenon/SessionOverlay.cpp
  - src/echenon/SessionOverlay.h
  - src/main.cpp
  verification_summary: 'pio test -e native --verbose: ================ 261 test cases:
    261 succeeded in 00:00:00.432 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-21.2 — System Wiring & REST Endpoints

## 1. Role & Objective

Integrate `WahooUploader` into the application lifecycle across `EchenonModule`, `src/main.cpp`,
`UploadQueue`, and `SessionOverlay`, exposing the required REST API endpoints for Wahoo OAuth 2.0
and configuration management.

**Deliverables:**
- `EchenonModule`: own `_wahooUploader`, provide accessor `wahooUploader()`, register Wahoo tab under `Apps` tab (`order 30`, route `"/wahoo_settings.js"`).
- `src/main.cpp`: wire `_wahooUploader` dependencies (`KeyManager`, `Redactor`, config persistence, `begin()`), register `_wahooUploader` with `UploadQueue` (Sink #3) and `app.registerModule()`, and inject into `SessionOverlay`.
- `SessionOverlay`:
  - Pull in `WahooUploader.cpp` under `#ifdef UNIT_TEST` for native test builds.
  - Implement handlers: `handleGetWahooStatus()`, `handleSaveWahooCredentials()`, `handleGetWahooAuthUrl()`, `handleWahooCallback()`, `handleWahooDisconnect()`.
  - Register endpoints: `GET /api/wahoo/status`, `POST /api/wahoo/credentials`, `GET /api/wahoo/auth-url`, `GET /api/wahoo/callback`, `POST /api/wahoo/disconnect`.
  - Serve `/wahoo_settings.js` under `#if __has_include("include/portal_assets/wahoo_settings_js.hpp")`.

## 2. Strict Boundary Box & Constraints

1. **Sink Index Invariant:** Wahoo is Sink #3 (`_sinks[2]`, within `MAX_SINKS = 4`). Strava (Sink #1) and Intervals.icu (Sink #2) must remain unchanged.
2. **Zero Heap `std::function` (GRAVE-05):** All route handler lambdas must use existing capture patterns (`[this, checkAuth, sendResp]` or non-capturing).
3. **Unity Test Inclusion:** In `SessionOverlay.cpp`, add `#include "echenon/WahooUploader.cpp"` under `#ifdef UNIT_TEST` so native test binaries resolve linker symbols without duplicate definitions.
4. **OAuth Redirect:** In `handleWahooCallback()`, redirect to `/#tab-apps` on success or `/#tab-apps?wahoo_error=...` on failure.

**Forbidden:** No modifications to `WahooUploader.*`, `web/**`, `test/**`.

## 3. Line-Item Instructions

- [ ] In `src/echenon/EchenonModule.h`:
  - Include `"echenon/WahooUploader.h"`.
  - Add `WahooUploader _wahooUploader;` member variable.
  - Add `WahooUploader& wahooUploader() { return _wahooUploader; }` accessor.
- [ ] In `src/echenon/EchenonModule.cpp`:
  - In `setupTabs(ITabRegistry& registry)`, register:
    `registry.addTab("Wahoo Cloud", "Apps", 30, nullptr, "/wahoo_settings.js");`
- [ ] In `src/main.cpp`:
  - In the session/upload wiring block:
    ```cpp
    echenonModule.wahooUploader().setKeyManager(&app.getKeyManager());
    echenonModule.wahooUploader().setSecretRegistrar(echenon::WahooUploader::defaultRegisterSecret);
    echenonModule.wahooUploader().setMetadataSaveCallback([](uint32_t, const std::string&) {
        return app.getConfigManager().saveModuleConfig(&echenonModule.wahooUploader());
    });
    echenonModule.wahooUploader().begin();
    uploadQueue.registerSink(&echenonModule.wahooUploader());
    app.registerModule(&echenonModule.wahooUploader());
    sessionOverlay.setWahooUploader(&echenonModule.wahooUploader());
    ```
- [ ] In `src/echenon/SessionOverlay.h`:
  - Forward declare `class WahooUploader;`.
  - Add `setWahooUploader(WahooUploader* wu)` and `WahooUploader* _wahooUploader = nullptr;`.
  - Declare handlers: `handleGetWahooStatus(uint32_t now_epoch_s)`, `handleSaveWahooCredentials(const std::string& body_json)`, `handleGetWahooAuthUrl(const std::string& redirect_uri, uint32_t now_ms)`, `handleWahooCallback(const std::string& code, const std::string& state, uint32_t now_ms)`, `handleWahooDisconnect()`.
- [ ] In `src/echenon/SessionOverlay.cpp`:
  - Include `#include "echenon/WahooUploader.cpp"` under `#ifdef UNIT_TEST`.
  - Implement the declared Wahoo handlers using `ArduinoJson` (mirroring Strava handler patterns).
  - Register the routes in `registerRoutes()`:
    - `/api/wahoo/status` (GET)
    - `/api/wahoo/credentials` (POST)
    - `/api/wahoo/auth-url` (GET)
    - `/api/wahoo/callback` (GET)
    - `/api/wahoo/disconnect` (POST)
  - Under `#if __has_include("include/portal_assets/wahoo_settings_js.hpp")`, serve `/wahoo_settings.js`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` to confirm clean compilation and zero test regression.
- Run `pa wo verify WO-21.2` to generate cryptographic receipt.

## Completion Report

All line-item deliverables completed and verified:
1. `src/echenon/EchenonModule.h`: Included `"echenon/WahooUploader.h"`, added `_wahooUploader` instance and `wahooUploader()` accessor.
2. `src/echenon/EchenonModule.cpp`: Registered Wahoo tab in `registerTabs()`: `registry.addTab("Wahoo Cloud", "Apps", 30, nullptr, "/wahoo_settings.js")`.
3. `src/main.cpp`: Configured `wahooUploader()` with `KeyManager`, default secret registrar, config persistence callback, called `begin()`, registered as Sink #3 in `UploadQueue`, registered with `app.registerModule()`, and injected into `sessionOverlay.setWahooUploader()`.
4. `src/echenon/SessionOverlay.h`: Forward declared `WahooUploader`, added setter and member, declared testable handler methods (`handleGetWahooStatus`, `handleSaveWahooCredentials`, `handleGetWahooAuthUrl`, `handleWahooCallback`, `handleWahooDisconnect`).
5. `src/echenon/SessionOverlay.cpp`: Added `#include "echenon/WahooUploader.cpp"` under `#ifdef UNIT_TEST` for clean native unit test linking, implemented all five Wahoo handler methods, registered endpoints `/api/wahoo/status`, `/api/wahoo/credentials`, `/api/wahoo/auth-url`, `/api/wahoo/callback` (with redirect to `/#tab-apps`), `/api/wahoo/disconnect`, and served `/wahoo_settings.js` under `#if __has_include("include/portal_assets/wahoo_settings_js.hpp")`.

Verified cleanly via `pio test -e native --verbose` with 261/261 tests passing and receipt `rcpt_bac5b101d302` stamped.
