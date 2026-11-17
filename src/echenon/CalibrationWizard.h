#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/CalibrationWizard.h
 * Description: 32-level passive resistance potentiometer calibration wizard and linear endpoint fitter.
 */

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
    static constexpr uint8_t TOTAL_LEVELS = 32; ///< Number of discrete Echelon resistance levels (1-32)

    /// Calibration wizard lifecycle states.
    enum class State : uint8_t {
        IDLE = 0,      ///< Calibration inactive
        STARTED = 1,   ///< Calibration started, awaiting level 1
        RECORDING = 2, ///< Recording levels 1-32 sequentially
        COMPLETED = 3  ///< All 32 levels recorded
    };

    /// Construct a CalibrationWizard optionally bound to a ResistanceSensor.
    explicit CalibrationWizard(ResistanceSensor* sensor = nullptr);
    ~CalibrationWizard() = default;

    /// Attach or replace the ResistanceSensor used by single-arg recordLevel().
    void setResistanceSensor(ResistanceSensor* sensor) { _sensor = sensor; }

    /// Start calibration, moving state to STARTED (awaiting level 1).
    void start();

    /// Stop or cancel calibration, moving state to IDLE.
    void stop();

    /// Record the current filtered wiper count at consoleLevel (1–32) using attached sensor.
    bool recordLevel(uint8_t consoleLevel);

    /// Record wiper count directly at consoleLevel (1-32) for testing or direct injection.
    bool recordLevel(uint8_t consoleLevel, uint16_t wiperCount);

    /// Return true while calibration is in STARTED or RECORDING state.
    bool isActive() const { return _state == State::STARTED || _state == State::RECORDING; }
    /// Return the 1-indexed next expected level step (0 when idle).
    uint8_t getCurrentStep() const { return _currentStep; }
    /// Return true once all 32 levels have been recorded.
    bool isComplete() const { return _isComplete; }
    /// Return the current wizard state.
    State getState() const { return _state; }

    /// Return pointer to the 32-entry wiper calibration table.
    const uint16_t* getTable() const { return _table; }
    /// Return the recorded wiper count for 1-indexed level (1..32), or 0 if out of range.
    uint16_t getTableEntry(uint8_t level) const;
    /// Set the wiper count for 1-indexed level (1..32) and update fitted endpoints.
    void setTableEntry(uint8_t level, uint16_t wiper);

    /// Return true if all 32 entries are populated and differ from the stock default curve.
    bool isPosDone() const;

    /// Fit ordinary least-squares pot endpoints (potMin, potMax) across recorded entries.
    bool fitLinearEndpoints(float& outPotMin, float& outPotMax) const;
    /// Return the fitted minimum potentiometer count (level 1).
    float getPotMin() const { return _potMin; }
    /// Return the fitted maximum potentiometer count (level 32).
    float getPotMax() const { return _potMax; }

    /// Map a wiper reading to normalized cam position [0.0, 1.0] using fitted endpoints.
    float wiperToNormalized(float wiper) const {
        return LevelMapping::wiperToNormalized(wiper, _potMin, _potMax);
    }
    /// Map a wiper reading to continuous Echelon level [1.0, 32.0] using fitted endpoints.
    float wiperToLevel(float wiper) const {
        return LevelMapping::wiperToLevel(wiper, _potMin, _potMax);
    }
    /// Map a wiper reading to integer Echelon level [1..32] using fitted endpoints.
    uint8_t wiperToEchelon(float wiper) const {
        return LevelMapping::wiperToEchelon(wiper, _potMin, _potMax);
    }

    /// Clear calibration table, reset state to IDLE, and restore default pot endpoints.
    void reset();

private:
    void updateFittedEndpoints();

    ResistanceSensor* _sensor = nullptr;
    State _state = State::IDLE;
    uint8_t _currentStep = 0;
    bool _isComplete = false;
    uint16_t _table[TOTAL_LEVELS] = {};
    float _potMin = static_cast<float>(LevelMapping::WORKING_RANGE_MIN);
    float _potMax = static_cast<float>(LevelMapping::WORKING_RANGE_MAX);
};

} // namespace echenon
