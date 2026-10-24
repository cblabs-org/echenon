#include "echenon/CalibrationWizard.h"
#include "echenon/ResistanceSensor.h"
#include <cstring>

namespace echenon {

CalibrationWizard::CalibrationWizard(ResistanceSensor* sensor)
    : _sensor(sensor),
      _state(State::IDLE),
      _currentStep(0),
      _isComplete(false),
      _posDone(false) {
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
        _posDone = true;
    }
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
    }
}

void CalibrationWizard::reset() {
    _state = State::IDLE;
    _currentStep = 0;
    _isComplete = false;
    _posDone = false;
    std::memset(_table, 0, sizeof(_table));
}

} // namespace echenon
