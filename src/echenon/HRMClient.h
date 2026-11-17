#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HRMClient.h
 * Description: BLE central-role Heart Rate Monitor client provider (0x180D/0x2A37/0x2A38/0x2A19) publishing to EventBus.
 */

#ifdef WITH_BLE

#include "BLEManager/IBLEClientProvider.hpp"
#include "interfaces/ITickable.hpp"
#include "echenon/HREvents.h"
#include "echenon/HRParsing.h"
#include <cstdint>
#include <cstddef>

class EventBus;
class NimBLEClient;

/**
 * @brief Central-role client provider for Bluetooth Low Energy Heart Rate straps.
 *
 * Implements IBLEClientProvider to manage connection, subscriptions (0x180D/0x2A37
 * HR Measurement and 0x180F/0x2A19 Battery Level), and reads Body Sensor Location (0x2A38).
 * Implements ITickable for a 10 s stale-heartbeat timeout.
 * Publishes HrSample events to the EventBus on topic "echenon.hr".
 */
class HRMClient : public IBLEClientProvider, public ITickable {
public:
    /// Default constructor.
    HRMClient() = default;
    /// Default destructor.
    ~HRMClient() override = default;

    /// Return unique BLE client provider identifier ("hr_client").
    const char* getProviderId() const override { return "hr_client"; }
    /// Return human-readable device type label.
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }

    /// Populate target BLE service UUIDs to scan for (0x180D Heart Rate Service).
    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override;
    /// Populate characteristic subscriptions (0x2A37 HR Measurement required, 0x2A19 Battery optional).
    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override;

    /// Handle BLE peer connection establishment.
    void onConnected(const BLEStoredDevice& device) override;
    /// Decode incoming 0x2A37 HR or 0x2A19 Battery notifications and publish to EventBus.
    void onNotification(const BLENotification& notification) override;
    /// Mark strap disconnected and publish zeroed HR sample to EventBus.
    void onDisconnected(BLEDisconnectReason reason) override;
    /// Read Body Sensor Location (0x2A38) from BLE worker task once client services are ready.
    void onClientReadyWorker(NimBLEClient& client) override;

    /// Enforce 10 s stale-heartbeat timeout and transition to NO_CONTACT on expiry.
    void tick(uint32_t currentMillis) override;

    /// Attach the framework EventBus for publishing HrSample events.
    void setEventBus(EventBus* bus) { _bus = bus; }

    // Accessors for portal / MCP
    /// Return most recent heart rate in BPM (0 when disconnected or no contact).
    uint8_t getBpm() const { return _lastBpm; }
    /// Alias for getBpm() returning the most recent heart rate in BPM.
    uint8_t getHeartRateBpm() const { return _lastBpm; }
    /// Return current strap contact state.
    echenon::HRContactState getContactState() const { return _contactState; }
    /// Return last reported strap battery percentage (0-100).
    uint8_t getBatteryPct() const { return _batteryPct; }
    /// Return Bluetooth SIG Body Sensor Location byte (0x00 = Other).
    uint8_t getBodySensorLocation() const { return _bodySensorLocation; }

private:
    void publishHr(uint8_t bpm, echenon::HRContactState state, uint32_t receivedAtMs);

    EventBus*               _bus = nullptr;

    uint8_t                 _lastBpm = 0;
    echenon::HRContactState _contactState = echenon::HRContactState::UNKNOWN;
    uint8_t                 _batteryPct = 0;
    uint8_t                 _bodySensorLocation = 0;
    uint32_t                _lastValidHrMs = 0;
    float                   _lastRrMs = 0.0f;

    static constexpr size_t kRingSize = 4;
    echenon::HrSample       _sampleRing[kRingSize] = {};
    size_t                  _ringHead = 0;
};

#endif // WITH_BLE
