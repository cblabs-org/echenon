# web

## Invariants & contracts

### [docseed-0d3c1819] [`web/intervals_settings.js:17-18,220-234`](../web/intervals_settings.js#L17-L239) adds defensive dual tab ID support (`tab-intervals-icu` / `tab-intervals.icu`) aligned in `WO-09.12` with upstream `cblabs-org/esp32-app-foundation` `v1.0.0-beta.2` (`#329`) non-alphanumeric tab slugification.
`invariant · kind: semantic · anchors: web/intervals_settings.js · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-2d3e570f] Every color reference in `web/calibration-wizard.js`, `web/diagnostics.js`, `web/echenon-dashboard.js`, `web/intervals_settings.js`, `web/recalibration.js`, `web/ride.js`, `web/session.js`, `web/stored_rides.js`, `web/upload_settings.js`, and `web/wahoo_settings.js` uses `var(--token, #fallback)`.
`invariant · kind: semantic · anchors: web/calibration-wizard.js, web/diagnostics.js, web/echenon-dashboard.js, web/intervals_settings.js, web/recalibration.js, web/ride.js, web/session.js, web/stored_rides.js, web/upload_settings.js, web/wahoo_settings.js · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-625f3968] Portal scripts use `window.apiFetch` for authenticated API requests, including `POST /api/echenon/calibration/reset` in [`web/calibration-wizard.js`](../web/calibration-wizard.js).
`invariant · kind: semantic · anchors: web/calibration-wizard.js · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-720cdb8e] The only non-`var(--...)` hex strings in `web/` are the `:root` CSS custom property declarations and `<meta name="theme-color">` in standalone full-screen [`web/ride.html:1-2`](../web/ride.html#L1-L2) and PWA [`web/manifest.json:1`](../web/manifest.json#L1).
`invariant · kind: semantic · anchors: web/ride.html:1-2, web/manifest.json:1, web, web/ride.html, web/manifest.json · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

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
