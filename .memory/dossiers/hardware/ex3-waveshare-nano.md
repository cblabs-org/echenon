# hardware/ex3-waveshare-nano

## Invariants & contracts

### [docseed-569dd29a] `hardware/ex3-waveshare-nano/partitions.csv` — flash partition table
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/partitions.csv · source_doc: docs/implementation-strategy.md · evidence: doc:docs/implementation-strategy.md`

### [docseed-d11c20de] `hardware/ex3-waveshare-nano/sdkconfig.defaults` — SDK overrides
`invariant · kind: semantic · anchors: hardware/ex3-waveshare-nano/sdkconfig.defaults · source_doc: docs/implementation-strategy.md · evidence: doc:docs/implementation-strategy.md`

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
