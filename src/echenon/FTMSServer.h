#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/FTMSServer.h
 * Description: FTMS v1.0 read-only BLE GATT service provider (0x1826, 0x2ACC, 0x2AD2) with 2 Hz notifications and EMA power smoothing.
 * Reference: BLE FTMS GATT service structure references MIT-licensed prior art
 *            (jamesjmtaylor/esp32-ftms-server, dbsqp/bluetooth-reebok-57e, lefty01/ESP32_TTGO_FTMS; see SPEC.md §2.3).
 */

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

/**
 * @brief Read-only BLE Fitness Machine Service (FTMS 0x1826) GATT provider.
 *
 * Implements IBLEServiceProvider (under WITH_BLE) and ITickable to broadcast
 * 11-byte Indoor Bike Data (0x2AD2) packets at 2 Hz with optional EMA power
 * smoothing and EventBus HR subscription.
 */
class FTMSServer :
#ifdef WITH_BLE
    public IBLEServiceProvider,
#endif
    public ITickable
{
public:
    static constexpr uint32_t NOTIFY_INTERVAL_MS     = 500;     ///< 2 Hz notification cadence (ms)
    static constexpr uint32_t HR_STALE_TIMEOUT_MS    = 5000;    ///< 5 s HR staleness threshold (ms)
    static constexpr uint16_t kIndoorBikeAppearance  = 0x0485;  ///< Bluetooth SIG Indoor Bike GAP appearance

    /// Construct FTMSServer bound to the framework EventBus.
    explicit FTMSServer(EventBus& eventBus);
    /// Unsubscribe from EventBus HR updates on destruction.
    ~FTMSServer() override;

    // --- ITickable ---
    /// Notify connected BLE centrals with latest Indoor Bike Data at 2 Hz.
    void tick(uint32_t currentMillis) override;
    /// Return module name identifier.
    const char* getName() const { return "FTMSServer"; }

    // --- Telemetry update (must be called from main loop) ---
    /// Update instantaneous speed, cadence, canonical resistance, and power (applies EMA if enabled).
    void updateTelemetry(float speed_kph, float cadence_rpm,
                         uint8_t internal_resistance, int16_t power_watts);

    // --- FTMS Power Smoothing (EMA) ---
    /// Set the smoothing time constant in seconds.  0 = disabled (raw passthrough).
    void setPowerSmoothingWindow(float windowSeconds);
    /// Return the configured EMA power smoothing time constant in seconds.
    float getPowerSmoothingWindow() const { return _smoothWindowS; }

    // --- EventBus lifecycle ---
    /// Subscribe to EventBus HR_TOPIC notifications.
    void begin();
    /// Unsubscribe from EventBus HR_TOPIC notifications.
    void end();

    // --- Test/diagnostic accessors ---
    /// Copy the most recently encoded 11-byte Indoor Bike Data packet into out.
    void getLastPacket(uint8_t out[11]) const;
    /// Return total number of 2 Hz Indoor Bike Data ticks encoded.
    uint32_t getNotifyCount() const { return _notifyCount; }
    /// Return current heart rate in BPM, or 0 if stale beyond HR_STALE_TIMEOUT_MS.
    uint8_t getEffectiveHrBpm(uint32_t now_ms) const;
    /// Return true while BLE stack advertising/notifications are paused.
    bool isPaused() const { return _paused; }

#ifdef WITH_BLE
    // --- IBLEServiceProvider ---
    /// Return unique BLE service provider identifier ("ftms").
    const char* getProviderId() const override { return "ftms"; }
    /// Register FTMS (0x1826) service and characteristics on NimBLEServer.
    bool registerServices(NimBLEServer& server) override;
    /// Contribute FTMS UUID, service data, and Indoor Bike appearance to advertising.
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    /// Pause Indoor Bike Data notifications while BLE stack is paused.
    void onStackPaused() override { _paused = true; }
    /// Resume Indoor Bike Data notifications when BLE stack resumes.
    void onStackResumed() override { _paused = false; }
#else
    /// Return unique BLE service provider identifier ("ftms").
    const char* getProviderId() const { return "ftms"; }
    /// Pause Indoor Bike Data notifications while BLE stack is paused.
    void onStackPaused() { _paused = true; }
    /// Resume Indoor Bike Data notifications when BLE stack resumes.
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
