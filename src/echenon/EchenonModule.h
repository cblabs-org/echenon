#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/EchenonModule.h
 * Description: Primary Echenon application module orchestrating sensors, power engine, BLE, and web/MCP interfaces.
 */

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
#include "echenon/IntervalsIcuUploader.h"
#include "echenon/WahooUploader.h"
#endif

class PsychicHttpServer;
class ConfigManager;
class EventBus;
class AuthManager;
class MCPToolRegistry;

/**
 * @brief Echenon primary application module following the Service Module Pattern.
 *
 * Implements ITickable, IConfigurable, IWebContributor, and IMCPContributor.
 */
class EchenonModule : public ITickable,
                      public IConfigurable,
                      public IWebContributor,
                      public IMCPContributor {
public:
    EchenonModule() = default;
    ~EchenonModule() override = default;

    // --- ITickable ---
    /// Advance sensors, power calculation, BLE telemetry, live feed, and session updates.
    void tick(uint32_t currentMillis) override;

    // --- IConfigurable ---
    /// Return the NVS configuration namespace ("echenon").
    const char* getConfigNamespace() const override;
    /// Return the current configuration schema version.
    int getSchemaVersion() const override;
    /// Apply persisted NVS configuration (calibration, power table, diagnostics settings).
    void applyConfig(const JsonObject& config) override;
    /// Serialize active module configuration into JSON for NVS persistence.
    void serializeConfig(JsonObject& config) const override;
    /// Reset module configuration and calibration state to compiled-in defaults.
    void resetConfig() override;

    // --- IWebContributor ---
    /// Register Echenon REST API, WebSocket live feed, and portal asset routes.
    void registerRoutes(PsychicHttpServer& server) override;
    /// Register Echenon navigation tabs in the web portal shell.
    void registerTabs(ITabRegistry& registry) override;
    /// Return the base API path prefix ("/api/echenon").
    const char* getApiPrefix() const override { return "/api/echenon"; }
    /// Return maximum permitted HTTP POST body size in bytes.
    size_t getMaxPostBody() const override { return 8192; }

    // --- IMCPContributor ---
    /// Register Echenon diagnostic, calibration, and rig MCP tools.
    void registerMCPTools(MCPToolRegistry& registry) override;

    // --- Dependency Injection ---
    /// Inject the framework ConfigManager pointer.
    void setConfigManager(ConfigManager* cm) { _configManager = cm; }
    /// Inject the framework AuthManager pointer for portal auth guards.
    void setAuthManager(AuthManager* am) { _authManager = am; }
    /// Inject the framework EventBus pointer and forward it to optional SignalLogger.
    void setEventBus(EventBus* eb) {
        _eventBus = eb;
#ifdef WITH_SIGNAL_LOGGER
        _signalLogger.setEventBus(eb);
#endif
    }

    /// Inject the optional RidePersistence service pointer.
    void setRidePersistence(echenon::RidePersistence* rp) { _ridePersistence = rp; }
    /// Return mutable reference to the production DiagnosticsModule.
    echenon::DiagnosticsModule& diagnostics() { return _diagnostics; }

    // --- Epoch Propagation ---
    /// Update the cached SNTP wall-clock epoch in seconds for diagnostic timestamps.
    void updateEpoch(uint32_t epoch_s) { _currentEpoch = epoch_s; }
    /// Return the most recently propagated wall-clock epoch in seconds.
    uint32_t getCurrentEpoch() const { return _currentEpoch; }

    // --- Sensors & Level Mapping ---
    /// Return mutable reference to the reed-switch cadence sensor.
    echenon::CadenceSensor& cadenceSensor() { return _cadenceSensor; }
    /// Return const reference to the reed-switch cadence sensor.
    const echenon::CadenceSensor& cadenceSensor() const { return _cadenceSensor; }

    /// Return mutable reference to the potentiometer resistance sensor.
    echenon::ResistanceSensor& resistanceSensor() { return _resistanceSensor; }
    /// Return const reference to the potentiometer resistance sensor.
    const echenon::ResistanceSensor& resistanceSensor() const { return _resistanceSensor; }

    /// Return the current crank cadence in RPM.
    float getCadenceRPM() const { return _cadenceSensor.getRPM(); }
    /// Return the current normalized cam position p̃ in [0.0, 1.0].
    float getNormalizedPosition() const {
        return _calibrationWizard.wiperToNormalized(_resistanceSensor.getFilteredWiperFloat());
    }
    /// Return the current continuous Echelon resistance level in [1.0, 32.0].
    float getContinuousLevel() const {
        return _calibrationWizard.wiperToLevel(_resistanceSensor.getFilteredWiperFloat());
    }
    /// Return the current integer Echelon resistance level in [1..32].
    uint8_t getEchelonLevel() const {
        return LevelMapping::wiperToEchelon(
            _resistanceSensor.getFilteredWiperFloat(),
            _calibrationWizard.getPotMin(),
            _calibrationWizard.getPotMax());
    }
    /// Return the current FTMS internal resistance level in [0..255].
    uint8_t getInternalLevel() const {
        return LevelMapping::wiperToInternal(
            _resistanceSensor.getFilteredWiperFloat(),
            _calibrationWizard.getPotMin(),
            _calibrationWizard.getPotMax());
    }
    /// Return the filtered 12-bit potentiometer wiper ADC reading.
    uint16_t getFilteredWiper() const { return _resistanceSensor.getFilteredWiper(); }

    // --- Power Engine ---
    /// Return mutable reference to the PowerEngine instance.
    echenon::PowerEngine& powerEngine() { return _powerEngine; }
    /// Return const reference to the PowerEngine instance.
    const echenon::PowerEngine& powerEngine() const { return _powerEngine; }
    /// Return the latest computed power output in watts.
    float getPowerWatts() const { return _powerEngine.getLastPower(); }

    // --- Calibration Wizard & Recalibration ---
    /// Return mutable reference to the passive position CalibrationWizard.
    echenon::CalibrationWizard& calibrationWizard() { return _calibrationWizard; }
    /// Return const reference to the passive position CalibrationWizard.
    const echenon::CalibrationWizard& calibrationWizard() const { return _calibrationWizard; }

    /// Return mutable reference to the RecalibrationUI point store.
    echenon::RecalibrationUI& recalibrationUI() { return _recalibrationUI; }
    /// Return const reference to the RecalibrationUI point store.
    const echenon::RecalibrationUI& recalibrationUI() const { return _recalibrationUI; }

    /// Return mutable reference to the stock RunawayDetector.
    echenon::RunawayDetector& runawayDetector() { return _runawayDetector; }
    /// Return const reference to the stock RunawayDetector.
    const echenon::RunawayDetector& runawayDetector() const { return _runawayDetector; }

#ifndef UNIT_TEST
    /// Return mutable reference to the WebSocket LiveFeed broadcaster.
    echenon::LiveFeed& liveFeed() { return _liveFeed; }
    /// Return const reference to the WebSocket LiveFeed broadcaster.
    const echenon::LiveFeed& liveFeed() const { return _liveFeed; }
#endif

#ifdef WITH_BLE
    /// Return mutable reference to the BLE HRMRepeater service provider.
    HRMRepeater& hrmRepeater() { return _hrmRepeater; }
    /// Return mutable reference to the BLE HRMClient central provider.
    HRMClient& hrmClient() { return _hrmClient; }
#endif

#ifdef WITH_FTMS
    /// Inject the FTMSServer instance constructed in main.cpp.
    void setFTMSServer(echenon::FTMSServer* server) { _ftmsServer = server; }
    /// Return mutable reference to the injected FTMSServer.
    echenon::FTMSServer& getFTMSServer() { return *_ftmsServer; }
#endif

#ifdef WITH_HR_SYNTHETIC
    /// Return mutable reference to the synthetic heart-rate generator.
    echenon::HRSynthetic& hrSynthetic() { return _hrSynthetic; }
#endif

#ifdef WITH_CALIBRATION_RIG
    /// Return mutable reference to the automated CalibrationRig.
    echenon::CalibrationRig& calibrationRig() { return _calibrationRig; }
    /// Return const reference to the automated CalibrationRig.
    const echenon::CalibrationRig& calibrationRig() const { return _calibrationRig; }
#endif

#ifdef WITH_SESSION
    /// Inject the SessionManager instance for live ride telemetry.
    void setSessionManager(echenon::SessionManager* sm) { _sessionManager = sm; }
    /// Return the injected SessionManager pointer (may be nullptr).
    echenon::SessionManager* getSessionManager() const { return _sessionManager; }
    /// Return mutable reference to the Intervals.icu upload sink.
    echenon::IntervalsIcuUploader& intervalsUploader() { return _intervalsUploader; }
    /// Return mutable reference to the Wahoo Cloud upload sink.
    echenon::WahooUploader& wahooUploader() { return _wahooUploader; }
#endif

private:
    void syncPhysicsCalibration();

    ConfigManager* _configManager = nullptr;
    AuthManager*   _authManager   = nullptr;
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
    float _ftmsPowerSmoothS = 0.0f;         // FTMS power EMA window (seconds)
#endif

#ifdef WITH_HR_SYNTHETIC
    echenon::HRSynthetic _hrSynthetic;
#endif

#ifdef WITH_CALIBRATION_RIG
    echenon::CalibrationRig _calibrationRig;
#endif

#ifdef WITH_SESSION
    echenon::SessionManager* _sessionManager = nullptr;
    echenon::IntervalsIcuUploader _intervalsUploader;
    echenon::WahooUploader _wahooUploader;
#endif
};
