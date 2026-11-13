#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/SignalLogger.h
 * Description: High-rate continuous ADC signal logger with decimated ride logging, edge capture, and burst export.
 */

#if defined(WITH_SIGNAL_LOGGER) || defined(UNIT_TEST)
#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#if !defined(UNIT_TEST)
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#else
typedef void* adc_continuous_handle_t;
typedef void* adc_cali_handle_t;
#endif
#include <ArduinoJson.h>
#include "echenon/SignalLogic.h"

class PsychicHttpServer;
class EventBus;
class AuthManager;

/**
 * @brief Continuous 8-channel DMA ADC signal logger for bench characterization
 *        and diagnostics (decimated ride log, edge events, and high-rate burst).
 */
class SignalLogger {
public:
    /// Allocate PSRAM ring/burst buffers and start continuous ADC DMA capture.
    void begin();
    /// Stop continuous ADC capture and release DMA resources.
    void stop();
    /// Drain DMA ADC frames, update decimation accumulators, and check burst expiry.
    void tick(uint32_t now);
    /// Register HTTP routes for signal capture control and CSV/burst exports.
    void registerRoutes(PsychicHttpServer& server, AuthManager* authManager = nullptr);
    /// Arm a high-rate raw ADC burst capture of durationMs for the given trigger tag.
    bool startBurst(const char* trigger, uint32_t durationMs);
    /// Return true while continuous ADC capture is active.
    bool isCapturing() const;
    /// Return true while a high-rate burst capture is in progress.
    bool isBurstActive() const;
    /// Return true while an HTTP CSV/burst export stream is currently active.
    bool isExportActive() const { return _exportActive.load(); }
    /// Return true if the decimated ride log ring buffer has wrapped.
    bool isRideLogWrapped() const { return _rideLogWrapped; }
    /// Return the total count of DMA ADC pool overflow events.
    uint32_t getOverflowCount() const { return _overflowCount; }
    /// Increment the DMA ADC pool overflow counter (called from ISR callback).
    void incrementOverflowCount() { _overflowCount = _overflowCount + 1; }

    /// Attach the framework EventBus and subscribe to encoder rotation events.
    void setEventBus(EventBus* bus);
    /// Append a knob encoder rotation delta edge event to the event log.
    void logEncoderEvent(int32_t delta);

    // Config
    /// Apply persisted decimation and burst configuration from JSON.
    void applyConfig(const JsonObject& config);
    /// Serialize active signal logger configuration into JSON.
    void serializeConfig(JsonObject& config) const;
    /// Restore signal logger configuration parameters to compiled-in defaults.
    void resetConfig();

private:
    static constexpr size_t kNumChannels = 8;               ///< Number of scanned ADC channels
    static constexpr uint32_t kAdcSampleFreq = 83333;       ///< Aggregate continuous ADC sample rate (Hz)
    static constexpr uint32_t kDecimationWindowMs = 50;     ///< Default decimation accumulator window (ms)
    static constexpr uint32_t kBurstDurationDefaultMs = 10000; ///< Default burst capture duration (ms)
    static constexpr uint32_t kBurstDurationMaxMs = 30000;  ///< Maximum permitted burst capture duration (ms)
    static constexpr uint32_t kCadenceThresholdMv = 1500;   ///< Reed switch threshold (mV): HIGH idle (~3.3V), LOW on magnet pass
    static constexpr size_t kRideLogMaxRows = 36000;        ///< Decimated ride log ring capacity (rows)
    static constexpr size_t kEventLogMaxEvents = 4000;      ///< Edge event ring buffer capacity (events)

    bool _initialized = false;
    bool _capturing = false;
    bool _burstActive = false;
    std::atomic<bool> _exportActive{false};
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

#endif // WITH_SIGNAL_LOGGER || UNIT_TEST
