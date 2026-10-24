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
    echenonModule.setEventBus(&app.getEventBus());

    // Register — compile-time introspection detects implemented interfaces
    app.registerModule(&echenonModule);

#ifdef WITH_ENCODER
    for (size_t i = 0; i < BOARD_ENCODER_COUNT; i++) {
        encoderModule.addEncoder(BOARD_ENCODERS[i]);
    }
    encoderModule.begin(app);
    app.registerModule(&encoderModule);
#endif

#if defined(WITH_BLE) && defined(WITH_FTMS)
    // Construct FTMSServer with EventBus reference and inject into EchenonModule
    static echenon::FTMSServer ftmsServer(app.getEventBus());
    echenonModule.setFTMSServer(&ftmsServer);
#endif

#ifdef WITH_BLE
    bleManager.setDefaultAdvertisedName("Echenon");
    bleManager.registerServiceProvider(&echenonModule.hrmRepeater());
    bleManager.registerClientProvider(&echenonModule.hrmClient());
#ifdef WITH_FTMS
    bleManager.registerServiceProvider(&echenonModule.getFTMSServer());
#endif
    app.registerModule(&bleManager);
    app.registerModule(&echenonModule.hrmClient());
#ifdef WITH_FTMS
    app.registerModule(&echenonModule.getFTMSServer());
#endif
#endif

#ifdef WITH_HR_SYNTHETIC
    echenonModule.hrSynthetic().setEventBus(&app.getEventBus());
    app.registerModule(&echenonModule.hrSynthetic());
#endif

    FrameworkConfig config;
    app.begin(config);

#ifdef WITH_BLE
    bleManager.begin(app);
    // SPEC §8.4: slowRetryMs is fixed at 15000 ms (15 s). This is not
    // user-configurable — it is applied unconditionally at every boot.
    JsonDocument bleCfg;
    bleCfg["slowRetryMs"] = 15000;
    bleManager.applyConfig(bleCfg.as<JsonObject>());
    echenonModule.hrmClient().setEventBus(&app.getEventBus());
    echenonModule.hrmRepeater().subscribe(app.getEventBus());
#endif

#if defined(WITH_BLE) && defined(WITH_FTMS)
    ftmsServer.begin();  // Subscribes to HR EventBus topic
#endif
}

void loop() {
    app.tick();
    delay(1);  // Yield to RTOS idle task — feeds the TWDT
}
