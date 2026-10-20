#pragma once

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
              int8_t motorDir = 0,     // -1, 0, +1 from motor sense (0 for v1)
              int32_t encoderDelta = 0 // from EncoderModule events (0 if not tracked)
              );

    // Configuration
    void setIntervalMs(uint32_t ms);  // default 333 ms (~3 Hz)
    uint32_t getIntervalMs() const;

private:
    PsychicWebSocketHandler* _handler = nullptr;
    uint32_t _intervalMs = 333;      // ~3 Hz
    uint32_t _lastBroadcast = 0;
};

} // namespace echenon

#endif // UNIT_TEST
