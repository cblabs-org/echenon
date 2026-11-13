/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/CalibrationWizard.cpp
 * Description: Implementation of the 32-level passive resistance calibration wizard and OLS endpoint fit.
 */

#include "echenon/CalibrationWizard.h"
#include "echenon/ResistanceSensor.h"
#include <cstring>

namespace echenon {

CalibrationWizard::CalibrationWizard(ResistanceSensor* sensor)
    : _sensor(sensor),
      _state(State::IDLE),
      _currentStep(0),
      _isComplete(false) {
    std::memset(_table, 0, sizeof(_table));
}

void CalibrationWizard::start() {
    _state = State::STARTED;
    _currentStep = 1;
    _isComplete = false;
}

void CalibrationWizard::stop() {
    _state = State::IDLE;
    _currentStep = 0;
}

bool CalibrationWizard::recordLevel(uint8_t consoleLevel) {
    uint16_t wiper = _sensor ? _sensor->getFilteredWiper() : 0;
    return recordLevel(consoleLevel, wiper);
}

bool CalibrationWizard::recordLevel(uint8_t consoleLevel, uint16_t wiperCount) {
    if (!isActive()) {
        return false;
    }
    if (consoleLevel < 1 || consoleLevel > TOTAL_LEVELS) {
        return false;
    }
    if (consoleLevel != _currentStep) {
        return false;
    }

    _table[consoleLevel - 1] = wiperCount;

    if (_currentStep < TOTAL_LEVELS) {
        _state = State::RECORDING;
        _currentStep++;
    } else {
        _state = State::COMPLETED;
        _isComplete = true;
    }
    updateFittedEndpoints();
    return true;
}

uint16_t CalibrationWizard::getTableEntry(uint8_t level) const {
    if (level < 1 || level > TOTAL_LEVELS) {
        return 0;
    }
    return _table[level - 1];
}

void CalibrationWizard::setTableEntry(uint8_t level, uint16_t wiper) {
    if (level >= 1 && level <= TOTAL_LEVELS) {
        _table[level - 1] = wiper;
        updateFittedEndpoints();
    }
}

bool CalibrationWizard::isPosDone() const {
    // Check all 32 entries populated and at least one differs from default
    bool anyDiffers = false;
    for (uint8_t l = 1; l <= TOTAL_LEVELS; ++l) {
        if (_table[l - 1] == 0) {
            return false;  // incomplete table
        }
        // Default linear interpolation: min + (max - min) * (l - 1) / 31
        uint16_t defaultVal = static_cast<uint16_t>(
            LevelMapping::WORKING_RANGE_MIN +
            (LevelMapping::WORKING_RANGE_MAX - LevelMapping::WORKING_RANGE_MIN) *
            (l - 1) / (TOTAL_LEVELS - 1));
        int16_t diff = static_cast<int16_t>(_table[l - 1]) - static_cast<int16_t>(defaultVal);
        if (diff > 2 || diff < -2) {
            anyDiffers = true;
        }
    }
    return anyDiffers;
}

bool CalibrationWizard::fitLinearEndpoints(float& outPotMin, float& outPotMax) const {
    const float defaultMin = static_cast<float>(LevelMapping::WORKING_RANGE_MIN);
    const float defaultMax = static_cast<float>(LevelMapping::WORKING_RANGE_MAX);

    if (!isPosDone()) {
        outPotMin = defaultMin;
        outPotMax = defaultMax;
        return false;
    }

    int count = 0;
    float sumL = 0.0f;
    float sumW = 0.0f;
    for (uint8_t l = 1; l <= TOTAL_LEVELS; ++l) {
        uint16_t w = _table[l - 1];
        if (w > 0) {
            sumL += static_cast<float>(l);
            sumW += static_cast<float>(w);
            ++count;
        }
    }

    if (count < 2) {
        outPotMin = defaultMin;
        outPotMax = defaultMax;
        return false;
    }

    const float meanL = sumL / static_cast<float>(count);
    const float meanW = sumW / static_cast<float>(count);

    float num = 0.0f;
    float den = 0.0f;
    for (uint8_t l = 1; l <= TOTAL_LEVELS; ++l) {
        uint16_t w = _table[l - 1];
        if (w > 0) {
            const float dL = static_cast<float>(l) - meanL;
            const float dW = static_cast<float>(w) - meanW;
            num += dL * dW;
            den += dL * dL;
        }
    }

    if (den <= 0.0f) {
        outPotMin = defaultMin;
        outPotMax = defaultMax;
        return false;
    }

    const float slope = num / den;
    const float intercept = meanW - slope * meanL;
    const float fittedMin = intercept + slope * 1.0f;
    const float fittedMax = intercept + slope * 32.0f;

    if (!std::isfinite(fittedMin) || !std::isfinite(fittedMax) || fittedMax <= fittedMin) {
        outPotMin = defaultMin;
        outPotMax = defaultMax;
        return false;
    }

    outPotMin = fittedMin;
    outPotMax = fittedMax;
    return true;
}

void CalibrationWizard::updateFittedEndpoints() {
    fitLinearEndpoints(_potMin, _potMax);
}

void CalibrationWizard::reset() {
    _state = State::IDLE;
    _currentStep = 0;
    _isComplete = false;
    std::memset(_table, 0, sizeof(_table));
    updateFittedEndpoints();
}

} // namespace echenon
