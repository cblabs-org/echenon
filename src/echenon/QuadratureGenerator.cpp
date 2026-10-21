#if defined(WITH_CALIBRATION_RIG) || defined(UNIT_TEST)

#include "echenon/QuadratureGenerator.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

namespace echenon {

QuadratureGenerator::QuadratureGenerator(int8_t pinA, int8_t pinB)
    : _pinA(pinA), _pinB(pinB), _state(0), _totalSteps(0) {
}

void QuadratureGenerator::begin() {
#ifndef UNIT_TEST
    if (_pinA >= 0) {
        pinMode(_pinA, OUTPUT);
    }
    if (_pinB >= 0) {
        pinMode(_pinB, OUTPUT);
    }
#endif
    applyState();
}

void QuadratureGenerator::begin(int8_t pinA, int8_t pinB) {
    _pinA = pinA;
    _pinB = pinB;
    begin();
}

void QuadratureGenerator::step(Direction dir) {
    if (dir == Direction::UP) {
        _state = (_state + 1) % 4;
        _totalSteps++;
    } else {
        _state = (_state + 3) % 4;
        _totalSteps--;
    }
    applyState();
}

void QuadratureGenerator::reset() {
    _state = 0;
    _totalSteps = 0;
    applyState();
}

void QuadratureGenerator::getPinLevels(uint8_t state, bool& pinA, bool& pinB) {
    switch (state % 4) {
        case 0:
            pinA = false;
            pinB = false;
            break;
        case 1:
            pinA = true;
            pinB = false;
            break;
        case 2:
            pinA = true;
            pinB = true;
            break;
        case 3:
            pinA = false;
            pinB = true;
            break;
        default:
            pinA = false;
            pinB = false;
            break;
    }
}

void QuadratureGenerator::applyState() {
#ifndef UNIT_TEST
    if (_pinA >= 0 && _pinB >= 0) {
        bool a = false;
        bool b = false;
        getPinLevels(_state, a, b);
        digitalWrite(_pinA, a ? HIGH : LOW);
        digitalWrite(_pinB, b ? HIGH : LOW);
    }
#endif
}

} // namespace echenon

#endif // WITH_CALIBRATION_RIG || UNIT_TEST
