#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <ArduinoJson.h>

class PsychicHttpServer;

struct DecimationRow {
    uint32_t timestamp_ms;
    struct ChannelStats {
        uint16_t min_val;
        uint16_t max_val;
        uint32_t sum;
        uint32_t count;
    } channels[10];
};

struct EdgeEvent {
    uint32_t timestamp_us;
    uint8_t type; // 0=cadence, 1=encA, 2=encB, 3=motorOn, 4=motorOff
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

    // Config
    void applyConfig(const JsonObject& config);
    void serializeConfig(JsonObject& config) const;
    void resetConfig();

private:
    static constexpr size_t kNumChannels = 10;
    static constexpr uint32_t kAdcSampleFreq = 83333;
    static constexpr uint32_t kDecimationWindowMs = 50;
    static constexpr uint32_t kBurstDurationDefaultMs = 10000;
    static constexpr uint32_t kBurstDurationMaxMs = 30000;
    static constexpr uint32_t kChBThresholdMv = 800;
    static constexpr uint32_t kMotorOnsetThresholdMv = 500;
    static constexpr size_t kRideLogMaxRows = 36000;
    static constexpr size_t kEventLogMaxEvents = 4000;

    bool _initialized = false;
    bool _capturing = false;
    bool _burstActive = false;
    uint32_t _burstStartMs = 0;
    uint32_t _burstDurationMs = kBurstDurationDefaultMs;
    uint32_t _decimationWindowMs = kDecimationWindowMs;
    
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

    // Ch B software threshold state
    bool _chBHigh = false;
    bool _motorOn = false;

    // Helper functions
    void processAdcFrame(uint8_t* buf, uint32_t length);
    void handleSoftwareThresholds(uint8_t channel, uint16_t mv, uint32_t ts_us);
    
    static void IRAM_ATTR gpioIsrHandler(void* arg);
};
