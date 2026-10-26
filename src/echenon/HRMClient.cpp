#include "echenon/HRMClient.h"

#ifdef WITH_BLE

#include "EventBus/EventBus.hpp"

#if defined(ESP_PLATFORM)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEClient.h>
#include <NimBLERemoteService.h>
#include <NimBLERemoteCharacteristic.h>
#include <esp_log.h>
#else
#include <cstring>
#include <cstdio>
#define ESP_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

static const char* TAG = "HRMClient";

static const char* disconnectReasonToString(BLEDisconnectReason reason) {
    switch (reason) {
        case BLEDisconnectReason::PeerTerminated:     return "PeerTerminated";
        case BLEDisconnectReason::LocalTerminated:    return "LocalTerminated";
        case BLEDisconnectReason::SupervisionTimeout: return "SupervisionTimeout";
        case BLEDisconnectReason::ConnectFailed:      return "ConnectFailed";
        case BLEDisconnectReason::Paused:             return "Paused";
        case BLEDisconnectReason::Other:              return "Other";
        default:                                      return "Unknown";
    }
}

size_t HRMClient::getTargetServiceUUIDs(uint16_t* out, size_t max) const {
    if (max == 0 || !out) return 0;
    out[0] = 0x180D;  // Heart Rate Service
    return 1;
}

size_t HRMClient::getSubscriptions(BLESubscriptionSpec* out, size_t max) const {
    size_t n = 0;
    if (n < max && out) out[n++] = { 0x180D, 0x2A37, false, true  }; // HR Measurement — required
    if (n < max && out) out[n++] = { 0x180F, 0x2A19, false, false }; // Battery Level — optional
    return n;
}

void HRMClient::onConnected(const BLEStoredDevice& device) {
    _contactState = echenon::HRContactState::ACTIVE;
#if defined(ESP_PLATFORM)
    _lastValidHrMs = millis();
#else
    _lastValidHrMs = 0;
#endif
    ESP_LOGI(TAG, "Connected to HR device: %s", device.name);
}

void HRMClient::onClientReadyWorker(NimBLEClient& client) {
    _bodySensorLocation = 0x00;  // default to 0x00 (Other)
#if defined(ESP_PLATFORM)
    NimBLERemoteService* pSvc = client.getService(NimBLEUUID(static_cast<uint16_t>(0x180D)));
    if (pSvc) {
        NimBLERemoteCharacteristic* pChr = pSvc->getCharacteristic(NimBLEUUID(static_cast<uint16_t>(0x2A38)));
        if (pChr && pChr->canRead()) {
            NimBLEAttValue val = pChr->readValue();
            if (val.length() > 0) {
                _bodySensorLocation = val.data()[0];
                ESP_LOGI(TAG, "Read Body Sensor Location: 0x%02X", _bodySensorLocation);
            }
        }
    }
#else
    (void)client;
#endif
    // Known limitation (S4): BSL is stored locally in _bodySensorLocation and exposed
    // via getBodySensorLocation(). Direct call into HRMRepeater is omitted to avoid
    // cross-thread calls from the BLE worker task. HRMRepeater defaults to 0x00 (Other)
    // until EventBus routing for BSL is introduced.
}

void HRMClient::onNotification(const BLENotification& n) {
    if (n.charUuid16 == 0x2A19) {          // Battery Level
        if (n.length >= 1) {
            _batteryPct = n.data[0];
            ESP_LOGI(TAG, "Battery Level: %u%%", _batteryPct);
        }
        return;
    }
    if (n.charUuid16 != 0x2A37) return;

    echenon::ParsedHrMeasurement parsed;
    if (!echenon::parse0x2A37(n.data, n.length, parsed)) return;

    // No-contact: BPM=0 OR (contact flags supported && not detected)
    if (parsed.contactState == echenon::HRContactState::NO_CONTACT) {
        _contactState = echenon::HRContactState::NO_CONTACT;
        publishHr(0, _contactState, n.receivedAtMs);
        return;
    }

    _contactState = echenon::HRContactState::ACTIVE;
    _lastValidHrMs = n.receivedAtMs;
    if (parsed.hasRr) {
        _lastRrMs = parsed.lastRrMs;
    }
    uint8_t bpmToPublish = parsed.bpm > 255 ? 255 : static_cast<uint8_t>(parsed.bpm);
    publishHr(bpmToPublish, _contactState, n.receivedAtMs);
}

void HRMClient::onDisconnected(BLEDisconnectReason reason) {
    _contactState = echenon::HRContactState::DISCONNECTED;
#if defined(ESP_PLATFORM)
    uint32_t nowMs = millis();
#else
    uint32_t nowMs = 0;
#endif
    publishHr(0, _contactState, nowMs);
    ESP_LOGI(TAG, "Disconnected (reason: %s)", disconnectReasonToString(reason));
}

void HRMClient::tick(uint32_t currentMillis) {
    if (_contactState != echenon::HRContactState::ACTIVE) return;
    if (static_cast<int32_t>(currentMillis - _lastValidHrMs) >= 10000) {
        _contactState = echenon::HRContactState::NO_CONTACT;
        publishHr(0, _contactState, currentMillis);
    }
}

void HRMClient::publishHr(uint8_t bpm, echenon::HRContactState state, uint32_t receivedAtMs) {
    _lastBpm = bpm;
    auto& slot = _sampleRing[_ringHead];
    _ringHead = (_ringHead + 1) % kRingSize;
    slot = { bpm, state, receivedAtMs };
    if (_bus) {
        _bus->publish(echenon::HR_TOPIC, &slot);
    }
}

#endif // WITH_BLE
