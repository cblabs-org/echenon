# framework/web/portal

## Invariants & contracts

### [docseed-2cfbb715] **Portal auto-starts WiFi scan on tab open:** `framework/web/portal/wifi.js` L550–561: The `tab-changed` event listener calls `startScan()` whenever the WiFi tab becomes active. If the portal is open with the WiFi tab selected, scans run automatically.
`invariant · kind: semantic · anchors: framework/web/portal/wifi.js · source_doc: docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md · evidence: doc:docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`
