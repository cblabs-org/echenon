#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HRMRepeater.h
 * Description: BLE peripheral-role Heart Rate Service (0x180D) repeater relaying EventBus HR samples to centrals.
 */

#ifdef WITH_BLE

#include "BLEManager/IBLEServiceProvider.hpp"
#include "BLEManager/BLEAdvertisingBuilder.hpp"
#include "echenon/HREncoding.h"

class EventBus;
class NimBLECharacteristic;

/**
 * @brief Peripheral-role BLE Heart Rate Service (0x180D) repeater.
 *
 * Subscribes to EventBus HR_TOPIC samples and notifies subscribed centrals
 * via 0x2A37 (Heart Rate Measurement) and 0x2A38 (Body Sensor Location).
 */
class HRMRepeater : public IBLEServiceProvider {
public:
    /// Return unique BLE service provider identifier ("hr_repeater").
    const char* getProviderId() const override { return "hr_repeater"; }

    /// Register Heart Rate Service (0x180D) and characteristics (0x2A37, 0x2A38).
    bool registerServices(NimBLEServer& server) override;
    /// Contribute Heart Rate Service UUID (0x180D) to BLE advertising payload.
    void contributeAdvertising(BLEAdvertisingBuilder& adv) override;
    /// Handle client CCCD subscribe/unsubscribe notifications on 0x2A37.
    void onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                               uint16_t charUuid16, bool notify, bool indicate) override;

    /// Subscribe to EventBus HR_TOPIC notifications for relay to connected centrals.
    void subscribe(EventBus& bus);

    /// Encode a 0x2A37 Heart Rate Measurement payload with Body Sensor Location (public for native testing).
    static size_t encode0x2A37(uint8_t bpm, uint8_t bodySensorLocation,
                               uint8_t* out, size_t outLen) {
        return echenon::encode0x2A37(bpm, bodySensorLocation, out, outLen);
    }

    /// Encode a 0x2A37 Heart Rate Measurement payload with default location 0x00 (public for native testing).
    static size_t encode0x2A37(uint8_t bpm, uint8_t* out, size_t outLen) {
        return echenon::encode0x2A37(bpm, 0x00, out, outLen);
    }

    /// Update the advertised Body Sensor Location byte (0x2A38).
    void setBodySensorLocation(uint8_t loc);

    /// Return the current Body Sensor Location byte.
    uint8_t getBodySensorLocation() const { return _bodySensorLocation; }
    /// Return the most recently relayed heart rate in BPM.
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
