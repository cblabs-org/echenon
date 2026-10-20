#pragma once

#include "AppFoundation.h"
#include "echenon/CadenceSensor.h"
#include "echenon/ResistanceSensor.h"
#include "echenon/LevelMapping.h"
#ifdef WITH_SIGNAL_LOGGER
#include "echenon/SignalLogger.h"
#endif

#ifdef WITH_BLE
#include "echenon/HRMRepeater.h"
#include "echenon/HRMClient.h"
#endif

#ifdef WITH_HR_SYNTHETIC
#include "echenon/HRSynthetic.h"
#endif

class PsychicHttpServer;
class ConfigManager;
#ifdef WITH_SIGNAL_LOGGER
class EventBus;
#endif

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
    size_t getMaxPostBody() const override { return 4096; }

    // --- Dependency Injection ---
    void setConfigManager(ConfigManager* cm) { _configManager = cm; }
#ifdef WITH_SIGNAL_LOGGER
    void setEventBus(EventBus* eb) { _signalLogger.setEventBus(eb); }
#endif

    // --- Sensors & Level Mapping ---
    echenon::CadenceSensor& cadenceSensor() { return _cadenceSensor; }
    const echenon::CadenceSensor& cadenceSensor() const { return _cadenceSensor; }

    echenon::ResistanceSensor& resistanceSensor() { return _resistanceSensor; }
    const echenon::ResistanceSensor& resistanceSensor() const { return _resistanceSensor; }

    float getCadenceRPM() const { return _cadenceSensor.getRPM(); }
    uint8_t getEchelonLevel() const { return LevelMapping::wiperToEchelon(_resistanceSensor.getFilteredWiper()); }
    uint8_t getInternalLevel() const { return LevelMapping::wiperToInternal(_resistanceSensor.getFilteredWiper()); }
    uint16_t getFilteredWiper() const { return _resistanceSensor.getFilteredWiper(); }

#ifdef WITH_BLE
    HRMRepeater& hrmRepeater() { return _hrmRepeater; }
    HRMClient& hrmClient() { return _hrmClient; }
#endif

#ifdef WITH_HR_SYNTHETIC
    echenon::HRSynthetic& hrSynthetic() { return _hrSynthetic; }
#endif

private:
    ConfigManager* _configManager = nullptr;

    echenon::CadenceSensor _cadenceSensor;
    echenon::ResistanceSensor _resistanceSensor;

#ifdef WITH_SIGNAL_LOGGER
    SignalLogger _signalLogger;
#endif

#ifdef WITH_BLE
    HRMRepeater _hrmRepeater;
    HRMClient   _hrmClient;
#endif

#ifdef WITH_HR_SYNTHETIC
    echenon::HRSynthetic _hrSynthetic;
#endif
};
