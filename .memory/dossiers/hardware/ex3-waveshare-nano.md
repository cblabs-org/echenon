# hardware/ex3-waveshare-nano

## Invariants & contracts

### [docseed-2438702f] [`hardware/ex3-waveshare-nano/sdkconfig.defaults`](../hardware/ex3-waveshare-nano/sdkconfig.defaults) sets:
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/sdkconfig.defaults · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-3934621c] [`hardware/ex3-waveshare-nano/partitions.csv`](../hardware/ex3-waveshare-nano/partitions.csv) matches the 16 MB dual-OTA layout (`nvs` `0x5000`, `otadata` `0x2000`, `app0` `6 MB`, `app1` `6 MB`, `config` `1 MB` LittleFS, `storage` `2.75 MB` LittleFS, `coredump` `64 KB`).
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/partitions.csv · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-569dd29a] `hardware/ex3-waveshare-nano/partitions.csv` — flash partition table
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/partitions.csv · source_doc: docs/implementation-strategy.md · evidence: doc:docs/implementation-strategy.md`

### [docseed-5fd84ff1] `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, and `CONFIG_SPIRAM_SPEED_80M=y` are active in [`hardware/ex3-waveshare-nano/sdkconfig.defaults:9-12`](../hardware/ex3-waveshare-nano/sdkconfig.defaults#L9-L12).
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/sdkconfig.defaults:9-12, hardware/ex3-waveshare-nano/sdkconfig.defaults · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-d11c20de] `hardware/ex3-waveshare-nano/sdkconfig.defaults` — SDK overrides
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/sdkconfig.defaults · source_doc: docs/implementation-strategy.md · evidence: doc:docs/implementation-strategy.md`

### [docseed-d46e48fd] Rotary knob decoding uses `EncoderModule` (`PCNT` hardware quadrature unit configured via `BOARD_ENCODERS` in [`hardware/ex3-waveshare-nano/board.h:34-43`](../hardware/ex3-waveshare-nano/board.h#L34-L43)).
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/board.h:34-43, hardware/ex3-waveshare-nano/board.h · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

### [docseed-e71e522f] `hardware/ex3-waveshare-nano/board.h` — pin map, bike characteristics and `HAS_*` signal flags. Built from the framework's `hardware/_common/board_contract.h` and `hardware/_TEMPLATE/`, not cloned from the reference `esp32s3nano` board.
`contract · kind: semantic · anchors: hardware/ex3-waveshare-nano/board.h · source_doc: docs/implementation-strategy.md · evidence: doc:docs/implementation-strategy.md`

## Ruled out, failure signatures and gotchas

### [seed-6ea6a7ad] Initial build failed with missing `EncoderConfig.h` when including `#include "EncoderConfig.h"` verbatim from the work order instruction. Resolved by including `"EncoderModule/EncoderConfig.h"` consistent with CrowPanel and native board headers.
`ruled-out · kind: episodic · anchors: platformio.ini, hardware/ex3-waveshare-nano/board.h, src/echenon/SignalLogger.cpp, src/main.cpp`

## Decisions

### [seed-23c800b6] Used `#include "EncoderModule/EncoderConfig.h"` in `board.h` because `EncoderConfig.h` is located within `framework/modules/EncoderModule/` rather than the top-level include path.
`decision · kind: episodic · anchors: platformio.ini, hardware/ex3-waveshare-nano/board.h, src/echenon/SignalLogger.cpp, src/main.cpp`

### [seed-4d09f62f] In `SignalLogger.cpp`, defined `adc_channels` at file scope so both `begin()` (for DMA pattern setup) and `tick()` (for `_currentRow` reset) cleanly share the exact channel scan map.
`decision · kind: episodic · anchors: platformio.ini, hardware/ex3-waveshare-nano/board.h, src/echenon/SignalLogger.cpp, src/main.cpp`

### [seed-c36b2296] `platformio.ini` native target: Set `-DBOARD_HEADER=\"hardware/native/board.h\"` per SPEC §12.2 to resolve native board headers cleanly.
`decision · kind: episodic · anchors: platformio.ini, hardware/ex3-waveshare-nano/board.h, hardware/ex3-waveshare-nano/partitions.csv, hardware/ex3-waveshare-nano/sdkconfig.defaults, sdkconfig.defaults, src/main.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, CMakeLists.txt, .gitignore`

### [seed-d04dbb3d] Partition table: Used partition name `storage` for LittleFS (2.75 MB) and `config` (1 MB) to match `framework/modules/Application/Application.cpp:143` (`LittleFS.begin(true, "/littlefs", 10, "storage")`).
`decision · kind: episodic · anchors: platformio.ini, hardware/ex3-waveshare-nano/board.h, hardware/ex3-waveshare-nano/partitions.csv, hardware/ex3-waveshare-nano/sdkconfig.defaults, sdkconfig.defaults, src/main.cpp, src/echenon/EchenonModule.h, src/echenon/EchenonModule.cpp, CMakeLists.txt, .gitignore`

## Procedures / Recipes

### [docseed-eec03825] [`platformio.ini:6`](../platformio.ini#L6) pins `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip` (`ESP-IDF 5.5.5`, `Arduino 3.3.11`) while preserving `board = um_nanos3` ([`platformio.ini:51`](../platformio.ini#L51)) for raw ESP32-S3 GPIO numbering (`1..18`) in `hardware/ex3-waveshare-nano/board.h`.
`recipe · kind: procedural · anchors: platformio.ini:6, platformio.ini:51, hardware/ex3-waveshare-nano/board.h, platformio.ini · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`
