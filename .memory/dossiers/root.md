# root

## Invariants & contracts

### [docseed-00d50b73] Open the **Upload Services** tab (`#upload_settings`), enter Strava API `Client ID` and `Client Secret`, and click **Save Credentials** (`POST /api/strava/credentials`).
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-04018788] Stop pedalling for `15 s` while `ACTIVE` (coasting): verify `session_state` remains `"ACTIVE"` and `0 W / 0 RPM` records are logged at 1 Hz.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-060b7488] **Streaming TCX & Gzip Scratch (`TCXGenerator`):** Strictly bounded to a single `4,096 B` (`4 KB`) formatting buffer (`kScratchBufferSize = 4096`) plus streaming `miniz` (`tdefl_compressor`) deflation directly to LittleFS or HTTP response stream — zero full-ride XML strings in RAM.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-107d34a9] Per `SPEC.md` §5.3, the standard published Peloton Bike power curve is: $$P_{\text{pel}}(R_{\text{pel}}, \text{cad}) = \left(0.000185 \cdot R_{\text{pel}}^{2.7} + 0.1\right) \times \left(\frac{\text{cad}}{60}\right)^2 \times 60$$
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-1e01684a] **TLS + BLE Coexistence (`UploadQueue`):** Each outbound HTTPS call (`StravaUploader::refreshTokenIfNeeded`, `postUpload`, `pollUploadStatus`) is executed inside a single split stage on the background `DataCollector` FreeRTOS task (`Core 0`) while holding a stack-allocated `BLEManager::ScanInhibit` RAII guard (`#ifdef WITH_BLE`) so internal SRAM retains $> 40\text{ KB}$ headroom during `mbedTLS...
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-22200df1] **Manual Discard (`4.2`):** Start a short session, click **Discard**, and verify `session_state` returns to `"IDLE"` and `/ride_active.bin` is removed without adding an entry to `GET /api/session/history`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-2dda879b] **Two flag families.** `HAS_*` macros in `board.h` state that a bike signal exists and is wired. `WITH_*` build flags in `platformio.ini` select which framework and application modules compile (§12.3).
`invariant · kind: semantic · anchors: platformio.ini · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-34d795bf] Click **Pause** in the Session tab (`POST /api/session/pause`): verify `session_state` becomes `"PAUSED"`. Wait `15 s`, then click **Resume** (`POST /api/session/resume`) and pedal for another `30 s`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-40500681] Confirm 1 Hz `<Trackpoint>` elements contain `<Time>`, `<DistanceMeters>`, `<Cadence>`, `<HeartRateBpm>` (when BLE HR connected), and `<Extensions><ns3:TPX><ns3:Speed>...</ns3:Speed><ns3:Watts>...</ns3:Watts></ns3:TPX></Extensions>`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-56371065] **Active Ride Ring Buffer (`RideStore::allocateBuffer()`):** Allocates `14,400 * sizeof(RideRecord) = 115,200 B` (`112.5 KB`, 4 hours at 1 Hz) in PSRAM via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` on ESP32-S3, falling back to `3,600 * 8 = 28,800 B` (`28.1 KB`, 1 hour) in internal DRAM only if PSRAM is unavailable.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-568c71d3] **Auto-Start (`4.1`):** With `auto_start_enabled: true` (`GET /api/session/status`), pedal continuously for `> 5 s`. Verify `/ws/echenon/live` transitions `session_state: "IDLE" -> "ACTIVE"` and `session_elapsed_s` increments at 1 Hz.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-73a5436e] Replay the same `/api/strava/callback` URL in the browser and confirm it returns `HTTP 403 Forbidden` (single-use `state` consumed).
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-8342d150] Observe `UploadQueue` progress through `STAGE_GENERATE_TCX_GZ -> STAGE_REFRESH_TOKEN -> STAGE_POST_UPLOAD -> STAGE_POLL_STATUS` (one stage per `fetchUpdate()` tick on `Core 0`).
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-850adaa6] **Coasting vs. Manual Pause (`4.2, 4.9`):**
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-86cc9195] Pedal for `> 65 s` (`>= 60` 1 Hz records, crossing at least two `30 s` LittleFS checkpoints to `/ride_active.bin`), then press hardware reset (or power-cycle USB-C) mid-ride.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-9550efd3] On reboot, query `GET /api/session/history` and confirm `RideStore::recoverInterruptedRide()` finalized the interrupted ride into `/rides/ride_<id>.bin`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-a6e7a0a3] Authenticate via the portal and download `/api/session/download?id=<id>&format=tcx` and `format=tcx.gz`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-b436ea2d] Confirm boot log reports PSRAM ride buffer allocation (`14,400` records = `115,200 B`), LittleFS mount (`/rides` directory ready), and registration of `Session` (order `80`) and `Upload Services` (order `90`) portal tabs.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-bd8306ed] Confirm `<TrainingCenterDatabase>` root schema and `<Creator><Name>Echenon EX-3</Name></Creator>`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-bda7ff90] **Subsequent Ride After `ENDED` (`4.1`):** Resume pedalling for `> 5 s` without rebooting; verify `SessionManager` transitions cleanly from `ENDED -> ACTIVE` with a fresh session ID.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-d4599601] Confirm serial logs redact `client_secret`, `access_token`, and `refresh_token` (`***REDACTED***`), `internal_free_bytes` remains $> 40\text{ KB}$ during TLS handshakes (`BLEManager::ScanInhibit` active), `/rides/ride_<id>.tcx.gz` is deleted upon `STAGE_COMPLETE`, and the activity appears on Strava with power, cadence, heart rate, virtual speed, and distance curves.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-e0585866] Verify unauthenticated `curl -i "http://192.168.1.100/api/session/download?id=<id>&format=tcx"` returns `HTTP/1.1 401 Unauthorized`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-f2237a5a] Click **Connect with Strava** (`GET /api/strava/auth-url`), authorize the app on Strava, and confirm redirect back to `/api/strava/callback?code=...&state=...` succeeds and `GET /api/strava/status` reports `"token_valid": true`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-f7b4f7ba] Complete a 6th stored ride and verify `RideStore` evicts the oldest `.bin` (and any `.tcx.gz`), maintaining a maximum of `5` stored rides.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-f7d8c498] Confirm the `15 s` coasting window contains continuous 1 Hz `<Trackpoint>` entries with `<ns3:Watts>0</ns3:Watts>` and `<ns3:Speed>0.00</ns3:Speed>`, whereas the `15 s` manual `PAUSED` window has no `<Trackpoint>` timestamps during the pause gap.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-f89b5b84] **Inactivity Auto-End (`4.4`) or Manual End (`4.2`):** Stop pedalling for `180 s` (or click **End Session**). Verify trailing zero-cadence records from the final stop are stripped and the completed ride appears in `GET /api/session/history`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-fa409e1c] **SDK config chain.** The project-root `sdkconfig.defaults` holds the project-level defaults. The framework's `sync_sdkconfig.py` pre-script keeps it in step with the framework root file, as it does for the template app. The framework BLE defaults follow, and the target's own `sdkconfig.defaults` comes last, so the target's octal-PSRAM, 16 MB flash, USB-console and NimBLE slot settings win.
`invariant · kind: semantic · anchors: sdkconfig.defaults · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-fccc16af] Trigger **Upload to Strava** (`POST /api/session/upload?id=<id>`) or end a ride with auto-upload enabled:
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

## Ruled out, failure signatures and gotchas

### [seed-582ed4d1] macOS native USB CDC port switching: When the board is running Arduino/CDC (VID:PID 2341:0070), flashing requires 1200bps touch to flip into the ROM bootloader/JTAG mode (VID:PID 303A:1001). Enabling `board_upload.use_1200bps_touch = yes` and `board_upload.wait_for_upload_port = yes` in `platformio.ini` is mandatory to avoid pySerial `Device not configured (Errno 6)` errors.
`ruled-out · kind: episodic · anchors: platformio.ini, sdkconfig.defaults, CMakeLists.txt, src/CMakeLists.txt, src/main.cpp, docs/measurements/phase0/A08-20260926-chb-adc-spike.csv, .gitignore`

## Decisions

### [seed-a4d04869] `gpio_input_enable()` was explicitly called on GPIO 5, 8, and 9 after `adc_continuous_start()`. The ESP-IDF continuous ADC driver invokes `gpio_config_as_analog()`, which disables digital input buffers (`gpio_input_disable()`). Re-enabling the input buffer restores full digital edge interrupt capabilities without interfering with DMA-driven analog sampling.
`decision · kind: episodic · anchors: platformio.ini, sdkconfig.defaults, CMakeLists.txt, src/CMakeLists.txt, src/main.cpp, docs/measurements/phase0/A08-20260926-chb-adc-spike.csv, .gitignore`

### [seed-d832c579] Retained clean tap safety by setting pull modes back to `GPIO_FLOATING` after the controlled transition test.
`decision · kind: episodic · anchors: platformio.ini, sdkconfig.defaults, CMakeLists.txt, src/CMakeLists.txt, src/main.cpp, docs/measurements/phase0/A08-20260926-chb-adc-spike.csv, .gitignore`

## Procedures / Recipes

### [docseed-1914be52] **Build wiring.** The PlatformIO env passes `-DBOARD_HEADER=\"hardware/ex3-waveshare-nano/board.h\"`, the target's partition table and the target's `sdkconfig.defaults` (§12.2).
`recipe · kind: procedural · anchors: sdkconfig.defaults · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-f39a7963] Flash production or diagnostic firmware via PlatformIO MCP (`upload` for environment `ex3-waveshare-nano` or `ex3-waveshare-nano-diag`).
`recipe · kind: procedural · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`
