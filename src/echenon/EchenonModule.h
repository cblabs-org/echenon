#pragma once

#include "AppFoundation.h"
#ifdef WITH_SIGNAL_LOGGER
#include "echenon/SignalLogger.h"
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

private:
    ConfigManager* _configManager = nullptr;

#ifdef WITH_SIGNAL_LOGGER
    SignalLogger _signalLogger;
#endif
};
