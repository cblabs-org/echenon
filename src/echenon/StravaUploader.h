#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/StravaUploader.h
 * Description: Strava v3 API uploader implementing IUploader and IConfigurable ("echenon_strava") with BYOA OAuth 2.0, CSRF nonce, KeyManager secrets, and Redactor integration (SPEC.md §10.3–§10.5).
 */

#include "echenon/IUploader.h"
#include "echenon/RideStore.h"
#include "interfaces/IConfigurable.hpp"
#include "lib/Redactor/Redactor.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

class KeyManager;

namespace echenon {

// =============================================================================
// Secret Store Abstraction (KeyManager on ESP32, Injectable Spy in Native Tests)
// =============================================================================

/**
 * @brief Interface for encrypted secret persistence (SPEC.md §10.4).
 *
 * Keys used by StravaUploader:
 * - "client_secret" ("strava.client_secret" in KeyManager)
 * - "refresh_token" ("strava.refresh_token" in KeyManager)
 * - "access_token"  ("strava.access_token" in KeyManager)
 */
class ISecretStore {
public:
    virtual ~ISecretStore() = default;
    /// Persist secret value under key; return true on success.
    virtual bool putSecret(const std::string& key, const std::string& value) = 0;
    /// Retrieve secret value for key (empty string if absent).
    virtual std::string getSecret(const std::string& key) const = 0;
    /// Delete secret entry for key; return true on success.
    virtual bool deleteSecret(const std::string& key) = 0;
};

/**
 * @brief Default in-memory secret store used when no external ISecretStore or
 *        KeyManager is attached.
 */
class InMemorySecretStore : public ISecretStore {
public:
    /// Store secret value under key in memory.
    bool putSecret(const std::string& key, const std::string& value) override {
        if (key.empty()) return false;
        _secrets[key] = value;
        return true;
    }

    /// Retrieve secret value from in-memory map.
    std::string getSecret(const std::string& key) const override {
        auto it = _secrets.find(key);
        return (it != _secrets.end()) ? it->second : std::string{};
    }

    /// Remove secret key from in-memory map.
    bool deleteSecret(const std::string& key) override {
        _secrets.erase(key);
        return true;
    }

    /// Remove all stored secrets from memory.
    void clear() { _secrets.clear(); }
    /// Return number of stored secret entries.
    size_t size() const { return _secrets.size(); }

private:
    std::map<std::string, std::string> _secrets;
};

// =============================================================================
// Injectable HTTP Transport Abstraction (TLS Cert Bundle on ESP32, Mock on Host)
// =============================================================================

/// HTTP client response tuple returned by IHttpClient transport methods.
struct HttpResponse {
    int         status_code = 0; ///< HTTP status code (<=0 on transport error)
    std::string body;            ///< Response payload body
    std::string error;           ///< Transport error description when status_code <= 0
};

/// Multipart/form-data upload descriptor built for Strava/Wahoo `.tcx.gz` uploads.
struct MultipartUploadRequest {
    std::string          url;              ///< Target HTTPS upload endpoint URL
    std::string          bearer_token;     ///< OAuth2 Bearer access token
    std::string          boundary;         ///< Multipart form-data boundary string
    std::string          content_type;     ///< "multipart/form-data; boundary=..."
    std::string          data_type;        ///< "tcx.gz"
    std::string          external_id;      ///< "echenon_<session_id>.tcx.gz"
    std::string          trainer;          ///< "1"
    std::string          file_path;        ///< "/rides/ride_<session_id>.tcx.gz"
    std::string          file_name;        ///< "echenon_<session_id>.tcx.gz"
    std::string          multipart_header; ///< Pre-file multipart preamble
    std::string          multipart_footer; ///< Post-file multipart closing boundary
    std::vector<uint8_t> file_data;        ///< Compressed .tcx.gz payload bytes
};

/**
 * @brief Injectable HTTPS client abstraction (ESP32 HTTPClient + cert bundle vs host mock).
 */
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// Execute an application/x-www-form-urlencoded POST request.
    virtual HttpResponse postForm(
        const std::string& url,
        const std::string& form_body,
        uint32_t timeout_ms) = 0;

    /// Execute a multipart/form-data file upload POST request with Bearer auth.
    virtual HttpResponse postMultipart(
        const MultipartUploadRequest& req,
        uint32_t timeout_ms) = 0;

    /// Execute a GET request returning JSON with Bearer auth.
    virtual HttpResponse getJson(
        const std::string& url,
        const std::string& bearer_token,
        uint32_t timeout_ms) = 0;

    /// Optional hook to receive framework transport client (e.g. WiFiClientSecure*).
    virtual void setTransportContext(void* client_ctx) {
        (void)client_ctx;
    }
};

// =============================================================================
// StravaUploader (SPEC.md §10.3–§10.5)
// =============================================================================

/**
 * @brief Strava v3 API uploader supporting BYOA OAuth 2.0 token rotation,
 *        CSRF state nonces, and async multipart `.tcx.gz` activity uploads.
 */
class StravaUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* AUTHORIZE_URL = "https://www.strava.com/oauth/authorize"; ///< Strava OAuth authorize URL
    static constexpr const char* TOKEN_URL     = "https://www.strava.com/oauth/token";     ///< Strava OAuth token exchange/refresh URL
    static constexpr const char* UPLOADS_URL   = "https://www.strava.com/api/v3/uploads";  ///< Strava v3 multipart upload endpoint

    static constexpr const char* KEY_CLIENT_SECRET = "client_secret"; ///< ISecretStore key for OAuth client_secret
    static constexpr const char* KEY_REFRESH_TOKEN = "refresh_token"; ///< ISecretStore key for OAuth refresh_token
    static constexpr const char* KEY_ACCESS_TOKEN  = "access_token";  ///< ISecretStore key for OAuth access_token

    static constexpr uint32_t STATE_TTL_MS          = 600000u; ///< 10-minute single-use nonce TTL
    static constexpr size_t   STATE_HEX_LEN         = 32u;     ///< 128-bit hex nonce length
    static constexpr uint32_t TOKEN_EXPIRY_MARGIN_S = 60u;     ///< Refresh 60 s before expiry

    /// Callback invoked to register a secret with the Redactor; returns true if protected.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only during cold credential load/refresh, never in tick(); supports native test spies.
    using SecretRegistrarFn = std::function<bool(const std::string& secret)>;

    /// Optional callback invoked when non-secret token metadata (expires_at, athlete_name) changes.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only on OAuth token refresh cold path, never in tick().
    using MetadataSaveFn = std::function<bool(uint32_t expires_at, const std::string& athlete_name)>;

    /// Optional injectable 32-bit random word generator for state nonces.
    // GRAVE-05: Bound once at boot or unit-test setup (never in tick() hot path); supports deterministic RNG injection on env:native.
    using RandomU32Fn = std::function<uint32_t()>;

    /// Default constructor using in-memory secret store and default Redactor registrar.
    StravaUploader();
    /// Construct StravaUploader with injectable secret store, HTTP client, and Redactor registrar.
    explicit StravaUploader(ISecretStore* secretStore,
                            IHttpClient* httpClient = nullptr,
                            SecretRegistrarFn secretRegistrar = nullptr);
    /// Default destructor.
    ~StravaUploader() override = default;

    // --- Dependency Injection & Initialization ---
    /// Load persisted Strava secrets from ISecretStore and register them with Redactor.
    bool begin();
    /// Reload client_secret, refresh_token, and access_token from ISecretStore.
    bool loadCredentials();

    /// Override backing ISecretStore (falls back to internal InMemorySecretStore if nullptr).
    void setSecretStore(ISecretStore* store);
    /// Return active ISecretStore pointer.
    ISecretStore* getSecretStore() const { return _secretStore; }

    /// Attach framework KeyManager and switch secret storage to KeyManagerSecretStore on ESP32.
    void setKeyManager(KeyManager* keyManager);
    /// Return attached KeyManager pointer.
    KeyManager* getKeyManager() const { return _keyManager; }

    /// Inject custom IHttpClient transport (used for native unit tests).
    void setHttpClient(IHttpClient* httpClient);
    /// Return active IHttpClient pointer.
    IHttpClient* getHttpClient() const { return _httpClient; }

    /// Attach IRideFileSystem for reading `.tcx.gz` upload payloads.
    void setFileSystem(IRideFileSystem* fs) { _fs = fs; }
    /// Return attached IRideFileSystem pointer.
    IRideFileSystem* getFileSystem() const { return _fs; }

    /// Override Redactor secret registration callback (bound once at boot).
    void setSecretRegistrar(SecretRegistrarFn fn);
    /// Register callback invoked to persist updated OAuth token metadata (bound once at boot).
    void setMetadataSaveCallback(MetadataSaveFn fn) { _metadataSaveFn = std::move(fn); }
    /// Override random word generator used for OAuth CSRF state nonces.
    void setRandomGenerator(RandomU32Fn fn) { _randomU32Fn = std::move(fn); }

    /// Default Redactor registration helper wrapping Redactor::instance().registerSecret().
    static bool defaultRegisterSecret(const std::string& secret);

    // --- BYOA Credentials & OAuth 2.0 Flow (SPEC.md §10.4) ---

    /// Save BYOA Client ID & Client Secret (persists secret via ISecretStore and registers with Redactor).
    bool setClientCredentials(const std::string& client_id, const std::string& client_secret);
    /// Alias for setClientCredentials().
    bool saveCredentials(const std::string& client_id, const std::string& client_secret) {
        return setClientCredentials(client_id, client_secret);
    }

    /// Generate single-use 32-hex state nonce (10 min TTL) and return Strava OAuth authorize URL.
    std::string buildAuthorizeUrl(const std::string& redirect_uri, uint32_t now_ms = 0);

    /// Validate and immediately consume the active OAuth state nonce (single-use, 10 min TTL).
    bool validateAndConsumeState(const std::string& state, uint32_t now_ms = 0);

    /// Exchange an OAuth authorization code for access_token & refresh_token.
    UploadStageResult exchangeAuthCode(
        const std::string& code,
        uint32_t timeout_ms,
        std::string& err_msg);

    /// Disconnect Strava account (deletes access_token & refresh_token from ISecretStore).
    bool disconnect();

    /// Clear all Strava credentials including client_id & client_secret.
    bool clearCredentials();

    // --- Accessors ---
    /// Return configured BYOA Strava client_id.
    const std::string& getClientId() const { return _clientId; }
    /// Set BYOA Strava client_id.
    void setClientId(const std::string& client_id) { _clientId = client_id; }

    /// Return authenticated Strava athlete display name.
    const std::string& getAthleteName() const { return _athleteName; }
    /// Set Strava athlete display name.
    void setAthleteName(const std::string& name) { _athleteName = name; }

    /// Return OAuth access_token expiration Unix epoch (seconds).
    uint32_t getTokenExpiresAt() const { return _tokenExpiresAt; }
    /// Set OAuth access_token expiration Unix epoch (seconds).
    void setTokenExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }

    /// Return true if Strava auto-upload sink is enabled.
    bool isEnabled() const override { return _enabled; }
    /// Enable or disable the Strava upload sink.
    void setEnabled(bool enabled) { _enabled = enabled; }

    /// Return currently active OAuth CSRF state nonce (for unit tests).
    const std::string& getPendingState() const { return _pendingState; }
    /// Return true if an OAuth CSRF state nonce is currently active.
    bool isStateActive() const { return _stateActive; }

    /// Return cached OAuth access_token.
    const std::string& getAccessToken() const { return _accessToken; }
    /// Return cached OAuth refresh_token.
    const std::string& getRefreshToken() const { return _refreshToken; }
    /// Return cached BYOA client_secret.
    const std::string& getClientSecret() const { return _clientSecret; }

    /// Return true if authenticated and access_token is not within TOKEN_EXPIRY_MARGIN_S of expiry.
    bool isTokenValid(uint32_t now_epoch_s) const {
        return isAuthenticated() && !needsTokenRefresh(now_epoch_s);
    }

    /// Return count of Redactor registration warnings encountered.
    uint32_t getRedactorWarningCount() const { return _redactorWarningCount; }
    /// Return true if Redactor secret slot table reached saturation.
    bool isRedactorSaturated() const { return _redactorSaturated; }
    /// Return most recent Redactor warning message.
    const std::string& getLastRedactorWarning() const { return _lastRedactorWarning; }

    // --- Multipart Request Builder Helpers ---
    /// Build multipart/form-data request headers, preamble, and payload for Strava `/api/v3/uploads`.
    static void buildMultipartRequest(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        const std::string& bearer_token,
        const uint8_t* file_data,
        size_t file_len,
        MultipartUploadRequest& out_req);

    /// Concatenate multipart header, binary `.tcx.gz` bytes, and closing boundary into a request body.
    static std::string assembleMultipartBody(const MultipartUploadRequest& req);
    /// Percent-encode a string per RFC 3986 for form-urlencoded OAuth bodies and URL query parameters.
    static std::string urlEncode(const std::string& value);

    // --- IUploader Interface ---
    /// Return canonical sink identifier ("strava").
    const char* serviceId() const override { return "strava"; }
    /// Return human-readable service name ("Strava").
    const char* name() const override { return "Strava"; }
    /// Return true when enabled and client_id + client_secret are configured.
    bool isConfigured() const override;
    /// Return true when OAuth refresh_token is present.
    bool isAuthenticated() const override;
    /// Return true if access_token is empty or expires within TOKEN_EXPIRY_MARGIN_S of now_epoch_s.
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;

    /// Refresh OAuth access_token and persist rotated refresh_token first.
    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override;

    /// POST compressed `.tcx.gz` multipart upload to Strava `/api/v3/uploads`.
    UploadStageResult postUpload(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        uint32_t timeout_ms,
        std::string& out_upload_id,
        std::string& err_msg) override;

    /// Poll Strava `/api/v3/uploads/{id}` until activity_id is assigned or error occurs.
    UploadStageResult pollUploadStatus(
        const std::string& upload_id,
        uint32_t timeout_ms,
        std::string& out_activity_id,
        std::string& err_msg) override;

    /// Forward framework transport client (WiFiClientSecure*) to IHttpClient.
    void setTransportContext(void* client_ctx) override;

    // --- IConfigurable ("echenon_strava", schema v1) ---
    /// Return configuration namespace ("echenon_strava").
    const char* getConfigNamespace() const override { return "echenon_strava"; }
    /// Return configuration schema version (1).
    int getSchemaVersion() const override { return 1; }
    /// Apply persisted non-secret metadata (enabled, client_id, expires_at, athlete_name).
    void applyConfig(const JsonObject& section) override;
    /// Serialize non-secret metadata into JSON object.
    void serializeConfig(JsonObject& section) const override;
    /// Reset non-secret configuration fields to defaults.
    void resetConfig() override;

private:
    UploadStageResult handleTokenResponse(const std::string& json_body, std::string& err_msg);
    bool registerSecretWithRedactor(const std::string& secret, const char* secret_label);
    std::string generateRandomStateHex();
    uint32_t nextRandomU32();
    bool readTcxGzFile(const std::string& path, std::vector<uint8_t>& out_bytes) const;

    InMemorySecretStore _defaultSecretStore;
    ISecretStore*       _secretStore    = nullptr;
    KeyManager*         _keyManager     = nullptr;
    IHttpClient*        _httpClient     = nullptr;
    IRideFileSystem*    _fs             = nullptr;

    // GRAVE-05 deviation: DI callbacks set once at boot, never reallocated.
    // Framework's own DataManager uses the same pattern for IDataSource injection.
    SecretRegistrarFn   _secretRegistrar;
    MetadataSaveFn      _metadataSaveFn;
    RandomU32Fn         _randomU32Fn;

    bool        _enabled        = true;
    std::string _clientId;
    std::string _clientSecret;
    std::string _accessToken;
    std::string _refreshToken;
    uint32_t    _tokenExpiresAt = 0;
    std::string _athleteName;

    std::string _pendingState;
    uint32_t    _stateIssuedMs  = 0;
    bool        _stateActive    = false;

    uint32_t    _redactorWarningCount = 0;
    bool        _redactorSaturated    = false;
    std::string _lastRedactorWarning;
};

} // namespace echenon
