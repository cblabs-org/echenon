#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/IntervalsIcuUploader.h
 * Description: Intervals.icu API-key uploader implementing IUploader and IConfigurable ("echenon_intervals") with HTTP Basic Auth and Redactor protection (SPEC.md §10.2, §10.4).
 */

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

/**
 * @brief Cloud activity uploader for Intervals.icu using HTTP Basic Auth
 *        (`API_KEY:`) and encrypted KeyManager/ISecretStore credential storage.
 */
class IntervalsIcuUploader : public IUploader, public IConfigurable {
public:
    // --- Constants ---
    static constexpr const char* API_BASE_URL = "https://intervals.icu"; ///< Intervals.icu HTTPS API base URL

    static constexpr const char* KEY_API_KEY    = "intervals.api_key";    ///< KeyManager/ISecretStore key for API key
    static constexpr const char* KEY_ATHLETE_ID = "intervals.athlete_id"; ///< KeyManager/ISecretStore key for athlete ID

    /// Callback invoked to register a secret with the Redactor; returns true if protected.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only during cold credential load/save, never in tick(); supports native test spies.
    using SecretRegistrarFn = std::function<bool(const std::string& secret)>;

    /// Default constructor using in-memory secret store and default Redactor registrar.
    IntervalsIcuUploader();
    /// Construct IntervalsIcuUploader with injectable secret store, HTTP client, and Redactor registrar.
    explicit IntervalsIcuUploader(ISecretStore* secretStore,
                                  IHttpClient* httpClient = nullptr,
                                  SecretRegistrarFn secretRegistrar = nullptr);
    /// Default destructor.
    ~IntervalsIcuUploader() override = default;

    // --- Dependency Injection & Initialization ---
    /// Load persisted API credentials from ISecretStore and register API key with Redactor.
    bool begin();
    /// Reload API key and athlete ID from ISecretStore.
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
    /// Return configured Intervals.icu API key.
    const std::string& getApiKey() const { return _apiKey; }
    /// Return configured Intervals.icu athlete ID (e.g. "i12345" or "0").
    const std::string& getAthleteId() const { return _athleteId; }
    /// Return optional athlete display name.
    const std::string& getAthleteName() const { return _athleteName; }
    /// Set optional athlete display name.
    void setAthleteName(const std::string& name) { _athleteName = name; }

    /// Return true if Intervals.icu auto-upload sink is enabled.
    bool isEnabled() const override { return _enabled; }
    /// Enable or disable the Intervals.icu upload sink.
    void setEnabled(bool enabled) { _enabled = enabled; }

    /// Return count of Redactor registration warnings encountered.
    uint32_t getRedactorWarningCount() const { return _redactorWarningCount; }
    /// Return true if Redactor secret slot table reached saturation.
    bool isRedactorSaturated() const { return _redactorSaturated; }
    /// Return most recent Redactor warning message.
    const std::string& getLastRedactorWarning() const { return _lastRedactorWarning; }

    // --- Base64 Encoding Helper ---

    /// Encode binary data as Base64 (RFC 4648). Used for HTTP Basic Auth header.
    static std::string base64Encode(const std::string& input);

    /// Build the HTTP Basic Auth header value: "Basic base64(api_key:)"
    std::string buildBasicAuthHeader() const;

    /// Build the upload URL for the configured athlete ID.
    std::string buildUploadUrl() const;

    // --- IUploader Interface ---
    /// Return canonical sink identifier ("intervals").
    const char* serviceId() const override { return "intervals"; }
    /// Return human-readable service name ("Intervals.icu").
    const char* name() const override { return "Intervals.icu"; }
    /// Return true when enabled and API key + athlete ID are present.
    bool isConfigured() const override;
    /// Return true when API key and athlete ID are present.
    bool isAuthenticated() const override;
    /// Return false (Intervals.icu uses static API keys without token expiry).
    bool needsTokenRefresh(uint32_t now_epoch_s) const override;

    /// No-op OAuth token refresh (static API key; always returns SUCCESS).
    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override;

    /// POST compressed `.tcx.gz` activity file with HTTP Basic Auth.
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

    // --- IConfigurable ("echenon_intervals", schema v1) ---
    /// Return configuration namespace ("echenon_intervals").
    const char* getConfigNamespace() const override { return "echenon_intervals"; }
    /// Return configuration schema version (1).
    int getSchemaVersion() const override { return 1; }
    /// Apply persisted non-secret metadata (enabled, athlete_id, athlete_name).
    void applyConfig(const JsonObject& section) override;
    /// Serialize non-secret metadata into JSON object.
    void serializeConfig(JsonObject& section) const override;
    /// Reset non-secret configuration fields to defaults.
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
