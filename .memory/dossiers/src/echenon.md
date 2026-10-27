# src/echenon

## Invariants & contracts

### [docseed-02c56dbf] **Zero-Cadence Guard:** If $\text{cad} \le 0.0\text{f}$ (or non-finite), return $0.0\text{f}$ ($0\text{ W}$) immediately.
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-0b56dc2a] **Hardware Bug 3 — `CadenceSensor` 71.58-Minute `uint32_t micros()` Wraparound vs `millis()` (`src/echenon/CadenceSensor.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/CadenceSensor.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-49cb262b] **Cam Follower Displacement ($u$):** $$u = 1.0\text{f} - \cosf(2.635447\text{f} \cdot \tilde{p})$$
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-6d117a5f] **Hardware Bug 1 — Physical Reed Switch Rising-Edge RC Threshold Bounce (`src/echenon/CadenceSensor.h`, `src/echenon/CadenceSensor.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/CadenceSensor.h, src/echenon/CadenceSensor.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-80c8c5b6] **Output Clamping:** Clamp $P$ to $[0.0\text{f}, 2000.0\text{f}]$ (`uint16_t` rounded watts for FTMS / BLE / UI telemetry, plus `float` watts for internal filtering).
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-81978d11] **Normalized Cam Position ($\tilde{p} \in [0.0, 1.0]$):** Computed directly from the unrounded EMA float wiper $w$ (`ResistanceSensor::getFilteredWiperFloat()`) and the OLS-calibrated linear endpoints $[\text{pot}_{\min}, \text{pot}_{\max}]$ (`CalibrationWizard::fitLinearEndpoints`): $$\tilde{p} = \text{clamp}\!\left(\frac{w - \text{pot}_{\min}}{\text{pot}_{\max} - \text{pot}_{\min}},\; 0.0\tex...
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-9bd6e106] **Kloss Eddy-Current + Friction Power ($P$):** $$r = \frac{\text{cad}}{\text{cad}_c}, \qquad P_{\text{eddy}} = A \cdot \expf(\beta \cdot u) \cdot \frac{\text{cad}^2}{1.0\text{f} + r^2}$$ $$P = P_{\text{eddy}} + c_{\text{fric}} \cdot \text{cad}$$
`invariant · kind: semantic · anchors: src/echenon/PowerEngine.h · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-cd25b342] **Hardware Bug 2 — Optical HR Armband Transient `0 BPM` Drops & Cross-Core `millis()` Underflow (`src/echenon/HRMClient.cpp`, `src/echenon/FTMSServer.cpp`):**
`invariant · kind: semantic · anchors: src/echenon/HRMClient.cpp, src/echenon/FTMSServer.cpp · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

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
