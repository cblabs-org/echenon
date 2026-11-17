#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/CalibrationRig.h
 * Description: Automated hardware test rig driving quadrature resistance steps and simulated cadence pulses.
 */

#ifdef WITH_CALIBRATION_RIG

#include <cstdint>
#include <cstddef>
#include <Arduino.h>
#include "echenon/QuadratureGenerator.h"

namespace echenon {

/**
 * @brief Automated calibration rig state machine simulating knob quadrature
 *        steps and reed-switch cadence pulses while monitoring pot settle.
 */
class CalibrationRig {
public:
    /// Rig operating states.
    enum class State {
        IDLE,     ///< Rig inactive
        SEEKING,  ///< Stepping quadrature output toward target level
        SETTLED,  ///< Potentiometer settled within tolerance (or timed out)
        ERROR     ///< Rig error state
    };

    /// Construct an uninitialized CalibrationRig.
    CalibrationRig();

    /// Initialize quadrature encoder and cadence simulation GPIO pins.
    void begin(int8_t encA, int8_t encB, int8_t cadencePin);

    /// Command the rig to seek to targetLevel (1-32) and simulate cadenceRPM.
    void handleSet(uint8_t targetLevel, uint16_t cadenceRPM);
    /// Command the rig to return to level 1 with zero cadence.
    void handleHome();
    /// Halt stepping and cadence simulation immediately, returning to IDLE.
    void handleStop();
    /// Format and return the rig's current status summary string.
    String handleStatus();

    /// Parse and execute a single text command (from Serial, MCP, or tests).
    void handleCommand(const String& cmd);

    /// Advance quadrature stepping, cadence toggling, and pot settle checks.
    void tick(uint32_t currentMillis, uint16_t potAdc);

    /// Return the current rig state.
    State getState() const { return _state; }
    /// Return the estimated current console resistance level (1-32).
    uint8_t getCurrentLevel() const { return _currentLevel; }
    /// Return the commanded target resistance level (1-32).
    uint16_t getTargetLevel() const { return _targetLevel; }
    /// Return the simulated cadence setpoint in RPM.
    uint16_t getCadenceRPM() const { return _cadenceRPM; }
    /// Return the number of quadrature steps remaining to reach target.
    int32_t getStepsRemaining() const { return _stepsRemaining; }
    /// Return true if the most recent settle phase completed via timeout.
    bool didSettleTimeout() const { return _settleTimedOut; }

private:
    // Quadrature generation
    QuadratureGenerator _encoder;
    uint32_t _lastStepMs = 0;
    static constexpr uint32_t STEP_INTERVAL_MS = 150; ///< Interval between quadrature edges (ms)
    static constexpr int32_t  STEPS_PER_LEVEL  = 8;   ///< Bench-verified: 8 quadrature edges per console level (A08 docs 16 PCNT steps, but empirical 8 works with 200ms step interval)

    // Cadence simulation
    int8_t _cadencePin = -1;
    int8_t _encPinA = -1;
    int8_t _encPinB = -1;
    uint16_t _cadenceRPM = 0;
    uint32_t _cadencePeriodMs = 0;
    uint32_t _lastCadenceToggle = 0;
    bool _cadencePinHigh = true;
    static constexpr uint32_t CADENCE_PULSE_MS = 30;  ///< Simulated LOW cadence pulse width (ms)

    // Pot settle monitoring
    static constexpr uint16_t SETTLE_TOLERANCE = 20;    ///< ADC settle tolerance window (counts; Phase 3 level spacing ~37 ADC)
    static constexpr uint32_t SETTLE_DURATION_MS = 500; ///< Required stable duration in ms
    static constexpr uint32_t SETTLE_TIMEOUT_MS = 30000; ///< Settle timeout before transition to SETTLED with warning (ms)
    uint32_t _settleStartMs = 0;
    uint32_t _settlePhaseStartMs = 0;  // when steps_remaining hit 0
    bool _settling = false;
    bool _settleTimedOut = false;
    uint16_t _lastPotAdc = 0;

    // State
    State _state = State::IDLE;
    uint8_t _currentLevel = 1;
    uint8_t _targetLevel = 1;
    int32_t _stepsRemaining = 0;  // encoder steps to reach target
    QuadratureGenerator::Direction _stepDirection = QuadratureGenerator::Direction::UP;

    // B03 fit for expected pot value
    uint16_t expectedPot(uint8_t level) const;

    // Serial input buffer
    String _serialBuffer;
    void processSerialInput();
};

} // namespace echenon

#endif // WITH_CALIBRATION_RIG
