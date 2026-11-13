/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/LiveFeed.cpp
 * Description: Implementation of WebSocket telemetry JSON formatting and broadcast rate limiting.
 */

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
                    int32_t encoderDelta,
                    bool runaway,
                    uint8_t pelotonResistance,
                    float speedKph) {
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

    char buf[320];
    int len = snprintf(buf, sizeof(buf),
        "{\"cadence_rpm\":%.1f,\"level_echelon\":%u,\"level_i\":%u,\"power_watts\":%.1f,\"pot_adc\":%u,\"motor_dir\":%d,\"encoder_delta\":%ld,\"runaway\":%s,\"peloton_resistance\":%u,\"speed_kph\":%.1f,\"timestamp_ms\":%lu}",
        (double)cadenceRPM,
        (unsigned int)echelonLevel,
        (unsigned int)internalLevel,
        (double)powerWatts,
        (unsigned int)potAdc,
        (int)motorDir,
        (long)encoderDelta,
        runaway ? "true" : "false",
        (unsigned int)pelotonResistance,
        (double)speedKph,
        (unsigned long)currentMillis);

    if (len > 0 && len < (int)sizeof(buf)) {
        _handler->sendAll(buf);
    }
}

void LiveFeed::tick(uint32_t currentMillis,
                    float cadenceRPM,
                    uint8_t echelonLevel,
                    uint8_t internalLevel,
                    float powerWatts,
                    uint16_t potAdc,
                    int8_t motorDir,
                    int32_t encoderDelta,
                    bool runaway,
                    uint8_t pelotonResistance,
                    float speedKph,
                    const char* sessionState,
                    uint32_t sessionElapsedS,
                    uint8_t hrBpm) {
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

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"cadence_rpm\":%.1f,\"level_echelon\":%u,\"level_i\":%u,\"power_watts\":%.1f,\"pot_adc\":%u,\"motor_dir\":%d,\"encoder_delta\":%ld,\"runaway\":%s,\"peloton_resistance\":%u,\"speed_kph\":%.1f,\"timestamp_ms\":%lu,\"ses\":\"%s\",\"ses_s\":%lu,\"hr\":%u}",
        (double)cadenceRPM,
        (unsigned int)echelonLevel,
        (unsigned int)internalLevel,
        (double)powerWatts,
        (unsigned int)potAdc,
        (int)motorDir,
        (long)encoderDelta,
        runaway ? "true" : "false",
        (unsigned int)pelotonResistance,
        (double)speedKph,
        (unsigned long)currentMillis,
        sessionState ? sessionState : "IDLE",
        (unsigned long)sessionElapsedS,
        (unsigned int)hrBpm);

    if (len > 0 && len < (int)sizeof(buf)) {
        _handler->sendAll(buf);
    }
}

void LiveFeed::tick(float cadenceRpm,
                    uint8_t levelEchelon,
                    uint8_t levelInternal,
                    float powerWatts,
                    uint16_t potAdc,
                    int8_t motorDir,
                    int32_t encoderDelta,
                    bool runaway,
                    uint8_t pelotonResistance,
                    float speedKph,
                    const char* sessionState,
                    uint32_t sessionElapsedS,
                    uint32_t currentMillis,
                    uint8_t hrBpm) {
    tick(currentMillis, cadenceRpm, levelEchelon, levelInternal, powerWatts,
         potAdc, motorDir, encoderDelta, runaway, pelotonResistance, speedKph,
         sessionState, sessionElapsedS, hrBpm);
}

void LiveFeed::tick(uint32_t currentMillis,
                    float cadenceRPM,
                    uint8_t echelonLevel,
                    uint8_t internalLevel,
                    float powerWatts,
                    uint16_t potAdc,
                    int8_t motorDir,
                    int32_t encoderDelta,
                    bool runaway,
                    uint8_t pelotonResistance,
                    float speedKph,
                    const char* sessionState,
                    uint32_t sessionElapsedS,
                    uint8_t hrBpm,
                    uint16_t potRaw,
                    float potFiltered,
                    float pTilde,
                    uint32_t cadenceIntervalUs,
                    bool cadencePin,
                    uint16_t encVcc,
                    uint16_t motorSenseP,
                    uint16_t motorSenseN,
                    bool bleConnected,
                     bool calibrated) {
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

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"cadence_rpm\":%.1f,\"level_echelon\":%u,\"level_i\":%u,\"power_watts\":%.1f,\"pot_adc\":%u,\"motor_dir\":%d,\"encoder_delta\":%ld,\"runaway\":%s,\"peloton_resistance\":%u,\"speed_kph\":%.1f,\"timestamp_ms\":%lu,\"ses\":\"%s\",\"ses_s\":%lu,\"hr\":%u,\"pot_raw\":%u,\"pot_filt\":%.1f,\"p_tilde\":%.3f,\"cad_us\":%lu,\"cad_pin\":%s,\"enc_vcc\":%u,\"mot_p\":%u,\"mot_n\":%u,\"ble_conn\":%s,\"cal\":%s}",
        (double)cadenceRPM,
        (unsigned int)echelonLevel,
        (unsigned int)internalLevel,
        (double)powerWatts,
        (unsigned int)potAdc,
        (int)motorDir,
        (long)encoderDelta,
        runaway ? "true" : "false",
        (unsigned int)pelotonResistance,
        (double)speedKph,
        (unsigned long)currentMillis,
        sessionState ? sessionState : "IDLE",
        (unsigned long)sessionElapsedS,
        (unsigned int)hrBpm,
        (unsigned int)potRaw,
        (double)potFiltered,
        (double)pTilde,
        (unsigned long)cadenceIntervalUs,
        cadencePin ? "true" : "false",
        (unsigned int)encVcc,
        (unsigned int)motorSenseP,
        (unsigned int)motorSenseN,
        bleConnected ? "true" : "false",
        calibrated ? "true" : "false");

    if (len > 0 && len < (int)sizeof(buf)) {
        _handler->sendAll(buf);
    }
}

} // namespace echenon

#endif // UNIT_TEST
