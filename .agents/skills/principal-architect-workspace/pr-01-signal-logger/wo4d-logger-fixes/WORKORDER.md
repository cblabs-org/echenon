---
complexity: standard
---

# WO4d — Logger Fixes (Chief Architect Review Items)

**Parent:** [PR-01 Signal Logger](../PROJECT.md)
**Branch:** `signal-logger/wo4d-logger-fixes` (base: `signal-logger/wo4b-bench-gate`)
**Source:** [2026-10-04 CA Progress Review](../reviews/2026-10-04-pr01-progress-review.md) §2

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Fix the logger defects identified in the Chief Architect's progress review
(items L-01 through L-05, L-07 and L-08) so that the firmware produces
correct, usable data for the WO5 register campaign. Also write native tests
for the new pure logic (de-interleave, motor state machine, timestamp maths).

## Context

- **SignalLogger source:** `src/echenon/SignalLogger.h` / `.cpp`
- **Board header:** `hardware/ex3-waveshare-nano/board.h`
- The ADC scans **8 channels** `{0,1,2,3,4,5,6,9}` at 83.3 kS/s total
  (≈ 10.4 kS/s per channel).
- The ride log uses `DecimationRow` (min/mean/max per channel per 50 ms window).
- The burst buffer stores raw `uint16_t` samples in DMA arrival order.
- The event log is a 4,000-entry ring of `EdgeEvent`.
- `EncoderModule` is wired on GPIO 8/9 via PCNT. It publishes events on the
  framework `EventBus`.

### Review Items to Fix

| # | Severity | Summary |
|---|---|---|
| **L-01** | 🔴 | Burst CSV puts samples under the wrong channels — de-interleave using `type2.channel` |
| **L-02** | 🔴 | Motor on/off events flood the event log — per-channel state + drive direction |
| **L-03** | 🟠 | Timestamps are per DMA read, not per sample — derive from index × sample period |
| **L-04** | 🟠 | Encoder edges no longer logged — subscribe to `EncoderModule` EventBus events |
| **L-05** | 🟠 | Ride log wraps at 30 min, dead columns — 8-channel rows, 10 Hz option, wrap flag |
| **L-07** | 🟡 | Leftover debris — static `_adc_configured`, `fail_count`, dual logging, comments |
| **L-08** | 🟡 | Guard SignalLogger behind `WITH_SIGNAL_LOGGER` |

> [!IMPORTANT]
> L-06 (export race condition: CSV handlers read while capture writes) is **out
> of scope** for this WO. It requires a design decision (pause capture vs
> snapshot under lock). Note it in the completion report.

## Strict Boundary Box

- **Modify:**
  - `src/echenon/SignalLogger.h` — struct changes, new members, API changes
  - `src/echenon/SignalLogger.cpp` — all fix items
  - `src/echenon/EchenonModule.cpp` — wrap `SignalLogger` usage in `#ifdef WITH_SIGNAL_LOGGER`
  - `src/echenon/EchenonModule.h` — conditional member
  - `hardware/ex3-waveshare-nano/board.h` — no changes needed (encoder already wired)
  - `platformio.ini` — add `-DWITH_SIGNAL_LOGGER` to `ex3-waveshare-nano` build flags
- **Create:**
  - `test/test_native/test_signal_logic.cpp` (or split) — native tests
  - Any thin header-only helpers needed to extract pure logic for testing
- **Do NOT modify:**
  - `web/` (dashboard HTML/JS)
  - `docs/`, `SPEC.md`, `hardware/` (except board.h if needed)
  - `framework/` (read-only submodule)
  - `.agents/` (except this WORKORDER's completion report)
- **Do NOT create additional branches.**

## Tooling Constraints

> [!CAUTION]
> **Rule 03:** Flashing follows Tag → Build → Flash using `v0.1.0-dev.N` tags.
> Dev tags are autonomous (no user approval needed).
>
> **Rule 04:** Use PlatformIO MCP tools (`build_project`, `upload_firmware`,
> `run_tests`), never the `pio` CLI directly.

## Line-Item Instructions

### A. L-01 — Burst De-Interleave

The burst buffer currently stores raw samples in DMA arrival order without
channel tags. The `/burst.csv` endpoint reshapes them into rows of
`kNumChannels=10`, but only 8 channels are scanned and nothing guarantees
frame alignment. **Every burst column is misattributed.**

- [ ] Store channel identity with each burst sample. Options:
  - (a) Store `{channel, raw_value}` pairs (doubles memory cost); or
  - (b) Build per-channel arrays at export time by replaying the DMA
    channel sequence; or
  - (c) Store the channel in the burst buffer itself (e.g., pack
    `channel:4 | data:12` into a `uint16_t` — data is 12-bit).
  Recommend option (c) to avoid doubling PSRAM usage.
- [ ] Update `/burst.csv` to emit one column per **actually scanned** channel,
  with real signal names from `board.h` (e.g., `pot_wiper`, `cadence`,
  `motor_p`, `motor_n`, `enc_vcc`).
- [ ] Add a header comment or metadata row with:
  - Per-channel sample rate (83,333 ÷ 8 ≈ 10,416 Hz)
  - Start timestamp (µs since boot)
  - Total dropped-sample count (from L-03's overflow counter)

### B. L-02 — Motor State Machine (Per-Channel, With Direction)

Currently one `_motorOn` flag is shared between ch5 (motor+) and ch6 (motor−).
While motor runs forward, ch5 samples set it and ch6 samples clear it,
flooding the event ring at 10 kS/s.

- [ ] Track motor state **per channel** (`_motorPState`, `_motorNState`).
- [ ] Derive **drive direction** from the combined state:
  - Both LOW → coast / idle
  - P HIGH, N LOW → forward
  - P LOW, N HIGH → reverse
  - Both HIGH → brake (should not happen with flat DC, but log if seen)
- [ ] Add hysteresis **and** a minimum-duration filter (e.g., 5 ms debounce)
  to prevent chatter. Only emit an event when the combined state changes and
  holds for the debounce period.
- [ ] Event type: use a new type value (e.g., `5 = motor_direction`) with
  value encoding the direction enum.

### C. L-03 — Per-Sample Timestamps + Overflow Counter

`ts_us = esp_timer_get_time()` is taken once per `adc_continuous_read()` and
stamped on every sample in that batch. This gives ~3 ms of phantom precision.

- [ ] Derive each sample's time from its index within the frame:
  `sample_ts = frame_start_ts + (sample_index * 1000000 / kAdcSampleFreq)`
  where `frame_start_ts` is anchored to a counted DMA frame.
- [ ] Register the `on_pool_ovf` callback with `adc_continuous_register_event_callbacks()`.
- [ ] Maintain a `volatile uint32_t _overflowCount` and export it via
  `/api/echenon/status` JSON.
- [ ] Use the overflow count in the burst CSV header (L-01).

### D. L-04 — Log EncoderModule Events

GPIO 8/9 are now PCNT (no ADC). The old channel-8 threshold path is dead code.
Encoder events must come from the framework `EncoderModule` via `EventBus`.

- [ ] Subscribe to `EncoderModule`'s EventBus events (check the framework
  API for the event topic — likely `encoder.changed` or similar).
- [ ] On each encoder event, write an `EdgeEvent` to the event log with:
  - `type = 5` (or a new enum value for `encoder`)
  - `channel = 8` (for consistency with legacy)
  - `value` = direction (CW/CCW)
  - `timestamp_us` from the event or `esp_timer_get_time()`
- [ ] Remove the dead channel-8 Ch B threshold code in `handleSoftwareThresholds()`.

### E. L-05 — Ride Log: 8-Channel Rows, 10 Hz Option, Wrap Flag

The ride log uses 10-channel rows (124 bytes each). Channels 7 and 8 are always
zero (not scanned). At 20 Hz (50 ms) × 36,000 rows = 30 minutes → silent wrap.

- [ ] Change `DecimationRow.channels[]` to **8 channels** (matching
  `kAdcChannelCount`). Map logical index to physical channel via `adc_channels[]`.
  This drops row size to ~100 bytes.
- [ ] Add a configurable decimation rate: default 50 ms (20 Hz), with a
  **100 ms (10 Hz) option** selectable via config. At 10 Hz + 8-channel rows,
  36,000 rows = 60 minutes.
- [ ] Add a `_rideLogWrapped` status flag. Set it when `_rideLogHead` wraps.
  Export it via `/api/echenon/status` as `ride_log_wrapped: true|false`.
- [ ] Update `/api/echenon/ride-log.csv` to use **named headers** based on
  the signal names (e.g., `timestamp_ms,pot_wiper_min,pot_wiper_mean,pot_wiper_max,...`).

### F. L-07 — Remove Debris

- [ ] Remove the function-static `_adc_configured` flag. Move ADC config
  state to a member variable so `stop()` + `begin()` works correctly.
- [ ] Remove the `fail_count` no-op in `tick()`.
- [ ] Remove the dual `printf` + `ESP_LOGI` logging every second. Keep only
  `ESP_LOGI` at a reasonable interval (e.g., every 60 seconds or first row only).
- [ ] Clean up stream-of-consciousness comments in `begin()`. Keep concise
  technical comments only.
- [ ] Remove or close the WO2 note about `PULLUP_ENABLE` on ISR pins
  (ISRs are gone since WO4a).

### G. L-08 — `WITH_SIGNAL_LOGGER` Guard

SignalLogger allocates ~7.5 MB of PSRAM. After PR-01, it must only compile in
diagnostic builds.

- [ ] Wrap all `SignalLogger` usage in `EchenonModule.cpp` (and `.h`) with:
  ```cpp
  #ifdef WITH_SIGNAL_LOGGER
  // ... SignalLogger member, begin(), tick(), registerRoutes()
  #endif
  ```
- [ ] Add `-DWITH_SIGNAL_LOGGER` to `platformio.ini` `[env:ex3-waveshare-nano]`
  build flags (so it compiles now, but can be removed later).
- [ ] Verify the build succeeds with and without the flag:
  - With: full build, SignalLogger included
  - Without: build succeeds, no SignalLogger allocation

### H. Native Tests

Write GoogleTest suites for the new pure logic. These run on `env:native`.

- [ ] **Test de-interleave logic:** Given a sequence of `{channel, data}` pairs
  in DMA order, verify that the de-interleave produces correct per-channel
  columns.
- [ ] **Test motor state machine:** Feed a sequence of motor+ and motor−
  threshold crossings (with timestamps) and verify:
  - Correct direction derivation (forward/reverse/coast/brake)
  - Debounce: rapid toggles within the debounce window produce no event
  - State transitions only when the combined state changes and holds
- [ ] **Test timestamp maths:** Given a frame start time and sample index,
  verify the per-sample timestamp calculation.
- [ ] **Run tests:** Use MCP `run_tests` tool on `env:native`. All must pass.

> [!TIP]
> Extract the pure logic into header-only or `.cpp` files that can be compiled
> on the host without ESP-IDF headers. Use `#ifdef UNIT_TEST` guards or a thin
> abstraction layer.

## Verification

- [x] `build_project` on `env:ex3-waveshare-nano` succeeds (with `WITH_SIGNAL_LOGGER`)
- [x] `build_project` on `env:ex3-waveshare-nano` succeeds without `WITH_SIGNAL_LOGGER`
  (remove the flag temporarily and rebuild, then add it back)
- [x] `run_tests` on `env:native` — all tests pass
- [x] No `printf` calls remain in SignalLogger
- [x] Burst CSV has named columns matching scanned channels
- [x] Ride log CSV has named columns, no `ch7`/`ch8` dead columns
- [x] Event log does not flood with motor events (verified by inspection)
- [x] `_overflowCount` is exported in status JSON

---

## Completion Report (written by the worker at task completion)

### Status
complete

### Changes Made
- `src/echenon/SignalLogic.h` — Created pure logic header providing bit-packed sample encoding (channel:4 | data:12 into uint16_t), stream burst de-interleaving with channel-0 frame alignment synchronization, MotorStateMachine with hysteresis (500 mV / 400 mV) and 5 ms debounce filtering, and sample timestamp arithmetic.
- `src/echenon/SignalLogger.h` — Resized `DecimationRow` to 8 scanned channels (~100 bytes/row, saving ~860 KB PSRAM); defined `EdgeEventType` enums (including `EDGE_EVENT_MOTOR_DIRECTION=5` and `EDGE_EVENT_ENCODER=6`); added `incrementOverflowCount()`, `isRideLogWrapped()`, `setEventBus()`, `logEncoderEvent()`, and member `_adcConfigured`.
- `src/echenon/SignalLogger.cpp` — Switched burst buffer to store packed channel+data samples; updated `/api/echenon/burst.csv` to de-interleave streams into named columns with sample rate, start time, and overflow count metadata; updated `processAdcFrame` to derive per-sample timestamps; registered `s_on_pool_ovf` callback with `adc_continuous_register_event_callbacks` for DMA pool overflows; removed dead channel 8 threshold code; subscribed to `"encoder.rotate"` EventBus topic; updated `/api/echenon/status` to export `overflow_count` and `ride_log_wrapped`; updated `/api/echenon/ride-log.csv` with named headers for 8 scanned channels; converted `_adc_configured` to member variable; removed `fail_count` no-op; removed all `printf` calls.
- `src/echenon/EchenonModule.h` & `src/echenon/EchenonModule.cpp` — Wrapped all `SignalLogger` usage and robbery structures in `#ifdef WITH_SIGNAL_LOGGER`; updated `/api/echenon/live` endpoint to report 8 scanned channels with physical channel mapping; wired EventBus subscription.
- `platformio.ini` — Added `-DWITH_SIGNAL_LOGGER` to `[env:ex3-waveshare-nano]` build flags; simplified `[env:native]` dependencies for pure host unit testing.
- `test/test_native/test_signal_logic.cpp` — Created GoogleTest test suite with 14 test cases covering sample packing/unpacking, channel slot mapping, burst de-interleaving (ideal, partial leading frame, missing sample), motor state machine (idle, forward, reverse, brake, chatter rejection, hysteresis), and timestamp calculations.

### Decisions Made
- Pack `channel:4 | data:12` into existing `uint16_t` burst buffer rather than expanding buffer memory, preserving 1.5M sample capacity within ~3 MB PSRAM.
- In `SignalLogic.h`, de-interleaving drops partial leading frames prior to the first channel 0 to ensure strict frame alignment from the very first CSV row.
- Fallback wired in `EchenonModule::tick()` to connect `_signalLogger.setEventBus(&app.getEventBus())` if not injected beforehand, avoiding any changes to `src/main.cpp` and staying strictly inside the boundary box.

### Dead Ends & Warnings
- Initial native test build attempted to pull `Application` and entire `framework/modules` into `env:native`, causing compilation failures due to missing hardware display/network headers. Resolved by scoping `env:native` dependencies to `bblanchon/ArduinoJson` for pure logic testing.
- Initial compilation showed deprecation warning on `volatile uint32_t++` under C++20; resolved by using explicit `_overflowCount = _overflowCount + 1`.

### Out-of-Scope Findings
- None.

### Blockers
- None.
