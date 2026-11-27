/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: tools/ble-test/control-peripheral/src/main.cpp
 * Description: Standalone BLE control peripheral test firmware (HR service 0x180D, no WiFi).
 */

/**
 * control-peripheral/src/main.cpp
 *
 * Standalone BLE Control Peripheral (No WiFi).
 * Matches Echenon BLE presentation exactly:
 * - Public address (same hardware)
 * - Name pattern Echenon-XXXX derived from Bluetooth MAC
 * - Legacy advertising at 50 ms interval with HR Service 0x180D
 * - HR Service 0x180D: 0x2A37 (Notify, 1 Hz sine 60-120 BPM), 0x2A38 (Read, 1=Chest)
 * - MTU 185, TX power +9 dBm
 * - No WiFi headers, no WiFi init, zero WiFi RF interference.
 */

#include <cstdio>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"

static const char* TAG = "BLEStack";

static NimBLECharacteristic* s_hrMeasurementChar = nullptr;
static NimBLEServer* s_server = nullptr;

class ControlServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& connInfo) override {
        ESP_LOGI(TAG, ">>> Central connected: handle=%d, addr=%s, itvl=%d (%.2fms), latency=%d, timeout=%d (%dms), MTU=%d",
                 connInfo.getConnHandle(),
                 connInfo.getAddress().toString().c_str(),
                 connInfo.getConnInterval(),
                 connInfo.getConnInterval() * 1.25f,
                 connInfo.getConnLatency(),
                 connInfo.getConnTimeout(),
                 connInfo.getConnTimeout() * 10,
                 connInfo.getMTU());
        // No immediate connection-parameter update
    }

    void onDisconnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& connInfo, int reason) override {
        ESP_LOGW(TAG, "<<< Central disconnected: handle=%d, addr=%s, reason=0x%x ()",
                 connInfo.getConnHandle(),
                 connInfo.getAddress().toString().c_str(),
                 reason);

        // Re-arm advertising immediately after disconnect
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        ESP_LOGI(TAG, ">>> Central MTU changed: handle=%d, MTU=%d",
                 connInfo.getConnHandle(),
                 MTU);
    }
};

static void heartbeat_task(void* pvParameters) {
    uint8_t angle = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (s_server != nullptr && s_server->getConnectedCount() > 0 && s_hrMeasurementChar != nullptr) {
            float rad = (angle * 3.14159265f) / 180.0f;
            uint8_t bpm = static_cast<uint8_t>(90 + 30 * sinf(rad));
            angle = (angle + 36) % 360; // 10s cycle between 60 and 120 BPM

            uint8_t hrmData[2];
            hrmData[0] = 0x00; // UINT8 format, contact not supported
            hrmData[1] = bpm;

            s_hrMeasurementChar->setValue(hrmData, sizeof(hrmData));
            s_hrMeasurementChar->notify();
        }
    }
}

extern "C" void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    char devName[32];
    snprintf(devName, sizeof(devName), "Echenon-%02X%02X", mac[4], mac[5]);

    ESP_LOGI(TAG, "Starting BLE Control Peripheral (No WiFi). Device Name: %s", devName);

    NimBLEDevice::init(devName);
    NimBLEDevice::setPower(9);
    NimBLEDevice::setMTU(185);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(new ControlServerCallbacks());

    // 0x180D Heart Rate Service
    NimBLEService* pHrService = s_server->createService(NimBLEUUID((uint16_t)0x180D));

    // 0x2A37 Heart Rate Measurement (Notify)
    s_hrMeasurementChar = pHrService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A37),
        NIMBLE_PROPERTY::NOTIFY
    );

    // 0x2A38 Body Sensor Location (Read: 1 = Chest)
    NimBLECharacteristic* pLocChar = pHrService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A38),
        NIMBLE_PROPERTY::READ
    );
    uint8_t loc = 1; // Chest
    pLocChar->setValue(&loc, sizeof(loc));

    pHrService->start();

    // Spawn 1 Hz synthetic heartbeat generator task
    xTaskCreate(heartbeat_task, "hr_synth", 4096, nullptr, 5, nullptr);

    // Advertising
    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(pHrService->getUUID());
    pAdv->setName(devName);
    pAdv->setMinInterval(80); // 50ms (80 * 0.625ms)
    pAdv->setMaxInterval(80);
    pAdv->enableScanResponse(true);
    pAdv->start();

    ESP_LOGI(TAG, "Advertising legacy at 50ms interval with HR service 0x180D");
}
