# src

## Invariants & contracts

### [docseed-4831212e] `member_naming` hits reported by the script (`115` hits) reside entirely inside the upstream `framework/` submodule (`0` hits in `src/`).
`invariant · kind: semantic · anchors: src · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-4f3e333e] **Uploader Initialization Ordering (`src/main.cpp` & `IntervalsIcuUploader.cpp`):** [`src/main.cpp:238-242`](../src/main.cpp#L238-L242) defers `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` until after `app.begin(config)` right alongside `stravaUploader.begin()` (initializing credentials after `DeviceCrypto` / `KeyManager` are ready, eliminating boot-tim...
`invariant · kind: semantic · anchors: src/main.cpp, src/main.cpp:238-242 · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-6efa0d84] Ring-2 `mcpServer.begin(...)` is invoked after `timeManager.begin()` ([`src/main.cpp:202-216`](../src/main.cpp#L202-L216)).
`invariant · kind: semantic · anchors: src/main.cpp:202-216, src/main.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-8dadf460] Application code includes `"AppFoundation.h"` ([`src/main.cpp:8`](../src/main.cpp#L8), [`src/echenon/EchenonModule.h:10`](../src/echenon/EchenonModule.h#L10)).
`invariant · kind: semantic · anchors: src/main.cpp:8, src/echenon/EchenonModule.h:10, src/main.cpp, src/echenon/EchenonModule.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-b719e62c] Every `app.registerModule(...)` and `bleManager.registerServiceProvider(...)` / `registerClientProvider(...)` call in [`src/main.cpp:107-190`](../src/main.cpp#L107-L190) — including `mcpServer.setAppName("Echenon"); app.registerModule(&mcpServer);` ([`src/main.cpp:108-109`](../src/main.cpp#L108-L109)) and `timeManager.setEventBus(&app.getEventBus());` ([`src/main.cpp:187`](../src/main.cpp#L18...
`invariant · kind: semantic · anchors: src/main.cpp:107-190, src/main.cpp:108-109, src/main.cpp:187, src/main.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-fad9516f] All top-level module instances in [`src/main.cpp:15-58`](../src/main.cpp#L15-L58) (`bleManager`, `encoderModule`, `timeManager`, `timePrecondition`, `app`, `echenonModule`, `mcpServer` at [`src/main.cpp:46`](../src/main.cpp#L46), `rideStore`, `sessionManager`, `uploadQueue`, `sessionOverlay`, `ridePersistence`, `stravaUploader`) are declared `static` at file scope.
`invariant · kind: semantic · anchors: src/main.cpp:15-58, src/main.cpp:46, src/main.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

## Ruled out, failure signatures and gotchas

### [docseed-13d2b9cc] Per `WO-09.14`, `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` execute post-`app.begin(config)` alongside `stravaUploader.begin()` ([`src/main.cpp:238-242`](../src/main.cpp#L238-L242)), guaranteeing `DeviceCrypto` / `KeyManager` initialization before secret reads (`0` `KeyManager: DeviceCrypto not initialized.` errors observed in `v0.3.0-dev.51` live boo...
`failure-signature · kind: procedural · anchors: src/main.cpp:238-242, src/main.cpp · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`
