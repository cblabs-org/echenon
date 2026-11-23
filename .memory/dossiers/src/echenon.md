# src/echenon

## Invariants & contracts

### [docseed-02c56dbf] **Zero-Cadence Guard:** If $\text{cad} \le 0.0\text{f}$ (or non-finite), return $0.0\text{f}$ ($0\text{ W}$) immediately.
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-04c07b19] **`SignalLogic` `%llu` Replacement (`src/echenon/SignalLogic.h`):** Replaced `%llu` in [`formatBurstCsvHeader()`](../src/echenon/SignalLogic.h#L492-L510) (`L492-500`) with a zero-heap stack base-10 conversion (`char startUsBuf[21]`) formatted via `%s`, preventing 32-bit `newlib-nano` `va_list` misalignment / `LoadProhibited` crash hazards when `burstStartUs >= 2^32` (`> 71.5 min` uptime).
`invariant · kind: semantic · anchors: src/echenon/SignalLogic.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-0b56dc2a] **Hardware Bug 3 — `CadenceSensor` 71.58-Minute `uint32_t micros()` Wraparound vs `millis()` (`src/echenon/CadenceSensor.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/CadenceSensor.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-0fe8b0f4] [`src/echenon/StravaUploader.h:164-173`](../src/echenon/StravaUploader.h#L164-L173) (`SecretRegistrarFn`, `MetadataSaveFn`, `RandomU32Fn`)
`invariant · kind: semantic · anchors: src/echenon/StravaUploader.h:164-173, src/echenon/StravaUploader.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-113cbae1] [`src/echenon/TCXGenerator.h:23-31`](../src/echenon/TCXGenerator.h#L23-L31) (`WriteChunkFn`, `DeadlineCheckFn` — stack-scoped only)
`invariant · kind: semantic · anchors: src/echenon/TCXGenerator.h:23-31, src/echenon/TCXGenerator.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-136e8201] [`src/echenon/UploadQueue.h:105-112`](../src/echenon/UploadQueue.h#L105-L112) (`NowEpochFn`, `NtpSyncedFn`, `TcxGenerateFn`)
`invariant · kind: semantic · anchors: src/echenon/UploadQueue.h:105-112, src/echenon/UploadQueue.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-27ac34be] `EchenonModule::registerMCPTools(MCPToolRegistry&)` ([`src/echenon/EchenonModule.cpp:782-1100`](../src/echenon/EchenonModule.cpp#L782-L1100)) registers 5 core tools in production builds (`echenon_get_telemetry`, `echenon_calibration`, `echenon_recalibration`, `echenon_upload_power_table`, `echenon_set_ftms_smoothing`) plus 2 rig tools under `WITH_CALIBRATION_RIG` (`echenon_rig_status`, `echenon...
`invariant · kind: semantic · anchors: src/echenon/EchenonModule.cpp:782-1100, src/echenon/EchenonModule.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-290bdf86] Invoked on initial `tick()` startup ([`src/echenon/EchenonModule.cpp:89`](../src/echenon/EchenonModule.cpp#L89)), `applyConfig()` ([`src/echenon/EchenonModule.cpp:321`](../src/echenon/EchenonModule.cpp#L321)), `resetConfig()` ([`src/echenon/EchenonModule.cpp:383`](../src/echenon/EchenonModule.cpp#L383)), REST calibration record/reset and recalibration point/reset routes ([`src/echenon/EchenonMo...
`invariant · kind: semantic · anchors: src/echenon/EchenonModule.cpp:89, src/echenon/EchenonModule.cpp:321, src/echenon/EchenonModule.cpp:383, src/echenon/EchenonModule.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-39ecb857] [`src/echenon/SignalLogger.cpp`](../src/echenon/SignalLogger.cpp#L363-L505) — 5 routes (`WITH_SIGNAL_LOGGER` diagnostic builds): `/api/echenon/status` (`GET`), `/api/echenon/burst/start` (`POST`), `/api/echenon/ride-log.csv` (`GET`), `/api/echenon/events.csv` (`GET`), and `/api/echenon/burst.csv` (`GET`).
`invariant · kind: semantic · anchors: src/echenon/SignalLogger.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-49cb262b] **Cam Follower Displacement ($u$):** $$u = 1.0\text{f} - \cosf(2.635447\text{f} \cdot \tilde{p})$$
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-5b8d0d25] [`src/echenon/IntervalsIcuUploader.h:43-44`](../src/echenon/IntervalsIcuUploader.h#L43-L44) (`SecretRegistrarFn`)
`invariant · kind: semantic · anchors: src/echenon/IntervalsIcuUploader.h:43-44, src/echenon/IntervalsIcuUploader.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-6d117a5f] **Hardware Bug 1 — Physical Reed Switch Rising-Edge RC Threshold Bounce (`src/echenon/CadenceSensor.h`, `src/echenon/CadenceSensor.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/CadenceSensor.h, src/echenon/CadenceSensor.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-70b92eb4] [`src/echenon/EchenonModule.cpp`](../src/echenon/EchenonModule.cpp#L463-L748) — 13 routes: `/api/echenon/calibration/{start,stop,record-level,reset,status}`, `/api/echenon/recalibration/{point,points,reset}`, `/api/echenon/power-table/{upload,active}`, `/api/echenon/live`, `/api/echenon/ftms-smoothing` (`GET` & `POST`).
`invariant · kind: semantic · anchors: src/echenon/EchenonModule.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-78eaf8ad] Every sensitive application HTTP endpoint enforces `AuthManager::checkPortalAuth(request, response)` (`23` call sites verified across `src/echenon/`):
`invariant · kind: semantic · anchors: src/echenon · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-7f5c82b8] [`src/echenon/DiagnosticsRoutes.cpp`](../src/echenon/DiagnosticsRoutes.cpp#L132-L373) — 4 routes: `/api/echenon/diagnostics/events` (`GET` & `DELETE`), `/api/echenon/diagnostics/rides` (`GET`), `/api/echenon/diagnostics/rides/*` (`GET` & `DELETE`).
`invariant · kind: semantic · anchors: src/echenon/DiagnosticsRoutes.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-80c8c5b6] **Output Clamping:** Clamp $P$ to $[0.0\text{f}, 2000.0\text{f}]$ (`uint16_t` rounded watts for FTMS / BLE / UI telemetry, plus `float` watts for internal filtering).
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-81978d11] **Normalized Cam Position ($\tilde{p} \in [0.0, 1.0]$):** Computed directly from the unrounded EMA float wiper $w$ (`ResistanceSensor::getFilteredWiperFloat()`) and the OLS-calibrated linear endpoints $[\text{pot}_{\min}, \text{pot}_{\max}]$ (`CalibrationWizard::fitLinearEndpoints`): $$\tilde{p} = \text{clamp}\!\left(\frac{w - \text{pot}_{\min}}{\text{pot}_{\max} - \text{pot}_{\min}},\; 0.0\tex...
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-85b9f86c] **File Headers (`🟡 MECHANICAL`):** All 63 C++ source and header files under `src/` (`src/main.cpp` + 62 files in `src/echenon/`) include the canonical `Echenon (c) 2026 CB Labs` / `SPDX-License-Identifier: Apache-2.0` header block (`find src -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) | wc -l` = `63`; `grep -rnL "SPDX-License-Identifier: Apache-2.0" src/` returns only `src/CM...
`invariant · kind: semantic · anchors: src/main.cpp, src/echenon, src · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-9237c687] [`EchenonModule::syncPhysicsCalibration()`](../src/echenon/EchenonModule.cpp#L287-L293) propagates fitted OLS potentiometer endpoints (`_calibrationWizard.getPotMin()`, `_calibrationWizard.getPotMax()`) and recalibration eddy-current scaling (`_recalibrationUI.applyToPhysicsParams(params)`) into `_powerEngine.setPhysicsParams(params)`.
`invariant · kind: semantic · anchors: src/echenon/EchenonModule.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-9bd6e106] **Kloss Eddy-Current + Friction Power ($P$):** $$r = \frac{\text{cad}}{\text{cad}_c}, \qquad P_{\text{eddy}} = A \cdot \expf(\beta \cdot u) \cdot \frac{\text{cad}^2}{1.0\text{f} + r^2}$$ $$P = P_{\text{eddy}} + c_{\text{fric}} \cdot \text{cad}$$
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-a6731613] The only `extern` symbol in `src/` is `extern uint32_t millis();` in [`src/echenon/FTMSServer.cpp:37`](../src/echenon/FTMSServer.cpp#L35-L38), which is strictly compiled only under `#if !defined(ESP_PLATFORM)` and `#ifdef UNIT_TEST` for host-side unit testing.
`invariant · kind: semantic · anchors: src/echenon/FTMSServer.cpp:37, src, src/echenon/FTMSServer.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-c6a7ad8f] [`src/echenon/SessionManager.h:120-121`](../src/echenon/SessionManager.h#L120-L121) (`SessionCompletedCallback`)
`invariant · kind: semantic · anchors: src/echenon/SessionManager.h:120-121, src/echenon/SessionManager.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-cc346256] **`RidePersistence` `%zu` Replacement (`src/echenon/RidePersistence.cpp`):** Replaced unsupported `newlib-nano` `%zu` format specifiers at [`L195`](../src/echenon/RidePersistence.cpp#L195-L196), [`L325`](../src/echenon/RidePersistence.cpp#L325-L327), and [`L355`](../src/echenon/RidePersistence.cpp#L356-L357) with `%u` + `static_cast<unsigned>(...)`.
`invariant · kind: semantic · anchors: src/echenon/RidePersistence.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-cd25b342] **Hardware Bug 2 — Optical HR Armband Transient `0 BPM` Drops & Cross-Core `millis()` Underflow (`src/echenon/HRMClient.cpp`, `src/echenon/FTMSServer.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/HRMClient.cpp, src/echenon/FTMSServer.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-d892bab3] [`src/echenon/WahooUploader.h:50-59`](../src/echenon/WahooUploader.h#L50-L59) (`SecretRegistrarFn`, `MetadataSaveFn`, `RandomU32Fn`)
`invariant · kind: semantic · anchors: src/echenon/WahooUploader.h:50-59, src/echenon/WahooUploader.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-e3125dc2] All 13 `std::function` type aliases/callbacks in `src/echenon/` carry explicit `// GRAVE-05:` justification comments confirming they are either bound once at boot (stateless/SBO-small lambdas `<= 16 B` invoked only on cold background paths such as `DataManager::fetchUpdate()`, OAuth token refresh, or session end) or passed as transient stack-scoped parameters in `TCXGenerator`:
`invariant · kind: semantic · anchors: src/echenon · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-e76767a3] Top-level tabs (`Echenon`, `Apps`) use standard 24×24 Feather SVG `<polyline>` / `<rect>` primitives ([`src/echenon/EchenonModule.cpp:1105-1122`](../src/echenon/EchenonModule.cpp#L1105-L1122)).
`invariant · kind: semantic · anchors: src/echenon/EchenonModule.cpp:1105-1122, src/echenon/EchenonModule.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-fdfbeca5] [`src/echenon/SessionOverlay.cpp`](../src/echenon/SessionOverlay.cpp#L1271) — `checkAuth_()` helper guarding all `/api/session/*`, `/api/strava/*`, `/api/intervals/*`, `/api/wahoo/*`, and `/api/upload/*` endpoints (with OAuth redirect callbacks `/api/strava/callback` and `/api/wahoo/callback` additionally protected by single-use 128-bit cryptographic state nonces and 600 s TTLs).
`invariant · kind: semantic · anchors: src/echenon/SessionOverlay.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

## Ruled out, failure signatures and gotchas

### [seed-ae051f78] Initial native test build attempted to pull `Application` and entire `framework/modules` into `env:native`, causing compilation failures due to missing hardware display/network headers. Resolved by scoping `env:native` dependencies to `bblanchon/ArduinoJson` for pure logic testing.
`ruled-out · kind: episodic · anchors: src/echenon/SignalLogic.h, src/echenon/SignalLogger.h, src/echenon/SignalLogger.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, platformio.ini, test/test_native/test_signal_logic.cpp`

### [seed-bb80cf47] Initial compilation showed deprecation warning on `volatile uint32_t++` under C++20; resolved by using explicit `_overflowCount = _overflowCount + 1`.
`ruled-out · kind: episodic · anchors: src/echenon/SignalLogic.h, src/echenon/SignalLogger.h, src/echenon/SignalLogger.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, platformio.ini, test/test_native/test_signal_logic.cpp`

## Decisions

### [seed-8e807271] Fallback wired in `EchenonModule::tick()` to connect `_signalLogger.setEventBus(&app.getEventBus())` if not injected beforehand, avoiding any changes to `src/main.cpp` and staying strictly inside the boundary box.
`decision · kind: episodic · anchors: src/echenon/SignalLogic.h, src/echenon/SignalLogger.h, src/echenon/SignalLogger.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, platformio.ini, test/test_native/test_signal_logic.cpp`

### [seed-a895ee5a] Pack `channel:4 | data:12` into existing `uint16_t` burst buffer rather than expanding buffer memory, preserving 1.5M sample capacity within ~3 MB PSRAM.
`decision · kind: episodic · anchors: src/echenon/SignalLogic.h, src/echenon/SignalLogger.h, src/echenon/SignalLogger.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, platformio.ini, test/test_native/test_signal_logic.cpp`

### [seed-d176e641] In `SignalLogic.h`, de-interleaving drops partial leading frames prior to the first channel 0 to ensure strict frame alignment from the very first CSV row.
`decision · kind: episodic · anchors: src/echenon/SignalLogic.h, src/echenon/SignalLogger.h, src/echenon/SignalLogger.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, platformio.ini, test/test_native/test_signal_logic.cpp`
