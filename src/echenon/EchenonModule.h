#pragma once

#include "AppFoundation.h"

class PsychicHttpServer;
class ConfigManager;

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

private:
    ConfigManager* _configManager = nullptr;

    // TODO [WO2]: SignalLogger instance and ADC continuous capture integration
    // TODO [WO3]: Live dashboard WebSocket and CSV export handlers
};
