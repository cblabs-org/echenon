#include "AppFoundation.h"
#include <Arduino.h>
#include "echenon/EchenonModule.h"

#ifdef WITH_BLE
#include "BLEManager/BLEManager.hpp"
static BLEManager bleManager;
#endif

#ifdef WITH_ENCODER
#include "EncoderModule.hpp"
#include "board.h"
EncoderModule encoderModule;
#endif

Application app;
EchenonModule echenonModule;

void setup() {
    Serial.begin(115200);
    app.setAppName("Echenon");
    app.getWifiManager().setDefaultHostname("echenon");

    // Wire dependencies
    echenonModule.setConfigManager(&app.getConfigManager());

    // Register — compile-time introspection detects implemented interfaces
    app.registerModule(&echenonModule);

#ifdef WITH_ENCODER
    for (size_t i = 0; i < BOARD_ENCODER_COUNT; i++) {
        encoderModule.addEncoder(BOARD_ENCODERS[i]);
    }
    encoderModule.begin(app);
    app.registerModule(&encoderModule);
#endif

#ifdef WITH_BLE
    bleManager.setDefaultAdvertisedName("Echenon");
    bleManager.registerServiceProvider(&echenonModule.hrmRepeater());
    // HRMClient registered in WO4
    app.registerModule(&bleManager);
#endif

#ifdef WITH_HR_SYNTHETIC
    echenonModule.hrSynthetic().setEventBus(&app.getEventBus());
    app.registerModule(&echenonModule.hrSynthetic());
#endif

    FrameworkConfig config;
    app.begin(config);

#ifdef WITH_BLE
    bleManager.begin(app);
    echenonModule.hrmRepeater().subscribe(app.getEventBus());
#endif
}

void loop() {
    app.tick();
    delay(1);  // Yield to RTOS idle task — feeds the TWDT
}
