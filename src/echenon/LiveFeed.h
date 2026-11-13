#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/LiveFeed.h
 * Description: Rate-limited WebSocket telemetry broadcaster pushing live ride and diagnostic JSON frames.
 */

#if defined(ESP_PLATFORM) && !defined(UNIT_TEST)
#include <Print.h>
#ifdef HEX
#undef HEX
#endif
#endif

#ifndef UNIT_TEST

#include <cstdint>
#include <cstddef>

class PsychicWebSocketHandler;

namespace echenon {

/**
 * @brief Rate-limited WebSocket broadcaster pushing live bike telemetry
 *        and optional diagnostic sensor fields to connected portal clients.
 */
class LiveFeed {
public:
    LiveFeed() = default;
    ~LiveFeed() = default;

    /// Set the PsychicWebSocketHandler (called during route registration).
    void setHandler(PsychicWebSocketHandler* handler);

    /// Broadcast core telemetry JSON to connected WS clients at the configured interval.
    void tick(uint32_t currentMillis,
              float cadenceRPM,
              uint8_t echelonLevel,
              uint8_t internalLevel,
              float powerWatts,
              uint16_t potAdc,
              int8_t motorDir = 0,           // -1, 0, +1 from motor sense (0 for v1)
              int32_t encoderDelta = 0,       // from EncoderModule events (0 if not tracked)
              bool runaway = false,           // runaway detection state
              uint8_t pelotonResistance = 0,  // Peloton 0–100 power-equivalence
              float speedKph = 0.0f           // virtual flat-road speed (km/h)
              );

    /// Broadcast extended telemetry including session state, elapsed seconds, and heart rate.
    void tick(uint32_t currentMillis,
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
              uint8_t hrBpm = 0);

    /// Broadcast extended telemetry using cadence-first argument ordering.
    void tick(float cadenceRpm,
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
              uint8_t hrBpm = 0);

    /// Broadcast full telemetry including raw/filtered ADC, cadence timing, and BLE status.
    void tick(uint32_t currentMillis,
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
               bool calibrated);

    /// Set the minimum broadcast interval in milliseconds (default 500 ms, ~2 Hz).
    void setIntervalMs(uint32_t ms);
    /// Return the configured broadcast interval in milliseconds.
    uint32_t getIntervalMs() const;

    /// Seed the last-broadcast timestamp for phase-offset interleaving.
    void seedBroadcastTime(uint32_t ms) { _lastBroadcast = ms; }

private:
    PsychicWebSocketHandler* _handler = nullptr;
    uint32_t _intervalMs = 500;      // ~2 Hz (matches FTMS notification cadence)
    uint32_t _lastBroadcast = 0;
};

} // namespace echenon

#endif // UNIT_TEST
