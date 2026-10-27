---
schema_version: 1
wo_id: WO-02.5
pr_id: PR-02
title: CalibrationRig Firmware Module
complexity: standard
status: IN_PROGRESS
allowed_files:
- src/echenon/CalibrationRig.h
- src/echenon/CalibrationRig.cpp
- src/echenon/QuadratureGenerator.h
- src/echenon/QuadratureGenerator.cpp
- src/echenon/EchenonModule.h
- src/echenon/EchenonModule.cpp
- src/main.cpp
- hardware/ex3-waveshare-nano/board.h
- platformio.ini
- test/test_native/test_quadrature_generator.cpp
forbidden_files: []
required_skills:
- /Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md
verification_command: pio test -e native && pio run -e ex3-waveshare-nano && pio run
  -e ex3-waveshare-nano-rig
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-02.5 — CalibrationRig Firmware Module

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo5-rig-firmware` from `calibration-power/wo4-ws-feed`
> **Charter sections:** §8 (firmware portion) of the PR-02 Work Order Ledger

## 1. Objective

Build the `CalibrationRig` module behind `WITH_CALIBRATION_RIG`: encoder
quadrature synthesis, cadence reed-switch simulation, pot monitoring for
motor settle, and the serial command protocol. Add the rig-specific build
environment.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/CalibrationRig.h` — new
- `src/echenon/CalibrationRig.cpp` — new
- `src/echenon/QuadratureGenerator.h` — new
- `src/echenon/QuadratureGenerator.cpp` — new
- `src/echenon/EchenonModule.h` — modify (add CalibrationRig member under `#ifdef WITH_CALIBRATION_RIG`)
- `src/echenon/EchenonModule.cpp` — modify (tick CalibrationRig, register routes if needed)
- `src/main.cpp` — modify (register CalibrationRig if needed)
- `hardware/ex3-waveshare-nano/board.h` — modify (add `PIN_RIG_*` under `#ifdef WITH_CALIBRATION_RIG`)
- `platformio.ini` — modify (add `ex3-waveshare-nano-rig` env)
- `test/test_native/test_quadrature_generator.cpp` — new

### Out of scope
- Host script (WO-02.6)
- Rig sweep (WO-02.7)
- Runaway detection (WO-02.8)

## 3. Specification

### 3.1 board.h Additions

Add under `#ifdef WITH_CALIBRATION_RIG`:

```cpp
#ifdef WITH_CALIBRATION_RIG
// Calibration rig output pins (owner decision 2026-10-18)
constexpr int8_t PIN_RIG_ENC_A    = 17;  // D8  → console encoder Ch A
constexpr int8_t PIN_RIG_ENC_B    = 18;  // D12 → console encoder Ch B
constexpr int8_t PIN_RIG_CADENCE  = 11;  // D9  → cadence MOSFET gate
#endif
```

### 3.2 QuadratureGenerator

A state machine that generates Gray-code quadrature on two GPIOs.

```cpp
namespace echenon {

class QuadratureGenerator {
public:
    enum class Direction { UP, DOWN };

    // Constructor takes two GPIO pins
    QuadratureGenerator(int8_t pinA, int8_t pinB);

    void begin();  // Configure GPIOs as OUTPUT

    // Advance one quadrature state (one edge)
    // Call repeatedly at ~50 ms intervals for ~200 ms per detent
    void step(Direction dir);

    // Get current quadrature state (0-3)
    uint8_t getState() const { return _state; }

    // Get total step count since begin
    int32_t getTotalSteps() const { return _totalSteps; }

    // Reset state to 0
    void reset();

    // Test helper: get the expected pin levels for a given state
    static void getPinLevels(uint8_t state, bool& pinA, bool& pinB);

private:
    void applyState();  // Write current state to GPIOs

    int8_t _pinA, _pinB;
    uint8_t _state = 0;       // 0-3 Gray code state
    int32_t _totalSteps = 0;
};

} // namespace echenon
```

**Gray code sequence (4 states per detent):**

| State | Pin A | Pin B |
|-------|-------|-------|
| 0     | LOW   | LOW   |
| 1     | HIGH  | LOW   |
| 2     | HIGH  | HIGH  |
| 3     | LOW   | HIGH  |

- UP direction: 0→1→2→3→0→...
- DOWN direction: 0→3→2→1→0→...
- 4 detents × 4 steps = 16 edges per level
- ~50 ms between state transitions = ~200 ms per detent, ~800 ms per level

### 3.3 CalibrationRig

The main rig controller module.

```cpp
namespace echenon {

class CalibrationRig {
public:
    enum class State { IDLE, SEEKING, SETTLED, ERROR };

    CalibrationRig();

    void begin(int8_t encA, int8_t encB, int8_t cadencePin);

    // Serial command handlers
    void handleSet(uint8_t targetLevel, uint16_t cadenceRPM);
    void handleHome();
    void handleStop();
    String handleStatus();

    // Called from tick — drives the state machine
    void tick(uint32_t currentMillis, uint16_t potAdc);

    State getState() const { return _state; }
    uint8_t getCurrentLevel() const { return _currentLevel; }
    uint16_t getTargetLevel() const { return _targetLevel; }

private:
    // Quadrature generation
    QuadratureGenerator _encoder;
    uint32_t _lastStepMs = 0;
    static constexpr uint32_t STEP_INTERVAL_MS = 50;

    // Cadence simulation
    int8_t _cadencePin = -1;
    uint16_t _cadenceRPM = 0;
    uint32_t _cadencePeriodMs = 0;
    uint32_t _lastCadenceToggle = 0;
    bool _cadencePinHigh = true;
    static constexpr uint32_t CADENCE_PULSE_MS = 30;  // LOW pulse width

    // Pot settle monitoring
    static constexpr uint16_t SETTLE_TOLERANCE = 15;   // ADC counts
    static constexpr uint32_t SETTLE_DURATION_MS = 500; // must be stable for 500 ms
    uint32_t _settleStartMs = 0;
    bool _settling = false;

    // State
    State _state = State::IDLE;
    uint8_t _currentLevel = 1;
    uint8_t _targetLevel = 1;
    int32_t _stepsRemaining = 0;  // encoder steps to reach target

    // B03 fit for expected pot value
    uint16_t expectedPot(uint8_t level) const;
};

} // namespace echenon
```

**Serial command protocol (parsed in EchenonModule or CalibrationRig):**
- `SET level=<1-32> cadence=<0-200>` → seek to level, set cadence
- `HOME` → return to level 1 (31 × 16 = 496 reverse steps)
- `STATUS` → report pot ADC, derived level, cadence, state
- `STOP` → stop cadence, hold encoder

**Responses:**
- `SETTLED level=N pot=ADC cadence=RPM`
- `SEEKING level=N pot=ADC`
- `ERROR <message>`

**Serial parsing:** Read from `Serial` in `tick()`. Parse simple text
commands. Respond on `Serial`.

### 3.4 Cadence Simulation

- Toggle the cadence GPIO LOW for 30 ms then HIGH for the remainder of the
  period: `period_ms = 60000 / RPM`.
- RPM = 0 → no toggling, pin stays HIGH.
- Supports 0–200 RPM.
- Use `digitalWrite()` on the target; guard behind `#ifndef UNIT_TEST`.

### 3.5 Pot Settle Monitor

- Compare the live pot ADC (from ResistanceSensor or passed directly) to the
  expected value from B03 fit: `expected = 39.1 × level + 97`.
- Settled when `|actual − expected| ≤ 15 counts` for 500 ms continuously.
- Used to determine when the console's motor has finished seeking.

### 3.6 platformio.ini Changes

Add a new environment:

```ini
; ── Calibration rig env ──
[env:ex3-waveshare-nano-rig]
extends = env:ex3-waveshare-nano
build_flags =
    ${env:ex3-waveshare-nano.build_flags}
    -DWITH_CALIBRATION_RIG
```

The `WITH_CALIBRATION_RIG` flag is NOT added to the production
`ex3-waveshare-nano` env. The rig env is a separate build target.

### 3.7 EchenonModule Integration

Under `#ifdef WITH_CALIBRATION_RIG`:
- Add CalibrationRig member
- In `tick()`: call `_calibrationRig.tick()` with current potAdc
- In `registerRoutes()`: optionally add a `/api/echenon/rig/status` endpoint
  (nice-to-have, not required — serial is the primary interface)

### 3.8 Native Tests (test_quadrature_generator.cpp)

Test the QuadratureGenerator state machine (no GPIO, just state tracking):
1. **Initial state**: state = 0
2. **Forward sequence**: step UP 4 times → states 1, 2, 3, 0 (one full detent)
3. **Reverse sequence**: step DOWN 4 times → states 3, 2, 1, 0
4. **Direction reversal**: step UP twice, then DOWN twice → returns to start
5. **Level count**: 16 steps UP = 1 full level (4 detents × 4 states)
6. **Pin levels**: verify getPinLevels matches the Gray code table
7. **Total step tracking**: verify totalSteps increments correctly

## 4. Verification

1. `pio test -e native` — all native tests pass
2. `pio run -e ex3-waveshare-nano` — production build compiles (WITHOUT `WITH_CALIBRATION_RIG`)
3. `pio run -e ex3-waveshare-nano-rig` — rig build compiles (WITH `WITH_CALIBRATION_RIG`)

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- Framework submodule drift — NEVER commit
- The `WITH_CALIBRATION_RIG` build and `ex3-waveshare-nano-rig` env are
  excluded from the production build
- Known gotchas: `<cstddef>` before BOARD_HEADER; Arduino `#define DEFAULT 1`
## Completion Report

_No completion report recorded._
