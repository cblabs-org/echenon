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

### [docseed-1dea07ff] Inspected via `xtensa-esp32s3-elf-size -A` and `xtensa-esp32s3-elf-objdump -t` on `.pio/build/ex3-waveshare-nano/firmware.elf` (`v0.3.0-dev.51`): the largest `.dram0` symbol is `g_cnxMgr` at `3,880 B` (`1.18%` of 320 KB internal DRAM), well below the 10% (`~32.7 KB`) advisory threshold; `mcpServer` (`_ZL9mcpServer`) occupies `104 B` in `.dram0.data`.
`invariant · kind: semantic · anchors: .pio/build/ex3-waveshare-nano/firmware.elf · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-1e01684a] **TLS + BLE Coexistence (`UploadQueue`):** Each outbound HTTPS call (`StravaUploader::refreshTokenIfNeeded`, `postUpload`, `pollUploadStatus`) is executed inside a single split stage on the background `DataCollector` FreeRTOS task (`Core 0`) while holding a stack-allocated `BLEManager::ScanInhibit` RAII guard (`#ifdef WITH_BLE`) so internal SRAM retains $> 40\text{ KB}$ headroom during `mbedTLS...
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-22200df1] **Manual Discard (`4.2`):** Start a short session, click **Discard**, and verify `session_state` returns to `"IDLE"` and `/ride_active.bin` is removed without adding an entry to `GET /api/session/history`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-2dda879b] **Two flag families.** `HAS_*` macros in `board.h` state that a bike signal exists and is wired. `WITH_*` build flags in `platformio.ini` select which framework and application modules compile (§12.3).
`invariant · kind: semantic · anchors: platformio.ini · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-32d0ebce] `platformio.ini` chains `SDKCONFIG_DEFAULTS="sdkconfig.defaults;framework/hardware/_common/sdkconfig.ble.defaults;hardware/ex3-waveshare-nano/sdkconfig.defaults"`.
`invariant · kind: semantic · anchors: platformio.ini · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-34d795bf] Click **Pause** in the Session tab (`POST /api/session/pause`): verify `session_state` becomes `"PAUSED"`. Wait `15 s`, then click **Resume** (`POST /api/session/resume`) and pedal for another `30 s`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-40500681] Confirm 1 Hz `<Trackpoint>` elements contain `<Time>`, `<DistanceMeters>`, `<Cadence>`, `<HeartRateBpm>` (when BLE HR connected), and `<Extensions><ns3:TPX><ns3:Speed>...</ns3:Speed><ns3:Watts>...</ns3:Watts></ns3:TPX></Extensions>`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-46e81ac4] [`sdkconfig.defaults:245-254`](../sdkconfig.defaults#L245-L254) and [`CMakeLists.txt:3-49`](../CMakeLists.txt#L3-L49) incorporate `WO-09.12` alignment with `cblabs-org/esp32-app-foundation#320`:
`invariant · kind: semantic · anchors: sdkconfig.defaults:245-254, CMakeLists.txt:3-49, sdkconfig.defaults, CMakeLists.txt · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-56371065] **Active Ride Ring Buffer (`RideStore::allocateBuffer()`):** Allocates `14,400 * sizeof(RideRecord) = 115,200 B` (`112.5 KB`, 4 hours at 1 Hz) in PSRAM via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` on ESP32-S3, falling back to `3,600 * 8 = 28,800 B` (`28.1 KB`, 1 hour) in internal DRAM only if PSRAM is unavailable.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-568c71d3] **Auto-Start (`4.1`):** With `auto_start_enabled: true` (`GET /api/session/status`), pedal continuously for `> 5 s`. Verify `/ws/echenon/live` transitions `session_state: "IDLE" -> "ACTIVE"` and `session_elapsed_s` increments at 1 Hz.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-5c1d6641] **Power:** VIN **or** USB, never both (below). **Manual OTA** for routine updates (§8.5, PR-04a; for **`v0.3.0-beta.1`**, `WITH_OTA` is intentionally disabled in `platformio.ini` and beta updates are performed over **USB-C** with the mechanical `SW0` `VIN`/`USB-C` interlock set to USB). UART0 pads left free.
`invariant · kind: semantic · anchors: platformio.ini · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-73a5436e] Replay the same `/api/strava/callback` URL in the browser and confirm it returns `HTTP 403 Forbidden` (single-use `state` consumed).
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-8342d150] Observe `UploadQueue` progress through `STAGE_GENERATE_TCX_GZ -> STAGE_REFRESH_TOKEN -> STAGE_POST_UPLOAD -> STAGE_POLL_STATUS` (one stage per `fetchUpdate()` tick on `Core 0`).
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-850adaa6] **Coasting vs. Manual Pause (`4.2, 4.9`):**
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-86cc9195] Pedal for `> 65 s` (`>= 60` 1 Hz records, crossing at least two `30 s` LittleFS checkpoints to `/ride_active.bin`), then press hardware reset (or power-cycle USB-C) mid-ride.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-8aeb9ad5] [`CHANGELOG.md`](../CHANGELOG.md) — `v0.3.0-beta.1` release notes and milestone history (`6,416 B`).
`invariant · kind: semantic · anchors: CHANGELOG.md · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-9550efd3] On reboot, query `GET /api/session/history` and confirm `RideStore::recoverInterruptedRide()` finalized the interrupted ride into `/rides/ride_<id>.bin`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-a6e7a0a3] Authenticate via the portal and download `/api/session/download?id=<id>&format=tcx` and `format=tcx.gz`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-ae914bbb] [`SPEC.md`](../SPEC.md) & [`AGENTS.md`](../AGENTS.md) — Synced in `WO-09.12` (`5383b781087e` + `178027cc8a7d3bbd2ff09af1fe5510814f1edb58`, receipt `rcpt_d73d60391631`) to `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2`, `02d0b2de`, `isPrivate: false`) and `.agents/skills/framework-portal-design`.
`invariant · kind: semantic · anchors: SPEC.md, AGENTS.md, .agents/skills/framework-portal-design · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-b436ea2d] Confirm boot log reports PSRAM ride buffer allocation (`14,400` records = `115,200 B`), LittleFS mount (`/rides` directory ready), and registration of `Session` (order `80`) and `Upload Services` (order `90`) portal tabs.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-b7d6eba6] [`LICENSE`](../LICENSE) — Apache-2.0 license text (`12,554 B`).
`invariant · kind: semantic · anchors: LICENSE · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-bd8306ed] Confirm `<TrainingCenterDatabase>` root schema and `<Creator><Name>Echenon EX-3</Name></Creator>`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-bda7ff90] **Subsequent Ride After `ENDED` (`4.1`):** Resume pedalling for `> 5 s` without rebooting; verify `SessionManager` transitions cleanly from `ENDED -> ACTIVE` with a fresh session ID.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-d4599601] Confirm serial logs redact `client_secret`, `access_token`, and `refresh_token` (`***REDACTED***`), `internal_free_bytes` remains $> 40\text{ KB}$ during TLS handshakes (`BLEManager::ScanInhibit` active), `/rides/ride_<id>.tcx.gz` is deleted upon `STAGE_COMPLETE`, and the activity appears on Strava with power, cadence, heart rate, virtual speed, and distance curves.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-edd60365] Per `PROJECT.md` owner design decision and [`SPEC.md`](../SPEC.md), `WITH_OTA` is disabled for the v1 Beta release (`v0.3.0-beta.1`). Beta builders flash updates over USB-C with `SW0` (`VIN`) switched OFF. Consequently, the OTA upgrade boot-log capture check is waived for `v0.3.0-beta.1` and deferred to the post-beta OTA enablement milestone (tracking upstream `cblabs-org/esp32-app-foundation#2...
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-f2237a5a] Click **Connect with Strava** (`GET /api/strava/auth-url`), authorize the app on Strava, and confirm redirect back to `/api/strava/callback?code=...&state=...` succeeds and `GET /api/strava/status` reports `"token_valid": true`.
`invariant · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

### [docseed-f3bfebb1] Verify unauthenticated `curl -i "http://192.168.1.100/api/session/download?id=<id>&format=tcx"` returns `HTTP/1.1 401 Unauthorized`.
`contract · kind: semantic · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`

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

### [docseed-338b3630] [`CONTRIBUTING.md`](../CONTRIBUTING.md) — Developer setup, `cblabs-org/esp32-app-foundation` submodule instructions, native test floor, and PR workflow (`6,856 B`).
`recipe · kind: procedural · anchors: CONTRIBUTING.md · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-f0f2f65f] [`README.md`](../README.md) — Beta tester recruitment guide, hardware overview, v1 stock runaway limitation disclosure, mechanical `VIN`/`USB-C` power switch procedure, and public `cblabs-org/esp32-app-foundation` (`v1.0.0-beta.2`) submodule clone instructions (`14,130 B`).
`recipe · kind: procedural · anchors: README.md · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-f39a7963] Flash production or diagnostic firmware via PlatformIO MCP (`upload` for environment `ex3-waveshare-nano` or `ex3-waveshare-nano-diag`).
`recipe · kind: procedural · anchors: SPEC.md · source_doc: docs/measurements/phase4/C09-20261026-session-strava-verification.md · evidence: doc:docs/measurements/phase4/C09-20261026-session-strava-verification.md`
