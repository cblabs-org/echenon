#pragma once

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
    HRMClient() = default;
    ~HRMClient() override = default;

    const char* getProviderId() const override { return "hr_client"; }
    const char* getDeviceTypeLabel() const override { return "Heart Rate Monitor"; }

    size_t getTargetServiceUUIDs(uint16_t* out, size_t max) const override;
    size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override;

    void onConnected(const BLEStoredDevice& device) override;
    void onNotification(const BLENotification& notification) override;
    void onDisconnected(BLEDisconnectReason reason) override;
    void onClientReadyWorker(NimBLEClient& client) override;

    void tick(uint32_t currentMillis) override;  // 10 s stale timeout

    void setEventBus(EventBus* bus) { _bus = bus; }

    // Accessors for portal / MCP
    uint8_t getBpm() const { return _lastBpm; }
    echenon::HRContactState getContactState() const { return _contactState; }
    uint8_t getBatteryPct() const { return _batteryPct; }
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
