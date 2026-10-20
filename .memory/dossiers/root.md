# root

## Invariants & contracts

### [docseed-2dda879b] **Two flag families.** `HAS_*` macros in `board.h` state that a bike signal exists and is wired. `WITH_*` build flags in `platformio.ini` select which framework and application modules compile (§12.3).
`invariant · kind: semantic · anchors: platformio.ini · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-fa409e1c] **SDK config chain.** The project-root `sdkconfig.defaults` holds the project-level defaults. The framework's `sync_sdkconfig.py` pre-script keeps it in step with the framework root file, as it does for the template app. The framework BLE defaults follow, and the target's own `sdkconfig.defaults` comes last, so the target's octal-PSRAM, 16 MB flash, USB-console and NimBLE slot settings win.
`invariant · kind: semantic · anchors: sdkconfig.defaults · source_doc: SPEC.md · evidence: doc:SPEC.md`

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
