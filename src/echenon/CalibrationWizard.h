#pragma once

#include <cstdint>
#include <cstddef>
#include "echenon/LevelMapping.h"

namespace echenon {

class ResistanceSensor;

/**
 * @brief State machine for passive position calibration (SPEC §6.3).
 *
 * Steps through levels 1–32 while recording the filtered wiper counts
 * from ResistanceSensor at each level.
 */
class CalibrationWizard {
public:
    static constexpr uint8_t TOTAL_LEVELS = 32;

    enum class State : uint8_t {
        IDLE = 0,
        STARTED = 1,
        RECORDING = 2,
        COMPLETED = 3
    };

    explicit CalibrationWizard(ResistanceSensor* sensor = nullptr);
    ~CalibrationWizard() = default;

    void setResistanceSensor(ResistanceSensor* sensor) { _sensor = sensor; }

    // Start calibration, moving state to STARTED (awaiting level 1)
    void start();

    // Stop / cancel calibration, moving state to IDLE
    void stop();

    // Record the current filtered wiper count at consoleLevel (1–32) using attached sensor
    bool recordLevel(uint8_t consoleLevel);

    // Record wiper count directly (for testing or direct injection)
    bool recordLevel(uint8_t consoleLevel, uint16_t wiperCount);

    bool isActive() const { return _state == State::STARTED || _state == State::RECORDING; }
    uint8_t getCurrentStep() const { return _currentStep; }
    bool isComplete() const { return _isComplete; }
    State getState() const { return _state; }

    const uint16_t* getTable() const { return _table; }
    uint16_t getTableEntry(uint8_t level) const; // 1-indexed (1..32)
    void setTableEntry(uint8_t level, uint16_t wiper);

    bool isPosDone() const { return _posDone; }
    void setPosDone(bool done);

    bool fitLinearEndpoints(float& outPotMin, float& outPotMax) const;
    float getPotMin() const { return _potMin; }
    float getPotMax() const { return _potMax; }

    float wiperToNormalized(float wiper) const {
        return LevelMapping::wiperToNormalized(wiper, _potMin, _potMax);
    }
    float wiperToLevel(float wiper) const {
        return LevelMapping::wiperToLevel(wiper, _potMin, _potMax);
    }
    uint8_t wiperToEchelon(float wiper) const {
        return LevelMapping::wiperToEchelon(wiper, _potMin, _potMax);
    }

    void reset();

private:
    void updateFittedEndpoints();

    ResistanceSensor* _sensor = nullptr;
    State _state = State::IDLE;
    uint8_t _currentStep = 0;
    bool _isComplete = false;
    bool _posDone = false;
    uint16_t _table[TOTAL_LEVELS] = {};
    float _potMin = static_cast<float>(LevelMapping::WORKING_RANGE_MIN);
    float _potMax = static_cast<float>(LevelMapping::WORKING_RANGE_MAX);
};

} // namespace echenon
