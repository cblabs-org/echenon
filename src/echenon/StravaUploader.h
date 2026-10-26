#pragma once

/// @file StravaUploader.h
/// @brief Strava v3 API uploader implementing IUploader and IConfigurable ("echenon_strava")
///        with Bring-Your-Own-App (BYOA) OAuth 2.0, single-use CSRF state nonce,
///        KeyManager / ISecretStore encrypted NVS secret storage, rotated refresh_token-first
///        persistence, and Redactor::registerSecret() integration (SPEC.md §10.3–§10.5).

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
    virtual bool putSecret(const std::string& key, const std::string& value) = 0;
    virtual std::string getSecret(const std::string& key) const = 0;
    virtual bool deleteSecret(const std::string& key) = 0;
};

/**
 * @brief Default in-memory secret store used when no external ISecretStore or
 *        KeyManager is attached.
 */
class InMemorySecretStore : public ISecretStore {
public:
    bool putSecret(const std::string& key, const std::string& value) override {
        if (key.empty()) return false;
        _secrets[key] = value;
        return true;
    }

    std::string getSecret(const std::string& key) const override {
        auto it = _secrets.find(key);
        return (it != _secrets.end()) ? it->second : std::string{};
    }

    bool deleteSecret(const std::string& key) override {
        _secrets.erase(key);
        return true;
    }

    void clear() { _secrets.clear(); }
    size_t size() const { return _secrets.size(); }

private:
    std::map<std::string, std::string> _secrets;
};

// =============================================================================
// Injectable HTTP Transport Abstraction (TLS Cert Bundle on ESP32, Mock on Host)
// =============================================================================

struct HttpResponse {
    int         status_code = 0;
    std::string body;
    std::string error;
};

struct MultipartUploadRequest {
    std::string          url;
    std::string          bearer_token;
    std::string          boundary;
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

class StravaUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* AUTHORIZE_URL = "https://www.strava.com/oauth/authorize";
    static constexpr const char* TOKEN_URL     = "https://www.strava.com/oauth/token";
    static constexpr const char* UPLOADS_URL   = "https://www.strava.com/api/v3/uploads";

    static constexpr const char* KEY_CLIENT_SECRET = "client_secret";
    static constexpr const char* KEY_REFRESH_TOKEN = "refresh_token";
    static constexpr const char* KEY_ACCESS_TOKEN  = "access_token";

    static constexpr uint32_t STATE_TTL_MS          = 600000u; ///< 10-minute single-use nonce TTL
    static constexpr size_t   STATE_HEX_LEN         = 32u;     ///< 128-bit hex nonce length
    static constexpr uint32_t TOKEN_EXPIRY_MARGIN_S = 60u;     ///< Refresh 60 s before expiry

    /// Callback invoked to register a secret with the Redactor; returns true if protected.
    using SecretRegistrarFn = std::function<bool(const std::string& secret)>;

    /// Optional callback invoked when non-secret token metadata (expires_at, athlete_name) changes.
    using MetadataSaveFn = std::function<bool(uint32_t expires_at, const std::string& athlete_name)>;

    /// Optional injectable 32-bit random word generator for state nonces.
    using RandomU32Fn = std::function<uint32_t()>;

    StravaUploader();
    explicit StravaUploader(ISecretStore* secretStore,
                            IHttpClient* httpClient = nullptr,
                            SecretRegistrarFn secretRegistrar = nullptr);
    ~StravaUploader() override = default;

    // --- Dependency Injection & Initialization ---
    bool begin();
    bool loadCredentials();

    void setSecretStore(ISecretStore* store);
    ISecretStore* getSecretStore() const { return _secretStore; }

    void setKeyManager(KeyManager* keyManager);
    KeyManager* getKeyManager() const { return _keyManager; }

    void setHttpClient(IHttpClient* httpClient);
    IHttpClient* getHttpClient() const { return _httpClient; }

    void setFileSystem(IRideFileSystem* fs) { _fs = fs; }
    IRideFileSystem* getFileSystem() const { return _fs; }

    void setSecretRegistrar(SecretRegistrarFn fn);
    void setMetadataSaveCallback(MetadataSaveFn fn) { _metadataSaveFn = std::move(fn); }
    void setRandomGenerator(RandomU32Fn fn) { _randomU32Fn = std::move(fn); }

    /// Default Redactor registration helper wrapping Redactor::instance().registerSecret().
    static bool defaultRegisterSecret(const std::string& secret);

    // --- BYOA Credentials & OAuth 2.0 Flow (SPEC.md §10.4) ---

    /// Save BYOA Client ID & Client Secret (persists secret via ISecretStore and registers with Redactor).
    bool setClientCredentials(const std::string& client_id, const std::string& client_secret);
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
    const std::string& getClientId() const { return _clientId; }
    void setClientId(const std::string& client_id) { _clientId = client_id; }

    const std::string& getAthleteName() const { return _athleteName; }
    void setAthleteName(const std::string& name) { _athleteName = name; }

    uint32_t getTokenExpiresAt() const { return _tokenExpiresAt; }
    void setTokenExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

    const std::string& getPendingState() const { return _pendingState; }
    bool isStateActive() const { return _stateActive; }

    const std::string& getAccessToken() const { return _accessToken; }
    const std::string& getRefreshToken() const { return _refreshToken; }
    const std::string& getClientSecret() const { return _clientSecret; }

    bool isTokenValid(uint32_t now_epoch_s) const {
        return isAuthenticated() && !needsTokenRefresh(now_epoch_s);
    }

    uint32_t getRedactorWarningCount() const { return _redactorWarningCount; }
    bool isRedactorSaturated() const { return _redactorSaturated; }
    const std::string& getLastRedactorWarning() const { return _lastRedactorWarning; }

    // --- Multipart Request Builder Helpers ---
    static void buildMultipartRequest(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        const std::string& bearer_token,
        const uint8_t* file_data,
        size_t file_len,
        MultipartUploadRequest& out_req);

    static std::string assembleMultipartBody(const MultipartUploadRequest& req);
    static std::string urlEncode(const std::string& value);

    // --- IUploader Interface ---
    const char* serviceId() const override { return "strava"; }
    const char* name() const override { return "Strava"; }
    bool isConfigured() const override;
    bool isAuthenticated() const override;
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;

    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override;

    UploadStageResult postUpload(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        uint32_t timeout_ms,
        std::string& out_upload_id,
        std::string& err_msg) override;

    UploadStageResult pollUploadStatus(
        const std::string& upload_id,
        uint32_t timeout_ms,
        std::string& out_activity_id,
        std::string& err_msg) override;

    void setTransportContext(void* client_ctx) override;

    // --- IConfigurable ("echenon_strava", schema v1) ---
    const char* getConfigNamespace() const override { return "echenon_strava"; }
    int getSchemaVersion() const override { return 1; }
    void applyConfig(const JsonObject& section) override;
    void serializeConfig(JsonObject& section) const override;
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
