#include "echenon/HRMRepeater.h"

#ifdef WITH_BLE

#include "EventBus/EventBus.hpp"
#include "echenon/HREvents.h"

#if defined(ESP_PLATFORM)
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLECharacteristic.h>
#include <esp_log.h>
#endif

static const char* TAG = "HRMRepeater";

bool HRMRepeater::registerServices(NimBLEServer& server) {
#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "Registering Heart Rate Service (0x180D)...");

    NimBLEService* pHrs = server.createService(NimBLEUUID((uint16_t)0x180D));
    if (!pHrs) {
        ESP_LOGE(TAG, "Failed to create Heart Rate service (0x180D)");
        return false;
    }

    // 0x2A37 (HR Measurement): Notify, no read, no write
    _hrMeasChar = pHrs->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A37),
        NIMBLE_PROPERTY::NOTIFY
    );
    if (!_hrMeasChar) {
        ESP_LOGE(TAG, "Failed to create HR Measurement characteristic (0x2A37)");
        return false;
    }

    // 0x2A38 (Body Sensor Location): Read only
    _bodySensChar = pHrs->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A38),
        NIMBLE_PROPERTY::READ
    );
    if (!_bodySensChar) {
        ESP_LOGE(TAG, "Failed to create Body Sensor Location characteristic (0x2A38)");
        return false;
    }

    // Initial location: 0x00 (Other)
    _bodySensChar->setValue(&_bodySensorLocation, 1);

    ESP_LOGI(TAG, "Heart Rate Service (0x180D) registered successfully");
    return true;
#else
    (void)server;
    return true;
#endif
}

void HRMRepeater::contributeAdvertising(BLEAdvertisingBuilder& adv) {
    adv.addServiceUuid16(0x180D, BLEAdvertisingBuilder::Priority::Normal);
}

void HRMRepeater::onSubscriptionChanged(BLEConnHandle handle, uint16_t charHandle,
                                       uint16_t charUuid16, bool notify, bool indicate) {
    (void)handle;
    (void)charHandle;
    (void)indicate;
    if (charUuid16 == 0x2A37 || charUuid16 == 0) {
#if defined(ESP_PLATFORM)
        ESP_LOGI(TAG, "HR Measurement subscription changed: %s", notify ? "SUBSCRIBED" : "UNSUBSCRIBED");
#else
        (void)notify;
#endif
    }
}

void HRMRepeater::subscribe(EventBus& bus) {
    bus.subscribe(echenon::HR_TOPIC, HRMRepeater::onHrEvent, this);
}

void HRMRepeater::onHrEvent(const char* topic, const void* payload, void* ctx) {
    (void)topic;
    if (!payload || !ctx) {
        return;
    }
    auto* self = static_cast<HRMRepeater*>(ctx);
    const auto* sample = static_cast<const echenon::HrSample*>(payload);
    self->_lastBpm = sample->bpm;
    self->notifyHr(sample->bpm);
}

void HRMRepeater::notifyHr(uint8_t bpm) {
    _lastBpm = bpm;

#if defined(ESP_PLATFORM)
    if (!_hrMeasChar) {
        return;
    }

    uint8_t payload[2];
    size_t len = encode0x2A37(bpm, _bodySensorLocation, payload, sizeof(payload));
    if (len > 0) {
        _hrMeasChar->setValue(payload, len);
        _hrMeasChar->notify();
    }
#endif
}

void HRMRepeater::setBodySensorLocation(uint8_t loc) {
    _bodySensorLocation = loc;
#if defined(ESP_PLATFORM)
    if (_bodySensChar) {
        _bodySensChar->setValue(&_bodySensorLocation, 1);
    }
#endif
}

#endif // WITH_BLE
