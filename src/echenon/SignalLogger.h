#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <ArduinoJson.h>
#include "echenon/SignalLogic.h"

class PsychicHttpServer;
class EventBus;

struct DecimationRow {
    uint32_t timestamp_ms;
    struct ChannelStats {
        uint16_t min_val;
        uint16_t max_val;
        uint32_t sum;
        uint32_t count;
    } channels[8]; // 8 scanned channels: {0, 1, 2, 3, 4, 5, 6, 9}
};

enum EdgeEventType : uint8_t {
    EDGE_EVENT_CADENCE = 0,
    EDGE_EVENT_ENCODER_LEGACY_A = 1,
    EDGE_EVENT_ENCODER_LEGACY_B = 2,
    EDGE_EVENT_MOTOR_ON_LEGACY = 3,
    EDGE_EVENT_MOTOR_OFF_LEGACY = 4,
    EDGE_EVENT_MOTOR_DIRECTION = 5,
    EDGE_EVENT_ENCODER = 6,
};

struct EdgeEvent {
    uint32_t timestamp_us;
    uint8_t type; // EdgeEventType
    uint8_t channel;
    uint16_t value;
};

class SignalLogger {
public:
    void begin();
    void stop();
    void tick(uint32_t now);
    void registerRoutes(PsychicHttpServer& server);
    bool startBurst(const char* trigger, uint32_t durationMs);
    bool isCapturing() const;
    bool isBurstActive() const;
    bool isRideLogWrapped() const { return _rideLogWrapped; }
    uint32_t getOverflowCount() const { return _overflowCount; }
    void incrementOverflowCount() { _overflowCount = _overflowCount + 1; }

    void setEventBus(EventBus* bus);
    void logEncoderEvent(int32_t delta);

    // Config
    void applyConfig(const JsonObject& config);
    void serializeConfig(JsonObject& config) const;
    void resetConfig();

private:
    static constexpr size_t kNumChannels = 8;
    static constexpr uint32_t kAdcSampleFreq = 83333;
    static constexpr uint32_t kDecimationWindowMs = 50;
    static constexpr uint32_t kBurstDurationDefaultMs = 10000;
    static constexpr uint32_t kBurstDurationMaxMs = 30000;
    static constexpr uint32_t kCadenceThresholdMv = 1500; // Reed switch: HIGH idle (~3.3V), LOW on magnet pass
    static constexpr size_t kRideLogMaxRows = 36000;
    static constexpr size_t kEventLogMaxEvents = 4000;

    bool _initialized = false;
    bool _capturing = false;
    bool _burstActive = false;
    uint32_t _burstStartMs = 0;
    uint64_t _burstStartUs = 0;
    uint32_t _burstDurationMs = kBurstDurationDefaultMs;
    uint32_t _decimationWindowMs = kDecimationWindowMs;
    bool _rideLogWrapped = false;
    bool _adcConfigured = false;
    volatile uint32_t _overflowCount = 0;
    
    adc_continuous_handle_t _adcHandle = nullptr;
    adc_cali_handle_t _adcCaliHandle = nullptr;

    DecimationRow* _rideLogBuffer = nullptr;
    size_t _rideLogHead = 0;
    size_t _rideLogCount = 0;

    EdgeEvent* _eventLogBuffer = nullptr;
    volatile size_t _eventLogHead = 0;
    size_t _eventLogCount = 0;

    uint16_t* _burstBuffer = nullptr;
    size_t _burstBufferCapacity = 0; // samples
    size_t _burstBufferCount = 0;

    // Current window accumulators
    DecimationRow _currentRow;
    uint32_t _lastWindowMs = 0;

    // Software threshold state
    bool _cadenceHigh = true; // Idle state is HIGH (console pull-up)
    echenon::MotorStateMachine _motorFsm;
    EventBus* _eventBus = nullptr;

    // Helper functions
    void processAdcFrame(uint8_t* buf, uint32_t length);
    void handleSoftwareThresholds(uint8_t channel, uint16_t mv, uint32_t ts_us);

    // Friend classes/structs for member access if needed
    friend struct SignalLogger_rideLogBuffer;
    friend struct SignalLogger_rideLogCount;
    friend struct SignalLogger_rideLogHead;
    friend struct SignalLogger_currentRow;
    friend struct SignalLogger_eventLogCount;
};
