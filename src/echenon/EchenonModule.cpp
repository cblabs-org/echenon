#include "echenon/EchenonModule.h"
#include "ConfigManager.hpp"
#include <PsychicHttp.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_log.h>
#include "utils/serveGzipAsset.hpp"
#include "include/portal_assets/echenon-dashboard_js.hpp"

static const char* TAG = "EchenonModule";

// Standard C++ member-pointer thief to access SignalLogger capture buffers
template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};

struct SignalLogger_rideLogBuffer {
    typedef DecimationRow* SignalLogger::*type;
    friend type get(SignalLogger_rideLogBuffer);
};
template struct Rob<SignalLogger_rideLogBuffer, &SignalLogger::_rideLogBuffer>;

struct SignalLogger_rideLogCount {
    typedef size_t SignalLogger::*type;
    friend type get(SignalLogger_rideLogCount);
};
template struct Rob<SignalLogger_rideLogCount, &SignalLogger::_rideLogCount>;

struct SignalLogger_rideLogHead {
    typedef size_t SignalLogger::*type;
    friend type get(SignalLogger_rideLogHead);
};
template struct Rob<SignalLogger_rideLogHead, &SignalLogger::_rideLogHead>;

struct SignalLogger_currentRow {
    typedef DecimationRow SignalLogger::*type;
    friend type get(SignalLogger_currentRow);
};
template struct Rob<SignalLogger_currentRow, &SignalLogger::_currentRow>;

struct SignalLogger_eventLogCount {
    typedef size_t SignalLogger::*type;
    friend type get(SignalLogger_eventLogCount);
};
template struct Rob<SignalLogger_eventLogCount, &SignalLogger::_eventLogCount>;

// =============================================================================
// ITickable
// =============================================================================

void EchenonModule::tick(uint32_t currentMillis) {
    (void)currentMillis;
    _signalLogger.begin();
    _signalLogger.tick(currentMillis);

    // Initialize mDNS responder so echenon.local resolves
    static bool s_mdnsStarted = false;
    if (!s_mdnsStarted && (WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_AP))) {
        if (MDNS.begin("echenon")) {
            MDNS.addService("http", "tcp", 80);
            s_mdnsStarted = true;
            ESP_LOGI(TAG, "mDNS responder started: http://echenon.local");
        }
    }
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
    _signalLogger.registerRoutes(server);

    // Serve dashboard JavaScript
    server.on("/echenon-dashboard.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response,
                              echenon_dashboard_js_gz,
                              echenon_dashboard_js_gz_len,
                              echenon_dashboard_js_gz_etag,
                              "application/javascript");
    });

    // Lightweight live endpoint for 10-channel decimated stats + event count
    server.on("/api/echenon/live", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
        DecimationRow* buf = _signalLogger.*get(SignalLogger_rideLogBuffer());
        size_t count = _signalLogger.*get(SignalLogger_rideLogCount());
        size_t head = _signalLogger.*get(SignalLogger_rideLogHead());
        size_t events = _signalLogger.*get(SignalLogger_eventLogCount());

        DecimationRow row;
        if (count > 0 && buf != nullptr) {
            constexpr size_t kRideLogMax = 36000;
            size_t lastIdx = (head + kRideLogMax - 1) % kRideLogMax;
            row = buf[lastIdx];
        } else {
            row = _signalLogger.*get(SignalLogger_currentRow());
        }

        JsonDocument doc;
        doc["timestamp_ms"] = row.timestamp_ms;
        doc["event_count"] = events;
        JsonArray channels = doc["channels"].to<JsonArray>();
        for (int c = 0; c < 10; c++) {
            JsonObject chObj = channels.add<JsonObject>();
            chObj["ch"] = c;
            chObj["min"] = (row.channels[c].min_val == 0xFFFF) ? 0 : row.channels[c].min_val;
            uint32_t mean = (row.channels[c].count > 0) ? (row.channels[c].sum / row.channels[c].count) : 0;
            chObj["mean"] = mean;
            chObj["max"] = row.channels[c].max_val;
        }

        String res;
        serializeJson(doc, res);
        return response->send(200, "application/json", res.c_str());
    });
}

void EchenonModule::registerTabs(ITabRegistry& registry) {
    registry.addTab(
        "Echenon",
        nullptr,
        50,
        "<polyline points=\"22 12 18 12 15 21 9 3 6 12 2 12\"></polyline>",
        "/echenon-dashboard.js"
    );
}

