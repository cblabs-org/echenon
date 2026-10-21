#ifdef WITH_CALIBRATION_RIG

#include "echenon/CalibrationRig.h"
#include "echenon/LevelMapping.h"
#include <cmath>
#include <cstdlib>

namespace echenon {

CalibrationRig::CalibrationRig()
    : _encoder(-1, -1) {
}

void CalibrationRig::begin(int8_t encA, int8_t encB, int8_t cadencePin) {
    _cadencePin = cadencePin;
    _encoder.begin(encA, encB);
#ifndef UNIT_TEST
    if (_cadencePin >= 0) {
        pinMode(_cadencePin, OUTPUT);
        digitalWrite(_cadencePin, HIGH);
    }
#endif
    _cadencePinHigh = true;
    _state = State::IDLE;
    _currentLevel = 1;
    _targetLevel = 1;
    _stepsRemaining = 0;
    _cadenceRPM = 0;
    _cadencePeriodMs = 0;
    _lastCadenceToggle = 0;
    _lastStepMs = 0;
    _settling = false;
    _serialBuffer = "";
}

uint16_t CalibrationRig::expectedPot(uint8_t level) const {
    float exp = LevelMapping::B03_SLOPE * static_cast<float>(level) + LevelMapping::B03_INTERCEPT;
    if (exp < 0.0f) exp = 0.0f;
    return static_cast<uint16_t>(roundf(exp));
}

void CalibrationRig::handleSet(uint8_t targetLevel, uint16_t cadenceRPM) {
    if (targetLevel < 1) targetLevel = 1;
    if (targetLevel > 32) targetLevel = 32;

    _targetLevel = targetLevel;
    _cadenceRPM = cadenceRPM;
    if (_cadenceRPM > 0) {
        _cadencePeriodMs = 60000 / _cadenceRPM;
    } else {
        _cadencePeriodMs = 0;
    }
    _cadencePinHigh = true;
#ifndef UNIT_TEST
    if (_cadencePin >= 0) {
        digitalWrite(_cadencePin, HIGH);
    }
#endif

    if (_targetLevel > _currentLevel) {
        _stepsRemaining = (_targetLevel - _currentLevel) * 16;
        _stepDirection = QuadratureGenerator::Direction::UP;
        _state = State::SEEKING;
        _settling = false;
    } else if (_targetLevel < _currentLevel) {
        _stepsRemaining = (_currentLevel - _targetLevel) * 16;
        _stepDirection = QuadratureGenerator::Direction::DOWN;
        _state = State::SEEKING;
        _settling = false;
    } else {
        _stepsRemaining = 0;
        _state = State::SEEKING;
        _settling = false;
    }
}

void CalibrationRig::handleHome() {
    _targetLevel = 1;
    // 31 levels * 16 steps = 496 reverse steps
    _stepsRemaining = 31 * 16;
    _stepDirection = QuadratureGenerator::Direction::DOWN;
    _state = State::SEEKING;
    _settling = false;
}

void CalibrationRig::handleStop() {
    _cadenceRPM = 0;
    _cadencePeriodMs = 0;
    _cadencePinHigh = true;
#ifndef UNIT_TEST
    if (_cadencePin >= 0) {
        digitalWrite(_cadencePin, HIGH);
    }
#endif
    _stepsRemaining = 0;
    _settling = false;
    _state = State::IDLE;
}

String CalibrationRig::handleStatus() {
    const char* stateStr = "IDLE";
    switch (_state) {
        case State::IDLE:    stateStr = "IDLE"; break;
        case State::SEEKING: stateStr = "SEEKING"; break;
        case State::SETTLED: stateStr = "SETTLED"; break;
        case State::ERROR:   stateStr = "ERROR"; break;
    }
    uint8_t derivedLevel = LevelMapping::wiperToEchelon(_lastPotAdc);
    char buf[128];
    snprintf(buf, sizeof(buf), "STATUS state=%s level=%u target=%u derived=%u pot=%u cadence=%u",
             stateStr, _currentLevel, _targetLevel, derivedLevel, _lastPotAdc, _cadenceRPM);
    return String(buf);
}

void CalibrationRig::handleCommand(const String& cmd) {
    String line = cmd;
    line.trim();
    if (line.isEmpty()) return;

    if (line.startsWith("SET") || line.startsWith("set")) {
        int levelIdx = line.indexOf("level=");
        if (levelIdx < 0) levelIdx = line.indexOf("LEVEL=");
        int cadenceIdx = line.indexOf("cadence=");
        if (cadenceIdx < 0) cadenceIdx = line.indexOf("CADENCE=");

        if (levelIdx >= 0 && cadenceIdx >= 0) {
            int levelVal = line.substring(levelIdx + 6).toInt();
            int cadenceVal = line.substring(cadenceIdx + 8).toInt();

            if (levelVal >= 1 && levelVal <= 32 && cadenceVal >= 0 && cadenceVal <= 200) {
                handleSet(static_cast<uint8_t>(levelVal), static_cast<uint16_t>(cadenceVal));
#ifndef UNIT_TEST
                Serial.printf("SEEKING level=%u pot=%u\n", _targetLevel, _lastPotAdc);
#endif
            } else {
#ifndef UNIT_TEST
                Serial.println("ERROR invalid parameters");
#endif
            }
        } else {
#ifndef UNIT_TEST
            Serial.println("ERROR invalid parameters");
#endif
        }
    } else if (line.equalsIgnoreCase("HOME")) {
        handleHome();
#ifndef UNIT_TEST
        Serial.printf("SEEKING level=1 pot=%u\n", _lastPotAdc);
#endif
    } else if (line.equalsIgnoreCase("STATUS")) {
        String status = handleStatus();
#ifndef UNIT_TEST
        Serial.println(status);
#endif
    } else if (line.equalsIgnoreCase("STOP")) {
        handleStop();
#ifndef UNIT_TEST
        Serial.println("OK STOP");
#endif
    } else {
#ifndef UNIT_TEST
        Serial.println("ERROR unknown command");
#endif
    }
}

void CalibrationRig::processSerialInput() {
#ifndef UNIT_TEST
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            handleCommand(_serialBuffer);
            _serialBuffer = "";
        } else {
            if (_serialBuffer.length() < 128) {
                _serialBuffer += c;
            }
        }
    }
#endif
}

void CalibrationRig::tick(uint32_t currentMillis, uint16_t potAdc) {
    _lastPotAdc = potAdc;

    // 1. Process serial commands
    processSerialInput();

    // 2. Cadence simulation
    if (_cadenceRPM > 0 && _cadencePeriodMs > CADENCE_PULSE_MS) {
        uint32_t elapsed = currentMillis - _lastCadenceToggle;
        if (_cadencePinHigh) {
            if (elapsed >= (_cadencePeriodMs - CADENCE_PULSE_MS)) {
                _cadencePinHigh = false;
                _lastCadenceToggle = currentMillis;
#ifndef UNIT_TEST
                if (_cadencePin >= 0) {
                    digitalWrite(_cadencePin, LOW);
                }
#endif
            }
        } else {
            if (elapsed >= CADENCE_PULSE_MS) {
                _cadencePinHigh = true;
                _lastCadenceToggle = currentMillis;
#ifndef UNIT_TEST
                if (_cadencePin >= 0) {
                    digitalWrite(_cadencePin, HIGH);
                }
#endif
            }
        }
    } else {
        if (!_cadencePinHigh) {
            _cadencePinHigh = true;
#ifndef UNIT_TEST
            if (_cadencePin >= 0) {
                digitalWrite(_cadencePin, HIGH);
            }
#endif
        }
    }

    // 3. Encoder stepping & Pot Settle monitoring
    if (_state == State::SEEKING) {
        if (_stepsRemaining > 0) {
            if (currentMillis - _lastStepMs >= STEP_INTERVAL_MS) {
                _lastStepMs = currentMillis;
                _encoder.step(_stepDirection);
                _stepsRemaining--;
                if (_stepsRemaining == 0) {
                    _currentLevel = _targetLevel;
                    if (_currentLevel == 1) {
                        _encoder.reset();
                    }
                    _settling = false;
                }
            }
        } else {
            // Encoder stepping complete: monitor pot settle
            uint16_t exp = expectedPot(_targetLevel);
            int32_t diff = std::abs(static_cast<int32_t>(potAdc) - static_cast<int32_t>(exp));
            if (diff <= SETTLE_TOLERANCE) {
                if (!_settling) {
                    _settling = true;
                    _settleStartMs = currentMillis;
                } else if (currentMillis - _settleStartMs >= SETTLE_DURATION_MS) {
                    _state = State::SETTLED;
                    _settling = false;
#ifndef UNIT_TEST
                    Serial.printf("SETTLED level=%u pot=%u cadence=%u\n", _currentLevel, potAdc, _cadenceRPM);
#endif
                }
            } else {
                _settling = false;
            }
        }
    }
}

} // namespace echenon

#endif // WITH_CALIBRATION_RIG
