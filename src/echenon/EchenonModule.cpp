#include "echenon/EchenonModule.h"
#include "ConfigManager.hpp"
#include <PsychicHttp.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_log.h>
#if !defined(UNIT_TEST) && defined(BOARD_HEADER)
#include <cstddef>
#include BOARD_HEADER
#endif
#include "utils/serveGzipAsset.hpp"
#include "include/portal_assets/echenon-dashboard_js.hpp"
#include "include/portal_assets/calibration-wizard_js.hpp"
#include "include/portal_assets/recalibration_js.hpp"

static const char* TAG = "EchenonModule";

#ifdef WITH_SIGNAL_LOGGER
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
#endif

using namespace echenon;

// =============================================================================
// ITickable
// =============================================================================

void EchenonModule::tick(uint32_t currentMillis) {
    (void)currentMillis;

    static bool s_sensorsStarted = false;
    if (!s_sensorsStarted) {
        _cadenceSensor.begin();
        _resistanceSensor.begin();
        _calibrationWizard.setResistanceSensor(&_resistanceSensor);
        _recalibrationUI.setSensors(&_resistanceSensor, &_cadenceSensor);
        s_sensorsStarted = true;
    }
    _cadenceSensor.tick(currentMillis);
    _resistanceSensor.tick(currentMillis);

    static bool s_powerStarted = false;
    if (!s_powerStarted) {
        _powerEngine.loadDefault();
        syncPhysicsCalibration();
        s_powerStarted = true;
    }
    const float wiperFloat = _resistanceSensor.getFilteredWiperFloat();
    const float pTilde = _calibrationWizard.wiperToNormalized(wiperFloat);
    const float cadenceRpm = _cadenceSensor.getRPM();
    _powerEngine.tickFromNormalized(pTilde, cadenceRpm);

    // Runaway detection
    if (_runawayDetector.tick(currentMillis, _resistanceSensor.getFilteredWiper())) {
        ESP_LOGW(TAG, "Runaway detected: wiper %u outside [%u, %u] for %u s",
                 _resistanceSensor.getFilteredWiper(),
                 RunawayDetector::RANGE_MIN,
                 RunawayDetector::RANGE_MAX,
                 _runawayDetector.getTimeoutSeconds());
    }

    // Compute Peloton resistance and virtual speed
    uint8_t pelotonRes = _calibrationWizard.isPosDone()
        ? echenon::PowerEngine::computePelotonResistanceFromNormalized(
              pTilde, _powerEngine.getPhysicsParams())
        : 0;
    float speedKph = echenon::PowerEngine::computeVirtualSpeedKph(_powerEngine.getLastPower());

#ifdef WITH_FTMS
    // Phase-offset LiveFeed: seed 250 ms after FTMS epoch so BLE (t=500,1000,...)
    // and WebSocket (t=750,1250,...) interleave every 250 ms.
    static bool s_ftmsPhaseSeeded = false;
    if (!s_ftmsPhaseSeeded && _ftmsServer) {
#ifndef UNIT_TEST
        _liveFeed.seedBroadcastTime(250);  // first LiveFeed fires at t=750 (250+500)
#endif
        s_ftmsPhaseSeeded = true;
    }
    if (_ftmsServer) {
        uint8_t ftmsInternalRes = _calibrationWizard.isPosDone()
            ? LevelMapping::normalizedToInternal(pTilde)
            : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
        int16_t ftmsPowerWatts = static_cast<int16_t>(std::round(_powerEngine.getLastPower()));
        _ftmsServer->updateTelemetry(speedKph, cadenceRpm,
                                      ftmsInternalRes,
                                      ftmsPowerWatts);
    }
#endif

#ifdef WITH_SESSION
    const char* sessionStateStr = "IDLE";
    uint32_t sessionElapsedS = 0;
    if (_sessionManager != nullptr) {
        if (_sessionManager->getState() == SessionState::ENDED && cadenceRpm > 0.0f) {
            _sessionManager->resetToIdle(currentMillis);
        }
        uint8_t sessInternalRes = _calibrationWizard.isPosDone()
            ? LevelMapping::normalizedToInternal(pTilde)
            : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
        _sessionManager->updateTelemetry(
            _powerEngine.getLastPower(),
            cadenceRpm,
            sessInternalRes,
            _calibrationWizard.isPosDone());
        sessionStateStr = echenon::sessionStateToString(_sessionManager->getState());
        sessionElapsedS = _sessionManager->getElapsedSec();
    }
#endif

#ifndef UNIT_TEST
#ifdef WITH_SESSION
    _liveFeed.tick(currentMillis,
                   cadenceRpm,
                   getEchelonLevel(),
                   getInternalLevel(),
                   _powerEngine.getLastPower(),
                   _resistanceSensor.getFilteredWiper(),
                   0,
                   0,
                   _runawayDetector.isRunaway(),
                   pelotonRes,
                   speedKph,
                   sessionStateStr,
                   sessionElapsedS);
#else
    _liveFeed.tick(currentMillis,
                   cadenceRpm,
                   getEchelonLevel(),
                   getInternalLevel(),
                   _powerEngine.getLastPower(),
                   _resistanceSensor.getFilteredWiper(),
                   0,
                   0,
                   _runawayDetector.isRunaway(),
                   pelotonRes,
                   speedKph);
#endif
#endif
#ifdef WITH_SIGNAL_LOGGER
    static bool s_eventBusWired = false;
    if (!s_eventBusWired) {
        extern Application app;
        _signalLogger.setEventBus(&app.getEventBus());
        s_eventBusWired = true;
    }
    _signalLogger.begin();
    _signalLogger.tick(currentMillis);
#endif

#ifdef WITH_CALIBRATION_RIG
    static bool s_rigStarted = false;
    if (!s_rigStarted) {
#if defined(PIN_RIG_ENC_A) && defined(PIN_RIG_ENC_B) && defined(PIN_RIG_CADENCE)
        _calibrationRig.begin(PIN_RIG_ENC_A, PIN_RIG_ENC_B, PIN_RIG_CADENCE);
#else
        _calibrationRig.begin(17, 18, 11);
#endif
        s_rigStarted = true;
    }
    _calibrationRig.tick(currentMillis, _resistanceSensor.getFilteredWiper());
#endif

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

void EchenonModule::syncPhysicsCalibration() {
    PowerPhysicsParams params = _powerEngine.getPhysicsParams();
    params.pot_min = _calibrationWizard.getPotMin();
    params.pot_max = _calibrationWizard.getPotMax();
    _recalibrationUI.applyToPhysicsParams(params);
    _powerEngine.setPhysicsParams(params);
}

void EchenonModule::applyConfig(const JsonObject& config) {
    if (config["cal_pos_done"].is<bool>()) {
        _calibrationWizard.setPosDone(config["cal_pos_done"].as<bool>());
    }
    if (config["runaway_timeout_s"].is<int>()) {
        _runawayDetector.setTimeoutSeconds(config["runaway_timeout_s"].as<uint16_t>());
    }
    if (config["cal_level_table"].is<JsonArrayConst>()) {
        JsonArrayConst tableArr = config["cal_level_table"].as<JsonArrayConst>();
        uint8_t lvl = 1;
        for (JsonVariantConst v : tableArr) {
            if (lvl <= 32) {
                _calibrationWizard.setTableEntry(lvl++, v.as<uint16_t>());
            }
        }
    }
    if (config["cal_recal_points"].is<JsonArrayConst>()) {
        JsonArrayConst ptsArr = config["cal_recal_points"].as<JsonArrayConst>();
        _recalibrationUI.reset();
        for (JsonObjectConst ptObj : ptsArr) {
            uint8_t lvl = ptObj["level"] | 0;
            float pwr = ptObj["power"] | 0.0f;
            uint8_t internal = ptObj["internal"] | 0;
            float cad = ptObj["cadence"] | 0.0f;
            uint16_t wiper = ptObj["wiper"] | 0;
            uint32_t ts = ptObj["timestamp"] | 0;
            _recalibrationUI.addPoint(lvl, pwr, internal, cad, wiper, ts);
        }
    }
    syncPhysicsCalibration();
#ifdef WITH_SIGNAL_LOGGER
    _signalLogger.applyConfig(config);
#endif
}

void EchenonModule::serializeConfig(JsonObject& config) const {
    config["cal_pos_done"] = _calibrationWizard.isPosDone();
    config["runaway_timeout_s"] = _runawayDetector.getTimeoutSeconds();
    JsonArray tableArr = config["cal_level_table"].to<JsonArray>();
    for (uint8_t l = 1; l <= 32; ++l) {
        tableArr.add(_calibrationWizard.getTableEntry(l));
    }
    JsonArray ptsArr = config["cal_recal_points"].to<JsonArray>();
    for (const auto& pt : _recalibrationUI.getPoints()) {
        JsonObject ptObj = ptsArr.add<JsonObject>();
        ptObj["level"] = pt.echelonLevel;
        ptObj["power"] = pt.reportedPower;
        ptObj["internal"] = pt.internalLevel;
        ptObj["cadence"] = pt.cadenceRPM;
        ptObj["wiper"] = pt.wiperCount;
        ptObj["timestamp"] = pt.timestamp;
    }
#ifdef WITH_SIGNAL_LOGGER
    _signalLogger.serializeConfig(config);
#endif
}

void EchenonModule::resetConfig() {
    _calibrationWizard.reset();
    _recalibrationUI.reset();
    syncPhysicsCalibration();
    _runawayDetector.setTimeoutSeconds(RunawayDetector::DEFAULT_TIMEOUT_S);
#ifdef WITH_SIGNAL_LOGGER
    _signalLogger.resetConfig();
#endif
}

// =============================================================================
// IWebContributor
// =============================================================================

void EchenonModule::registerRoutes(PsychicHttpServer& server) {
#ifndef UNIT_TEST
    // Create WS handler on the endpoint
    static PsychicWebSocketHandler wsHandler;
    server.on("/ws/echenon/live", &wsHandler);
    _liveFeed.setHandler(&wsHandler);
#endif

#ifdef WITH_SIGNAL_LOGGER
    _signalLogger.registerRoutes(server);
#endif

#ifdef WITH_CALIBRATION_RIG
    server.on("/api/echenon/rig/status", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        // JSON diagnostic status with GPIO readback
        JsonDocument doc;

        // State machine
        const char* stateStr = "IDLE";
        switch (_calibrationRig.getState()) {
            case echenon::CalibrationRig::State::IDLE:    stateStr = "IDLE"; break;
            case echenon::CalibrationRig::State::SEEKING: stateStr = "SEEKING"; break;
            case echenon::CalibrationRig::State::SETTLED: stateStr = "SETTLED"; break;
            case echenon::CalibrationRig::State::ERROR:   stateStr = "ERROR"; break;
        }
        doc["state"] = stateStr;
        doc["level"] = _calibrationRig.getCurrentLevel();
        doc["target"] = _calibrationRig.getTargetLevel();
        doc["cadence"] = _calibrationRig.getCadenceRPM();
        doc["steps_remaining"] = _calibrationRig.getStepsRemaining();
        doc["settle_timeout"] = _calibrationRig.didSettleTimeout();

        // Pot ADC (filtered via ResistanceSensor)
        uint16_t potFilt = _resistanceSensor.getFilteredWiper();
        doc["pot"] = potFilt;
        doc["derived_level"] = LevelMapping::wiperToEchelon(potFilt);

        // GPIO readback of OUTPUT pins
        JsonObject gpio = doc["gpio"].to<JsonObject>();
#ifdef WITH_CALIBRATION_RIG
        gpio["enc_a_pin"] = PIN_RIG_ENC_A;
        gpio["enc_a_val"] = digitalRead(PIN_RIG_ENC_A);
        gpio["enc_b_pin"] = PIN_RIG_ENC_B;
        gpio["enc_b_val"] = digitalRead(PIN_RIG_ENC_B);
        gpio["cad_pin"] = PIN_RIG_CADENCE;
        gpio["cad_val"] = digitalRead(PIN_RIG_CADENCE);
#endif

        // Passive ADC sensors
        JsonObject adc = doc["adc"].to<JsonObject>();
        adc["pot_raw"] = analogRead(PIN_POT_WIPER);
        adc["cad_sense"] = analogRead(PIN_CADENCE);
        adc["enc_vcc"] = analogRead(PIN_KNOB_ENC_VCC);
        adc["enc_a_sense"] = analogRead(PIN_KNOB_ENC_A);
        adc["enc_b_sense"] = analogRead(PIN_KNOB_ENC_B);

        String res;
        serializeJson(doc, res);
        return response->send(200, "application/json", res.c_str());
    });

    // HTTP command endpoint — allows sending rig commands over WiFi when serial is dead
    server.on("/api/echenon/rig/command", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        String body = request->body();
        body.trim();
        if (body.isEmpty()) {
            return response->send(400, "text/plain", "ERROR empty command");
        }
        _calibrationRig.handleCommand(body);
        // Return current status as JSON response
        String status = _calibrationRig.handleStatus();
        return response->send(200, "text/plain", status.c_str());
    });
#endif

    // Serve dashboard JavaScript
    server.on("/echenon-dashboard.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response,
                              echenon_dashboard_js_gz,
                              echenon_dashboard_js_gz_len,
                              echenon_dashboard_js_gz_etag,
                              "application/javascript");
    });

    // Serve calibration wizard JavaScript
    server.on("/calibration-wizard.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response,
                              calibration_wizard_js_gz,
                              calibration_wizard_js_gz_len,
                              calibration_wizard_js_gz_etag,
                              "application/javascript");
    });

    // Serve recalibration JavaScript
    server.on("/recalibration.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response,
                              recalibration_js_gz,
                              recalibration_js_gz_len,
                              recalibration_js_gz_etag,
                              "application/javascript");
    });

    // --- Calibration Wizard Endpoints ---
    server.on("/api/echenon/calibration/start", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        _calibrationWizard.start();
        JsonDocument doc;
        doc["success"] = true;
        doc["active"] = _calibrationWizard.isActive();
        doc["step"] = _calibrationWizard.getCurrentStep();
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    server.on("/api/echenon/calibration/stop", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        _calibrationWizard.stop();
        JsonDocument doc;
        doc["success"] = true;
        doc["active"] = false;
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    server.on("/api/echenon/calibration/record-level", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        const char* body = request->bodyCStr();
        JsonDocument inDoc;
        DeserializationError err = deserializeJson(inDoc, body ? body : "");
        if (err || !inDoc["level"].is<int>()) {
            return response->send(400, "application/json", "{\"success\":false,\"error\":\"Missing level parameter\"}");
        }
        uint8_t level = inDoc["level"].as<uint8_t>();
        bool ok = _calibrationWizard.recordLevel(level);
        if (ok) {
            syncPhysicsCalibration();
        }
        JsonDocument outDoc;
        outDoc["success"] = ok;
        outDoc["step"] = _calibrationWizard.getCurrentStep();
        outDoc["complete"] = _calibrationWizard.isComplete();
        if (!ok) {
            outDoc["error"] = "Invalid level or calibration not active";
        }
        String out;
        serializeJson(outDoc, out);
        return response->send(ok ? 200 : 400, "application/json", out.c_str());
    });

    server.on("/api/echenon/calibration/status", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        JsonDocument doc;
        doc["active"] = _calibrationWizard.isActive();
        doc["step"] = _calibrationWizard.getCurrentStep();
        doc["complete"] = _calibrationWizard.isComplete();
        doc["pos_done"] = _calibrationWizard.isPosDone();
        JsonArray tblArr = doc["table"].to<JsonArray>();
        const uint16_t* tbl = _calibrationWizard.getTable();
        for (int i = 0; i < 32; ++i) {
            tblArr.add(tbl[i]);
        }
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    // --- Recalibration UI Endpoints ---
    server.on("/api/echenon/recalibration/point", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        const char* body = request->bodyCStr();
        JsonDocument inDoc;
        DeserializationError err = deserializeJson(inDoc, body ? body : "");
        if (err || !inDoc["level"].is<int>() || (!inDoc["power"].is<float>() && !inDoc["power"].is<int>())) {
            return response->send(400, "application/json", "{\"success\":false,\"error\":\"Missing level or power\"}");
        }
        uint8_t level = inDoc["level"].as<uint8_t>();
        float power = inDoc["power"].as<float>();
        bool ok = _recalibrationUI.addPoint(level, power);
        if (ok) {
            syncPhysicsCalibration();
        }
        JsonDocument outDoc;
        outDoc["success"] = ok;
        outDoc["count"] = _recalibrationUI.getPointCount();
        if (!ok) {
            outDoc["error"] = "Failed to add point (invalid level or capacity reached)";
        }
        String out;
        serializeJson(outDoc, out);
        return response->send(ok ? 200 : 400, "application/json", out.c_str());
    });

    server.on("/api/echenon/recalibration/points", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        JsonDocument doc;
        doc["count"] = _recalibrationUI.getPointCount();
        doc["max"] = _recalibrationUI.getMaxPoints();
        JsonArray ptsArr = doc["points"].to<JsonArray>();
        for (const auto& pt : _recalibrationUI.getPoints()) {
            JsonObject obj = ptsArr.add<JsonObject>();
            obj["level"] = pt.echelonLevel;
            obj["power"] = pt.reportedPower;
            obj["internal"] = pt.internalLevel;
            obj["cadence"] = pt.cadenceRPM;
            obj["wiper"] = pt.wiperCount;
            obj["timestamp"] = pt.timestamp;
        }
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    server.on("/api/echenon/recalibration/reset", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        _recalibrationUI.reset();
        syncPhysicsCalibration();
        return response->send(200, "application/json", "{\"success\":true,\"count\":0}");
    });

    // --- Power Table Upload & Active Preview ---
    server.on("/api/echenon/power-table/upload", HTTP_POST, [this](PsychicRequest* request, PsychicResponse* response) {
        const char* body = request->bodyCStr();
        size_t len = request->contentLength();
        if (!body || len == 0) {
            return response->send(400, "application/json", "{\"success\":false,\"error\":\"Empty body\"}");
        }
        PowerTable newTable;
        bool ok = newTable.parseCSV(body, len);
        if (!ok) {
            return response->send(400, "application/json", "{\"success\":false,\"error\":\"Failed to parse CSV power table\"}");
        }
        _powerEngine.setTable(newTable);
        return response->send(200, "application/json", "{\"success\":true,\"message\":\"Power table loaded\"}");
    });

    server.on("/api/echenon/power-table/active", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        (void)request;
        const PowerTable& tbl = _powerEngine.getTable();
        JsonDocument doc;
        doc["tier"] = static_cast<uint8_t>(tbl.tier);
        doc["id"] = tbl.id;
        doc["levels"] = POWER_TABLE_LEVELS;
        doc["rpm_bins"] = POWER_TABLE_RPM_BINS;
        doc["rpm_step"] = POWER_TABLE_RPM_STEP;
        doc["rpm_max"] = POWER_TABLE_RPM_MAX;
        JsonArray wattsArr = doc["watts"].to<JsonArray>();
        for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
            JsonArray rowArr = wattsArr.add<JsonArray>();
            for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
                rowArr.add(tbl.watts[l][r]);
            }
        }
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

#ifdef WITH_SIGNAL_LOGGER
    // Lightweight live endpoint for 8-channel decimated stats + event count
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
        static const int live_adc_channels[] = {0, 1, 2, 3, 4, 5, 6, 9};
        for (int s = 0; s < 8; s++) {
            JsonObject chObj = channels.add<JsonObject>();
            chObj["ch"] = live_adc_channels[s];
            chObj["min"] = (row.channels[s].min_val == 0xFFFF) ? 0 : row.channels[s].min_val;
            uint32_t mean = (row.channels[s].count > 0) ? (row.channels[s].sum / row.channels[s].count) : 0;
            chObj["mean"] = mean;
            chObj["max"] = row.channels[s].max_val;
        }

        String res;
        serializeJson(doc, res);
        return response->send(200, "application/json", res.c_str());
    });
#endif
}

void EchenonModule::registerTabs(ITabRegistry& registry) {
    registry.addTab(
        "Echenon",
        nullptr,
        50,
        "<polyline points=\"22 12 18 12 15 21 9 3 6 12 2 12\"></polyline>",
        "/echenon-dashboard.js"
    );

    // Setup tab (order 60) with PillBar sub-navigation
    registry.addTab(
        "Setup",
        nullptr,
        60,
        "<circle cx=\"12\" cy=\"12\" r=\"3\"></circle>"
        "<path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z\"></path>",
        nullptr
    );

    // Pill sub-tabs under Setup
    registry.addTab("Apps",          "Setup", 10, nullptr, "/upload_settings.js");
    registry.addTab("Level Mapping", "Setup", 20, nullptr, "/calibration-wizard.js");
    registry.addTab("Power Tuning",  "Setup", 30, nullptr, "/recalibration.js");
}
