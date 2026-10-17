# src/echenon

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
