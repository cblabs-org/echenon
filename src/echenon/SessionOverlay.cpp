/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/SessionOverlay.cpp
 * Description: Implementation of session and upload REST API endpoints, portal tab registration, and IWebContributor wiring.
 */

#include "echenon/SessionOverlay.h"
#include "echenon/IntervalsIcuUploader.h"
#include "echenon/WahooUploader.h"
#include "echenon/LiveFeed.h"
#include "interfaces/ITabRegistry.hpp"

// Unity-build inclusion: In native test builds, SessionOverlay.cpp is included
// directly from the test file. IntervalsIcuUploader.cpp and WahooUploader.cpp are
// not included by any test file, so we pull them in here to satisfy linker symbols
// for the Intervals and Wahoo route handlers.
#ifdef UNIT_TEST
#include "echenon/IntervalsIcuUploader.cpp"
#include "echenon/WahooUploader.cpp"
#endif

#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#ifndef UNIT_TEST
#include <Arduino.h>
#include <PsychicHttp.h>
#include <PsychicWebSocket.h>
#include "AuthManager.hpp"
#include "ConfigManager.hpp"
#include "utils/serveGzipAsset.hpp"
#if __has_include("include/portal_assets/echenon_session_js.h")
#include "include/portal_assets/echenon_session_js.h"
#else
#include "include/portal_assets/session_js.hpp"
#endif
#if __has_include("include/portal_assets/echenon_upload_js.h")
#include "include/portal_assets/echenon_upload_js.h"
#else
#include "include/portal_assets/upload_settings_js.hpp"
#endif
#include "include/portal_assets/ride_html.hpp"
#include "include/portal_assets/ride_js.hpp"
#include "include/portal_assets/manifest_json.hpp"
#if __has_include("include/portal_assets/favicon_svg.hpp")
#include "include/portal_assets/favicon_svg.hpp"
#endif
#if __has_include("include/portal_assets/favicon_ico.hpp")
#include "include/portal_assets/favicon_ico.hpp"
#endif
#if __has_include("include/portal_assets/icon-192_png.hpp")
#include "include/portal_assets/icon-192_png.hpp"
#endif
#if __has_include("include/portal_assets/icon-512_png.hpp")
#include "include/portal_assets/icon-512_png.hpp"
#endif
#include "include/portal_assets/stored_rides_js.hpp"
#if __has_include("include/portal_assets/intervals_settings_js.h")
#include "include/portal_assets/intervals_settings_js.h"
#endif
#if __has_include("include/portal_assets/wahoo_settings_js.hpp")
#include "include/portal_assets/wahoo_settings_js.hpp"
#elif __has_include("include/portal_assets/wahoo_settings_js.h")
#include "include/portal_assets/wahoo_settings_js.h"
#endif
#endif

namespace echenon {

// =============================================================================
// LiveFeed 13-Field JSON Formatter & 13-Argument tick() Overloads
// =============================================================================

std::string SessionOverlay::formatLiveFeedJson(float cadenceRpm,
                                               uint8_t levelEchelon,
                                               uint8_t levelInternal,
                                               float powerWatts,
                                               uint16_t potAdc,
                                               int8_t motorDir,
                                               int32_t encoderDelta,
                                               bool runaway,
                                               uint8_t pelotonResistance,
                                               float speedKph,
                                               const char* sessionState,
                                               uint32_t sessionElapsedS,
                                               uint32_t currentMillis,
                                               uint8_t hrBpm) {
    const char* safeState = (sessionState && sessionState[0] != '\0') ? sessionState : "IDLE";
    char buf[384];
    int len = std::snprintf(
        buf,
        sizeof(buf),
        "{\"cadence_rpm\":%.1f,\"level_echelon\":%u,\"level_i\":%u,\"power_watts\":%.1f,"
        "\"pot_adc\":%u,\"motor_dir\":%d,\"encoder_delta\":%ld,\"runaway\":%s,"
        "\"peloton_resistance\":%u,\"speed_kph\":%.1f,\"session_state\":\"%s\","
        "\"session_elapsed_s\":%lu,\"hr\":%u,\"timestamp_ms\":%lu}",
        static_cast<double>(cadenceRpm),
        static_cast<unsigned int>(levelEchelon),
        static_cast<unsigned int>(levelInternal),
        static_cast<double>(powerWatts),
        static_cast<unsigned int>(potAdc),
        static_cast<int>(motorDir),
        static_cast<long>(encoderDelta),
        runaway ? "true" : "false",
        static_cast<unsigned int>(pelotonResistance),
        static_cast<double>(speedKph),
        safeState,
        static_cast<unsigned long>(sessionElapsedS),
        static_cast<unsigned int>(hrBpm),
        static_cast<unsigned long>(currentMillis));

    if (len <= 0) {
        return "{}";
    }
    size_t outLen = (static_cast<size_t>(len) < sizeof(buf))
                        ? static_cast<size_t>(len)
                        : (sizeof(buf) - 1);
    return std::string(buf, outLen);
}

// LiveFeed::tick() overloads are defined in LiveFeed.cpp (canonical home).

// =============================================================================
// Session Lifecycle & History Handlers (Native-Testable)
// =============================================================================

SessionOverlay::HttpResponse SessionOverlay::handleGetStatus() const {
    HttpResponse resp;
    JsonDocument doc;

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    const RideStore* rs = resolveRideStore();
    uint16_t recCount = (rs != nullptr && rs->isSessionActive()) ? rs->getRecordCount() : 0;

    uint16_t avgW   = _sessionManager->getAvgPowerWatts();
    uint16_t maxW   = _sessionManager->getMaxPowerWatts();
    uint16_t workKj = _sessionManager->getTotalWorkKj();

    if (_sessionManager->getState() == SessionState::ENDED) {
        const RideSummary& last = _sessionManager->getLastSummary();
        recCount = last.record_count;
        avgW     = last.avg_power_w;
        maxW     = last.max_power_w;
        workKj   = last.total_work_kj;
    }

    doc["state"]                  = sessionStateToString(_sessionManager->getState());
    doc["session_id"]             = _sessionManager->getSessionId();
    doc["start_epoch_s"]          = _sessionManager->getStartEpochS();
    doc["elapsed_s"]              = _sessionManager->getElapsedSec();
    doc["record_count"]           = recCount;
    doc["avg_power_w"]            = avgW;
    doc["avg_watts"]              = avgW;
    doc["max_power_w"]            = maxW;
    doc["max_watts"]              = maxW;
    doc["total_work_kj"]          = workKj;
    doc["calories_kcal"]          = workKj;
    doc["auto_start_enabled"]     = _sessionManager->isAutoStartEnabled();
    doc["auto_start"]             = _sessionManager->isAutoStartEnabled();
    doc["auto_start_delay_sec"]   = _sessionManager->getAutoStartDelaySec();
    doc["inactivity_timeout_sec"] = _sessionManager->getInactivityTimeoutSec();
    doc["pause_timeout_s"]        = _sessionManager->getInactivityTimeoutSec();
    doc["auto_upload_enabled"]    = (_uploadQueue != nullptr) ? _uploadQueue->isAutoUploadEnabled() : true;

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleStart(uint32_t now_epoch_s, uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_sessionManager->startSession(now_epoch_s, now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "Session cannot be started in current state";
        doc["state"] = sessionStateToString(_sessionManager->getState());
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["state"]      = sessionStateToString(_sessionManager->getState());
    doc["session_id"] = _sessionManager->getSessionId();
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handlePause(uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_sessionManager->pauseSession(now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "Session is not active";
        doc["state"] = sessionStateToString(_sessionManager->getState());
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["state"]      = sessionStateToString(_sessionManager->getState());
    doc["session_id"] = _sessionManager->getSessionId();
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleResume(uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_sessionManager->resumeSession(now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "Session is not paused";
        doc["state"] = sessionStateToString(_sessionManager->getState());
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["state"]      = sessionStateToString(_sessionManager->getState());
    doc["session_id"] = _sessionManager->getSessionId();
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleEnd(uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_sessionManager->endSession(now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "No active or paused session to end";
        doc["state"] = sessionStateToString(_sessionManager->getState());
        serializeJson(doc, resp.body);
        return resp;
    }

    const RideSummary& summary = _sessionManager->getLastSummary();

    if (_uploadQueue != nullptr && _uploadQueue->isAutoUploadEnabled() &&
        summary.session_id != 0 && _uploadQueue->findJob(summary.session_id) == nullptr) {
        _uploadQueue->enqueueSession(summary.session_id, now_ms);
    }

    doc["ok"]                = true;
    doc["state"]             = sessionStateToString(_sessionManager->getState());
    doc["session_id"]        = summary.session_id;
    doc["duration_s"]        = summary.duration_s;
    doc["active_duration_s"] = summary.active_duration_s;
    doc["record_count"]      = summary.record_count;
    doc["avg_power_w"]       = summary.avg_power_w;
    doc["max_power_w"]       = summary.max_power_w;
    doc["total_work_kj"]     = summary.total_work_kj;
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleDiscard(uint32_t session_id, uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (session_id != 0) {
        bool removed = false;
        if (_sessionManager != nullptr &&
            _sessionManager->getState() != SessionState::IDLE &&
            _sessionManager->getSessionId() == session_id) {
            removed = _sessionManager->discardSession(now_ms);
        }
        RideStore* rs = resolveRideStore();
        if (rs != nullptr && rs->discardSession(session_id)) {
            removed = true;
        }
        if (_uploadQueue != nullptr) {
            _uploadQueue->discardSession(session_id);
        }
        if (!removed) {
            resp.status_code = 404;
            doc["ok"] = false;
            doc["error"] = "Session not found";
            serializeJson(doc, resp.body);
            return resp;
        }
        doc["ok"]         = true;
        doc["session_id"] = session_id;
        serializeJson(doc, resp.body);
        return resp;
    }

    if (_sessionManager == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "SessionManager not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    uint32_t activeId = _sessionManager->getSessionId();
    if (!_sessionManager->discardSession(now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "No active session to discard";
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["session_id"] = activeId;
    doc["state"]      = sessionStateToString(_sessionManager->getState());
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleConfig(const std::string& json_body) {
    HttpResponse resp;
    JsonDocument inDoc;
    DeserializationError err = deserializeJson(inDoc, json_body);
    if (err || !inDoc.is<JsonObject>()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Invalid JSON body";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    bool anyField = false;

    if (_sessionManager != nullptr) {
        if (inDoc["auto_start_enabled"].is<bool>()) {
            _sessionManager->setAutoStartEnabled(inDoc["auto_start_enabled"].as<bool>());
            anyField = true;
        } else if (inDoc["auto_start"].is<bool>()) {
            _sessionManager->setAutoStartEnabled(inDoc["auto_start"].as<bool>());
            anyField = true;
        }

        if (inDoc["auto_start_delay_sec"].is<int>()) {
            int val = inDoc["auto_start_delay_sec"].as<int>();
            if (val <= 0 || val > 3600) {
                resp.status_code = 400;
                JsonDocument errDoc;
                errDoc["ok"] = false;
                errDoc["error"] = "auto_start_delay_sec out of range";
                serializeJson(errDoc, resp.body);
                return resp;
            }
            _sessionManager->setAutoStartDelaySec(static_cast<uint16_t>(val));
            anyField = true;
        }

        if (inDoc["inactivity_timeout_sec"].is<int>() || inDoc["pause_timeout_s"].is<int>()) {
            int val = inDoc["inactivity_timeout_sec"].is<int>()
                          ? inDoc["inactivity_timeout_sec"].as<int>()
                          : inDoc["pause_timeout_s"].as<int>();
            if (val <= 0 || val > 86400) {
                resp.status_code = 400;
                JsonDocument errDoc;
                errDoc["ok"] = false;
                errDoc["error"] = "inactivity_timeout_sec out of range";
                serializeJson(errDoc, resp.body);
                return resp;
            }
            _sessionManager->setInactivityTimeoutSec(static_cast<uint16_t>(val));
            anyField = true;
        }
    }

    if (_uploadQueue != nullptr) {
        if (inDoc["auto_upload_enabled"].is<bool>()) {
            _uploadQueue->setAutoUploadEnabled(inDoc["auto_upload_enabled"].as<bool>());
            anyField = true;
        } else if (inDoc["auto_upload"].is<bool>()) {
            _uploadQueue->setAutoUploadEnabled(inDoc["auto_upload"].as<bool>());
            anyField = true;
        }
    }

    if (!anyField) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "No recognized configuration fields provided";
        serializeJson(errDoc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        if (_sessionManager != nullptr) {
            _configManager->saveModuleConfig(_sessionManager);
        }
        if (_uploadQueue != nullptr) {
            _configManager->saveModuleConfig(_uploadQueue);
        }
    }
#endif

    JsonDocument outDoc;
    outDoc["ok"] = true;
    if (_sessionManager != nullptr) {
        outDoc["auto_start_enabled"]     = _sessionManager->isAutoStartEnabled();
        outDoc["auto_start"]             = _sessionManager->isAutoStartEnabled();
        outDoc["auto_start_delay_sec"]   = _sessionManager->getAutoStartDelaySec();
        outDoc["inactivity_timeout_sec"] = _sessionManager->getInactivityTimeoutSec();
        outDoc["pause_timeout_s"]        = _sessionManager->getInactivityTimeoutSec();
    }
    if (_uploadQueue != nullptr) {
        outDoc["auto_upload_enabled"] = _uploadQueue->isAutoUploadEnabled();
    }
    serializeJson(outDoc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleGetHistory() const {
    HttpResponse resp;
    JsonDocument doc;
    JsonArray ridesArr = doc["rides"].to<JsonArray>();

    const RideStore* rs = resolveRideStore();
    if (rs != nullptr) {
        const auto& rides = rs->getStoredRides();
        for (const auto& r : rides) {
            JsonObject obj = ridesArr.add<JsonObject>();
            obj["session_id"]        = r.session_id;
            obj["start_epoch_s"]     = r.start_epoch_s;
            obj["duration_s"]        = r.duration_s;
            obj["active_duration_s"] = r.active_duration_s;
            obj["record_count"]      = r.record_count;
            obj["avg_power_w"]       = r.avg_power_w;
            obj["max_power_w"]       = r.max_power_w;
            obj["avg_cadence_rpm"]   = r.avg_cadence_rpm;
            obj["avg_hr_bpm"]        = r.avg_hr_bpm;
            obj["max_hr_bpm"]        = r.max_hr_bpm;
            obj["total_work_kj"]     = r.total_work_kj;
            obj["ntp_valid"]         = r.ntp_valid;

            const char* uploadState = "NONE";
            if (_uploadQueue != nullptr) {
                const UploadJob* job = _uploadQueue->findJob(r.session_id);
                if (job != nullptr) {
                    if (job->stage == UploadStage::STAGE_COMPLETE) {
                        uploadState = "COMPLETE";
                    } else if (job->stage == UploadStage::STAGE_FAILED) {
                        uploadState = "FAILED";
                    } else {
                        uploadState = "PENDING";
                    }
                    obj["upload_stage"] = uploadStageToString(job->stage);
                    if (!job->remote_activity_id.empty()) {
                        obj["activity_id"] = job->remote_activity_id;
                    }
                }

                // Per-sink sync status array for multi-sink reporting
                JsonArray syncArr = obj["sync"].to<JsonArray>();
                for (size_t si = 0; si < _uploadQueue->getSinkCount(); ++si) {
                    IUploader* sink = _uploadQueue->getSink(si);
                    if (sink == nullptr) continue;
                    const UploadJob* sinkJob = _uploadQueue->findJob(r.session_id, sink->serviceId());
                    JsonObject syncObj = syncArr.add<JsonObject>();
                    syncObj["service_id"] = sink->serviceId();
                    if (sinkJob != nullptr) {
                        if (sinkJob->stage == UploadStage::STAGE_COMPLETE) {
                            syncObj["status"] = "COMPLETE";
                        } else if (sinkJob->stage == UploadStage::STAGE_FAILED) {
                            syncObj["status"] = "FAILED";
                        } else {
                            syncObj["status"] = "PENDING";
                        }
                        syncObj["remote_id"] = sinkJob->remote_activity_id;
                        syncObj["remote_url"] = "";  // URL construction is sink-specific
                        syncObj["error"] = sinkJob->last_error;
                    } else {
                        syncObj["status"] = "NONE";
                        syncObj["remote_id"] = "";
                        syncObj["remote_url"] = "";
                        syncObj["error"] = "";
                    }
                }
            }
            obj["upload_state"] = uploadState;
        }
    }

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleDownload(uint32_t session_id, bool gzip) const {
    HttpResponse resp;
    const RideStore* rs = resolveRideStore();
    if (rs == nullptr || session_id == 0 || rs->findStoredRide(session_id) == nullptr) {
        resp.status_code = 404;
        resp.body = "{\"ok\":false,\"error\":\"Ride session not found\"}";
        return resp;
    }

    std::string streamBuf;
    auto writeChunk = [&streamBuf](const uint8_t* data, size_t len) -> bool {
        if (data != nullptr && len > 0) {
            streamBuf.append(reinterpret_cast<const char*>(data), len);
        }
        return true;
    };

    bool ok = gzip ? TCXGenerator::generateTcxGzFromStore(*rs, session_id, writeChunk)
                   : TCXGenerator::generateTcxFromStore(*rs, session_id, writeChunk);
    if (!ok) {
        resp.status_code = 500;
        resp.body = "{\"ok\":false,\"error\":\"Failed to generate TCX stream\"}";
        return resp;
    }

    resp.status_code = 200;
    resp.content_type = gzip ? "application/gzip" : "application/vnd.garmin.tcx+xml";
    resp.content_disposition = "attachment; filename=\"echenon_" + std::to_string(session_id) +
                               (gzip ? ".tcx.gz\"" : ".tcx\"");
    if (gzip) {
        resp.content_encoding = "gzip";
    }
    resp.body = std::move(streamBuf);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleUpload(uint32_t session_id, uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    const RideStore* rs = resolveRideStore();
    if (rs == nullptr || session_id == 0 || rs->findStoredRide(session_id) == nullptr) {
        resp.status_code = 404;
        doc["ok"] = false;
        doc["error"] = "Ride session not found";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (_uploadQueue == nullptr) {
        resp.status_code = 503;
        doc["ok"] = false;
        doc["error"] = "Upload queue not available";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_uploadQueue->enqueueSession(session_id, now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "Failed to enqueue session for upload";
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["session_id"] = session_id;
    doc["state"]      = "PENDING";
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleUpload(uint32_t session_id, const std::string& service_id, uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    // If no service_id, fall back to all-sinks enqueue
    if (service_id.empty()) {
        return handleUpload(session_id, now_ms);
    }

    const RideStore* rs = resolveRideStore();
    if (rs == nullptr || session_id == 0 || rs->findStoredRide(session_id) == nullptr) {
        resp.status_code = 404;
        doc["ok"] = false;
        doc["error"] = "Ride session not found";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (_uploadQueue == nullptr) {
        resp.status_code = 503;
        doc["ok"] = false;
        doc["error"] = "Upload queue not available";
        serializeJson(doc, resp.body);
        return resp;
    }

    // Verify the requested sink exists
    IUploader* sink = _uploadQueue->findSinkById(service_id.c_str());
    if (sink == nullptr) {
        resp.status_code = 404;
        doc["ok"] = false;
        doc["error"] = "Unknown service_id";
        serializeJson(doc, resp.body);
        return resp;
    }

    // Remove any existing job for this (session_id, service_id) and re-enqueue
    // For now, enqueue all sinks — the job will be created for any missing sinks
    if (!_uploadQueue->enqueueSession(session_id, now_ms)) {
        resp.status_code = 409;
        doc["ok"] = false;
        doc["error"] = "Failed to enqueue session for upload";
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]         = true;
    doc["session_id"] = session_id;
    doc["service_id"] = service_id;
    doc["state"]      = "PENDING";
    serializeJson(doc, resp.body);
    return resp;
}

// =============================================================================
// Strava OAuth2 & Credentials Handlers (Native-Testable)
// =============================================================================

SessionOverlay::HttpResponse SessionOverlay::handleGetStravaStatus(uint32_t now_epoch_s) const {
    HttpResponse resp;
    JsonDocument doc;

    bool configured    = (_stravaUploader != nullptr) && _stravaUploader->isConfigured();
    bool authenticated = (_stravaUploader != nullptr) && _stravaUploader->isAuthenticated();
    bool tokenValid    = (_stravaUploader != nullptr) && _stravaUploader->isTokenValid(now_epoch_s);

    doc["configured"]    = configured;
    doc["authenticated"] = authenticated;
    doc["connected"]     = authenticated;
    doc["token_valid"]   = tokenValid;
    doc["client_id"]     = (_stravaUploader != nullptr) ? _stravaUploader->getClientId() : "";
    doc["athlete_name"]  = (_stravaUploader != nullptr) ? _stravaUploader->getAthleteName() : "";
    doc["expires_at"]    = (_stravaUploader != nullptr) ? _stravaUploader->getTokenExpiresAt() : 0u;
    doc["auto_upload"]   = (_uploadQueue != nullptr) ? _uploadQueue->isAutoUploadEnabled() : true;
    doc["queue_size"]    = (_uploadQueue != nullptr) ? _uploadQueue->getQueueSize() : 0u;
    doc["queue_depth"]   = (_uploadQueue != nullptr) ? _uploadQueue->getQueueSize() : 0u;
    doc["pending_count"] = (_uploadQueue != nullptr) ? _uploadQueue->getPendingCount() : 0u;

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleSaveStravaCredentials(const std::string& json_body) {
    HttpResponse resp;
    JsonDocument inDoc;
    DeserializationError err = deserializeJson(inDoc, json_body);
    if (err || !inDoc.is<JsonObject>()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Invalid JSON body";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    std::string clientId     = inDoc["client_id"] | "";
    std::string clientSecret = inDoc["client_secret"] | "";

    if (clientId.empty() || clientSecret.empty()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "client_id and client_secret are required";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (_stravaUploader == nullptr) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "StravaUploader not initialized";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (!_stravaUploader->setClientCredentials(clientId, clientSecret)) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Failed to store Strava credentials";
        serializeJson(errDoc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_stravaUploader);
    }
#endif

    JsonDocument outDoc;
    outDoc["ok"]         = true;
    outDoc["configured"] = _stravaUploader->isConfigured();
    outDoc["client_id"]  = _stravaUploader->getClientId();
    serializeJson(outDoc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleGetStravaAuthUrl(const std::string& redirect_uri,
                                                                    uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_stravaUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "StravaUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    std::string url = _stravaUploader->buildAuthorizeUrl(redirect_uri, now_ms);
    if (url.empty()) {
        resp.status_code = 400;
        doc["ok"] = false;
        doc["error"] = "Strava client_id not configured or invalid redirect_uri";
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]            = true;
    doc["url"]           = url;
    doc["authorize_url"] = url;
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleStravaCallback(const std::string& code,
                                                                  const std::string& state,
                                                                  uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_stravaUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "StravaUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_stravaUploader->validateAndConsumeState(state, now_ms)) {
        resp.status_code = 403;
        doc["ok"] = false;
        doc["error"] = "Invalid or expired OAuth state";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (code.empty()) {
        resp.status_code = 400;
        doc["ok"] = false;
        doc["error"] = "Missing authorization code";
        serializeJson(doc, resp.body);
        return resp;
    }

    std::string errMsg;
    UploadStageResult res = _stravaUploader->exchangeAuthCode(code, 5000, errMsg);
    if (res != UploadStageResult::SUCCESS) {
        resp.status_code = 502;
        doc["ok"] = false;
        doc["error"] = errMsg.empty() ? "Token exchange failed" : errMsg;
        serializeJson(doc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_stravaUploader);
    }
#endif

    doc["ok"]            = true;
    doc["authenticated"] = _stravaUploader->isAuthenticated();
    doc["athlete_name"]  = _stravaUploader->getAthleteName();
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleStravaDisconnect() {
    HttpResponse resp;
    JsonDocument doc;

    if (_stravaUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "StravaUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    _stravaUploader->disconnect();

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_stravaUploader);
    }
#endif

    doc["ok"]            = true;
    doc["authenticated"] = false;
    serializeJson(doc, resp.body);
    return resp;
}

// =============================================================================
// Multi-Sink Upload Status Handlers (Native-Testable)
// =============================================================================

SessionOverlay::HttpResponse SessionOverlay::handleGetUploadSinks() const {
    HttpResponse resp;
    JsonDocument doc;
    JsonArray sinksArr = doc["sinks"].to<JsonArray>();

    if (_uploadQueue != nullptr) {
        for (size_t i = 0; i < _uploadQueue->getSinkCount(); ++i) {
            IUploader* sink = _uploadQueue->getSink(i);
            if (sink == nullptr) continue;
            JsonObject sObj = sinksArr.add<JsonObject>();
            sObj["service_id"]    = sink->serviceId();
            sObj["name"]          = sink->name();
            sObj["configured"]    = sink->isConfigured();
            sObj["authenticated"] = sink->isAuthenticated();
            sObj["enabled"]       = sink->isEnabled();
        }
    }

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleGetUploadStatus() const {
    HttpResponse resp;
    JsonDocument doc;

    doc["pending_count"] = (_uploadQueue != nullptr) ? static_cast<uint32_t>(_uploadQueue->getPendingCount()) : 0u;

    JsonArray sinksArr = doc["sinks"].to<JsonArray>();
    if (_uploadQueue != nullptr) {
        for (size_t i = 0; i < _uploadQueue->getSinkCount(); ++i) {
            IUploader* sink = _uploadQueue->getSink(i);
            if (sink == nullptr) continue;
            JsonObject sObj = sinksArr.add<JsonObject>();
            sObj["service_id"]    = sink->serviceId();
            sObj["configured"]    = sink->isConfigured();
            sObj["authenticated"] = sink->isAuthenticated();
            sObj["enabled"]       = sink->isEnabled();
        }
    }

    // Aggregate last_error: pick the most recent error from any job in the queue
    std::string lastError;
    if (_uploadQueue != nullptr) {
        const auto& jobs = _uploadQueue->getJobs();
        for (const auto& job : jobs) {
            if (!job.last_error.empty()) {
                lastError = job.last_error;
            }
        }
    }
    doc["last_error"] = lastError;

    serializeJson(doc, resp.body);
    return resp;
}

// =============================================================================
// Intervals.icu Credential Management Handlers (Native-Testable)
// =============================================================================

SessionOverlay::HttpResponse SessionOverlay::handleGetIntervalsStatus() const {
    HttpResponse resp;
    JsonDocument doc;

    bool configured    = (_intervalsUploader != nullptr) && _intervalsUploader->isConfigured();
    bool authenticated = (_intervalsUploader != nullptr) && _intervalsUploader->isAuthenticated();

    doc["configured"]    = configured;
    doc["authenticated"] = authenticated;
    doc["athlete_id"]    = (_intervalsUploader != nullptr) ? _intervalsUploader->getAthleteId() : "";
    doc["athlete_name"]  = (_intervalsUploader != nullptr) ? _intervalsUploader->getAthleteName() : "";
    doc["enabled"]       = (_intervalsUploader != nullptr) ? _intervalsUploader->isEnabled() : false;

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleSaveIntervalsCredentials(const std::string& json_body) {
    HttpResponse resp;
    JsonDocument inDoc;
    DeserializationError err = deserializeJson(inDoc, json_body);
    if (err || !inDoc.is<JsonObject>()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Invalid JSON body";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    std::string athleteId = inDoc["athlete_id"] | "";
    std::string apiKey    = inDoc["api_key"] | "";

    if (athleteId.empty() || apiKey.empty()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "athlete_id and api_key are required";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (_intervalsUploader == nullptr) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "IntervalsIcuUploader not initialized";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (!_intervalsUploader->setApiCredentials(apiKey, athleteId)) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Failed to store Intervals.icu credentials";
        serializeJson(errDoc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_intervalsUploader);
    }
#endif

    JsonDocument outDoc;
    outDoc["ok"]         = true;
    outDoc["configured"] = _intervalsUploader->isConfigured();
    outDoc["athlete_id"] = _intervalsUploader->getAthleteId();
    serializeJson(outDoc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleTestIntervalsConnection() const {
    HttpResponse resp;
    JsonDocument doc;

    if (_intervalsUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "IntervalsIcuUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    bool success = _intervalsUploader->testConnection();
    doc["ok"]      = success;
    doc["message"] = success ? "Connection test passed" : "Connection test failed";
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleIntervalsDisconnect() {
    HttpResponse resp;
    JsonDocument doc;

    if (_intervalsUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "IntervalsIcuUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    _intervalsUploader->disconnect();

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_intervalsUploader);
    }
#endif

    doc["ok"] = true;
    serializeJson(doc, resp.body);
    return resp;
}

// =============================================================================
// Wahoo Cloud OAuth2 & Credentials Handlers (Native-Testable)
// =============================================================================

SessionOverlay::HttpResponse SessionOverlay::handleGetWahooStatus(uint32_t now_epoch_s) const {
    HttpResponse resp;
    JsonDocument doc;

    bool configured    = (_wahooUploader != nullptr) && _wahooUploader->isConfigured();
    bool authenticated = (_wahooUploader != nullptr) && _wahooUploader->isAuthenticated();
    bool tokenValid    = (_wahooUploader != nullptr) && _wahooUploader->isTokenValid(now_epoch_s);

    doc["configured"]    = configured;
    doc["authenticated"] = authenticated;
    doc["connected"]     = authenticated;
    doc["token_valid"]   = tokenValid;
    doc["client_id"]     = (_wahooUploader != nullptr) ? _wahooUploader->getClientId() : "";
    doc["athlete_name"]  = (_wahooUploader != nullptr) ? _wahooUploader->getAthleteName() : "";
    doc["expires_at"]    = (_wahooUploader != nullptr) ? _wahooUploader->getTokenExpiresAt() : 0u;
    doc["enabled"]       = (_wahooUploader != nullptr) ? _wahooUploader->isEnabled() : true;

    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleSaveWahooCredentials(const std::string& body_json) {
    HttpResponse resp;
    JsonDocument inDoc;
    DeserializationError err = deserializeJson(inDoc, body_json);
    if (err || !inDoc.is<JsonObject>()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Invalid JSON body";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    std::string clientId     = inDoc["client_id"] | "";
    std::string clientSecret = inDoc["client_secret"] | "";

    if (clientId.empty() || clientSecret.empty()) {
        resp.status_code = 400;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "client_id and client_secret are required";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (_wahooUploader == nullptr) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "WahooUploader not initialized";
        serializeJson(errDoc, resp.body);
        return resp;
    }

    if (!_wahooUploader->setClientCredentials(clientId, clientSecret)) {
        resp.status_code = 500;
        JsonDocument errDoc;
        errDoc["ok"] = false;
        errDoc["error"] = "Failed to store Wahoo credentials";
        serializeJson(errDoc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_wahooUploader);
    }
#endif

    JsonDocument outDoc;
    outDoc["ok"]         = true;
    outDoc["configured"] = _wahooUploader->isConfigured();
    outDoc["client_id"]  = _wahooUploader->getClientId();
    serializeJson(outDoc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleGetWahooAuthUrl(const std::string& redirect_uri,
                                                                   uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_wahooUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "WahooUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    std::string url = _wahooUploader->buildAuthorizeUrl(redirect_uri, now_ms);
    if (url.empty()) {
        resp.status_code = 400;
        doc["ok"] = false;
        doc["error"] = "Wahoo client_id not configured or invalid redirect_uri";
        serializeJson(doc, resp.body);
        return resp;
    }

    doc["ok"]            = true;
    doc["url"]           = url;
    doc["authorize_url"] = url;
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleWahooCallback(const std::string& code,
                                                                 const std::string& state,
                                                                 uint32_t now_ms) {
    HttpResponse resp;
    JsonDocument doc;

    if (_wahooUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "WahooUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (!_wahooUploader->validateAndConsumeState(state, now_ms)) {
        resp.status_code = 403;
        doc["ok"] = false;
        doc["error"] = "Invalid or expired OAuth state";
        serializeJson(doc, resp.body);
        return resp;
    }

    if (code.empty()) {
        resp.status_code = 400;
        doc["ok"] = false;
        doc["error"] = "Missing authorization code";
        serializeJson(doc, resp.body);
        return resp;
    }

    std::string errMsg;
    UploadStageResult res = _wahooUploader->exchangeAuthCode(code, 5000, errMsg);
    if (res != UploadStageResult::SUCCESS) {
        resp.status_code = 502;
        doc["ok"] = false;
        doc["error"] = errMsg.empty() ? "Token exchange failed" : errMsg;
        serializeJson(doc, resp.body);
        return resp;
    }

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_wahooUploader);
    }
#endif

    doc["ok"]            = true;
    doc["authenticated"] = _wahooUploader->isAuthenticated();
    doc["athlete_name"]  = _wahooUploader->getAthleteName();
    serializeJson(doc, resp.body);
    return resp;
}

SessionOverlay::HttpResponse SessionOverlay::handleWahooDisconnect() {
    HttpResponse resp;
    JsonDocument doc;

    if (_wahooUploader == nullptr) {
        resp.status_code = 500;
        doc["ok"] = false;
        doc["error"] = "WahooUploader not initialized";
        serializeJson(doc, resp.body);
        return resp;
    }

    _wahooUploader->disconnect();

#ifndef UNIT_TEST
    if (_configManager != nullptr) {
        _configManager->saveModuleConfig(_wahooUploader);
    }
#endif

    doc["ok"]            = true;
    doc["authenticated"] = false;
    serializeJson(doc, resp.body);
    return resp;
}

// =============================================================================
// IWebContributor — Tab & Route Registration
// =============================================================================

void SessionOverlay::registerTabs(ITabRegistry& registry) {
    // Session and Upload Services tabs removed — content moves to /ride page
    // (WO-15.3) and Setup pill (WO-15.2). Route registrations in registerRoutes()
    // are preserved — REST APIs and JS asset serving remain functional.
    (void)registry;
}

void SessionOverlay::registerRoutes(PsychicHttpServer& server) {
#ifdef UNIT_TEST
    (void)server;
#else
    // Serve gzipped portal tab JavaScript assets
    server.on("/session.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request,
                              response,
                              session_js_gz,
                              session_js_gz_len,
                              session_js_gz_etag,
                              "application/javascript");
    });

    server.on("/upload_settings.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request,
                              response,
                              upload_settings_js_gz,
                              upload_settings_js_gz_len,
                              upload_settings_js_gz_etag,
                              "application/javascript");
    });

    // Standalone /ride page assets
    server.on("/ride", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, ride_html_gz, ride_html_gz_len, ride_html_gz_etag, "text/html");
    });
    server.on("/ride.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, ride_js_gz, ride_js_gz_len, ride_js_gz_etag, "application/javascript");
    });
    server.on("/manifest.json", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, manifest_json_gz, manifest_json_gz_len, manifest_json_gz_etag, "application/manifest+json");
    });
#if __has_include("include/portal_assets/favicon_svg.hpp")
    server.on("/favicon.svg", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, favicon_svg_gz, favicon_svg_gz_len, favicon_svg_gz_etag, "image/svg+xml");
    });
#endif
#if __has_include("include/portal_assets/favicon_ico.hpp")
    server.on("/favicon.ico", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, favicon_ico_gz, favicon_ico_gz_len, favicon_ico_gz_etag, "image/x-icon");
    });
#endif
#if __has_include("include/portal_assets/icon-192_png.hpp")
    server.on("/icon-192.png", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, icon_192_png_gz, icon_192_png_gz_len, icon_192_png_gz_etag, "image/png");
    });
#endif
#if __has_include("include/portal_assets/icon-512_png.hpp")
    server.on("/icon-512.png", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response, icon_512_png_gz, icon_512_png_gz_len, icon_512_png_gz_etag, "image/png");
    });
#endif

    // Serve Stored Rides pill JavaScript
    server.on("/stored_rides.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request, response,
                              stored_rides_js_gz,
                              stored_rides_js_gz_len,
                              stored_rides_js_gz_etag,
                              "application/javascript");
    });

    auto checkAuth = [this](PsychicRequest* request, PsychicResponse* response) -> bool {
        if (_authManager != nullptr) {
            return _authManager->checkPortalAuth(request, response);
        }
        return true;
    };

    auto sendResp = [](PsychicResponse* response, const HttpResponse& r) -> esp_err_t {
        if (!r.content_disposition.empty()) {
            response->addHeader("Content-Disposition", r.content_disposition.c_str());
        }
        if (!r.content_encoding.empty()) {
            response->addHeader("Content-Encoding", r.content_encoding.c_str());
        }
        return response->send(r.status_code, r.content_type.c_str(), r.body.c_str());
    };

    auto extractSessionId = [](PsychicRequest* request) -> uint32_t {
        if (request->hasParam("id")) {
            return static_cast<uint32_t>(std::strtoul(request->getParam("id")->value().c_str(), nullptr, 10));
        }
        const char* body = request->bodyCStr();
        if (body != nullptr && body[0] != '\0') {
            JsonDocument doc;
            if (!deserializeJson(doc, body) && doc.is<JsonObject>()) {
                if (doc["id"].is<uint32_t>()) {
                    return doc["id"].as<uint32_t>();
                }
                if (doc["session_id"].is<uint32_t>()) {
                    return doc["session_id"].as<uint32_t>();
                }
            }
        }
        return 0;
    };

    server.on("/api/session/status", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetStatus());
    });

    server.on("/api/session/start", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        std::time_t nowEpoch = std::time(nullptr);
        uint32_t epochS = (nowEpoch > 0) ? static_cast<uint32_t>(nowEpoch) : 0u;
        return sendResp(response, handleStart(epochS, millis()));
    });

    server.on("/api/session/pause", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handlePause(millis()));
    });

    server.on("/api/session/resume", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleResume(millis()));
    });

    server.on("/api/session/end", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleEnd(millis()));
    });

    server.on("/api/session/stop", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleEnd(millis()));
    });

    server.on("/api/session/discard", HTTP_POST, [this, checkAuth, sendResp, extractSessionId](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        uint32_t sid = extractSessionId(request);
        return sendResp(response, handleDiscard(sid, millis()));
    });

    server.on("/api/session/config", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        const char* body = request->bodyCStr();
        return sendResp(response, handleConfig(body ? body : ""));
    });

    server.on("/api/session/history", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetHistory());
    });

    server.on("/api/session/download", HTTP_GET, [this, checkAuth](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
        if (!checkAuth(request, response)) return ESP_OK;
        uint32_t sid = request->hasParam("id")
                           ? static_cast<uint32_t>(std::strtoul(request->getParam("id")->value().c_str(), nullptr, 10))
                           : 0u;
        bool gzip = false;
        if (request->hasParam("format")) {
            String fmtVal = request->getParam("format")->value();
            gzip = (fmtVal == "tcx.gz" || fmtVal == "gz");
        }
        if (!gzip && request->hasParam("gz")) {
            String gzVal = request->getParam("gz")->value();
            gzip = (gzVal == "1" || gzVal == "true");
        }

        const RideStore* rs = resolveRideStore();
        if (rs == nullptr || sid == 0 || rs->findStoredRide(sid) == nullptr) {
            return response->send(404, "application/json", "{\"ok\":false,\"error\":\"Ride session not found\"}");
        }

        response->setContentType(gzip ? "application/gzip" : "application/vnd.garmin.tcx+xml");
        std::string disp = "attachment; filename=\"echenon_" + std::to_string(sid) +
                           (gzip ? ".tcx.gz\"" : ".tcx\"");
        response->addHeader("Content-Disposition", disp.c_str());

        auto writeChunk = [response](const uint8_t* data, size_t len) -> bool {
            if (data == nullptr || len == 0) {
                return true;
            }
            return response->sendChunk(const_cast<uint8_t*>(data), len) == ESP_OK;
        };

        bool ok = gzip ? TCXGenerator::generateTcxGzFromStore(*rs, sid, writeChunk)
                       : TCXGenerator::generateTcxFromStore(*rs, sid, writeChunk);
        if (!ok) {
            return ESP_FAIL;
        }
        return response->finishChunking();
    });

    server.on("/api/session/upload", HTTP_POST, [this, checkAuth, sendResp, extractSessionId](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        uint32_t sid = extractSessionId(request);
        // Extract optional service_id for single-sink retry
        std::string serviceId;
        const char* body = request->bodyCStr();
        if (body != nullptr && body[0] != '\0') {
            JsonDocument bdoc;
            if (!deserializeJson(bdoc, body) && bdoc.is<JsonObject>()) {
                if (bdoc["service_id"].is<const char*>()) {
                    serviceId = bdoc["service_id"].as<std::string>();
                }
            }
        }
        if (!serviceId.empty()) {
            return sendResp(response, handleUpload(sid, serviceId, millis()));
        }
        return sendResp(response, handleUpload(sid, millis()));
    });

    server.on("/api/strava/status", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        std::time_t nowEpoch = std::time(nullptr);
        uint32_t epochS = (nowEpoch > 0) ? static_cast<uint32_t>(nowEpoch) : 0u;
        return sendResp(response, handleGetStravaStatus(epochS));
    });

    server.on("/api/strava/credentials", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        const char* body = request->bodyCStr();
        return sendResp(response, handleSaveStravaCredentials(body ? body : ""));
    });

    server.on("/api/strava/auth-url", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        std::string redirectUri;
        if (request->hasParam("redirect_uri")) {
            redirectUri = request->getParam("redirect_uri")->value().c_str();
        } else {
            String host = request->host();
            std::string hostStr = host.length() > 0 ? host.c_str() : "echenon.local";
            redirectUri = "http://" + hostStr + "/api/strava/callback";
        }
        return sendResp(response, handleGetStravaAuthUrl(redirectUri, millis()));
    });

    server.on("/api/strava/callback", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        // No checkAuth — the OAuth state parameter is validated inside handleStravaCallback
        std::string code  = request->hasParam("code") ? request->getParam("code")->value().c_str() : "";
        std::string state = request->hasParam("state") ? request->getParam("state")->value().c_str() : "";
        HttpResponse result = handleStravaCallback(code, state, millis());

        if (result.status_code == 200) {
            return response->redirect("/#tab-strava");
        }
        // Extract error message from JSON body for the redirect query param
        std::string errMsg = "Authorization failed";
        JsonDocument errDoc;
        if (deserializeJson(errDoc, result.body) == DeserializationError::Ok && errDoc.containsKey("error")) {
            errMsg = errDoc["error"].as<std::string>();
        }
        // URL-encode the error message (minimal: spaces and special chars)
        std::string encoded;
        for (char c : errMsg) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += '+';
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
                encoded += hex;
            }
        }
        std::string redirectUrl = "/#tab-strava?strava_error=" + encoded;
        return response->redirect(redirectUrl.c_str());
    });

    server.on("/api/strava/disconnect", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleStravaDisconnect());
    });

    // --- Multi-Sink Upload Status Routes ---

    server.on("/api/upload/sinks", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetUploadSinks());
    });

    server.on("/api/upload/status", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetUploadStatus());
    });

    // --- Intervals.icu Credential Routes ---

    server.on("/api/intervals/status", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetIntervalsStatus());
    });

    server.on("/api/intervals/credentials", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        const char* body = request->bodyCStr();
        return sendResp(response, handleSaveIntervalsCredentials(body ? body : ""));
    });

    server.on("/api/intervals/test", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleTestIntervalsConnection());
    });

    server.on("/api/intervals/disconnect", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleIntervalsDisconnect());
    });

    // --- Intervals.icu Settings JS Asset (PROGMEM, WO-19.5 creates the asset) ---
#if __has_include("include/portal_assets/intervals_settings_js.h")
    server.on("/intervals_settings.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request,
                              response,
                              intervals_settings_js_gz,
                              intervals_settings_js_gz_len,
                              intervals_settings_js_gz_etag,
                              "application/javascript");
    });
#endif

    // --- Wahoo Cloud Credential & OAuth2 Routes ---

    server.on("/api/wahoo/status", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleGetWahooStatus());
    });

    server.on("/api/wahoo/credentials", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        const char* body = request->bodyCStr();
        return sendResp(response, handleSaveWahooCredentials(body ? body : ""));
    });

    server.on("/api/wahoo/auth-url", HTTP_GET, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        std::string redirectUri;
        if (request->hasParam("redirect_uri")) {
            redirectUri = request->getParam("redirect_uri")->value().c_str();
        } else {
            String host = request->host();
            std::string hostStr = host.length() > 0 ? host.c_str() : "echenon.local";
            redirectUri = "http://" + hostStr + "/api/wahoo/callback";
        }
        return sendResp(response, handleGetWahooAuthUrl(redirectUri, millis()));
    });

    server.on("/api/wahoo/callback", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
        // No checkAuth — the OAuth state parameter is validated inside handleWahooCallback
        std::string code  = request->hasParam("code") ? request->getParam("code")->value().c_str() : "";
        std::string state = request->hasParam("state") ? request->getParam("state")->value().c_str() : "";
        HttpResponse result = handleWahooCallback(code, state, millis());

        if (result.status_code == 200) {
            return response->redirect("/#tab-apps");
        }
        // Extract error message from JSON body for the redirect query param
        std::string errMsg = "Authorization failed";
        JsonDocument errDoc;
        if (deserializeJson(errDoc, result.body) == DeserializationError::Ok && errDoc.containsKey("error")) {
            errMsg = errDoc["error"].as<std::string>();
        }
        // URL-encode the error message
        std::string encoded;
        for (char c : errMsg) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += '+';
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
                encoded += hex;
            }
        }
        std::string redirectUrl = "/#tab-apps?wahoo_error=" + encoded;
        return response->redirect(redirectUrl.c_str());
    });

    server.on("/api/wahoo/disconnect", HTTP_POST, [this, checkAuth, sendResp](PsychicRequest* request, PsychicResponse* response) {
        if (!checkAuth(request, response)) return ESP_OK;
        return sendResp(response, handleWahooDisconnect());
    });

    // --- Wahoo Cloud Settings JS Asset (PROGMEM, WO-21.3 creates the asset) ---
#if __has_include("include/portal_assets/wahoo_settings_js.hpp")
    server.on("/wahoo_settings.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request,
                              response,
                              wahoo_settings_js_gz,
                              wahoo_settings_js_gz_len,
                              wahoo_settings_js_gz_etag,
                              "application/javascript");
    });
#elif __has_include("include/portal_assets/wahoo_settings_js.h")
    server.on("/wahoo_settings.js", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
        return serveGzipAsset(request,
                              response,
                              wahoo_settings_js_gz,
                              wahoo_settings_js_gz_len,
                              wahoo_settings_js_gz_etag,
                              "application/javascript");
    });
#endif

#endif // UNIT_TEST
}

} // namespace echenon
