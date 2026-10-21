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
