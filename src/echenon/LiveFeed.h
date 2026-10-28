#pragma once

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

class LiveFeed {
public:
    LiveFeed() = default;
    ~LiveFeed() = default;

    // Set the WS handler (called during registerRoutes)
    void setHandler(PsychicWebSocketHandler* handler);

    // Called every tick from EchenonModule
    // Broadcasts JSON to all connected WS clients at the configured rate
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

    // Extended overloads including session_state, session_elapsed_s, and heart rate
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

    // Extended overload including diagnostic sensor fields
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

    // Configuration
    void setIntervalMs(uint32_t ms);  // default 500 ms (~2 Hz)
    uint32_t getIntervalMs() const;

    // Seed the last-broadcast timestamp for phase-offset interleaving
    void seedBroadcastTime(uint32_t ms) { _lastBroadcast = ms; }

private:
    PsychicWebSocketHandler* _handler = nullptr;
    uint32_t _intervalMs = 500;      // ~2 Hz (matches FTMS notification cadence)
    uint32_t _lastBroadcast = 0;
};

} // namespace echenon

#endif // UNIT_TEST
