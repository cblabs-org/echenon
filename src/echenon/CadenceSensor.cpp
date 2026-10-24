#include "echenon/CadenceSensor.h"

#if !defined(UNIT_TEST) && defined(ARDUINO)
#include <Arduino.h>

static echenon::CadenceSensor* s_cadenceInstance = nullptr;

void IRAM_ATTR echenon::CadenceSensor::handleISR() {
    if (s_cadenceInstance) {
        s_cadenceInstance->handlePulse(static_cast<uint32_t>(micros()));
    }
}
#endif

namespace echenon {

CadenceSensor::CadenceSensor(int8_t pin)
    : _pin(pin) {}

CadenceSensor::~CadenceSensor() {
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0 && s_cadenceInstance == this) {
        detachInterrupt(digitalPinToInterrupt(_pin));
        s_cadenceInstance = nullptr;
    }
#endif
}

void CadenceSensor::begin() {
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0) {
        pinMode(_pin, INPUT);
        s_cadenceInstance = this;
        attachInterrupt(digitalPinToInterrupt(_pin), &CadenceSensor::handleISR, FALLING);
    }
#endif
}

void CadenceSensor::handlePulse(uint32_t pulseMicros) {
    if (_hasFirstPulse) {
        uint32_t dt = pulseMicros - _lastPulseMicros;
        if (dt < DEBOUNCE_LOCKOUT_US) {
            return; // Contact bounce lockout (25 ms)
        }
        if (dt <= TIMEOUT_US) {
            _lastIntervalUs = dt;
            _newPulse = true;
        }
    }
    _lastPulseMicros = pulseMicros;
    _lastPulseMs = pulseMicros / 1000;
    _hasFirstPulse = true;
}

void CadenceSensor::tick(uint32_t currentMillis) {
    if (_hasFirstPulse && (currentMillis - _lastPulseMs >= TIMEOUT_MS)) {
        // Timed out: 3 seconds without a pulse
        _rpm = 0.0f;
        _newPulse = false;
    } else if (_newPulse) {
        _newPulse = false;
        if (_lastIntervalUs > 0) {
            float calculatedRpm = 60000000.0f / static_cast<float>(_lastIntervalUs);
            if (calculatedRpm > MAX_RPM) {
                calculatedRpm = MAX_RPM;
            } else if (calculatedRpm < 0.0f) {
                calculatedRpm = 0.0f;
            }
            _rpm = calculatedRpm;
        }
    }
}

void CadenceSensor::injectPulse(uint32_t pulseMicros) {
    handlePulse(pulseMicros);
}

void CadenceSensor::reset() {
    _lastPulseMicros = 0;
    _lastPulseMs = 0;
    _lastIntervalUs = 0;
    _newPulse = false;
    _hasFirstPulse = false;
    _rpm = 0.0f;
}

} // namespace echenon
