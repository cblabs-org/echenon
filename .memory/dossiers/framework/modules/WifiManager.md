# framework/modules/WifiManager

## Ruled out, failure signatures and gotchas

### [docseed-5f497ec1] **MCP scan uses non-default active scan time:** `framework/modules/WifiManager/WifiManager.cpp` L632: When `WITH_BLE` is defined, `WiFi.scanNetworks(false, false, false, 120)` passes 120 ms/channel active scan time. The same 120 ms parameter is used in the async scan path at L741. ESP-IDF's warning specifically calls out non-default active scan time as problematic when Bluetooth is enabled.
`gotcha · kind: semantic · anchors: framework/modules/WifiManager/WifiManager.cpp · source_doc: docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md · evidence: doc:docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`
