#pragma once

#include "AppFoundation.h"
#include "echenon/CadenceSensor.h"
#include "echenon/ResistanceSensor.h"
#include "echenon/LevelMapping.h"
#include "echenon/PowerEngine.h"
#include "echenon/CalibrationWizard.h"
#include "echenon/RecalibrationUI.h"
#include "echenon/RunawayDetector.h"
#include "echenon/DiagnosticsModule.h"
#include "echenon/RidePersistence.h"
#ifdef WITH_SIGNAL_LOGGER
#include "echenon/SignalLogger.h"
#endif

#ifndef UNIT_TEST
#include "echenon/LiveFeed.h"
#include "echenon/DiagnosticsRoutes.h"
#endif

#ifdef WITH_BLE
#include "echenon/HRMRepeater.h"
#include "echenon/HRMClient.h"
#endif

#ifdef WITH_FTMS
#include "echenon/FTMSServer.h"
#endif

#ifdef WITH_HR_SYNTHETIC
#include "echenon/HRSynthetic.h"
#endif

#ifdef WITH_CALIBRATION_RIG
#include "echenon/CalibrationRig.h"
#endif

#ifdef WITH_SESSION
#include "echenon/SessionManager.h"
#endif

class PsychicHttpServer;
class ConfigManager;
class EventBus;

/**
 * @brief Echenon primary application module following the Service Module Pattern.
 *
 * Implements ITickable, IConfigurable, and IWebContributor.
 */
class EchenonModule : public ITickable,
                      public IConfigurable,
                      public IWebContributor {
public:
    EchenonModule() = default;
    ~EchenonModule() override = default;

    // --- ITickable ---
    void tick(uint32_t currentMillis) override;

    // --- IConfigurable ---
    const char* getConfigNamespace() const override;
    int getSchemaVersion() const override;
    void applyConfig(const JsonObject& config) override;
    void serializeConfig(JsonObject& config) const override;
    void resetConfig() override;

    // --- IWebContributor ---
    void registerRoutes(PsychicHttpServer& server) override;
    void registerTabs(ITabRegistry& registry) override;
    const char* getApiPrefix() const override { return "/api/echenon"; }
    size_t getMaxPostBody() const override { return 8192; }

    // --- Dependency Injection ---
    void setConfigManager(ConfigManager* cm) { _configManager = cm; }
    void setEventBus(EventBus* eb) {
        _eventBus = eb;
#ifdef WITH_SIGNAL_LOGGER
        _signalLogger.setEventBus(eb);
#endif
    }

    void setRidePersistence(echenon::RidePersistence* rp) { _ridePersistence = rp; }
    echenon::DiagnosticsModule& diagnostics() { return _diagnostics; }

    // --- Epoch Propagation ---
    void updateEpoch(uint32_t epoch_s) { _currentEpoch = epoch_s; }
    uint32_t getCurrentEpoch() const { return _currentEpoch; }

    // --- Sensors & Level Mapping ---
    echenon::CadenceSensor& cadenceSensor() { return _cadenceSensor; }
    const echenon::CadenceSensor& cadenceSensor() const { return _cadenceSensor; }

    echenon::ResistanceSensor& resistanceSensor() { return _resistanceSensor; }
    const echenon::ResistanceSensor& resistanceSensor() const { return _resistanceSensor; }

    float getCadenceRPM() const { return _cadenceSensor.getRPM(); }
    float getNormalizedPosition() const {
        return _calibrationWizard.wiperToNormalized(_resistanceSensor.getFilteredWiperFloat());
    }
    float getContinuousLevel() const {
        return _calibrationWizard.wiperToLevel(_resistanceSensor.getFilteredWiperFloat());
    }
    uint8_t getEchelonLevel() const {
        return LevelMapping::wiperToEchelon(
            _resistanceSensor.getFilteredWiperFloat(),
            _calibrationWizard.getPotMin(),
            _calibrationWizard.getPotMax());
    }
    uint8_t getInternalLevel() const {
        return LevelMapping::wiperToInternal(
            _resistanceSensor.getFilteredWiperFloat(),
            _calibrationWizard.getPotMin(),
            _calibrationWizard.getPotMax());
    }
    uint16_t getFilteredWiper() const { return _resistanceSensor.getFilteredWiper(); }

    // --- Power Engine ---
    echenon::PowerEngine& powerEngine() { return _powerEngine; }
    const echenon::PowerEngine& powerEngine() const { return _powerEngine; }
    float getPowerWatts() const { return _powerEngine.getLastPower(); }

    // --- Calibration Wizard & Recalibration ---
    echenon::CalibrationWizard& calibrationWizard() { return _calibrationWizard; }
    const echenon::CalibrationWizard& calibrationWizard() const { return _calibrationWizard; }

    echenon::RecalibrationUI& recalibrationUI() { return _recalibrationUI; }
    const echenon::RecalibrationUI& recalibrationUI() const { return _recalibrationUI; }

    echenon::RunawayDetector& runawayDetector() { return _runawayDetector; }
    const echenon::RunawayDetector& runawayDetector() const { return _runawayDetector; }

#ifndef UNIT_TEST
    echenon::LiveFeed& liveFeed() { return _liveFeed; }
    const echenon::LiveFeed& liveFeed() const { return _liveFeed; }
#endif

#ifdef WITH_BLE
    HRMRepeater& hrmRepeater() { return _hrmRepeater; }
    HRMClient& hrmClient() { return _hrmClient; }
#endif

#ifdef WITH_FTMS
    void setFTMSServer(echenon::FTMSServer* server) { _ftmsServer = server; }
    echenon::FTMSServer& getFTMSServer() { return *_ftmsServer; }
#endif

#ifdef WITH_HR_SYNTHETIC
    echenon::HRSynthetic& hrSynthetic() { return _hrSynthetic; }
#endif

#ifdef WITH_CALIBRATION_RIG
    echenon::CalibrationRig& calibrationRig() { return _calibrationRig; }
    const echenon::CalibrationRig& calibrationRig() const { return _calibrationRig; }
#endif

#ifdef WITH_SESSION
    void setSessionManager(echenon::SessionManager* sm) { _sessionManager = sm; }
    echenon::SessionManager* getSessionManager() const { return _sessionManager; }
#endif

private:
    void syncPhysicsCalibration();

    ConfigManager* _configManager = nullptr;
    EventBus*      _eventBus      = nullptr;

    echenon::CadenceSensor _cadenceSensor;
    echenon::ResistanceSensor _resistanceSensor;
    echenon::PowerEngine _powerEngine;
    echenon::CalibrationWizard _calibrationWizard;
    echenon::RecalibrationUI _recalibrationUI;
    echenon::RunawayDetector _runawayDetector;
    echenon::DiagnosticsModule _diagnostics;
    echenon::RidePersistence*  _ridePersistence = nullptr;
    uint32_t _currentEpoch = 0;
    bool     _saveRidesToFlash = false;
    uint32_t _diagRideCapKb    = echenon::RidePersistence::DEFAULT_CAP_KB;

#ifndef UNIT_TEST
    echenon::LiveFeed _liveFeed;
    echenon::DiagnosticsRoutes* _diagRoutes = nullptr;
#endif

#ifdef WITH_SIGNAL_LOGGER
    SignalLogger _signalLogger;
#endif

#ifdef WITH_BLE
    HRMRepeater _hrmRepeater;
    HRMClient   _hrmClient;
#endif

#ifdef WITH_FTMS
    echenon::FTMSServer* _ftmsServer = nullptr;
#endif

#ifdef WITH_HR_SYNTHETIC
    echenon::HRSynthetic _hrSynthetic;
#endif

#ifdef WITH_CALIBRATION_RIG
    echenon::CalibrationRig _calibrationRig;
#endif

#ifdef WITH_SESSION
    echenon::SessionManager* _sessionManager = nullptr;
#endif
};
