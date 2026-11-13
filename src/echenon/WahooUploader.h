#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/WahooUploader.h
 * Description: Wahoo Cloud API uploader implementing IUploader and IConfigurable ("echenon_wahoo") with BYOA OAuth 2.0, CSRF nonce, KeyManager secrets, and Redactor integration (SPEC.md §10.2–§10.5).
 */

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

/**
 * @brief Wahoo Cloud API uploader supporting BYOA OAuth 2.0 token rotation,
 *        CSRF state nonces, and synchronous multipart `.tcx.gz` workout uploads.
 */
class WahooUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* AUTHORIZE_URL = "https://api.wahooligan.com/oauth/authorize"; ///< Wahoo OAuth authorize URL
    static constexpr const char* TOKEN_URL     = "https://api.wahooligan.com/oauth/token";     ///< Wahoo OAuth token exchange/refresh URL
    static constexpr const char* WORKOUTS_URL  = "https://api.wahooligan.com/v1/workouts";     ///< Wahoo Cloud workout upload endpoint

    static constexpr const char* KEY_CLIENT_SECRET = "wahoo.client_secret"; ///< ISecretStore key for OAuth client_secret
    static constexpr const char* KEY_REFRESH_TOKEN = "wahoo.refresh_token"; ///< ISecretStore key for OAuth refresh_token
    static constexpr const char* KEY_ACCESS_TOKEN  = "wahoo.access_token";  ///< ISecretStore key for OAuth access_token

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
    WahooUploader();
    /// Construct WahooUploader with injectable secret store, HTTP client, and Redactor registrar.
    explicit WahooUploader(ISecretStore* secretStore,
                           IHttpClient* httpClient = nullptr,
                           SecretRegistrarFn secretRegistrar = nullptr);
    /// Default destructor.
    ~WahooUploader() override = default;

    // --- Dependency Injection & Initialization ---
    /// Load persisted Wahoo secrets from ISecretStore and register them with Redactor.
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

    // --- BYOA Credentials & OAuth 2.0 Flow ---

    /// Save BYOA Client ID & Client Secret (persists secret via ISecretStore and registers with Redactor).
    bool setClientCredentials(const std::string& client_id, const std::string& client_secret);
    /// Alias for setClientCredentials().
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

    /// Exchange an OAuth authorization code with explicit redirect_uri for access_token & refresh_token.
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
    /// Return configured BYOA Wahoo client_id.
    const std::string& getClientId() const { return _clientId; }
    /// Set BYOA Wahoo client_id.
    void setClientId(const std::string& client_id) { _clientId = client_id; }

    /// Return authenticated Wahoo athlete/user display name.
    const std::string& getAthleteName() const { return _athleteName; }
    /// Set Wahoo athlete/user display name.
    void setAthleteName(const std::string& name) { _athleteName = name; }

    /// Alias for getAthleteName().
    const std::string& getUserName() const { return _athleteName; }
    /// Alias for setAthleteName().
    void setUserName(const std::string& name) { _athleteName = name; }

    /// Return OAuth access_token expiration Unix epoch (seconds).
    uint32_t getTokenExpiresAt() const { return _tokenExpiresAt; }
    /// Set OAuth access_token expiration Unix epoch (seconds).
    void setTokenExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }
    /// Alias for getTokenExpiresAt().
    uint32_t getExpiresAt() const { return _tokenExpiresAt; }
    /// Alias for setTokenExpiresAt().
    void setExpiresAt(uint32_t expires_at) { _tokenExpiresAt = expires_at; }

    /// Return true if Wahoo auto-upload sink is enabled.
    bool isEnabled() const override { return _enabled; }
    /// Enable or disable the Wahoo upload sink.
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

    /// Return configured OAuth redirect_uri.
    const std::string& getRedirectUri() const { return _redirectUri; }
    /// Set OAuth redirect_uri.
    void setRedirectUri(const std::string& uri) { _redirectUri = uri; }

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
    /// Build multipart/form-data request headers, preamble, and payload for Wahoo `/v1/workouts`.
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
    /// Return canonical sink identifier ("wahoo").
    const char* serviceId() const override { return "wahoo"; }
    /// Return human-readable service name ("Wahoo Cloud").
    const char* name() const override { return "Wahoo Cloud"; }
    /// Static helper returning human-readable service name ("Wahoo Cloud").
    static const char* serviceName() { return "Wahoo Cloud"; }
    /// Return true when enabled and client_id + client_secret are configured.
    bool isConfigured() const override;
    /// Return true when OAuth refresh_token is present.
    bool isAuthenticated() const override;
    /// Return true if access_token is empty or expires within TOKEN_EXPIRY_MARGIN_S of now_epoch_s.
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;

    /// POST compressed `.tcx.gz` workout upload to Wahoo `/v1/workouts`.
    UploadStageResult postUpload(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        uint32_t timeout_ms,
        std::string& out_upload_id,
        std::string& err_msg) override;

    /// Confirm upload status synchronously (returns SUCCESS immediately).
    UploadStageResult pollUploadStatus(
        const std::string& upload_id,
        uint32_t timeout_ms,
        std::string& out_activity_id,
        std::string& err_msg) override;

    /// Forward framework transport client (WiFiClientSecure*) to IHttpClient.
    void setTransportContext(void* client_ctx) override;

    // --- IConfigurable ("echenon_wahoo", schema v1) ---
    /// Return configuration namespace ("echenon_wahoo").
    const char* getConfigNamespace() const override { return "echenon_wahoo"; }
    /// Return configuration schema version (1).
    int getSchemaVersion() const override { return 1; }
    /// Alias for getSchemaVersion().
    int getConfigSchemaVersion() const { return getSchemaVersion(); }
    /// Apply persisted non-secret metadata (enabled, client_id, expires_at, athlete_name, redirect_uri).
    void applyConfig(const JsonObject& section) override;
    /// Serialize non-secret metadata into JSON object.
    void serializeConfig(JsonObject& section) const override;
    /// Alias for serializeConfig().
    void buildConfig(JsonObject& section) const { serializeConfig(section); }
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
    std::string _redirectUri;

    std::string _pendingState;
    uint32_t    _stateIssuedMs  = 0;
    bool        _stateActive    = false;

    uint32_t    _redactorWarningCount = 0;
    bool        _redactorSaturated    = false;
    std::string _lastRedactorWarning;
};

} // namespace echenon
