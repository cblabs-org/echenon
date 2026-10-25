#include "echenon/FTMSServer.h"

// EventBus include: native builds resolve via -I framework (modules/ prefix required),
// ESP32 builds resolve via lib_extra_dirs = framework/modules (no prefix needed).
#ifdef UNIT_TEST
#include "modules/EventBus/EventBus.hpp"
#else
#include "EventBus/EventBus.hpp"
#endif

#include "echenon/HREvents.h"

#if defined(ESP_PLATFORM)
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLECharacteristic.h>
#include <esp_log.h>
#endif

#include <cstring>

namespace {
constexpr const char* FTMS_TAG = "FTMSServer";
} // anonymous namespace

namespace echenon {

FTMSServer::FTMSServer(EventBus& eventBus)
    : _eventBus(eventBus) {}

FTMSServer::~FTMSServer() {
    end();
}

void FTMSServer::begin() {
    _eventBus.subscribe(echenon::HR_TOPIC, FTMSServer::onHrEvent, this);
#if defined(ESP_PLATFORM)
    ESP_LOGI(FTMS_TAG, "FTMSServer started — subscribed to HR topic");
#endif
}

void FTMSServer::end() {
    _eventBus.unsubscribe(echenon::HR_TOPIC, FTMSServer::onHrEvent, this);
#if defined(ESP_PLATFORM)
    ESP_LOGI(FTMS_TAG, "FTMSServer stopped — unsubscribed from HR topic");
#endif
}

void FTMSServer::onHrEvent(const char* topic, const void* payload, void* ctx) {
    (void)topic;
    if (!payload || !ctx) return;

    auto* self   = static_cast<FTMSServer*>(ctx);
    const auto* sample = static_cast<const echenon::HrSample*>(payload);

    if (sample->contactState == echenon::HRContactState::ACTIVE && sample->bpm > 0) {
        self->_lastHrBpm = sample->bpm;
        self->_lastHrMs  = sample->receivedAtMs;
    } else if (sample->contactState == echenon::HRContactState::DISCONNECTED ||
               sample->contactState == echenon::HRContactState::NO_CONTACT) {
        self->_lastHrBpm = 0;
    }
}

uint8_t FTMSServer::getEffectiveHrBpm(uint32_t now_ms) const {
    if (_lastHrBpm == 0) return 0;
    if (static_cast<int32_t>(now_ms - _lastHrMs) > static_cast<int32_t>(HR_STALE_TIMEOUT_MS)) return 0;
    return _lastHrBpm;
}

void FTMSServer::updateTelemetry(float speed_kph, float cadence_rpm,
                                  uint8_t internal_resistance, int16_t power_watts) {
    _speedKph           = speed_kph;
    _cadenceRpm         = cadence_rpm;
    _internalResistance = internal_resistance;
    _powerWatts         = power_watts;
}

void FTMSServer::tick(uint32_t currentMillis) {
    if (_paused) return;
    // Note: _lastNotifyMs starts at 0, so tick(0) is intentionally skipped (initial 500 ms settle window).
    if (currentMillis - _lastNotifyMs < NOTIFY_INTERVAL_MS) return;
    _lastNotifyMs = currentMillis;

    IndoorBikeTelemetry t;
    t.speed_kph           = _speedKph;
    t.cadence_rpm         = _cadenceRpm;
    t.internal_resistance = _internalResistance;
    t.power_watts         = _powerWatts;
    t.heart_rate_bpm      = getEffectiveHrBpm(currentMillis);

    encodeIndoorBikeData(t, _lastPacket);
    _notifyCount++;

#ifdef WITH_BLE
    if (_bikeDataChar && _server && _server->getConnectedCount() > 0) {
        _bikeDataChar->setValue(_lastPacket, FTMS_INDOOR_BIKE_PACKET_SIZE);
        _bikeDataChar->notify();
    }
#endif
}

void FTMSServer::getLastPacket(uint8_t out[11]) const {
    std::memcpy(out, _lastPacket, FTMS_INDOOR_BIKE_PACKET_SIZE);
}

#ifdef WITH_BLE

bool FTMSServer::registerServices(NimBLEServer& server) {
#if defined(ESP_PLATFORM)
    ESP_LOGI(FTMS_TAG, "Registering FTMS Service (0x1826)...");

    _server = &server;

    NimBLEService* pSvc = server.createService(NimBLEUUID((uint16_t)0x1826));
    if (!pSvc) {
        ESP_LOGE(FTMS_TAG, "Failed to create FTMS service (0x1826)");
        return false;
    }

    // 0x2ACC — Fitness Machine Feature (Read)
    _featureChar = pSvc->createCharacteristic(
        NimBLEUUID((uint16_t)0x2ACC),
        NIMBLE_PROPERTY::READ
    );
    if (!_featureChar) {
        ESP_LOGE(FTMS_TAG, "Failed to create Feature characteristic (0x2ACC)");
        return false;
    }
    uint8_t featureBuf[FTMS_FEATURE_PACKET_SIZE];
    encodeFitnessMachineFeature(featureBuf);
    _featureChar->setValue(featureBuf, FTMS_FEATURE_PACKET_SIZE);

    // 0x2AD2 — Indoor Bike Data (Notify)
    _bikeDataChar = pSvc->createCharacteristic(
        NimBLEUUID((uint16_t)0x2AD2),
        NIMBLE_PROPERTY::NOTIFY
    );
    if (!_bikeDataChar) {
        ESP_LOGE(FTMS_TAG, "Failed to create Indoor Bike Data characteristic (0x2AD2)");
        return false;
    }
    uint8_t zeroed[FTMS_INDOOR_BIKE_PACKET_SIZE] = {};
    _bikeDataChar->setValue(zeroed, FTMS_INDOOR_BIKE_PACKET_SIZE);

#if defined(FTMS_STUB_CONTROL_POINT) && FTMS_STUB_CONTROL_POINT
    class StubControlPointCallbacks : public NimBLECharacteristicCallbacks {
    public:
        void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& info) override {
            (void)info;
            if (!chr) return;
            NimBLEAttValue val = chr->getValue();
            uint8_t req = (val.size() > 0) ? val.data()[0] : 0xFF;
            // 0x00 (Request Control) -> 0x01 (Success); all other opcodes -> 0x02 (Op Code Not Supported)
            uint8_t resp[3] = {
                0x80,
                req,
                static_cast<uint8_t>(req == 0x00 ? 0x01 : 0x02)
            };
            chr->setValue(resp, sizeof(resp));
            chr->indicate();
        }
    };
    static StubControlPointCallbacks s_stubCpCallbacks;

    // 0x2AD9 — Fitness Machine Control Point (stub for clients that require discovery)
    NimBLECharacteristic* cpChar = pSvc->createCharacteristic(
        NimBLEUUID((uint16_t)0x2AD9),
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
    );
    if (cpChar) {
        cpChar->setCallbacks(&s_stubCpCallbacks);
        ESP_LOGI(FTMS_TAG, "Stub Control Point (0x2AD9) registered");
    }
#endif

    ESP_LOGI(FTMS_TAG, "FTMS Service (0x1826) registered successfully");
    return true;
#else
    (void)server;
    return true;
#endif
}

void FTMSServer::contributeAdvertising(BLEAdvertisingBuilder& adv) {
    static const uint8_t svcData[3] = { 0x01 /*available*/, 0x20, 0x00 /*indoor bike*/ };
    adv.addServiceUuid16(0x1826, BLEAdvertisingBuilder::Priority::Required);
    adv.addServiceData16(0x1826, svcData, sizeof(svcData), BLEAdvertisingBuilder::Priority::Normal);
    adv.setAppearance(kIndoorBikeAppearance);
}

#endif // WITH_BLE

} // namespace echenon
