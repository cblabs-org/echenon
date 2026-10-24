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

namespace echenon {

class FTMSServer :
#ifdef WITH_BLE
    public IBLEServiceProvider,
#endif
    public ITickable
{
public:
    static constexpr uint32_t NOTIFY_INTERVAL_MS  = 500;   // 2 Hz notification cadence
    static constexpr uint32_t HR_STALE_TIMEOUT_MS = 5000;  // 5 s HR staleness threshold

    explicit FTMSServer(EventBus& eventBus);
    ~FTMSServer() override;

    // --- ITickable ---
    void tick(uint32_t currentMillis) override;
    const char* getName() const { return "FTMSServer"; }

    // --- Telemetry update (thread-safe store) ---
    void updateTelemetry(float speed_kph, float cadence_rpm,
                         uint8_t internal_resistance, int16_t power_watts);

    // --- EventBus lifecycle ---
    void begin();
    void end();

    // --- Test/diagnostic accessors ---
    void getLastPacket(uint8_t out[11]) const;
    uint32_t getNotifyCount() const { return _notifyCount; }
    uint8_t getEffectiveHrBpm(uint32_t now_ms) const;

#ifdef WITH_BLE
    // --- IBLEServiceProvider ---
    const char* getProviderId() const override { return "ftms_server"; }
    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
#endif

private:
    EventBus& _eventBus;

    // Telemetry state
    float    _speedKph          = 0.0f;
    float    _cadenceRpm        = 0.0f;
    uint8_t  _internalResistance = 0;
    int16_t  _powerWatts        = 0;

    // HR from EventBus
    uint8_t  _lastHrBpm  = 0;
    uint32_t _lastHrMs   = 0;

    // Notification timing
    uint32_t _lastNotifyMs = 0;
    uint32_t _notifyCount  = 0;
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
