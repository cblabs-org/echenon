#pragma once

#ifdef WITH_CALIBRATION_RIG

#include <cstdint>
#include <cstddef>
#include <Arduino.h>
#include "echenon/QuadratureGenerator.h"

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

    // Process a single text command (e.g. from Serial or test)
    void handleCommand(const String& cmd);

    // Called from tick — drives the state machine
    void tick(uint32_t currentMillis, uint16_t potAdc);

    State getState() const { return _state; }
    uint8_t getCurrentLevel() const { return _currentLevel; }
    uint16_t getTargetLevel() const { return _targetLevel; }
    uint16_t getCadenceRPM() const { return _cadenceRPM; }
    int32_t getStepsRemaining() const { return _stepsRemaining; }
    bool didSettleTimeout() const { return _settleTimedOut; }

private:
    // Quadrature generation
    QuadratureGenerator _encoder;
    uint32_t _lastStepMs = 0;
    static constexpr uint32_t STEP_INTERVAL_MS = 150;
    static constexpr int32_t  STEPS_PER_LEVEL  = 8;   // bench-verified: 8 quadrature edges per console level (A08 docs 16 PCNT steps, but empirical 8 works with 200ms step interval)

    // Cadence simulation
    int8_t _cadencePin = -1;
    int8_t _encPinA = -1;
    int8_t _encPinB = -1;
    uint16_t _cadenceRPM = 0;
    uint32_t _cadencePeriodMs = 0;
    uint32_t _lastCadenceToggle = 0;
    bool _cadencePinHigh = true;
    static constexpr uint32_t CADENCE_PULSE_MS = 30;  // LOW pulse width

    // Pot settle monitoring
    static constexpr uint16_t SETTLE_TOLERANCE = 20;   // ADC counts (Phase 3: level spacing ~37 ADC)
    static constexpr uint32_t SETTLE_DURATION_MS = 500; // must be stable for 500 ms
    static constexpr uint32_t SETTLE_TIMEOUT_MS = 30000; // 30s timeout → SETTLED with warning
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
