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

#ifdef WITH_SESSION
#include "echenon/RideStore.h"
#include "echenon/SessionManager.h"
#include "echenon/UploadQueue.h"
#include "echenon/SessionOverlay.h"
#include "echenon/RidePersistence.h"
#endif

#ifdef WITH_STRAVA
#include "echenon/StravaUploader.h"
#endif

#ifdef WITH_TIME
#include "TimeManager/TimeManager.hpp"
#include "TimeManager/TimePreconditionContributor.hpp"
static TimeManager timeManager;
static TimePreconditionContributor timePrecondition{timeManager};
#endif

Application app;
EchenonModule echenonModule;

#ifdef WITH_SESSION
static echenon::RideStore rideStore;
static echenon::SessionManager sessionManager(rideStore);
static echenon::UploadQueue uploadQueue(rideStore);
static echenon::SessionOverlay sessionOverlay;
static echenon::RidePersistence ridePersistence(rideStore.fileSystem());
#endif

#ifdef WITH_STRAVA
static echenon::StravaUploader stravaUploader;
#endif

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

#ifdef WITH_SESSION
    sessionManager.setEventBus(&app.getEventBus());
    uploadQueue.setEventBus(&app.getEventBus());
    echenonModule.setSessionManager(&sessionManager);
    echenonModule.setRidePersistence(&ridePersistence);
#ifdef WITH_BLE
    uploadQueue.setBLEManager(&bleManager);
#endif
#ifdef WITH_STRAVA
    stravaUploader.setKeyManager(&app.getKeyManager());
    stravaUploader.setMetadataSaveCallback([](uint32_t, const std::string&) {
        return app.getConfigManager().saveModuleConfig(&stravaUploader);
    });
    uploadQueue.setUploader(&stravaUploader);
    sessionOverlay.setStravaUploader(&stravaUploader);
    app.registerModule(&stravaUploader);
#endif
    sessionOverlay.setSessionManager(&sessionManager);
    sessionOverlay.setRideStore(&rideStore);
    sessionOverlay.setUploadQueue(&uploadQueue);
    sessionOverlay.setAuthManager(&app.getAuthManager());
    sessionOverlay.setConfigManager(&app.getConfigManager());

    app.registerModule(&sessionManager);
    app.registerModule(&uploadQueue);
    app.registerModule(&sessionOverlay);
#endif

#ifdef WITH_TIME
    timeManager.setConfigManager(&app.getConfigManager());
    app.registerModule(&timeManager);
    app.registerModule(&timePrecondition);
#endif

    app.disableDefaultAppTab();

    FrameworkConfig config;
    app.begin(config);

    // Initialize diagnostics event log and record boot event
    echenonModule.diagnostics().begin();
    echenonModule.diagnostics().recordBoot(0, millis() / 1000);

#ifdef WITH_TIME
    timeManager.begin();
#endif

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

#ifdef WITH_SESSION
    rideStore.begin();
    rideStore.recoverInterruptedSession();
    ridePersistence.begin();
    sessionManager.begin();
#ifdef WITH_STRAVA
    stravaUploader.begin();
#endif
    uploadQueue.begin();
    app.getEventBus().subscribe(
        echenon::EVENT_SESSION_TOPIC,
        [](const char*, const void* data, void*) {
            if (data == nullptr) return;
            const auto* ev = static_cast<const echenon::SessionStateChangeEvent*>(data);
            // Record session state changes in diagnostics log
            if (ev->newState == echenon::SessionState::ACTIVE) {
                echenonModule.diagnostics().recordSessionStart(0, ev->sessionId);
            } else if (ev->newState == echenon::SessionState::ENDED) {
                echenonModule.diagnostics().recordSessionEnd(0, 0, 0);
            }
            if (ev->newState == echenon::SessionState::ENDED &&
                ev->sessionId != 0 && uploadQueue.isAutoUploadEnabled()) {
                uploadQueue.enqueueSession(ev->sessionId, millis());
            }
        });
#endif
}

void loop() {
    app.tick();
    delay(1);  // Yield to RTOS idle task — feeds the TWDT
}
