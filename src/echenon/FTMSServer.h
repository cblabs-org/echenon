#pragma once

/// @file FTMSServer.h
/// @brief FTMS v1.0 read-only BLE service provider (0x1826, 0x2ACC, 0x2AD2).
/// @details Implements IBLEServiceProvider (under WITH_BLE) and ITickable for
///          the read-only Indoor Bike FTMS GATT profile. Subscribes to
///          EventBus HR events and notifies 11-byte Indoor Bike Data packets
///          at 2 Hz.

#include "echenon/FTMSData.h"
#include <cstdint>
#include <cstring>

#include "interfaces/ITickable.hpp"

#ifdef WITH_BLE
#include "BLEManager/IBLEServiceProvider.hpp"
#include "BLEManager/BLEAdvertisingBuilder.hpp"
#endif

class EventBus;
class NimBLEServer;
class NimBLECharacteristic;

#ifndef FTMS_STUB_CONTROL_POINT
#define FTMS_STUB_CONTROL_POINT 0
#endif

namespace echenon {

class FTMSServer :
#ifdef WITH_BLE
    public IBLEServiceProvider,
#endif
    public ITickable
{
public:
    static constexpr uint32_t NOTIFY_INTERVAL_MS     = 500;     // 2 Hz notification cadence
    static constexpr uint32_t HR_STALE_TIMEOUT_MS    = 5000;    // 5 s HR staleness threshold
    static constexpr uint16_t kIndoorBikeAppearance  = 0x0485;  // Bluetooth SIG Indoor Bike appearance

    explicit FTMSServer(EventBus& eventBus);
    ~FTMSServer() override;

    // --- ITickable ---
    void tick(uint32_t currentMillis) override;
    const char* getName() const { return "FTMSServer"; }

    // --- Telemetry update (must be called from main loop) ---
    void updateTelemetry(float speed_kph, float cadence_rpm,
                         uint8_t internal_resistance, int16_t power_watts);

    // --- FTMS Power Smoothing (EMA) ---
    /// Set the smoothing time constant in seconds.  0 = disabled (raw passthrough).
    void setPowerSmoothingWindow(float windowSeconds);
    float getPowerSmoothingWindow() const { return _smoothWindowS; }

    // --- EventBus lifecycle ---
    void begin();
    void end();

    // --- Test/diagnostic accessors ---
    void getLastPacket(uint8_t out[11]) const;
    uint32_t getNotifyCount() const { return _notifyCount; }
    uint8_t getEffectiveHrBpm(uint32_t now_ms) const;
    bool isPaused() const { return _paused; }

#ifdef WITH_BLE
    // --- IBLEServiceProvider ---
    const char* getProviderId() const override { return "ftms"; }
    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    void onStackPaused() override { _paused = true; }
    void onStackResumed() override { _paused = false; }
#else
    const char* getProviderId() const { return "ftms"; }
    void onStackPaused() { _paused = true; }
    void onStackResumed() { _paused = false; }
#endif

private:
    EventBus& _eventBus;

    // Telemetry state
    float    _speedKph           = 0.0f;
    float    _cadenceRpm         = 0.0f;
    uint8_t  _internalResistance = 0;
    int16_t  _powerWatts         = 0;

    // EMA power smoothing — applied only to FTMS BLE output
    float    _smoothWindowS      = 0.0f;   // 0 = disabled (raw passthrough)
    float    _smoothedPower      = 0.0f;   // running EMA state
    uint32_t _lastSmoothMs       = 0;      // timestamp for dt calculation
    bool     _smoothSeeded       = false;  // first sample seeds without filtering

    // HR from EventBus
    uint8_t  _lastHrBpm  = 0;
    uint32_t _lastHrMs   = 0;

    // Notification timing & stack pause state
    uint32_t _lastNotifyMs = 0;
    uint32_t _notifyCount  = 0;
    bool     _paused       = false;
    uint8_t  _lastPacket[11] = {};

#ifdef WITH_BLE
    NimBLEServer*          _server       = nullptr;
    NimBLECharacteristic*  _featureChar  = nullptr;   // 0x2ACC
    NimBLECharacteristic*  _bikeDataChar = nullptr;   // 0x2AD2
#endif

    // EventBus HR callback
    static void onHrEvent(const char* topic, const void* payload, void* ctx);
};

} // namespace echenon
