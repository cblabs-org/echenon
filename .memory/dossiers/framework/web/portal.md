# framework/web/portal

## Invariants & contracts

### [docseed-2cfbb715] **Portal auto-starts WiFi scan on tab open:** `framework/web/portal/wifi.js` L550–561: The `tab-changed` event listener calls `startScan()` whenever the WiFi tab becomes active. If the portal is open with the WiFi tab selected, scans run automatically.
`invariant · kind: semantic · anchors: framework/web/portal/wifi.js · source_doc: docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md · evidence: doc:docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`

## Decisions

### [dec-01M639DS24ZFPGNNX6NCM7C65Z] Portal defaults unconditionally to hardcoded tab-wifi instead of app tab when WiFi is configured (STA mode), which triggers unnecessary WiFi scans on load. Filed upstream issue #314 on matthewmcneill/esp32-app-foundation to support intelligent tab defaulting and hash navigation.
`decision · kind: episodic · anchors: framework/web/portal/app.js, web/echenon-dashboard.js`
