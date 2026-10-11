#include "AppFoundation.h"
#include <Arduino.h>
#include "echenon/EchenonModule.h"

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

    FrameworkConfig config;
    app.begin(config);
}

void loop() {
    app.tick();
    delay(1);  // Yield to RTOS idle task — feeds the TWDT
}
