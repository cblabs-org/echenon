#pragma once

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
    struct HttpResponse {
        int         status_code = 200;
        std::string content_type = "application/json";
        std::string content_disposition;
        std::string content_encoding;
        std::string body;
    };

    SessionOverlay() = default;

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

    ~SessionOverlay() override = default;

    // Dependency injection setters
    void setSessionManager(SessionManager* sm) { _sessionManager = sm; }
    void setRideStore(RideStore* rs) { _rideStore = rs; }
    void setUploadQueue(UploadQueue* uq) { _uploadQueue = uq; }
    void setStravaUploader(StravaUploader* su) { _stravaUploader = su; }
    void setAuthManager(AuthManager* am) { _authManager = am; }
    void setConfigManager(ConfigManager* cm) { _configManager = cm; }
    void setIntervalsUploader(IntervalsIcuUploader* iu) { _intervalsUploader = iu; }
    void setWahooUploader(WahooUploader* wu) { _wahooUploader = wu; }

    // IWebContributor
    void registerRoutes(PsychicHttpServer& server) override;
    void registerTabs(ITabRegistry& registry) override;

    // =========================================================================
    // Native-Testable Handler Methods
    // =========================================================================

    HttpResponse handleGetStatus() const;
    HttpResponse handleStart(uint32_t now_epoch_s = 0, uint32_t now_ms = 0);
    HttpResponse handlePause(uint32_t now_ms = 0);
    HttpResponse handleResume(uint32_t now_ms = 0);
    HttpResponse handleEnd(uint32_t now_ms = 0);
    HttpResponse handleDiscard(uint32_t session_id = 0, uint32_t now_ms = 0);
    HttpResponse handleConfig(const std::string& json_body);
    HttpResponse handleGetHistory() const;
    HttpResponse handleDownload(uint32_t session_id, bool gzip = false) const;
    HttpResponse handleUpload(uint32_t session_id, uint32_t now_ms = 0);
    HttpResponse handleUpload(uint32_t session_id, const std::string& service_id, uint32_t now_ms = 0);

    HttpResponse handleGetStravaStatus(uint32_t now_epoch_s = 0) const;
    HttpResponse handleSaveStravaCredentials(const std::string& json_body);
    HttpResponse handleGetStravaAuthUrl(const std::string& redirect_uri, uint32_t now_ms = 0);
    HttpResponse handleStravaCallback(const std::string& code,
                                      const std::string& state,
                                      uint32_t now_ms = 0);
    HttpResponse handleStravaDisconnect();

    // Multi-sink upload status endpoints
    HttpResponse handleGetUploadSinks() const;
    HttpResponse handleGetUploadStatus() const;

    // Intervals.icu credential management endpoints
    HttpResponse handleGetIntervalsStatus() const;
    HttpResponse handleSaveIntervalsCredentials(const std::string& json_body);
    HttpResponse handleTestIntervalsConnection() const;
    HttpResponse handleIntervalsDisconnect();

    // Wahoo Cloud OAuth2 & credential management endpoints
    HttpResponse handleGetWahooStatus(uint32_t now_epoch_s = 0) const;
    HttpResponse handleSaveWahooCredentials(const std::string& body_json);
    HttpResponse handleGetWahooAuthUrl(const std::string& redirect_uri, uint32_t now_ms = 0);
    HttpResponse handleWahooCallback(const std::string& code,
                                     const std::string& state,
                                     uint32_t now_ms = 0);
    HttpResponse handleWahooDisconnect();

    /**
     * Format the 14-field /ws/echenon/live JSON telemetry frame including
     * session_state, session_elapsed_s, and instantaneous heart rate (hr).
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
