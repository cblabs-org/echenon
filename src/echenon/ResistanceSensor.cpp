/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/ResistanceSensor.cpp
 * Description: Implementation of the potentiometer ADC resistance sensor and EMA filter.
 */

#include "echenon/ResistanceSensor.h"
#include "echenon/LevelMapping.h"
#include <cmath>

#if !defined(UNIT_TEST) && defined(ARDUINO)
#include <Arduino.h>
#endif

namespace echenon {

ResistanceSensor::ResistanceSensor(int8_t pin)
    : _pin(pin), _rawWiper(0), _filteredWiper(0.0f), _initialized(false) {}

void ResistanceSensor::begin() {
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0) {
        pinMode(_pin, INPUT);
    }
#endif
}

void ResistanceSensor::tick(uint32_t currentMillis) {
    (void)currentMillis;
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0) {
        uint16_t sample = static_cast<uint16_t>(analogRead(_pin));
        _rawWiper = sample;
        applySample(sample);
    }
#endif
}

void ResistanceSensor::applySample(uint16_t sample) {
    if (!_initialized) {
        _filteredWiper = static_cast<float>(sample);
        _initialized = true;
    } else {
        _filteredWiper = DEFAULT_ALPHA * static_cast<float>(sample) + (1.0f - DEFAULT_ALPHA) * _filteredWiper;
    }
}

uint16_t ResistanceSensor::getFilteredWiper() const {
    if (!_initialized) {
        return 0;
    }
    return static_cast<uint16_t>(std::round(_filteredWiper));
}

bool ResistanceSensor::isInWorkingRange() const {
    uint16_t filtered = getFilteredWiper();
    return (filtered >= LevelMapping::WORKING_RANGE_MIN && filtered <= LevelMapping::WORKING_RANGE_MAX);
}

void ResistanceSensor::injectSample(uint16_t adcValue) {
    _rawWiper = adcValue;
    applySample(adcValue);
}

void ResistanceSensor::reset() {
    _rawWiper = 0;
    _filteredWiper = 0.0f;
    _initialized = false;
}

void ResistanceSensor::reset(float initialFiltered) {
    _rawWiper = static_cast<uint16_t>(initialFiltered);
    _filteredWiper = initialFiltered;
    _initialized = true;
}

} // namespace echenon
