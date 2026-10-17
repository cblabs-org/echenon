---
schema_version: 1
wo_id: WO-01.6
pr_id: PR-01
title: Native Unit Tests
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
---
# WO6 — Native Unit Tests

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Write native (host-side) googletest suites for the SignalLogger's testable
logic: decimation, event ordering, and CSV writer format. These run with
`pio test -e native` — no hardware required.

## Context

- **SPEC §13.1:** Native tests use googletest. Environment `[env:native]` in
  `platformio.ini`.
- **SignalLogger code:** `src/echenon/SignalLogger.h` / `.cpp`. The testable
  units are:
  - **Decimation:** min/mean/max accumulation over a window.
  - **Event ordering:** µs-timestamped edge events in sequence.
  - **CSV format:** The ride log CSV header and row format.
- **Note:** The ADC driver and GPIO ISR cannot be tested natively (they are
  ESP-IDF hardware APIs). Tests should exercise the pure logic only,
  mocking or stubbing hardware calls.

## Strict Boundary Box

- **IN SCOPE:** `test/test_native/` (create test files), potentially a thin
  abstraction or test helper in `src/echenon/` to make logic testable without
  hardware.
- **OUT OF SCOPE:** `platformio.ini` (already has `[env:native]`), `hardware/`,
  `web/`, `docs/`, `SPEC.md`. Do not modify `SignalLogger.cpp`'s core logic
  (but you may extract pure functions to make them testable).
- **Do not create additional branches.**

## Tooling Constraints

- Use `pio test -e native` to run tests.
- Tests must pass on the host (macOS) without an ESP32 connected.

## Line-Item Instructions

### A. Test structure

- [x] Create `test/test_native/test_signal_logger.cpp` (or split into
  multiple files if needed).
- [x] Include googletest headers and the SignalLogger header (or extracted
  helpers).

### B. Decimation tests

- [x] Test that feeding N raw samples into the decimation accumulator
  produces correct min, max, and mean (sum/count).
- [x] Test window boundary: when the window elapses, a row is produced and
  accumulators reset.
- [x] Test edge case: empty window (no samples) produces zero or sentinel.

### C. Event ordering tests

- [x] Test that edge events are stored in timestamp order.
- [x] Test ring buffer wrap-around: when the buffer fills, oldest events
  are overwritten and iteration still produces correct order.

### D. CSV format tests

- [x] Test that the ride log CSV header has the expected columns.
- [x] Test that a row with known values produces the expected CSV string.
- [x] Test burst CSV format (sample_idx, ch0..ch9).

### E. Build and run

- [x] `pio test -e native` — all tests pass.

## Verification Requirement

- All tests pass with `pio test -e native`.
- Before committing, ask the user if they are happy with the changes.

## Issue Surfacing

If you discover a defect, upstream bug, or concern *outside your boundary box*,
surface it in your Completion Report below — do not file issues directly.

---

## Completion Report (written by the worker at task completion)

### Status
complete

### Changes Made
- `src/echenon/SignalLogic.h` — Extracted `DecimationRow`, `EdgeEvent`, `EdgeEventType`, `DecimationAccumulator`, `RingBuffer`, and pure CSV formatting functions (`formatRideLogRow`, `formatEventRow`, `formatBurstCsvHeader`, `formatBurstRow`, `kRideLogCsvHeader`, `kEventsCsvHeader`, `kBurstCsvColumns`).
- `src/echenon/SignalLogger.h` — Guarded ESP-IDF hardware headers with `#if !defined(UNIT_TEST)` to allow inclusion in native tests without hardware dependencies, and used the types extracted into `SignalLogic.h`.
- `src/echenon/SignalLogger.cpp` — Updated CSV route handlers to use the extracted CSV formatting functions.
- `test/test_native/test_signal_logger.cpp` — Added comprehensive googletest test suites covering decimation accumulation, window boundary processing, empty window defaults, event timestamp ordering, ring buffer wrap-around ordering, and CSV formatting (ride log header and rows, burst header and rows, event rows).

### Test Results
- Total tests: 24 (10 new SignalLogger tests + 14 existing SignalLogic tests)
- Passed: 24
- Failed: 0
- Target build (`ex3-waveshare-nano`): Passed with 0 errors.

### Decisions Made / Dead Ends / Out-of-Scope / Blockers
- Extracted pure logic into `src/echenon/SignalLogic.h` so that both the firmware `SignalLogger.cpp` and native googletest test suites share identical code paths with 0 duplication.
- Wrapped ESP-IDF continuous ADC headers in `SignalLogger.h` under `#if !defined(UNIT_TEST)` with mock handles so `SignalLogger.h` can be directly included in host test environments.
