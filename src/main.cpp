#include "AppFoundation.h"
#include <Arduino.h>
#include "echenon/EchenonModule.h"

Application app;
EchenonModule echenonModule;

void setup() {
    Serial.begin(115200);
    app.setAppName("Echenon");

    // Wire dependencies
    echenonModule.setConfigManager(&app.getConfigManager());

    // Register — compile-time introspection detects implemented interfaces
    app.registerModule(&echenonModule);

    FrameworkConfig config;
    app.begin(config);
}

void loop() {
    app.tick();
    delay(1);  // Yield to RTOS idle task — feeds the TWDT
}
