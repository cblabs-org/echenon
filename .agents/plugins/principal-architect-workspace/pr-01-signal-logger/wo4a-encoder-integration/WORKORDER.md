---
schema_version: 1
wo_id: WO-01.41
pr_id: PR-01
title: EncoderModule Code Integration
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
---
# WO4a — EncoderModule Code Integration

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create branches, and do not plan. Your task is strictly
> limited to executing the checklist below on the current branch
> (`fix/adc-gpio-conflict`).

## Objective

Wire up the framework's `EncoderModule` (PCNT peripheral) on GPIO 8/9 by
releasing those pins from the ADC continuous scan, adding the encoder
descriptor to `board.h`, enabling `WITH_ENCODER`, registering the module in
`main.cpp`, and stripping temporary diagnostic instrumentation from
`SignalLogger.cpp`. The build must compile with 0 errors.

## Parent Project

PR-01 Signal Logger (Phase 0) —
`.agents/plugins/principal-architect-workspace/pr-01-signal-logger/PROJECT.md`
Decomposition approved by user on 2026-10-04 (recorded in SESSION LOG).

## Strict Boundary Box

- **IN SCOPE:** `platformio.ini`, `board.h`, `SignalLogger.cpp`, `main.cpp`
- **OUT OF SCOPE:** Everything else. Do not modify the framework submodule,
  the dashboard JS, `EchenonModule.*`, docs, or measurement files.
- **Do NOT flash firmware** — build verification only.
- **Do NOT create branches** — work on the current `fix/adc-gpio-conflict`.
- **Do NOT modify the ADC channel mapping** for channels 0–6 or channel 9.
  Only channels 7 and 8 (GPIO 8/9) are removed from the ADC scan.

## Tooling Constraints

- Use the `platformio-mcp_platformio` MCP tool `build_project` for build
  verification. Do NOT run `pio` CLI directly.
- Environment name: `ex3-waveshare-nano`

## Context — Key Facts

- **GPIO 8 = Encoder Ch A (ADC1_CH7), GPIO 9 = Encoder Ch B (ADC1_CH8)**
- **GPIO 10 = Encoder VCC (ADC1_CH9)** — keep this in the ADC scan
- The ADC pattern is at `SignalLogger.cpp` lines 74–82: a loop `for (i=0; i<10)`
  creating channels 0–9. Change to 8 channels: 0–6 plus 9 (skip 7 and 8).
- `_currentRow` init loop and `pattern_num` must match the new channel count.
- The `EncoderModule` reference is at:
  `framework/.agents/skills/framework-expert/systems/input/encoder-module.md`
- The `BoardEncoder` struct is at:
  `framework/modules/EncoderModule/EncoderConfig.h`
- The registration pattern uses `addEncoder()` → `begin(app)` → `registerModule()`.

## Line-Item Instructions

### 1. `platformio.ini` — Enable `WITH_ENCODER`

- [ ] In the `[env:ex3-waveshare-nano]` section's `build_flags`, add
  `-DWITH_ENCODER` (on a new line in the flag list).

### 2. `hardware/ex3-waveshare-nano/board.h` — Add encoder descriptor

- [ ] Add an `#ifdef WITH_ENCODER` block after the existing pin definitions
  (before the `HAS_MOTOR_CONTROL` line):

```cpp
#ifdef WITH_ENCODER
#include "EncoderConfig.h"
constexpr BoardEncoder BOARD_ENCODERS[] = {
    {.id = "knob", .pinA = PIN_KNOB_ENC_A, .pinB = PIN_KNOB_ENC_B,
     .stepsPerDetent = 4, .reversed = false,
     .accel = {.enabled = false}}
};
constexpr size_t BOARD_ENCODER_COUNT =
    sizeof(BOARD_ENCODERS) / sizeof(BOARD_ENCODERS[0]);
#endif
```

### 3. `src/echenon/SignalLogger.cpp` — Release GPIO 8/9 from ADC scan

- [ ] **Change the ADC pattern** from 10 channels (0–9) to 8 channels
  (0–6 plus 9). Replace the simple `for (i=0; i<10)` loop with one
  that builds the correct channel list:

```cpp
// Channels to scan: 0-6 (pot, cadence, motor) + 9 (encoder VCC)
// Channels 7,8 (encoder A,B) released for PCNT EncoderModule
const int adc_channels[] = {0, 1, 2, 3, 4, 5, 6, 9};
constexpr int kAdcChannelCount = sizeof(adc_channels) / sizeof(adc_channels[0]);

adc_digi_pattern_config_t adc_pattern[kAdcChannelCount] = {};
for (int i = 0; i < kAdcChannelCount; i++) {
    adc_pattern[i].atten = ADC_ATTEN_DB_12;
    adc_pattern[i].channel = adc_channels[i];
    adc_pattern[i].unit = ADC_UNIT_1;
    adc_pattern[i].bit_width = ADC_BITWIDTH_12;
}
dig_cfg.pattern_num = kAdcChannelCount;
```

- [ ] **Update `_currentRow` initialisation** — wherever the code initialises
  the 10-channel DecimationRow (setting min=4095, max=0, mean=0 for each
  channel), update the loop bound from 10 to 8 or use `kAdcChannelCount`.
  **IMPORTANT:** The DecimationRow struct likely stores data indexed by
  ADC channel number (0–9), not array index. If so, only clear channels
  that are in the scan pattern. Read the code carefully before changing.

- [ ] **Strip diagnostic instrumentation** — Remove the following temporary
  additions (search for `diag:` in git log for the exact changes):
  - Per-channel ADC sample counter (added in commit `2a12820`)
  - CH4 range diagnostic logging (added in commit `2a12820`)
  - Raw DMA sample dump per channel (added in commit `fe60d76`)
  
  These are tagged with comments like `// diag` or `// DEBUG`. Remove them
  cleanly without disturbing the surrounding logic.

### 4. `src/main.cpp` — Register EncoderModule

- [ ] Add the encoder module registration inside `#ifdef WITH_ENCODER` guards:

```cpp
#ifdef WITH_ENCODER
#include "EncoderModule.hpp"
EncoderModule encoderModule;
#endif
```

And in `setup()`, after `app.registerModule(&echenonModule)` but before
`app.begin(config)`:

```cpp
#ifdef WITH_ENCODER
for (size_t i = 0; i < BOARD_ENCODER_COUNT; i++) {
    encoderModule.addEncoder(BOARD_ENCODERS[i]);
}
encoderModule.begin(app);
app.registerModule(&encoderModule);
#endif
```

### 5. Build verification

- [ ] Run `build_project` via the PlatformIO MCP tool for env
  `ex3-waveshare-nano`. Must compile with 0 errors.
- [ ] Verify the build output shows `WITH_ENCODER` is active and
  `EncoderModule` is being compiled.

## Verification Requirement

- Build succeeds with 0 errors
- `WITH_ENCODER` flag active in build output
- ADC pattern has 8 channels (not 10)
- Diagnostic instrumentation removed
- No changes to files outside the boundary box
- **Commit all changes** with message:
  `feat(encoder): wire up EncoderModule on GPIO 8/9 via PCNT`

## Issue Surfacing

If you discover a defect, upstream bug, or concern outside your boundary box,
surface it in your Completion Report below — do not file issues directly.
The Principal Architect will triage and file them.

---

## Completion Report (written by the worker at task completion)

### Status
complete

### Changes Made
- `platformio.ini` — Added `-DWITH_ENCODER` to `[env:ex3-waveshare-nano]` build flags to activate `EncoderModule`.
- `hardware/ex3-waveshare-nano/board.h` — Added `#ifdef WITH_ENCODER` block declaring `BOARD_ENCODERS` (`"knob"`, GPIO 8/9, 4 steps per detent, acceleration disabled) and `BOARD_ENCODER_COUNT`. Included `"EncoderModule/EncoderConfig.h"` to adhere to the framework's module co-location include structure.
- `src/echenon/SignalLogger.cpp` — Released GPIO 8 and 9 (ADC1 channels 7 and 8) from continuous ADC scanning, re-indexing the pattern to 8 channels ({0, 1, 2, 3, 4, 5, 6, 9}); updated `_currentRow` reset logic to clear only active scan channels; cleanly removed temporary diagnostic logging and DMA dump hooks (`s_diagSamples`, `s_ch4Min`/`s_ch4Max`, and `s_chanSeen`).
- `src/main.cpp` — Conditionally included `"EncoderModule.hpp"` and `"board.h"`, defined `EncoderModule encoderModule`, populated configured encoders in `setup()`, called `begin(app)`, and registered the module with `app` before `app.begin(config)`.

### Decisions Made
- Used `#include "EncoderModule/EncoderConfig.h"` in `board.h` because `EncoderConfig.h` is located within `framework/modules/EncoderModule/` rather than the top-level include path.
- In `SignalLogger.cpp`, defined `adc_channels` at file scope so both `begin()` (for DMA pattern setup) and `tick()` (for `_currentRow` reset) cleanly share the exact channel scan map.

### Dead Ends & Warnings
- Initial build failed with missing `EncoderConfig.h` when including `#include "EncoderConfig.h"` verbatim from the work order instruction. Resolved by including `"EncoderModule/EncoderConfig.h"` consistent with CrowPanel and native board headers.

### Out-of-Scope Findings
- (none)

### Blockers
(none)
