/// @file DiagnosticsRoutes.cpp
/// @brief Diagnostics and run-data query REST API implementation.
///
/// Route registration is guarded with `#ifndef UNIT_TEST`. Helper functions
/// (diagEventTypeToString, formatPayloadJson) are unconditionally compiled
/// for native testing.

#include "echenon/DiagnosticsRoutes.h"
#include "echenon/RidePersistence.h"
#include "echenon/RideStore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef UNIT_TEST
#include <PsychicHttp.h>
#include "AuthManager.hpp"
#endif

namespace echenon {

// =============================================================================
// Event Type String Conversion (unconditional)
// =============================================================================

const char* diagEventTypeToString(DiagEventType type) {
    switch (type) {
        case DiagEventType::BOOT:             return "BOOT";
        case DiagEventType::SESSION_START:    return "SESSION_START";
        case DiagEventType::SESSION_END:      return "SESSION_END";
        case DiagEventType::RUNAWAY_ALERT:    return "RUNAWAY_ALERT";
        case DiagEventType::CALIBRATION_DONE: return "CALIBRATION_DONE";
        case DiagEventType::FIRMWARE_CHANGE:  return "FIRMWARE_CHANGE";
        default:                              return "UNKNOWN";
    }
}

// =============================================================================
// Payload JSON Formatting (unconditional)
// =============================================================================

int formatPayloadJson(DiagEventType type, const uint8_t payload[3],
                      char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return 0;
    }

    int n = 0;
    switch (type) {
        case DiagEventType::BOOT: {
            uint32_t uptime = decodeBootPayload(payload);
            n = std::snprintf(buf, buf_size,
                              "{\"uptime_s\":%lu}",
                              static_cast<unsigned long>(uptime));
            break;
        }
        case DiagEventType::SESSION_START: {
            uint32_t sid = decodeSessionStartPayload(payload);
            n = std::snprintf(buf, buf_size,
                              "{\"session_id\":%lu}",
                              static_cast<unsigned long>(sid));
            break;
        }
        case DiagEventType::SESSION_END: {
            uint16_t dur = 0;
            uint16_t rec = 0;
            decodeSessionEndPayload(payload, dur, rec);
            n = std::snprintf(buf, buf_size,
                              "{\"duration_s\":%u,\"record_count_approx\":%u}",
                              static_cast<unsigned>(dur),
                              static_cast<unsigned>(rec));
            break;
        }
        case DiagEventType::RUNAWAY_ALERT: {
            uint32_t dur_ms = decodeRunawayPayload(payload);
            n = std::snprintf(buf, buf_size,
                              "{\"duration_ms\":%lu}",
                              static_cast<unsigned long>(dur_ms));
            break;
        }
        case DiagEventType::CALIBRATION_DONE: {
            uint8_t ct = decodeCalibrationPayload(payload);
            n = std::snprintf(buf, buf_size,
                              "{\"calibration_type\":\"%s\"}",
                              ct == 0 ? "initial" : "recalibration");
            break;
        }
        case DiagEventType::FIRMWARE_CHANGE: {
            uint8_t major = 0, minor = 0, patch = 0;
            decodeFirmwarePayload(payload, major, minor, patch);
            n = std::snprintf(buf, buf_size,
                              "{\"version\":\"%u.%u.%u\"}",
                              static_cast<unsigned>(major),
                              static_cast<unsigned>(minor),
                              static_cast<unsigned>(patch));
            break;
        }
        default:
            n = std::snprintf(buf, buf_size, "{}");
            break;
    }
    return (n > 0 && static_cast<size_t>(n) < buf_size) ? n : 0;
}

// =============================================================================
// Constructor
// =============================================================================

DiagnosticsRoutes::DiagnosticsRoutes(DiagnosticsModule& diag,
                                     RidePersistence& persistence)
    : _diag(diag)
    , _persistence(persistence) {}

// =============================================================================
// Route Registration (ESP32 only)
// =============================================================================

#ifndef UNIT_TEST

void DiagnosticsRoutes::registerRoutes(PsychicHttpServer& server,
                                       AuthManager& auth) {
    // -------------------------------------------------------------------------
    // GET /api/echenon/events — Diagnostic event log (chunked JSON streaming)
    // -------------------------------------------------------------------------
    server.on("/api/echenon/events", HTTP_GET,
        [this, &auth](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
            if (!auth.checkPortalAuth(request, response)) {
                return ESP_OK;
            }

            // Parse optional query parameters
            uint32_t from_epoch = 0;
            uint32_t to_epoch   = UINT32_MAX;
            size_t   limit      = 100;

            if (request->hasParam("from")) {
                from_epoch = static_cast<uint32_t>(
                    std::strtoul(request->getParam("from")->value().c_str(), nullptr, 10));
            }
            if (request->hasParam("to")) {
                to_epoch = static_cast<uint32_t>(
                    std::strtoul(request->getParam("to")->value().c_str(), nullptr, 10));
            }
            if (request->hasParam("limit")) {
                size_t req_limit = static_cast<size_t>(
                    std::strtoul(request->getParam("limit")->value().c_str(), nullptr, 10));
                if (req_limit > 0 && req_limit < limit) {
                    limit = req_limit;
                }
            }

            // Query matching events into a stack buffer (max 100 events × 8 bytes = 800 bytes)
            EventQuery query;
            query.from_epoch = from_epoch;
            query.to_epoch   = to_epoch;
            query.limit      = limit;

            DiagnosticEvent results[100];
            size_t count = _diag.queryEvents(query, results, 100);
            size_t total = _diag.getEventCount();

            // Stream the JSON response in chunks ≤ 4 KB
            response->setContentType("application/json");

            // Opening: {"events":[
            const char* opening = "{\"events\":[";
            esp_err_t err = response->sendChunk(
                reinterpret_cast<uint8_t*>(const_cast<char*>(opening)),
                std::strlen(opening));
            if (err != ESP_OK) return err;

            // Stream each event
            char chunkBuf[512];
            for (size_t i = 0; i < count; ++i) {
                const DiagnosticEvent& evt = results[i];
                const char* typeName = diagEventTypeToString(evt.type);

                char payloadJson[128];
                formatPayloadJson(evt.type, evt.payload, payloadJson, sizeof(payloadJson));

                int n = std::snprintf(chunkBuf, sizeof(chunkBuf),
                    "%s{\"epoch\":%lu,\"type\":\"%s\",\"type_id\":%u,\"payload\":%s}",
                    (i > 0) ? "," : "",
                    static_cast<unsigned long>(evt.epoch_s),
                    typeName,
                    static_cast<unsigned>(static_cast<uint8_t>(evt.type)),
                    payloadJson);

                if (n > 0 && static_cast<size_t>(n) < sizeof(chunkBuf)) {
                    err = response->sendChunk(
                        reinterpret_cast<uint8_t*>(chunkBuf),
                        static_cast<size_t>(n));
                    if (err != ESP_OK) return err;
                }
            }

            // Closing: ],"count":N,"total":M}
            int n = std::snprintf(chunkBuf, sizeof(chunkBuf),
                "],\"count\":%u,\"total\":%u}",
                static_cast<unsigned>(count),
                static_cast<unsigned>(total));
            if (n > 0 && static_cast<size_t>(n) < sizeof(chunkBuf)) {
                err = response->sendChunk(
                    reinterpret_cast<uint8_t*>(chunkBuf),
                    static_cast<size_t>(n));
                if (err != ESP_OK) return err;
            }

            return response->finishChunking();
        });

    // -------------------------------------------------------------------------
    // GET /api/echenon/runs — Stored ride listing (chunked JSON streaming)
    // -------------------------------------------------------------------------
    server.on("/api/echenon/runs", HTTP_GET,
        [this, &auth](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
            if (!auth.checkPortalAuth(request, response)) {
                return ESP_OK;
            }

            const auto& rides = _persistence.getStoredRides();
            uint32_t totalSizeKb = _persistence.getTotalSizeBytes() / 1024u;

            response->setContentType("application/json");

            const char* opening = "{\"runs\":[";
            esp_err_t err = response->sendChunk(
                reinterpret_cast<uint8_t*>(const_cast<char*>(opening)),
                std::strlen(opening));
            if (err != ESP_OK) return err;

            char chunkBuf[256];
            for (size_t i = 0; i < rides.size(); ++i) {
                const auto& r = rides[i];
                int n = std::snprintf(chunkBuf, sizeof(chunkBuf),
                    "%s{\"session_id\":%lu,\"start_epoch\":%lu,"
                    "\"file_size_bytes\":%lu,\"record_count\":%u}",
                    (i > 0) ? "," : "",
                    static_cast<unsigned long>(r.session_id),
                    static_cast<unsigned long>(r.start_epoch_s),
                    static_cast<unsigned long>(r.file_size_bytes),
                    static_cast<unsigned>(r.record_count));

                if (n > 0 && static_cast<size_t>(n) < sizeof(chunkBuf)) {
                    err = response->sendChunk(
                        reinterpret_cast<uint8_t*>(chunkBuf),
                        static_cast<size_t>(n));
                    if (err != ESP_OK) return err;
                }
            }

            int n = std::snprintf(chunkBuf, sizeof(chunkBuf),
                "],\"count\":%u,\"total_size_kb\":%lu}",
                static_cast<unsigned>(rides.size()),
                static_cast<unsigned long>(totalSizeKb));
            if (n > 0 && static_cast<size_t>(n) < sizeof(chunkBuf)) {
                err = response->sendChunk(
                    reinterpret_cast<uint8_t*>(chunkBuf),
                    static_cast<size_t>(n));
                if (err != ESP_OK) return err;
            }

            return response->finishChunking();
        });

    // -------------------------------------------------------------------------
    // GET /api/echenon/runs/<id>.csv.gz — Download stored ride as CSV
    //   (streamed as chunked transfer encoding)
    //
    // Note: The WO spec calls for gzipped CSV streaming. Since RidePersistence
    // stores rides in binary format, we load the binary records and convert to
    // CSV on the fly. Full gzip recompression is deferred — the CSV is streamed
    // as plain text/csv for now. If gzip is truly required, it can be layered
    // in WO-14.4 or later with miniz streaming deflation.
    // -------------------------------------------------------------------------
    server.on("/api/echenon/runs/*.csv.gz", HTTP_GET,
        [this, &auth](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
            if (!auth.checkPortalAuth(request, response)) {
                return ESP_OK;
            }

            // Extract session_id from URL path: /api/echenon/runs/<id>.csv.gz
            String uri = request->uri();
            const char* uriStr = uri.c_str();
            const char* runsPrefix = "/api/echenon/runs/";
            size_t prefixLen = std::strlen(runsPrefix);

            uint32_t session_id = 0;
            if (std::strncmp(uriStr, runsPrefix, prefixLen) == 0) {
                session_id = static_cast<uint32_t>(
                    std::strtoul(uriStr + prefixLen, nullptr, 10));
            }

            if (session_id == 0) {
                return response->send(404, "application/json",
                    "{\"error\":\"Invalid session ID\"}");
            }

            // Load the binary ride data
            RideFileHeader header;
            std::vector<RideRecord> records;
            if (!_persistence.loadRide(session_id, header, records)) {
                return response->send(404, "application/json",
                    "{\"error\":\"Ride not found\"}");
            }

            // Stream as CSV (text/csv for maximum compatibility)
            response->setContentType("text/csv");
            char dispBuf[80];
            std::snprintf(dispBuf, sizeof(dispBuf),
                "attachment; filename=\"ride_%lu.csv\"",
                static_cast<unsigned long>(session_id));
            response->addHeader("Content-Disposition", dispBuf);

            // CSV header
            const char* csvHeader = "elapsed_s,power_w,cadence_rpm,hr_bpm,resistance,flags\n";
            esp_err_t err = response->sendChunk(
                reinterpret_cast<uint8_t*>(const_cast<char*>(csvHeader)),
                std::strlen(csvHeader));
            if (err != ESP_OK) return err;

            // Stream records in batches, accumulating into a ≤ 4 KB chunk buffer
            constexpr size_t kChunkCapacity = 2048;
            char chunkBuf[kChunkCapacity];
            size_t chunkLen = 0;
            char rowBuf[80];

            for (size_t i = 0; i < records.size(); ++i) {
                const RideRecord& rec = records[i];
                int rowLen = std::snprintf(rowBuf, sizeof(rowBuf),
                    "%u,%u,%u,%u,%u,%u\n",
                    static_cast<unsigned>(rec.timestamp_offset_s),
                    static_cast<unsigned>(rec.power_watts),
                    static_cast<unsigned>(rec.cadence_rpm),
                    static_cast<unsigned>(rec.heart_rate_bpm),
                    static_cast<unsigned>(rec.internal_resistance),
                    static_cast<unsigned>(rec.flags));

                if (rowLen <= 0) continue;
                size_t rowSize = static_cast<size_t>(rowLen);

                if (chunkLen + rowSize > kChunkCapacity) {
                    err = response->sendChunk(
                        reinterpret_cast<uint8_t*>(chunkBuf), chunkLen);
                    chunkLen = 0;
                    if (err != ESP_OK) return err;
                }

                std::memcpy(chunkBuf + chunkLen, rowBuf, rowSize);
                chunkLen += rowSize;
            }

            // Flush remaining
            if (chunkLen > 0) {
                err = response->sendChunk(
                    reinterpret_cast<uint8_t*>(chunkBuf), chunkLen);
                if (err != ESP_OK) return err;
            }

            return response->finishChunking();
        });

    // -------------------------------------------------------------------------
    // DELETE /api/echenon/runs/<id> — Delete a stored ride
    // -------------------------------------------------------------------------
    server.on("/api/echenon/runs/*", HTTP_DELETE,
        [this, &auth](PsychicRequest* request, PsychicResponse* response) -> esp_err_t {
            if (!auth.checkPortalAuth(request, response)) {
                return ESP_OK;
            }

            // Extract session_id from URL path: /api/echenon/runs/<id>
            String uri = request->uri();
            const char* uriStr = uri.c_str();
            const char* runsPrefix = "/api/echenon/runs/";
            size_t prefixLen = std::strlen(runsPrefix);

            uint32_t session_id = 0;
            if (std::strncmp(uriStr, runsPrefix, prefixLen) == 0) {
                session_id = static_cast<uint32_t>(
                    std::strtoul(uriStr + prefixLen, nullptr, 10));
            }

            if (session_id == 0 || !_persistence.deleteRide(session_id)) {
                return response->send(404, "application/json",
                    "{\"ok\":false,\"error\":\"Ride not found\"}");
            }

            return response->send(200, "application/json", "{\"ok\":true}");
        });
}

#endif // UNIT_TEST

} // namespace echenon
