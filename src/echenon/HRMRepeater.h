#pragma once
#ifdef WITH_BLE

#include "BLEManager/IBLEServiceProvider.hpp"
#include "BLEManager/BLEAdvertisingBuilder.hpp"
#include "echenon/HREncoding.h"

class EventBus;
class NimBLECharacteristic;

class HRMRepeater : public IBLEServiceProvider {
public:
    const char* getProviderId() const override { return "hr_repeater"; }

    bool registerServices(NimBLEServer& server) override;
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                               uint16_t charUuid16, bool notify, bool indicate) override;

    void subscribe(EventBus& bus);

    // 0x2A37 encoding — public for native testing
    static size_t encode0x2A37(uint8_t bpm, uint8_t bodySensorLocation,
                               uint8_t* out, size_t outLen) {
        return echenon::encode0x2A37(bpm, bodySensorLocation, out, outLen);
    }

    static size_t encode0x2A37(uint8_t bpm, uint8_t* out, size_t outLen) {
        return echenon::encode0x2A37(bpm, 0x00, out, outLen);
    }

    void setBodySensorLocation(uint8_t loc);

    uint8_t getBodySensorLocation() const { return _bodySensorLocation; }
    uint8_t getLastBpm() const { return _lastBpm; }

private:
    NimBLECharacteristic* _hrMeasChar   = nullptr;  // 0x2A37
    NimBLECharacteristic* _bodySensChar = nullptr;  // 0x2A38
    uint8_t  _bodySensorLocation = 0x00;  // Other; updated from strap
    uint8_t  _lastBpm = 0;

    static void onHrEvent(const char* topic, const void* payload, void* ctx);
    void notifyHr(uint8_t bpm);
};

#endif // WITH_BLE
