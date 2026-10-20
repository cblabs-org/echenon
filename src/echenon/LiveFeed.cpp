#ifndef UNIT_TEST

#include "echenon/LiveFeed.h"
#include <PsychicWebSocket.h>
#include <cstdio>

namespace echenon {

void LiveFeed::setHandler(PsychicWebSocketHandler* handler) {
    _handler = handler;
}

void LiveFeed::setIntervalMs(uint32_t ms) {
    _intervalMs = ms;
}

uint32_t LiveFeed::getIntervalMs() const {
    return _intervalMs;
}

void LiveFeed::tick(uint32_t currentMillis,
                    float cadenceRPM,
                    uint8_t echelonLevel,
                    uint8_t internalLevel,
                    float powerWatts,
                    uint16_t potAdc,
                    int8_t motorDir,
                    int32_t encoderDelta) {
    if (_handler == nullptr) {
        return;
    }

    if (_lastBroadcast != 0 && (currentMillis - _lastBroadcast) < _intervalMs) {
        return;
    }

    _lastBroadcast = currentMillis;

    // Avoid formatting JSON when no clients are connected
    if (_handler->count() == 0) {
        return;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"cadence_rpm\":%.1f,\"level_echelon\":%u,\"level_i\":%u,\"power_watts\":%.1f,\"pot_adc\":%u,\"motor_dir\":%d,\"encoder_delta\":%ld,\"timestamp_ms\":%lu}",
        (double)cadenceRPM,
        (unsigned int)echelonLevel,
        (unsigned int)internalLevel,
        (double)powerWatts,
        (unsigned int)potAdc,
        (int)motorDir,
        (long)encoderDelta,
        (unsigned long)currentMillis);

    if (len > 0 && len < (int)sizeof(buf)) {
        _handler->sendAll(buf);
    }
}

} // namespace echenon

#endif // UNIT_TEST
