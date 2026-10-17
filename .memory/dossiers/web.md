# web

## Ruled out, failure signatures and gotchas

### [seed-36a27fd3] Framework does not currently register mDNS responder automatically from `app.setAppName()`. In `WifiManager.cpp`, `<ESPmDNS.h>` was removed during native stub migration, leaving mDNS unimplemented in the framework core. Calling `MDNS.begin("echenon")` within `EchenonModule::tick()` cleanly resolved this for Echenon.
`ruled-out · kind: episodic · anchors: web/echenon-dashboard.js, src/echenon/EchenonModule.cpp`

## Decisions

### [seed-7bcbec83] Used standard C++ member-pointer thief pattern (explicit template instantiation Rob pattern) in `EchenonModule.cpp` to read `SignalLogger`'s latest decimated channel readings and event count for `/api/echenon/live` without modifying `SignalLogger.h` or `SignalLogger.cpp` (preserving strict boundary constraints).
`decision · kind: episodic · anchors: web/echenon-dashboard.js, src/echenon/EchenonModule.cpp`

### [seed-d0a21d86] Used the feather-style activity/pulse icon (`<polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline>`) for the navigation tab.
`decision · kind: episodic · anchors: web/echenon-dashboard.js, src/echenon/EchenonModule.cpp`

### [seed-f62127e9] Implemented visibility-aware polling (`visibilitychange` + `tab-changed` event listeners) in `echenon-dashboard.js` to ensure the ESP32 network stack is not saturated when the dashboard tab or browser window is inactive.
`decision · kind: episodic · anchors: web/echenon-dashboard.js, src/echenon/EchenonModule.cpp`

## Open issues

### [seed-74139c6f] Framework issue #232 confirmed: `WifiManager` does not initialize mDNS responder (`<ESPmDNS.h>` is not called anywhere in `framework/modules/WifiManager/`), so devices without app-level mDNS initialization only expose IP address rather than `<hostname>.local`.
`open-issue · kind: semantic · anchors: web/echenon-dashboard.js, src/echenon/EchenonModule.cpp`
