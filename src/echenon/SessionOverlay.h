#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/SessionOverlay.h
 * Description: Portal Web UI and REST API contributor (IWebContributor) for session control, ride history/TCX download, and Strava/Intervals/Wahoo OAuth/uploads.
 */

#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <string>

#include "interfaces/IWebContributor.hpp"
#include "echenon/RideStore.h"
#include "echenon/SessionManager.h"
#include "echenon/StravaUploader.h"
#include "echenon/TCXGenerator.h"
#include "echenon/UploadQueue.h"

class AuthManager;
class ConfigManager;
class PsychicRequest;
class PsychicResponse;

namespace echenon { class IntervalsIcuUploader; class WahooUploader; }

namespace echenon {

/**
 * @brief Portal Web UI & REST API contributor for Ride Session Management,
 *        TCX/TCX.gz Export, and Strava OAuth2 / Auto-Upload Queue.
 *
 * Implements IWebContributor to register:
 *   - Portal Tabs:
 *     - "Session"         (order 80, /session.js)
 *     - "Upload Services" (order 90, /upload_settings.js)
 *   - Session Lifecycle & Ride History Endpoints:
 *     - GET  /api/session/status
 *     - POST /api/session/start
 *     - POST /api/session/pause
 *     - POST /api/session/resume
 *     - POST /api/session/end
 *     - POST /api/session/discard
 *     - POST /api/session/config
 *     - GET  /api/session/history
 *     - GET  /api/session/download
 *     - POST /api/session/upload
 *   - Strava OAuth2 & Credentials Endpoints:
 *     - GET  /api/strava/status
 *     - POST /api/strava/credentials
 *     - GET  /api/strava/auth-url
 *     - GET  /api/strava/callback
 *     - POST /api/strava/disconnect
 */
class SessionOverlay : public IWebContributor {
public:
    /// HTTP response value type returned by native-testable handler methods.
    struct HttpResponse {
        int         status_code = 200;               ///< HTTP status code
        std::string content_type = "application/json"; ///< MIME Content-Type header
        std::string content_disposition;             ///< Optional Content-Disposition attachment header
        std::string content_encoding;                ///< Optional Content-Encoding header (e.g. "gzip")
        std::string body;                            ///< Serialized JSON or binary/TCX response body
    };

    /// Default constructor.
    SessionOverlay() = default;

    /// Construct SessionOverlay wired to session, store, queue, uploader, auth, and config services.
    explicit SessionOverlay(SessionManager* sessionManager,
                            RideStore* rideStore = nullptr,
                            UploadQueue* uploadQueue = nullptr,
                            StravaUploader* stravaUploader = nullptr,
                            AuthManager* authManager = nullptr,
                            ConfigManager* configManager = nullptr)
        : _sessionManager(sessionManager)
        , _rideStore(rideStore)
        , _uploadQueue(uploadQueue)
        , _stravaUploader(stravaUploader)
        , _authManager(authManager)
        , _configManager(configManager) {}

    /// Default destructor.
    ~SessionOverlay() override = default;

    // Dependency injection setters
    /// Attach SessionManager dependency.
    void setSessionManager(SessionManager* sm) { _sessionManager = sm; }
    /// Attach RideStore dependency.
    void setRideStore(RideStore* rs) { _rideStore = rs; }
    /// Attach UploadQueue dependency.
    void setUploadQueue(UploadQueue* uq) { _uploadQueue = uq; }
    /// Attach StravaUploader dependency.
    void setStravaUploader(StravaUploader* su) { _stravaUploader = su; }
    /// Attach AuthManager dependency for portal auth enforcement.
    void setAuthManager(AuthManager* am) { _authManager = am; }
    /// Attach ConfigManager dependency for config persistence.
    void setConfigManager(ConfigManager* cm) { _configManager = cm; }
    /// Attach IntervalsIcuUploader dependency.
    void setIntervalsUploader(IntervalsIcuUploader* iu) { _intervalsUploader = iu; }
    /// Attach WahooUploader dependency.
    void setWahooUploader(WahooUploader* wu) { _wahooUploader = wu; }

    // IWebContributor
    /// Register session, history, download, and upload-service REST endpoints and static portal assets.
    void registerRoutes(PsychicHttpServer& server) override;
    /// Register "Session" and "Upload Services" navigation tabs in the portal UI.
    void registerTabs(ITabRegistry& registry) override;

    // =========================================================================
    // Native-Testable Handler Methods
    // =========================================================================

    /// Handle GET /api/session/status returning current session state, metrics, and config.
    HttpResponse handleGetStatus() const;
    /// Handle POST /api/session/start transitioning IDLE/ENDED -> ACTIVE.
    HttpResponse handleStart(uint32_t now_epoch_s = 0, uint32_t now_ms = 0);
    /// Handle POST /api/session/pause transitioning ACTIVE -> PAUSED.
    HttpResponse handlePause(uint32_t now_ms = 0);
    /// Handle POST /api/session/resume transitioning PAUSED -> ACTIVE.
    HttpResponse handleResume(uint32_t now_ms = 0);
    /// Handle POST /api/session/end finalizing active/paused session and queuing auto-upload.
    HttpResponse handleEnd(uint32_t now_ms = 0);
    /// Handle POST /api/session/discard deleting active or stored session by session_id.
    HttpResponse handleDiscard(uint32_t session_id = 0, uint32_t now_ms = 0);
    /// Handle POST /api/session/config updating auto-start, inactivity, and auto-upload settings.
    HttpResponse handleConfig(const std::string& json_body);
    /// Handle GET /api/session/history listing stored rides and per-sink upload sync states.
    HttpResponse handleGetHistory() const;
    /// Handle GET /api/session/download streaming `.tcx` or `.tcx.gz` for session_id.
    HttpResponse handleDownload(uint32_t session_id, bool gzip = false) const;
    /// Handle POST /api/session/upload enqueuing session_id across all enabled sinks.
    HttpResponse handleUpload(uint32_t session_id, uint32_t now_ms = 0);
    /// Handle POST /api/session/upload enqueuing session_id for a specific service_id sink.
    HttpResponse handleUpload(uint32_t session_id, const std::string& service_id, uint32_t now_ms = 0);

    /// Handle GET /api/strava/status returning Strava OAuth configuration and queue counters.
    HttpResponse handleGetStravaStatus(uint32_t now_epoch_s = 0) const;
    /// Handle POST /api/strava/credentials saving BYOA client_id and client_secret.
    HttpResponse handleSaveStravaCredentials(const std::string& json_body);
    /// Handle GET /api/strava/auth-url returning Strava OAuth authorize URL with CSRF state nonce.
    HttpResponse handleGetStravaAuthUrl(const std::string& redirect_uri, uint32_t now_ms = 0);
    /// Handle GET /api/strava/callback validating CSRF nonce and exchanging auth code for tokens.
    HttpResponse handleStravaCallback(const std::string& code,
                                      const std::string& state,
                                      uint32_t now_ms = 0);
    /// Handle POST /api/strava/disconnect clearing Strava OAuth tokens.
    HttpResponse handleStravaDisconnect();

    // Multi-sink upload status endpoints
    /// Handle GET /api/upload/sinks returning configured upload sink descriptors.
    HttpResponse handleGetUploadSinks() const;
    /// Handle GET /api/upload/status returning overall upload queue jobs and status.
    HttpResponse handleGetUploadStatus() const;

    // Intervals.icu credential management endpoints
    /// Handle GET /api/intervals/status returning Intervals.icu configuration status.
    HttpResponse handleGetIntervalsStatus() const;
    /// Handle POST /api/intervals/credentials saving API key and athlete ID.
    HttpResponse handleSaveIntervalsCredentials(const std::string& json_body);
    /// Handle POST /api/intervals/test verifying Intervals.icu credential readiness.
    HttpResponse handleTestIntervalsConnection() const;
    /// Handle POST /api/intervals/disconnect clearing Intervals.icu credentials.
    HttpResponse handleIntervalsDisconnect();

    // Wahoo Cloud OAuth2 & credential management endpoints
    /// Handle GET /api/wahoo/status returning Wahoo OAuth configuration and token state.
    HttpResponse handleGetWahooStatus(uint32_t now_epoch_s = 0) const;
    /// Handle POST /api/wahoo/credentials saving BYOA client_id, client_secret, and redirect_uri.
    HttpResponse handleSaveWahooCredentials(const std::string& body_json);
    /// Handle GET /api/wahoo/auth-url returning Wahoo OAuth authorize URL with CSRF state nonce.
    HttpResponse handleGetWahooAuthUrl(const std::string& redirect_uri, uint32_t now_ms = 0);
    /// Handle GET /api/wahoo/callback validating CSRF nonce and exchanging auth code for tokens.
    HttpResponse handleWahooCallback(const std::string& code,
                                     const std::string& state,
                                     uint32_t now_ms = 0);
    /// Handle POST /api/wahoo/disconnect clearing Wahoo OAuth tokens.
    HttpResponse handleWahooDisconnect();

    /**
     * @brief Format the 14-field /ws/echenon/live JSON telemetry frame including
     *        session_state, session_elapsed_s, and instantaneous heart rate (hr).
     */
    static std::string formatLiveFeedJson(float cadenceRpm,
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
                                          uint8_t hrBpm = 0);

private:
    RideStore* resolveRideStore() const {
        if (_rideStore != nullptr) {
            return _rideStore;
        }
        if (_sessionManager != nullptr) {
            return &_sessionManager->getRideStore();
        }
        return nullptr;
    }

    SessionManager* _sessionManager = nullptr;
    RideStore*      _rideStore      = nullptr;
    UploadQueue*    _uploadQueue    = nullptr;
    StravaUploader* _stravaUploader = nullptr;
    IntervalsIcuUploader* _intervalsUploader = nullptr;
    WahooUploader*        _wahooUploader     = nullptr;
    AuthManager*    _authManager    = nullptr;
    ConfigManager*  _configManager  = nullptr;
};

} // namespace echenon
