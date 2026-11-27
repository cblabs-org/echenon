# Upstream Assessment: BLE Operation Without WiFi STA (`AppState::Running` Gate)

> **Status:** Consumer assessment & upstream issue draft (no `framework/` edits made in Echenon)
> **Target Upstream Repo:** [`cblabs-org/esp32-app-foundation`](https://github.com/cblabs-org/esp32-app-foundation)
> **Evaluated Framework Version:** `v1.0.0-beta.2` (`02d0b2de`)
> **Cross-Reference:** [BLE 0x3e Debug Session (2026-10-13) — v1 Finding: BLE Depends on WiFi STA](measurements/phase2/BLE-0x3e-debug-2026-10-13.md#v1-finding-ble-depends-on-wifi-sta)

---

## 1. Executive Summary

In `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2`, commit `02d0b2de`), `BLEManager` cannot operate standalone without an active WiFi Station (`STA`) connection:

1. **Cold boot without WiFi STA (unconfigured credentials, setup AP mode, or offline venue):** `WifiPreconditionContributor` registers `PreconditionNames::WIFI_STATION` with `PreconditionRole::Startup`. While WiFi STA is not connected, `Application` remains in `AppState::Configuring`. Because `BLEConfig::advertiseInConfiguring` defaults to `false`, `BLEManager::tick()` never leaves `State::Idle`, never initializes NimBLE (`_stack.init()`), never registers GATT services, and never starts BLE advertising.
2. **Even with `advertiseInConfiguring = true`:** Peripheral advertising and GATT server initialization can start in `AppState::Configuring`, but **all BLE Central operations** (`startDiscovery()` and `BLEReconnectPolicy` initialization/scheduling for `IBLEClientProvider` clients) are strictly hard-gated on `getEffectiveAppState() == AppState::Running`.
3. **Runtime router/AP outage while already running:** Sustained WiFi STA loss accumulates damping penalty on `WiFi Station` until `state.suppressed` trips, causing `Application::tick()` to regress `AppState::Running -> AppState::Configuring`. Once regressed, HR strap / central discovery and automatic reconnect loops freeze until WiFi STA recovers—even though BLE FTMS broadcast and BLE HR strap relay have no functional dependency on WiFi.

For Echenon v1, this means a rider who takes their bike offline (no WiFi router, router power outage, or before configuring WiFi) loses BLE FTMS telemetry (`0x1826`) and/or BLE HR strap relay (`0x180D`) for no RF or protocol reason.

---

## 2. Verified Framework Code Paths (`v1.0.0-beta.2` / `02d0b2de`)

Every path below was inspected directly in the vendored `framework/` submodule (`02d0b2de`):

### 2.1 `BLEManager::tick()` — `State::Idle -> State::Starting` Gate

- [`framework/modules/BLEManager/BLEManager.hpp:114`](../framework/modules/BLEManager/BLEManager.hpp#L114):
  ```cpp
  bool advertiseInConfiguring = false;
  ```
- [`framework/modules/BLEManager/BLEManager.cpp:759-765`](../framework/modules/BLEManager/BLEManager.cpp#L759-L765):
  ```cpp
  if (_state == State::Idle && (_app != nullptr || _hasAppStateForTesting)) {
      AppState appState = getEffectiveAppState();
      bool startNow = (appState == AppState::Running) ||
                      (appState == AppState::Configuring && _config.advertiseInConfiguring);

      if (startNow) {
  ```
- With the default `advertiseInConfiguring == false`, `BLEManager` stays in `State::Idle` indefinitely while `Application` is in `AppState::Configuring`. NimBLE is never initialized, `IBLEServiceProvider` GATT services (FTMS `0x1826`, HR `0x180D`, DIS `0x180A`) are never registered, and advertising never starts.
- `advertiseInConfiguring` is exposed in JSON config (`applyConfig` at [`BLEManager.cpp:1272`](../framework/modules/BLEManager/BLEManager.cpp#L1272) and `serializeConfig` at [`BLEManager.cpp:1333`](../framework/modules/BLEManager/BLEManager.cpp#L1333)), with no dedicated C++ setter on `BLEManager`.

### 2.2 `BLEManager::startDiscovery()` — Hard `AppState::Running` Rejection

- [`framework/modules/BLEManager/BLEManager.cpp:395-400`](../framework/modules/BLEManager/BLEManager.cpp#L395-L400):
  ```cpp
  bool BLEManager::startDiscovery(const char* providerId, uint32_t durationMs) {
      // Coexistence hardening: Running-only rule (RFC §6.1, RFC §9)
      if (getEffectiveAppState() != AppState::Running) {
          ESP_LOGW(TAG, "startDiscovery rejected: only allowed in AppState::Running");
          return false;
      }
  ```
- User-initiated discovery scans for paired `IBLEClientProvider` peripherals (such as Echenon's `HRMClient` for BLE heart-rate straps) are unconditionally rejected whenever `getEffectiveAppState() != AppState::Running`, even if `advertiseInConfiguring` is `true` and `_state == State::Active`.

### 2.3 `BLEManager::tick()` — Central Reconnect Policy Initialization & Execution Gate

- [`framework/modules/BLEManager/BLEManager.cpp:853-864`](../framework/modules/BLEManager/BLEManager.cpp#L853-L864): During `Idle -> Starting`, reconnect policies for stored `IBLEClientProvider` devices are started **only** if `getEffectiveAppState() == AppState::Running`.
- [`framework/modules/BLEManager/BLEManager.cpp:868-886`](../framework/modules/BLEManager/BLEManager.cpp#L868-L886): Lazy policy initialization and config sync (`setFastSchedule`, `setSlowRetryMs`, `policy.start(nowMs)`) require `_state == State::Active && getEffectiveAppState() == AppState::Running`.
- [`framework/modules/BLEManager/BLEManager.cpp:888-918`](../framework/modules/BLEManager/BLEManager.cpp#L888-L918):
  ```cpp
  // Drive Reconnect Policies (Running-only rule: RFC §6.1, RFC §9)
  if (_state == State::Active &&
      getEffectiveAppState() == AppState::Running &&
      _scanInhibitCount == 0 &&
      !_discovery.active) {
  ```
- Consequently, even if `advertiseInConfiguring` is `true` so that peripheral GATT services start during `Configuring`, an already-paired `IBLEClientProvider` (HR strap) will neither start its reconnect policy nor issue `WorkerCommand::Type::Connect` until `AppState::Running` is reached—and will stop reconnecting if `Application` later regresses to `AppState::Configuring`.

### 2.4 `WifiPreconditionContributor` & `Application` Regression to `AppState::Configuring`

- [`framework/modules/WifiManager/WifiPreconditionContributor.hpp:48-59`](../framework/modules/WifiManager/WifiPreconditionContributor.hpp#L48-L59): Registers `PreconditionNames::WIFI_STATION` with **`PreconditionRole::Startup`**:
  - Returns `{true, Cause::Met}` **only** when `self->_wifi.isReady() && !self->_wifi.getAPMode()`.
  - Returns `{false, Cause::FallbackEngaged}` in AP mode, `{false, Cause::Connecting}` while STA credentials exist and STA is connecting, and `{false, Cause::NotConfigured}` when no WiFi credentials are saved.
- [`framework/modules/Application/Application.cpp:347-356`](../framework/modules/Application/Application.cpp#L347-L356) & [`413-419`](../framework/modules/Application/Application.cpp#L413-L419): `Application` enters `AppState::Running` at boot or from `Configuring` **only** when `_preconditionEngine.allStartupMet()` returns `true` (meaning every `Startup` and `Both` precondition has `dampedStatus.met == true`).
- [`framework/modules/Application/Application.cpp:471-477`](../framework/modules/Application/Application.cpp#L471-L477): During `Application::tick()`, whenever any `PreconditionRole::Startup` or `PreconditionRole::Both` precondition becomes `state.suppressed == true`, `Application` regresses from `Running` back to `Configuring`:
  ```cpp
  if (_state == AppState::Running && (state.role == PreconditionRole::Startup || state.role == PreconditionRole::Both) && state.suppressed) {
      ESP_LOGW(TAG, "Precondition '%s' suppressed — regressing to CONFIGURING", state.name);
      setState(AppState::Configuring);
      _publishConfiguringEvent(true);
      _onEnterConfiguring(true);
      break;
  }
  ```
- Per [`framework/modules/PreconditionEngine/PreconditionEngine.cpp:207-244`](../framework/modules/PreconditionEngine/PreconditionEngine.cpp#L207-L244) and `DampingPolicy` defaults in [`framework/include/interfaces/IPrecondition.hpp:109-114`](../framework/include/interfaces/IPrecondition.hpp#L109-L114) (`penaltyPerSecond = 10`, `suppressThreshold = 2000`, `reuseThreshold = 750`), sustained WiFi STA loss accumulates penalty (`weight = 1.0f` for `Connecting`/`FallbackEngaged`, `5.0f` for `NeedsUser`), suppresses `WiFi Station` (`~200 s` outage for `FallbackEngaged`/`Connecting`), and forces `AppState::Configuring`.

### 2.5 Summary Matrix

| Device Scenario | `Application` State | `BLEManager` (`advertiseInConfiguring = false`, default) | `BLEManager` (`advertiseInConfiguring = true`) |
|---|---|---|---|
| **Boot with no WiFi credentials** | Stuck in `Configuring` (`WiFi Station`: `NotConfigured`) | `State::Idle` — no NimBLE init, no FTMS/HR advertising, no HR strap reconnect | `State::Active` — FTMS/HR peripheral advertises; `startDiscovery()` & HR strap reconnect blocked |
| **Boot while home router is offline** | Stuck in `Configuring` (`Connecting` → `FallbackEngaged`) | `State::Idle` — completely offline on BLE as well as WiFi | `State::Active` — peripheral advertises; `startDiscovery()` & HR strap reconnect blocked |
| **Router outage mid-ride (after `everMet`)** | Regresses `Running -> Configuring` once `WiFi Station` is suppressed (`Application.cpp:471-473`) | Remains `State::Active` (already initialized), but HR strap `startDiscovery()` & reconnect loop freeze | Remains `State::Active`, but HR strap `startDiscovery()` & reconnect loop freeze |

---

## 3. Observed Consumer Evidence in Echenon

During Phase 2 bench testing ([`docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`](measurements/phase2/BLE-0x3e-debug-2026-10-13.md#test-a--wifi-sta-disconnected-designed--baseline-comparison-actual), `Test A` and [`§ v1 Finding: BLE Depends on WiFi STA`](measurements/phase2/BLE-0x3e-debug-2026-10-13.md#v1-finding-ble-depends-on-wifi-sta)):

- Removing WiFi credentials to test BLE in AP-only isolation prevented `AppState::Running`:
  ```text
  I (2994) WifiManager: softAP('esp32-device') returned true
  I (3281) Application: Precondition unsatisfied: WiFi Station (cause=8, severity=3)
    ... (repeats every 500ms, BLE never initialises)
  ```
- Echenon wires `BLEManager` in [`src/main.cpp:218-227`](../src/main.cpp#L218-L227) with `FTMSServer`, `HRMRepeater`, and `HRMClient`, without enabling `advertiseInConfiguring`. Even if Echenon enabled `advertiseInConfiguring` via config JSON, `HRMClient` (BLE HR strap discovery and reconnect) would still be blocked in `Configuring`, and a sustained WiFi outage during a ride would still regress `Application` at `Application.cpp:471-473` and disable HR strap reconnect recovery.

---

## 4. Consumer Mitigations vs. Upstream Fix Boundary

- **Not worked around in `framework/` here:** Per Echenon governance, `framework/` is an upstream-maintained submodule (`cblabs-org/esp32-app-foundation`) and is not patched locally in `PR-22`.
- **Note on partial config mitigation:** Enabling `advertiseInConfiguring = true` in `BLEManager` JSON config allows peripheral-only FTMS (`0x1826`) and HR (`0x180D`) advertising to start during `AppState::Configuring`, but **does not** fix `IBLEClientProvider` discovery (`BLEManager.cpp:397-399`) or reconnect scheduling (`BLEManager.cpp:854-892`), and conflicts with the framework's own coexistence rationale during initial AP captive-portal provisioning unless WiFi-STA provisioning vs. normal offline operation are distinguished.
- **Recommended path:** Address cleanly upstream in `cblabs-org/esp32-app-foundation` (tracked by the issue draft below).

---

## 5. Ready-to-File GitHub Issue Draft (`cblabs-org/esp32-app-foundation`)

> Copy the block below when filing against `cblabs-org/esp32-app-foundation`. Do not file automatically from this repository.

````markdown
Title: feat(ble,app): Allow BLE peripheral & central operation without an active WiFi STA connection

### Problem Statement

In `esp32-app-foundation` (`v1.0.0-beta.2`, `02d0b2de`), BLE operation is coupled to an active WiFi Station (`STA`) connection because:

1. `WifiPreconditionContributor` registers `PreconditionNames::WIFI_STATION` with `PreconditionRole::Startup`.
2. `Application` remains in `AppState::Configuring` until all `Startup` preconditions are met—and regresses from `AppState::Running` back to `AppState::Configuring` if `WiFi Station` becomes `suppressed` during a sustained STA outage.
3. `BLEManager` defaults `advertiseInConfiguring` to `false` (blocking `Idle -> Starting` in `Configuring`), and unconditionally rejects `startDiscovery()` and all `IBLEClientProvider` reconnect policy initialization/execution unless `getEffectiveAppState() == AppState::Running`.

As a result, devices whose core functionality includes local BLE (GATT peripheral broadcast and/or BLE central sensor relay) cannot operate offline, in WiFi-free environments, or across a sustained WiFi router outage—even though BLE has no functional dependency on WiFi.

---

### Verified Code Paths (`v1.0.0-beta.2` / `02d0b2de`)

- **`framework/modules/BLEManager/BLEManager.hpp:114`**
  - `bool advertiseInConfiguring = false;` — defaults to `false` (with no C++ setter; only configurable via JSON `applyConfig` at `BLEManager.cpp:1272`).
- **`framework/modules/BLEManager/BLEManager.cpp:759-762`**
  - `Idle -> Starting` in `BLEManager::tick()` checks:
    `bool startNow = (appState == AppState::Running) || (appState == AppState::Configuring && _config.advertiseInConfiguring);`
  - When `advertiseInConfiguring == false` and WiFi STA is absent at boot, NimBLE never initializes and GATT services never start.
- **`framework/modules/BLEManager/BLEManager.cpp:397-399`**
  - `BLEManager::startDiscovery()` rejects all user-initiated central discovery scans when `getEffectiveAppState() != AppState::Running`.
- **`framework/modules/BLEManager/BLEManager.cpp:854-892`** (and through `:918`)
  - Initial `BLEReconnectPolicy::start()` (`:854-864`), lazy policy initialization/config sync (`:869-886`), and the reconnect scheduler (`:889-918`) are all gated on `getEffectiveAppState() == AppState::Running`.
- **`framework/modules/Application/Application.cpp:471-473`** (plus `:347-356`, `:413-419`)
  - Boot/Configuring require `_preconditionEngine.allStartupMet()` to enter `AppState::Running`.
  - At `:471-473`, `if (_state == AppState::Running && (state.role == PreconditionRole::Startup || state.role == PreconditionRole::Both) && state.suppressed)` regresses `Application` from `Running -> Configuring`.
- **`framework/modules/WifiManager/WifiPreconditionContributor.hpp:48-59`**
  - Registers `PreconditionNames::WIFI_STATION` with `PreconditionRole::Startup`, satisfied only when `_wifi.isReady() && !_wifi.getAPMode()`.

---

### Consumer Impact (Echenon v1 — Smart Bike FTMS + HR Relay)

In Echenon (`cblabs-org/echenon`), the ESP32-S3 bike computer acts as:
- A **BLE Peripheral** (`FTMSServer` `0x1826` + `HRMRepeater` `0x180D`) broadcasting live watts, cadence, and heart rate to Zwift, TrainerRoad, Xert, and the Echelon tablet app.
- A **BLE Central** (`HRMClient` `IBLEClientProvider`) connecting to the rider's heart-rate strap (`0x180D`) and auto-reconnecting on drop.

Impact of the current gating:
- **Offline / unconfigured WiFi boot:** Neither FTMS broadcast nor HR strap relay initializes (verified in bench testing: `Precondition unsatisfied: WiFi Station (cause=8, severity=3)`, BLE stays in `State::Idle`).
- **Home router outage mid-ride:** Once `WiFi Station` accumulates damping penalty (`PreconditionEngine.cpp:207-244`, `DampingPolicy::suppressThreshold = 2000`) and sets `suppressed = true`, `Application` regresses to `AppState::Configuring` (`Application.cpp:471-473`). While existing peripheral connections survive, HR strap discovery and reconnect recovery stop until WiFi returns.
- **No clean workaround with `advertiseInConfiguring`:** Even setting `advertiseInConfiguring = true` in JSON config only unblocks peripheral advertising; `HRMClient` discovery (`BLEManager.cpp:397`) and reconnect (`BLEManager.cpp:854-892`) remain locked to `AppState::Running`.

---

### Proposed Framework Changes

1. **Decouple "WiFi STA is required to leave `Configuring`" from apps that can operate locally without cloud connectivity:**
   - Either allow `WifiPreconditionContributor` (or `FrameworkConfig`) to configure `WIFI_STATION` as `PreconditionRole::Operational` for local-first / BLE-capable consumers (so `Application` can reach `AppState::Running` while `WifiManager` continues background STA retries / fallback AP without regressing `AppState`), **or** separate `BLEManager`'s radio-coexistence gating from WiFi-provisioning state.
2. **Allow explicit BLE central & peripheral operation without WiFi STA when configured:**
   - Expose a clean C++ configuration API on `BLEManager` (e.g., `setAdvertiseInConfiguring(bool)` / `setAllowCentralInConfiguring(bool)` or an offline-capable BLE policy) rather than requiring raw JSON patching before `tick()`.
   - Guard only active WiFi-connection-handshake contention (e.g., transient `WifiManager` STA association or active captive-portal AP onboarding) using `ScanInhibit` tokens rather than permanently gating `startDiscovery()` and `BLEReconnectPolicy` on `AppState::Running`.
3. **Avoid regressing active local services on router outage:**
   - When `WiFi Station` becomes `suppressed` after having previously connected (`everMet == true`), cloud-dependent features (`DataManager` / uploaders / NTP) should degrade via `Operational` preconditions rather than demoting `Application` out of `AppState::Running` and freezing unrelated BLE Central providers.

---

### Acceptance Criteria

- [ ] A consumer app booting with **no saved WiFi credentials** (or with the configured SSID unreachable) can initialize `BLEManager`, advertise `IBLEServiceProvider` GATT services, run `startDiscovery()`, and connect/auto-reconnect `IBLEClientProvider` devices without waiting for WiFi STA.
- [ ] A sustained WiFi STA outage while already running does **not** stop `IBLEClientProvider` reconnect policies or reject `startDiscovery()`.
- [ ] Captive-portal / WiFi-association coexistence remains protected (e.g., transient `ScanInhibit` during active WiFi STA association / scan windows instead of permanent `AppState::Configuring` lockout).
- [ ] Existing WiFi-required consumer apps retain backward-compatible default behavior if configured for `PreconditionRole::Startup`.
- [ ] Native unit tests cover `BLEManager` peripheral + central behavior in offline / `Configuring` / STA-outage transitions.
````
