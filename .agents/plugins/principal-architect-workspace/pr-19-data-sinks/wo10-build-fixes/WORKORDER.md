---
schema_version: 1
wo_id: WO-19.10
pr_id: PR-19
title: Firmware Build Fixes
complexity: mechanical
status: PENDING
allowed_files:
- src/echenon/IntervalsIcuUploader.h
- src/echenon/IntervalsIcuUploader.cpp
- src/echenon/SessionOverlay.cpp
- src/main.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: a1a0b9562c9c614fd227c77f1c130a947d94996c77367bba5c36dfee22c289b3
completion: null
---

# WO-19.10 — Firmware Build Fixes

## 1. Role & Objective

Fix two compilation errors that prevent the firmware from building for the
hardware target (`ex3-waveshare-nano`). These errors only manifest during
`pio run -e ex3-waveshare-nano`, not during `pio test -e native` because
native tests use mocks and don't link against the real ESP32 libraries.

## 2. Strict Boundary Box & Constraints

1. Only modify files listed in `allowed_files`.
2. Must NOT break any existing native tests (261/261 must pass).
3. Follow the existing `StravaUploader` pattern for the `KeyManager` adapter.
4. Preserve all existing functionality.

**Forbidden:** No `web/` edits. No `test/` edits. No `docs/` edits.

## 3. Line-Item Instructions

### 3.1 Fix Error 1: KeyManager to ISecretStore conversion in main.cpp

**Error:** `src/main.cpp:148: cannot convert 'KeyManager*' to 'echenon::ISecretStore*'`

The `IntervalsIcuUploader::setSecretStore()` expects `ISecretStore*` but
`app.getKeyManager()` returns `KeyManager*` which does not implement `ISecretStore`.

**Fix — follow the StravaUploader pattern exactly:**

1. In `IntervalsIcuUploader.cpp`, add a `KeyManagerSecretStore` adapter class
   inside an anonymous namespace guarded by `#if defined(ESP_PLATFORM)`, just
   like `StravaUploader.cpp` lines 31-66. The adapter must:
   - Inherit from `ISecretStore`
   - Wrap `KeyManager*` with `putSecret()`, `getSecret()`, `deleteSecret()`
   - Use `qualifyKey()` with prefix `"intervals."` (not `"strava."`)

2. Add a `setKeyManager(KeyManager*)` method to `IntervalsIcuUploader` (both
   `.h` and `.cpp`), following `StravaUploader::setKeyManager()` at line 245
   of `StravaUploader.cpp`. It should:
   - Store `_keyManager` pointer
   - Under `#if defined(ESP_PLATFORM)`, set up the static
     `KeyManagerSecretStore` adapter and assign it as `_secretStore`

3. In `src/main.cpp`, change line 148 from:
   ```cpp
   echenonModule.intervalsUploader().setSecretStore(&app.getKeyManager());
   ```
   to:
   ```cpp
   echenonModule.intervalsUploader().setKeyManager(&app.getKeyManager());
   ```

### 3.2 Fix Error 2: PROGMEM lambda capture in SessionOverlay.cpp

**Error:** `src/echenon/SessionOverlay.cpp:1314: 'intervals_settings_js_gz' is not captured`

The `#include "include/portal_assets/intervals_settings_js.h"` at line 1310
defines `const` variables inside the function body. The lambda at line 1311
is non-capturing `[]` but references these variables.

**Fix:** Move the `#include "include/portal_assets/intervals_settings_js.h"`
and its `#if __has_include(...)` guard from inside the `registerRoutes()`
function body (lines 1309-1319) to file scope — place it near the top of the
file alongside the other PROGMEM asset includes. Then keep the `server.on()`
route registration in the same place inside `registerRoutes()` but remove the
`#if __has_include` guard around the route (since the include is now at file
scope and guaranteed to be present). Alternatively, wrap the route in
`#ifdef INTERVALS_SETTINGS_JS_AVAILABLE` if you want conditional compilation.

The key requirement: the `const` arrays (`intervals_settings_js_gz`, `_len`,
`_etag`) must be at file scope so the non-capturing `[]` lambda can reference
them as globals.

### 3.3 Build Verification

After both fixes:
- Run `pio test -e native --verbose` (all 261 tests must pass)
- Run PlatformIO MCP `build_project` with environment `ex3-waveshare-nano` —
  the hardware build must succeed with zero errors.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.10` -> `pio test -e native --verbose`. All tests must pass.
Additionally confirm hardware build success via `build_project` MCP tool.

## Completion Report

_Pending execution._
