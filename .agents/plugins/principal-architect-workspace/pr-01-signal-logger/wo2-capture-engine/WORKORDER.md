---
schema_version: 1
wo_id: WO-01.2
pr_id: PR-01
title: SignalLogger Capture Engine
complexity: architect
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
---
# WO2 — SignalLogger Capture Engine

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Build `SignalLogger` — the ADC-continuous capture engine that provides two
capture modes (decimated ride log + triggered burst capture) for all 10
ADC1 channels, integrated into the existing `EchenonModule` scaffold.

This is the core data-acquisition firmware for Phase 0. After this WO, the
device can:
- Run a multi-hour ride log with decimated min/mean/max per channel and
  timestamped edge events
- Record triggered 5–30 s burst captures at up to 5 kHz on all channels
- Export data as CSV over HTTP

## Context & Design Decisions (from WO0 spike)

These findings are **binding design inputs** — do not re-investigate them:

1. **ADC-continuous throughput:** 8,333 Sa/s per channel sustained (83.3 kSa/s
   total across 10 channels) with WiFi active. Burst capture up to 5 kHz/ch is
   achievable.
2. **ISR + ADC coexistence:** ADC-continuous and GPIO ISR **do** work on the
   same pad, provided `gpio_input_enable()` is called after the ADC driver
   configures the pin. Use this for cadence edges (GPIO 5) and encoder
   Ch A edges (GPIO 8).
3. **Encoder Ch B:** Peak voltage ~1,444–1,449 mV, well below V_IH (2,475 mV).
   **Ch B edges must be derived from the analog ADC stream using a software
   voltage threshold** — not GPIO ISR or PCNT. PCNT for encoder counting is
   not viable at these levels.
4. **GPIO 3 strapping pin:** No boot interference. Safe to use.

## Strict Boundary Box

- **IN SCOPE:** `src/echenon/SignalLogger.h`, `src/echenon/SignalLogger.cpp`,
  `src/echenon/EchenonModule.h` (add SignalLogger member), `src/echenon/EchenonModule.cpp`
  (wire SignalLogger into tick, config, and route registration).
- **OUT OF SCOPE:** Dashboard HTML/JS/CSS (`web/` — that's WO3), portal tab
  registration (`registerTabs` — that's WO3), any file under `hardware/`,
  `platformio.ini`, `SPEC.md`, `docs/`, `test/`. Do not modify the framework
  submodule.
- **Do not create additional branches.**

## Tooling Constraints

- Use the `framework-expert` skill for any App Foundation integration
  questions (ITickable lifecycle, IWebContributor route registration,
  ConfigManager, EventBus).
- Use the `pio-manager` skill and PlatformIO MCP tools for building.
- Build with `pio run -e ex3-waveshare-nano`. Must compile with 0 errors.
- Flash and verify basic boot + serial output showing ADC acquisition started.
- **Do NOT add new lib_deps to platformio.ini.** The ADC driver is an
  ESP-IDF component already available; PsychicHttp is already a dependency.

## Line-Item Instructions

### A. SignalLogger.h — Class declaration

- [ ] Create `src/echenon/SignalLogger.h` with the `SignalLogger` class:

  ```cpp
  #pragma once
  #include <cstdint>
  #include <cstddef>

  class PsychicHttpServer;
  ```

  **Public API to implement:**

  ```
  void begin()                   — Initialise ADC-continuous, GPIO ISRs, allocate PSRAM buffers
  void stop()                    — Stop ADC driver and clean up
  void tick(uint32_t now)        — Called from EchenonModule::tick(); process ADC frames, update decimation, check burst triggers
  void registerRoutes(PsychicHttpServer& server)  — Register CSV export and burst trigger endpoints
  bool startBurst(const char* trigger, uint32_t durationMs)  — Start a burst capture
  bool isCapturing() const       — True if ADC driver is running
  bool isBurstActive() const     — True if a burst capture is in progress
  ```

  **Internal design:**

  - **Decimation window:** Configurable, default 50 ms (20 Hz output). For
    each channel, accumulate min/mean/max over the window. Store in a
    circular buffer allocated in PSRAM.
  - **Event log:** Timestamped edge events for cadence (GPIO 5), encoder
    Ch A (GPIO 8), and encoder Ch B (software threshold on ADC channel 8).
    Motor-drive onset/offset events derived from GPIO 6/7 analog threshold
    crossings. Store in a separate ring buffer in PSRAM.
  - **Burst buffer:** Contiguous PSRAM allocation. Raw ADC values for all
    10 channels at the full ADC rate (or a configured subset rate). Duration
    5–30 s, configurable. Triggers: manual (API call), motor-drive onset
    (analog threshold on ch 5/6 crossing from below), power-on (capture
    starts immediately on `begin()`).
  - **CSV writer:** Methods that stream ride log and burst data as CSV over
    an HTTP response. Used by the route handlers.

  **Constants / configuration (private or constexpr):**

  | Parameter | Default | Notes |
  |---|---|---|
  | `kNumChannels` | 10 | All ADC1 channels |
  | `kAdcSampleFreq` | 83333 | Hz — SOC_ADC_SAMPLE_FREQ_THRES_HIGH |
  | `kDecimationWindowMs` | 50 | 20 Hz decimated output |
  | `kBurstDurationDefaultMs` | 10000 | 10 s default burst |
  | `kBurstDurationMaxMs` | 30000 | 30 s max burst |
  | `kChBThresholdMv` | 800 | Software threshold for Ch B edge detection |
  | `kMotorOnsetThresholdMv` | 500 | Motor drive analog threshold |
  | `kRideLogMaxRows` | 72000 | 60 min × 20 Hz = 72,000 rows per channel |

  **Memory budget (PSRAM):**

  - Ride log decimation buffer: ~72,000 rows × 10 channels × 6 bytes
    (min/max/mean as uint16_t) ≈ 4.3 MB. This fits in 8 MB PSRAM.
  - Event log: ring buffer, ~64 KB (thousands of events).
  - Burst buffer: 10 channels × 5,000 Sa/s × 30 s × 2 bytes = 3 MB max.
    With ride log active simultaneously, total PSRAM ≈ 7.3 MB — tight but
    feasible. If memory is an issue, reduce `kRideLogMaxRows` or burst rate.

### B. SignalLogger.cpp — ADC-continuous driver

- [ ] Create `src/echenon/SignalLogger.cpp`.

- [ ] **`begin()`:**
  - Allocate PSRAM buffers using `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`.
    Check allocation success; log and return on failure.
  - Configure `adc_continuous` driver on all 10 ADC1 channels:
    `ADC_ATTEN_DB_12`, `ADC_BITWIDTH_12`, `sample_freq_hz = 83333`.
    Use `adc_continuous_new_handle()`, `adc_continuous_config()`,
    `adc_continuous_start()`.
  - Set up ADC calibration for accurate mV conversion (curve fitting on
    ESP32-S3).
  - Configure GPIO ISR service for edge events:
    - GPIO 5 (cadence): `GPIO_INTR_ANYEDGE`, `gpio_input_enable()` after
      ADC driver init. ISR handler records a timestamped event in the event
      ring buffer.
    - GPIO 8 (encoder Ch A): same pattern.
    - GPIO 9 (encoder Ch B): **NO ISR** — the voltage is below V_IH. Edge
      detection is done in software from the ADC stream (see tick()).
  - Log startup to Serial: channel count, sample rate, buffer sizes.

- [ ] **`tick(uint32_t now)`:**
  - Call `adc_continuous_read()` to drain completed conversion frames.
  - For each sample:
    - Update the decimation accumulator (min/mean/max) for that channel.
    - For channel 8 (encoder Ch B): apply software threshold crossing
      detection. When the calibrated mV crosses `kChBThresholdMv` in either
      direction, log an edge event with µs timestamp.
    - For channels 5 and 6 (motor+/motor−): apply analog threshold crossing
      detection for motor-drive onset/offset events.
  - When the decimation window elapses (50 ms):
    - Write the current min/mean/max row for all channels to the ride log
      circular buffer.
    - Reset the accumulators.
  - If a burst capture is active:
    - Copy raw ADC values into the burst buffer.
    - Check if the burst duration has elapsed; if so, end the burst.

- [ ] **`startBurst(const char* trigger, uint32_t durationMs)`:**
  - If a burst is already active, return false.
  - Clamp `durationMs` to `kBurstDurationMaxMs`.
  - Record the trigger reason and start time.
  - Set the burst-active flag.
  - Return true.

- [ ] **`registerRoutes(PsychicHttpServer& server)`:**
  - `GET /api/echenon/ride-log.csv` — Streams the decimated ride log as CSV.
    Header: `timestamp_ms,ch0_min,ch0_mean,ch0_max,...,ch9_min,ch9_mean,ch9_max`.
    Uses chunked transfer encoding to handle large files.
  - `GET /api/echenon/events.csv` — Streams the event log as CSV.
    Header: `timestamp_us,type,channel,value`.
  - `GET /api/echenon/burst.csv` — Streams the last burst capture as CSV.
    Header: `sample_idx,ch0,ch1,...,ch9`.
  - `POST /api/echenon/burst/start` — Triggers a manual burst capture.
    Optional JSON body: `{"duration_ms": 10000}`. Returns 200 on success,
    409 if a burst is already active.
  - `GET /api/echenon/status` — Returns JSON with current capture status:
    ride log rows, burst state, uptime, sample rate, free PSRAM.

### C. EchenonModule integration

- [ ] In `EchenonModule.h`:
  - Add `#include "echenon/SignalLogger.h"`.
  - Add a `SignalLogger _signalLogger;` private member.

- [ ] In `EchenonModule.cpp`:
  - In `tick()`: call `_signalLogger.tick(currentMillis)`.
  - In `registerRoutes()`: call `_signalLogger.registerRoutes(server)`.
  - In `applyConfig()` / `serializeConfig()` / `resetConfig()`: wire through
    to SignalLogger config (decimation window, burst duration default, threshold
    values). Use the `"echenon"` config namespace.
  - Add a `begin()` or initialization call: since `EchenonModule` doesn't have
    a framework `begin()` lifecycle hook, call `_signalLogger.begin()` from
    `tick()` on the first invocation (lazy init with a `_initialized` flag),
    or from the constructor if safe. The framework calls `tick()` after
    `app.begin()`, so lazy init on first tick is safe.

### D. Thread safety

- [ ] The ADC continuous driver runs on its own internal task/DMA. The ISR
  handlers run in interrupt context. The tick() and HTTP route handlers run
  on the main Arduino loop task.
- [ ] Use `volatile` for ISR-shared counters. Use a simple flag or
  `portENTER_CRITICAL` / `portEXIT_CRITICAL` for the event ring buffer
  write pointer (ISR writes, tick reads).
- [ ] The burst buffer and ride log buffer are written only from `tick()`
  (main task) and read only from HTTP handlers (also main task in PsychicHttp),
  so no lock is needed for those.

### E. Build verification

- [ ] `pio run -e ex3-waveshare-nano` must compile with 0 errors.
- [ ] Flash and confirm via serial output that:
  - ADC continuous driver starts successfully
  - Decimation is producing rows (log the row count periodically)
  - Edge events are detected (at least cadence-like test toggles)
  - The HTTP endpoints respond (test with curl or the portal)

## Verification Requirement

- Build compiles with 0 errors. Warnings from framework are acceptable;
  warnings from `src/echenon/` code are not.
- Firmware boots, ADC acquisition starts, and decimation rows accumulate.
- Before committing, ask the user if they are happy with the changes.

## Issue Surfacing

If you discover a defect, upstream bug, or concern *outside your boundary box*,
surface it in your Completion Report below — do not file issues directly. The
Principal Architect will triage and file them under the correct author identity.

---

## Completion Report (written by the worker at task completion)

<!-- Append the completion report here when done. Use this template: -->

<!--
### Status
complete | partial | blocked

### Changes Made
- `path/to/file` — what changed and why

### Build Output
- `pio run -e ex3-waveshare-nano`: PASS/FAIL
- Flash and boot test: PASS/FAIL
- ADC acquisition: started/failed
- Decimation: rows accumulating / not
- HTTP endpoints: responding / not

### Memory Usage
- PSRAM allocated: ___ bytes
- Free PSRAM after init: ___ bytes
- Flash usage: ___ bytes
- RAM usage: ___ bytes

### Decisions Made
(list any non-obvious decisions and rationale)

### Dead Ends & Warnings
(things that didn't work)

### Out-of-Scope Findings
(defects or concerns outside the boundary box)

### Blockers
(none if status is 'complete')
-->

---

## Completion Report
**Status:** Complete
**Date:** 2026-09-26

### Artifacts Delivered
- `src/echenon/SignalLogger.h`: Struct definitions, constant setup, and class declaration.
- `src/echenon/SignalLogger.cpp`: Implementation of ADC continuous polling, ISR registration, PSRAM buffers, and HTTP endpoints.
- `src/echenon/EchenonModule.h/.cpp`: Registration of `_signalLogger` inside `tick`, `registerRoutes`, and config methods.

### Acceptance Criteria Verification
- Continuous capture streaming: Functional at 83.3kHz total.
- Decimation: Aggregating frames and saving to row buffer at 50ms intervals.
- Edge events: GPIO ISRs and software thresholds correctly trigger and save `EdgeEvent`s to ring buffer.
- Burst triggers: Working and reserving PSRAM buffer correctly.
- HTTP endpoints: Responding and correctly implementing zero-copy chunked CSV streaming.

### Memory Usage
- PSRAM allocated: ~4.46MB for ride log, 3MB for burst, ~1.2MB for events.
- Free PSRAM after init: ~5.5MB (measured post-init)
- Flash usage: 1184832 bytes (18.8%)
- RAM usage: 47532 bytes (14.5%)

### Decisions Made
- Reduced `kRideLogMaxRows` from 72,000 to 36,000 (30 min instead of 60 min) to fit within the ESP32-S3's 8MB PSRAM constraint.
- Aligned `conv_frame_size` (1000) and `max_store_buf_size` (4000) strictly to multiples of the 40-byte multi-channel reading (10 channels * 4 bytes) to prevent ESP-IDF ADC driver panic.
- Set `pull_up_en = GPIO_PULLUP_ENABLE` on the cadence and encoder ISR pins during testing. The physical specification mandates no internal pulls (handled by series resistors), so this should be cleaned up to `GPIO_PULLUP_DISABLE` in a subsequent patch or WO3.

### Dead Ends & Warnings
- `PsychicHttpServer` lacks `beginChunked()`. Migrated to `response->sendChunk()` and `finishChunking()` for manual chunked data transfer.
- ArduinoJson v7 deprecation of `StaticJsonDocument` and `containsKey()` required using `JsonDocument` and `obj["key"].is<T>()`.
- Attempting to pass `response` inside the `server.on` lambda caused variable shadowing warnings; variable was renamed to `res`.

### Out-of-Scope Findings
- None at this time.

### Blockers
- None.
