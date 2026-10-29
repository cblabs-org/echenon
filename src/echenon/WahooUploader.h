#pragma once

/// @file WahooUploader.h
/// @brief Wahoo Cloud (api.wahooligan.com) API uploader implementing IUploader and
///        IConfigurable ("echenon_wahoo", schema v1) with Bring-Your-Own-App (BYOA)
///        OAuth 2.0, single-use CSRF state nonce, KeyManager / ISecretStore encrypted
///        NVS secret storage, rotated refresh_token-first persistence, Redactor secret
///        registration, and synchronous multipart TCX.gz uploads (SPEC.md §10.2–§10.5).

#include "echenon/IUploader.h"
#include "echenon/RideStore.h"
#include "echenon/StravaUploader.h"   // ISecretStore, InMemorySecretStore, IHttpClient, HttpResponse, MultipartUploadRequest
#include "interfaces/IConfigurable.hpp"
#include "lib/Redactor/Redactor.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class KeyManager;

namespace echenon {

// =============================================================================
// WahooUploader (SPEC.md §10.2–§10.5)
// =============================================================================

class WahooUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* AUTHORIZE_URL = "https://api.wahooligan.com/oauth/authorize";
    static constexpr const char* TOKEN_URL     = "https://api.wahooligan.com/oauth/token";
    static constexpr const char* WORKOUTS_URL  = "https://api.wahooligan.com/v1/workouts";

    static constexpr const char* KEY_CLIENT_SECRET = "wahoo.client_secret";
    static constexpr const char* KEY_REFRESH_TOKEN = "wahoo.refresh_token";
    static constexpr const char* KEY_ACCESS_TOKEN  = "wahoo.access_token";

    static constexpr uint32_t STATE_TTL_MS          = 600000u; ///< 10-minute single-use nonce TTL
    static constexpr size_t   STATE_HEX_LEN         = 32u;     ///< 128-bit hex nonce length
    static constexpr uint32_t TOKEN_EXPIRY_MARGIN_S = 60u;     ///< Refresh 60 s before expiry

    /// Callback invoked to register a secret with the Redactor; returns true if protected.
    using SecretRegistrarFn = std::function<bool(const std::string& secret)>;

    /// Optional callback invoked when non-secret token metadata (expires_at, athlete_name) changes.
    using MetadataSaveFn = std::function<bool(uint32_t expires_at, const std::string& athlete_name)>;

    /// Optional injectable 32-bit random word generator for state nonces.
    using RandomU32Fn = std::function<uint32_t()>;

    WahooUploader();
    explicit WahooUploader(ISecretStore* secretStore,
                           IHttpClient* httpClient = nullptr,
                           SecretRegistrarFn secretRegistrar = nullptr);
    ~WahooUploader() override = default;

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

    // --- BYOA Credentials & OAuth 2.0 Flow ---

    /// Save BYOA Client ID & Client Secret (persists secret via ISecretStore and registers with Redactor).
    bool setClientCredentials(const std::string& client_id, const std::string& client_secret);
    bool saveCredentials(const std::string& client_id, const std::string& client_secret) {
        return setClientCredentials(client_id, client_secret);
    }

    /// Generate single-use 32-hex state nonce (10 min TTL) and return Wahoo OAuth authorize URL.
    std::string buildAuthorizeUrl(const std::string& redirect_uri, uint32_t now_ms = 0);

    /// Validate and immediately consume the active OAuth state nonce (single-use, 10 min TTL).
    bool validateAndConsumeState(const std::string& state, uint32_t now_ms = 0);

    /// Exchange an OAuth authorization code for access_token & refresh_token.
    UploadStageResult exchangeAuthCode(
        const std::string& code,
        uint32_t timeout_ms,
        std::string& err_msg);

    UploadStageResult exchangeAuthCode(
        const std::string& code,
        const std::string& redirect_uri,
        uint32_t timeout_ms,
        std::string& err_msg);

    /// Execute the OAuth token refresh stage within timeout_ms.
    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override;

    /// Disconnect Wahoo account (deletes access_token & refresh_token from ISecretStore).
    bool disconnect();

    /// Clear all Wahoo credentials including client_id & client_secret.
    bool clearCredentials();

    // --- Accessors ---
    const std::string& getClientId() const { return _clientId; }
    void setClientId(const std::string& client_id) { _clientId = client_id; }

    const std::string& getAthleteName() const { return _athleteName; }
    void setAthleteName(const std::string& name) { _athleteName = name; }

    const std::string& getUserName() const { return _athleteName; }
    void setUserName(const std::string& name) { _athleteName = name; }

    uint32_t getTokenExpiresAt() const { return _tokenExpiresAt; }
    void setTokenExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }
    uint32_t getExpiresAt() const { return _tokenExpiresAt; }
    void setExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }

    bool isEnabled() const override { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

    const std::string& getPendingState() const { return _pendingState; }
    bool isStateActive() const { return _stateActive; }

    const std::string& getAccessToken() const { return _accessToken; }
    const std::string& getRefreshToken() const { return _refreshToken; }
    const std::string& getClientSecret() const { return _clientSecret; }

    const std::string& getRedirectUri() const { return _redirectUri; }
    void setRedirectUri(const std::string& uri) { _redirectUri = uri; }

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
    const char* serviceId() const override { return "wahoo"; }
    const char* name() const override { return "Wahoo Cloud"; }
    static const char* serviceName() { return "Wahoo Cloud"; }
    bool isConfigured() const override;
    bool isAuthenticated() const override;
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;

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

    // --- IConfigurable ("echenon_wahoo", schema v1) ---
    const char* getConfigNamespace() const override { return "echenon_wahoo"; }
    int getSchemaVersion() const override { return 1; }
    int getConfigSchemaVersion() const { return getSchemaVersion(); }
    void applyConfig(const JsonObject& section) override;
    void serializeConfig(JsonObject& section) const override;
    void buildConfig(JsonObject& section) const { serializeConfig(section); }
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
    std::string _redirectUri;

    std::string _pendingState;
    uint32_t    _stateIssuedMs  = 0;
    bool        _stateActive    = false;

    uint32_t    _redactorWarningCount = 0;
    bool        _redactorSaturated    = false;
    std::string _lastRedactorWarning;
};

} // namespace echenon
