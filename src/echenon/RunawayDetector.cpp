#include "echenon/RunawayDetector.h"

namespace echenon {

void RunawayDetector::setTimeoutSeconds(uint16_t seconds) {
    _timeoutS = seconds;
}

uint16_t RunawayDetector::getTimeoutSeconds() const {
    return _timeoutS;
}

bool RunawayDetector::tick(uint32_t currentMillis, uint16_t filteredWiper) {
    _lastTickMs = currentMillis;

    bool inRange = (filteredWiper >= RANGE_MIN && filteredWiper <= RANGE_MAX);

    if (inRange) {
        // Wiper returned to range — clear out-of-range tracking and runaway
        _outOfRange = false;
        _outOfRangeStartMs = 0;
        _runawayDetected = false;
        // _wasTriggered stays latched
        return false;
    }

    // Wiper is out of range
    if (!_outOfRange) {
        // Start tracking
        _outOfRange = true;
        _outOfRangeStartMs = currentMillis;
        return false;
    }

    // Already tracking — check timeout
    if (!_runawayDetected) {
        uint32_t elapsed = currentMillis - _outOfRangeStartMs;
        if (elapsed >= static_cast<uint32_t>(_timeoutS) * 1000u) {
            _runawayDetected = true;
            _wasTriggered = true;
            return true;  // Edge: first detection tick
        }
    }

    return false;  // Still tracking, not yet timed out (or already detected)
}

bool RunawayDetector::isRunaway() const {
    return _runawayDetected;
}

bool RunawayDetector::wasTriggered() const {
    return _wasTriggered;
}

uint32_t RunawayDetector::getRunawayDurationMs() const {
    if (!_outOfRange) {
        return 0;
    }
    return _lastTickMs - _outOfRangeStartMs;
}

void RunawayDetector::reset() {
    _outOfRange = false;
    _outOfRangeStartMs = 0;
    _lastTickMs = 0;
    _runawayDetected = false;
    _wasTriggered = false;
}

} // namespace echenon
