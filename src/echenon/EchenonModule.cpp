#include "echenon/EchenonModule.h"
#include "ConfigManager.hpp"
#include <PsychicHttp.h>

// =============================================================================
// ITickable
// =============================================================================

void EchenonModule::tick(uint32_t currentMillis) {
    (void)currentMillis;
    _signalLogger.begin();
    _signalLogger.tick(currentMillis);
}

// =============================================================================
// IConfigurable
// =============================================================================

const char* EchenonModule::getConfigNamespace() const {
    return "echenon";
}

int EchenonModule::getSchemaVersion() const {
    return 1;
}

void EchenonModule::applyConfig(const JsonObject& config) {
    (void)config;
    _signalLogger.applyConfig(config);
}

void EchenonModule::serializeConfig(JsonObject& config) const {
    (void)config;
    _signalLogger.serializeConfig(config);
}

void EchenonModule::resetConfig() {
    _signalLogger.resetConfig();
}

// =============================================================================
// IWebContributor
// =============================================================================

void EchenonModule::registerRoutes(PsychicHttpServer& server) {
    (void)server;
    _signalLogger.registerRoutes(server);
    // TODO [WO3]: Register live status and WebSocket endpoints
}

void EchenonModule::registerTabs(ITabRegistry& registry) {
    (void)registry;
    // TODO [WO3]: Register Echenon dashboard tab
}
