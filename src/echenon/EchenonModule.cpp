#include "echenon/EchenonModule.h"
#include "ConfigManager.hpp"
#include <PsychicHttp.h>

// =============================================================================
// ITickable
// =============================================================================

void EchenonModule::tick(uint32_t currentMillis) {
    (void)currentMillis;
    // TODO [WO2]: SignalLogger periodic buffer processing / ride log decimation
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
    // TODO [WO2]: Apply SignalLogger configuration
}

void EchenonModule::serializeConfig(JsonObject& config) const {
    (void)config;
    // TODO [WO2]: Serialize SignalLogger configuration
}

void EchenonModule::resetConfig() {
    // TODO [WO2]: Reset SignalLogger configuration to defaults
}

// =============================================================================
// IWebContributor
// =============================================================================

void EchenonModule::registerRoutes(PsychicHttpServer& server) {
    (void)server;
    // TODO [WO2]: Register capture trigger and export endpoints
    // TODO [WO3]: Register live status and WebSocket endpoints
}

void EchenonModule::registerTabs(ITabRegistry& registry) {
    (void)registry;
    // TODO [WO3]: Register Echenon dashboard tab
}
