#pragma once

/// @file IntervalsIcuUploader.h
/// @brief Intervals.icu API-key-authenticated uploader implementing IUploader and
///        IConfigurable ("echenon_intervals") with HTTP Basic Auth, ISecretStore
///        encrypted secret storage, and Redactor::registerSecret() integration
///        (SPEC.md §10.2, §10.4).

#include "echenon/IUploader.h"
#include "echenon/RideStore.h"
#include "echenon/StravaUploader.h"   // ISecretStore, IHttpClient, HttpResponse, InMemorySecretStore
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
// IntervalsIcuUploader (SPEC.md §10.2, §10.4)
// =============================================================================

class IntervalsIcuUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* API_BASE_URL = "https://intervals.icu";

    static constexpr const char* KEY_API_KEY    = "intervals.api_key";
    static constexpr const char* KEY_ATHLETE_ID = "intervals.athlete_id";

    /// Callback invoked to register a secret with the Redactor; returns true if protected.
    using SecretRegistrarFn = std::function<bool(const std::string& secret)>;

    IntervalsIcuUploader();
    explicit IntervalsIcuUploader(ISecretStore* secretStore,
                                  IHttpClient* httpClient = nullptr,
                                  SecretRegistrarFn secretRegistrar = nullptr);
    ~IntervalsIcuUploader() override = default;

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

    /// Default Redactor registration helper wrapping Redactor::instance().registerSecret().
    static bool defaultRegisterSecret(const std::string& secret);

    // --- API Credentials (SPEC.md §10.4) ---

    /// Save API key and athlete ID (persists via ISecretStore and registers with Redactor).
    bool setApiCredentials(const std::string& api_key, const std::string& athlete_id);

    /// Test connectivity by verifying credentials are present (no-op network call for sync API).
    bool testConnection() const;

    /// Disconnect: clear API key and athlete ID from secret store and in-memory state.
    bool disconnect();

    // --- Accessors ---
    const std::string& getApiKey() const { return _apiKey; }
    const std::string& getAthleteId() const { return _athleteId; }
    const std::string& getAthleteName() const { return _athleteName; }
    void setAthleteName(const std::string& name) { _athleteName = name; }

    bool isEnabled() const override { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

    uint32_t getRedactorWarningCount() const { return _redactorWarningCount; }
    bool isRedactorSaturated() const { return _redactorSaturated; }
    const std::string& getLastRedactorWarning() const { return _lastRedactorWarning; }

    // --- Base64 Encoding Helper ---

    /// Encode binary data as Base64 (RFC 4648). Used for HTTP Basic Auth header.
    static std::string base64Encode(const std::string& input);

    /// Build the HTTP Basic Auth header value: "Basic base64(api_key:)"
    std::string buildBasicAuthHeader() const;

    /// Build the upload URL for the configured athlete ID.
    std::string buildUploadUrl() const;

    // --- IUploader Interface ---
    const char* serviceId() const override { return "intervals"; }
    const char* name() const override { return "Intervals.icu"; }
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

    // --- IConfigurable ("echenon_intervals", schema v1) ---
    const char* getConfigNamespace() const override { return "echenon_intervals"; }
    int getSchemaVersion() const override { return 1; }
    void applyConfig(const JsonObject& section) override;
    void serializeConfig(JsonObject& section) const override;
    void resetConfig() override;

private:
    bool registerSecretWithRedactor(const std::string& secret, const char* secret_label);
    bool readTcxGzFile(const std::string& path, std::vector<uint8_t>& out_bytes) const;

    InMemorySecretStore _defaultSecretStore;
    ISecretStore*       _secretStore = nullptr;
    IHttpClient*        _httpClient  = nullptr;
    IRideFileSystem*    _fs          = nullptr;
    KeyManager*         _keyManager  = nullptr;

    // GRAVE-05 deviation: DI callback set once at boot, never reallocated.
    // Framework's own DataManager uses the same pattern for IDataSource injection.
    SecretRegistrarFn   _secretRegistrar;

    bool        _enabled = true;
    std::string _apiKey;
    std::string _athleteId;
    std::string _athleteName;

    uint32_t    _redactorWarningCount = 0;
    bool        _redactorSaturated    = false;
    std::string _lastRedactorWarning;
};

} // namespace echenon
