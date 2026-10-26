#include "echenon/CadenceSensor.h"

#if !defined(UNIT_TEST) && defined(ARDUINO)
#include <Arduino.h>
#include <soc/soc_caps.h>
#if defined(SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER) && SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
#include <driver/gpio_filter.h>
#endif

static echenon::CadenceSensor* s_cadenceInstance = nullptr;

void IRAM_ATTR echenon::CadenceSensor::handleISR() {
    if (s_cadenceInstance) {
        s_cadenceInstance->onFallingEdgeISR(static_cast<uint32_t>(micros()));
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
#if defined(SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER) && SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
    if (_glitchFilter != nullptr) {
        auto handle = static_cast<gpio_glitch_filter_handle_t>(_glitchFilter);
        gpio_glitch_filter_disable(handle);
        gpio_del_glitch_filter(handle);
        _glitchFilter = nullptr;
    }
#endif
#endif
}

void CadenceSensor::begin() {
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0) {
        pinMode(_pin, INPUT);
#if defined(SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER) && SOC_GPIO_SUPPORT_PIN_GLITCH_FILTER
        gpio_pin_glitch_filter_config_t filterCfg = {
            .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
            .gpio_num = static_cast<gpio_num_t>(_pin),
        };
        gpio_glitch_filter_handle_t handle = nullptr;
        if (gpio_new_pin_glitch_filter(&filterCfg, &handle) == ESP_OK && handle) {
            gpio_glitch_filter_enable(handle);
            _glitchFilter = static_cast<void*>(handle);
        }
#endif
        s_cadenceInstance = this;
        attachInterrupt(digitalPinToInterrupt(_pin), &CadenceSensor::handleISR, FALLING);
    }
#endif
}

#if !defined(UNIT_TEST) && defined(ARDUINO)
void IRAM_ATTR CadenceSensor::onFallingEdgeISR(uint32_t nowMicros) {
#else
void CadenceSensor::onFallingEdgeISR(uint32_t nowMicros) {
#endif
    if (_waitingForOpen) {
        return;
    }
    if (_hasFirstPulse && static_cast<uint32_t>(nowMicros - _lastPulseMicros) < MIN_HW_INTERVAL_US) {
        return;
    }
    _waitingForOpen = true;
    _highTimingActive = false;
    _highStableStartMicros = 0;
    handlePulse(nowMicros);
}

void CadenceSensor::processHardwareState(uint32_t currentMicros, bool pinIsLow) {
    if (_waitingForOpen) {
        if (pinIsLow) {
            _highTimingActive = false;
        } else {
            if (!_highTimingActive) {
                _highTimingActive = true;
                _highStableStartMicros = currentMicros;
            } else if (static_cast<uint32_t>(currentMicros - _highStableStartMicros) >= REARM_HIGH_US) {
                _waitingForOpen = false;
                _highTimingActive = false;
            }
        }
    }
}

#if !defined(UNIT_TEST) && defined(ARDUINO)
void IRAM_ATTR CadenceSensor::handlePulse(uint32_t pulseMicros) {
#else
void CadenceSensor::handlePulse(uint32_t pulseMicros) {
#endif
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
#if !defined(UNIT_TEST) && defined(ARDUINO)
    if (_pin >= 0) {
        processHardwareState(static_cast<uint32_t>(micros()), digitalRead(_pin) == LOW);
    }
#endif
    if (_newPulse) {
        _newPulse = false;
        _lastPulseMs = currentMillis;
        if (_lastIntervalUs > 0) {
            float calculatedRpm = 60000000.0f / static_cast<float>(_lastIntervalUs);
            if (calculatedRpm > MAX_RPM) {
                calculatedRpm = MAX_RPM;
            } else if (calculatedRpm < 0.0f) {
                calculatedRpm = 0.0f;
            }
            _rpm = calculatedRpm;
        }
    } else if (_hasFirstPulse && static_cast<int32_t>(currentMillis - _lastPulseMs) >= static_cast<int32_t>(TIMEOUT_MS)) {
        // Timed out: 3 seconds without a pulse
        _rpm = 0.0f;
        _newPulse = false;
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
    _waitingForOpen = false;
    _highTimingActive = false;
    _highStableStartMicros = 0;
    _rpm = 0.0f;
}

} // namespace echenon
