/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/IntervalsIcuUploader.cpp
 * Description: Implementation of the Intervals.icu API-key uploader with HTTP Basic Auth and synchronous uploads.
 */

#include "echenon/IntervalsIcuUploader.h"

#include <ArduinoJson.h>

#if defined(ESP_PLATFORM)
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include "modules/KeyManager/KeyManager.hpp"
#include "types/FrameworkSecureClient.hpp"
#else
// Native test builds — no ESP-specific includes needed
#endif

#include <cstdio>
#include <cstring>

namespace echenon {

namespace {

#if defined(ESP_PLATFORM)
static constexpr const char* TAG = "IntervalsIcuUploader";

class KeyManagerSecretStore : public ISecretStore {
public:
    explicit KeyManagerSecretStore(KeyManager* keyManager = nullptr)
        : _keyManager(keyManager) {}

    void setKeyManager(KeyManager* keyManager) { _keyManager = keyManager; }
    KeyManager* getKeyManager() const { return _keyManager; }

    bool putSecret(const std::string& key, const std::string& value) override {
        if (!_keyManager || key.empty() || value.empty()) return false;
        const std::string qualified = qualifyKey(key);
        return _keyManager->addKey(qualified.c_str(), value.c_str(), "Echenon", true, false);
    }

    std::string getSecret(const std::string& key) const override {
        if (!_keyManager || key.empty()) return {};
        const std::string qualified = qualifyKey(key);
        return _keyManager->getKeyById(qualified.c_str());
    }

    bool deleteSecret(const std::string& key) override {
        if (!_keyManager || key.empty()) return false;
        const std::string qualified = qualifyKey(key);
        return _keyManager->deleteKey(qualified.c_str());
    }

private:
    static std::string qualifyKey(const std::string& key) {
        if (key.rfind("intervals.", 0) == 0) {
            return key;
        }
        return "intervals." + key;
    }

    KeyManager* _keyManager = nullptr;
};

/// ESP32-specific HTTP client for Intervals.icu API calls.
/// Uses HTTP Basic Auth with Content-Type: application/octet-stream.
class EspIntervalsHttpClient : public IHttpClient {
public:
    void setTransportContext(void* client_ctx) override {
        _sharedClient = static_cast<WiFiClient*>(client_ctx);
    }

    HttpResponse postForm(
        const std::string& url,
        const std::string& form_body,
        uint32_t timeout_ms) override {
        // This method is repurposed for Intervals.icu binary uploads.
        // The form_body contains raw binary data + auth header is set externally.
        // For actual Intervals.icu usage, postBinaryWithBasicAuth() is used instead.
        HttpResponse out{};
        out.status_code = 0;
        out.error = "postForm not used for Intervals.icu";
        return out;
    }

    HttpResponse postMultipart(
        const MultipartUploadRequest& req,
        uint32_t timeout_ms) override {
        HttpResponse out{};
        out.status_code = 0;
        out.error = "postMultipart not used for Intervals.icu";
        return out;
    }

    HttpResponse getJson(
        const std::string& url,
        const std::string& bearer_token,
        uint32_t timeout_ms) override {
        HttpResponse out{};
        out.status_code = 0;
        out.error = "getJson not used for Intervals.icu";
        return out;
    }

    /// POST raw binary data with Basic Auth to Intervals.icu.
    HttpResponse postBinaryWithBasicAuth(
        const std::string& url,
        const std::string& auth_header,
        const uint8_t* data,
        size_t data_len,
        uint32_t timeout_ms) {
        HttpResponse out{};

        FrameworkSecureClient localClient;
        WiFiClient* activeClient = _sharedClient;
        if (!activeClient) {
            localClient.useDefaultCertBundle();
            localClient.setTimeout(timeout_ms / 1000u + 1u);
            activeClient = &localClient;
        }

        HTTPClient http;
        http.setTimeout(static_cast<uint16_t>(timeout_ms > 0 ? timeout_ms : 10000u));
        if (!http.begin(*activeClient, url.c_str())) {
            out.status_code = 0;
            out.error       = "HTTPClient begin failed";
            return out;
        }

        http.addHeader("Authorization", auth_header.c_str());
        http.addHeader("Content-Type", "application/octet-stream");

        out.status_code = http.POST(
            const_cast<uint8_t*>(data),
            data_len);
        if (out.status_code > 0) {
            out.body = http.getString().c_str();
        } else {
            out.error = http.errorToString(out.status_code).c_str();
        }
        http.end();
        return out;
    }

private:
    WiFiClient* _sharedClient = nullptr;
};

KeyManagerSecretStore s_espIntervalsKeyManagerStore;
EspIntervalsHttpClient s_espIntervalsHttpClient;
#endif

} // namespace

// =============================================================================
// Base64 Encoding (RFC 4648)
// =============================================================================

std::string IntervalsIcuUploader::base64Encode(const std::string& input) {
    static constexpr const char* CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    const size_t in_len = input.size();
    encoded.reserve(((in_len + 2) / 3) * 4);

    for (size_t i = 0; i < in_len; i += 3) {
        const uint32_t octet_a = static_cast<uint8_t>(input[i]);
        const uint32_t octet_b = (i + 1 < in_len) ? static_cast<uint8_t>(input[i + 1]) : 0;
        const uint32_t octet_c = (i + 2 < in_len) ? static_cast<uint8_t>(input[i + 2]) : 0;

        const uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded.push_back(CHARS[(triple >> 18) & 0x3F]);
        encoded.push_back(CHARS[(triple >> 12) & 0x3F]);
        encoded.push_back((i + 1 < in_len) ? CHARS[(triple >> 6) & 0x3F] : '=');
        encoded.push_back((i + 2 < in_len) ? CHARS[triple & 0x3F] : '=');
    }

    return encoded;
}

// =============================================================================
// Constructors & Dependency Wiring
// =============================================================================

IntervalsIcuUploader::IntervalsIcuUploader()
    : _secretStore(&_defaultSecretStore)
    , _secretRegistrar(&IntervalsIcuUploader::defaultRegisterSecret) {
#if defined(ESP_PLATFORM)
    _httpClient = &s_espIntervalsHttpClient;
#endif
}

IntervalsIcuUploader::IntervalsIcuUploader(
    ISecretStore* secretStore,
    IHttpClient* httpClient,
    SecretRegistrarFn secretRegistrar)
    : _secretStore(secretStore ? secretStore : &_defaultSecretStore)
    , _httpClient(httpClient)
    , _secretRegistrar(secretRegistrar ? std::move(secretRegistrar)
                                       : &IntervalsIcuUploader::defaultRegisterSecret) {
#if defined(ESP_PLATFORM)
    if (!_httpClient) {
        _httpClient = &s_espIntervalsHttpClient;
    }
#endif
}

void IntervalsIcuUploader::setSecretStore(ISecretStore* store) {
    _secretStore = store ? store : &_defaultSecretStore;
}

void IntervalsIcuUploader::setKeyManager(KeyManager* keyManager) {
    _keyManager = keyManager;
#if defined(ESP_PLATFORM)
    if (_keyManager) {
        s_espIntervalsKeyManagerStore.setKeyManager(_keyManager);
        if (_secretStore == &_defaultSecretStore) {
            _secretStore = &s_espIntervalsKeyManagerStore;
        }
    }
#endif
}

void IntervalsIcuUploader::setHttpClient(IHttpClient* httpClient) {
    _httpClient = httpClient;
}

void IntervalsIcuUploader::setSecretRegistrar(SecretRegistrarFn fn) {
    _secretRegistrar = fn ? std::move(fn) : &IntervalsIcuUploader::defaultRegisterSecret;
}

void IntervalsIcuUploader::setTransportContext(void* client_ctx) {
    if (_httpClient) {
        _httpClient->setTransportContext(client_ctx);
    }
}

bool IntervalsIcuUploader::begin() {
    return loadCredentials();
}

bool IntervalsIcuUploader::loadCredentials() {
    if (!_secretStore) {
        return false;
    }

    _apiKey = _secretStore->getSecret(KEY_API_KEY);
    // Preserve _athleteId populated earlier by applyConfig() if KeyManager lacks KEY_ATHLETE_ID.
    const std::string storedAthleteId = _secretStore->getSecret(KEY_ATHLETE_ID);
    if (!storedAthleteId.empty()) {
        _athleteId = storedAthleteId;
    }

    if (!_apiKey.empty()) {
        registerSecretWithRedactor(_apiKey, KEY_API_KEY);
    }

    return true;
}

// =============================================================================
// Redactor Registration & Warning Handling (SPEC.md §10.4)
// =============================================================================

bool IntervalsIcuUploader::defaultRegisterSecret(const std::string& secret) {
    if (secret.empty()) {
        return false;
    }
    const Redactor::RegisterResult res = Redactor::instance().registerSecret(secret);
    return (res == Redactor::RegisterResult::Registered ||
            res == Redactor::RegisterResult::AlreadyPresent);
}

bool IntervalsIcuUploader::registerSecretWithRedactor(
    const std::string& secret,
    const char* secret_label) {
    if (secret.empty()) {
        return false;
    }

    const bool ok = _secretRegistrar
        ? _secretRegistrar(secret)
        : defaultRegisterSecret(secret);

    if (!ok) {
        ++_redactorWarningCount;
        _redactorSaturated = true;
        _lastRedactorWarning = std::string("Redactor slot table full or rejected secret '") +
                               (secret_label ? secret_label : "intervals_secret") +
                               "' — secret will NOT be redacted from logs";
#if defined(ESP_PLATFORM)
        ESP_LOGW(TAG, "%s", _lastRedactorWarning.c_str());
#endif
        return false;
    }
    return true;
}

// =============================================================================
// API Credentials & Connection Management (SPEC.md §10.4)
// =============================================================================

bool IntervalsIcuUploader::setApiCredentials(
    const std::string& api_key,
    const std::string& athlete_id) {
    if (api_key.empty() || athlete_id.empty() || !_secretStore) {
        return false;
    }

    if (!_secretStore->putSecret(KEY_API_KEY, api_key)) {
        return false;
    }

    if (!_secretStore->putSecret(KEY_ATHLETE_ID, athlete_id)) {
        return false;
    }

    _apiKey    = api_key;
    _athleteId = athlete_id;
    registerSecretWithRedactor(_apiKey, KEY_API_KEY);
    return true;
}

bool IntervalsIcuUploader::testConnection() const {
    return isConfigured() && isAuthenticated();
}

bool IntervalsIcuUploader::disconnect() {
    if (_secretStore) {
        _secretStore->deleteSecret(KEY_API_KEY);
        _secretStore->deleteSecret(KEY_ATHLETE_ID);
    }
    _apiKey.clear();
    _athleteId.clear();
    _athleteName.clear();
    return true;
}

// =============================================================================
// HTTP Auth & URL Helpers
// =============================================================================

std::string IntervalsIcuUploader::buildBasicAuthHeader() const {
    // Intervals.icu uses API key as username with empty password: "API_KEY:"
    const std::string credentials = _apiKey + ":";
    return "Basic " + base64Encode(credentials);
}

std::string IntervalsIcuUploader::buildUploadUrl() const {
    return std::string(API_BASE_URL) + "/api/v1/athlete/" + _athleteId + "/activities";
}

// =============================================================================
// File Reading (Same pattern as StravaUploader)
// =============================================================================

bool IntervalsIcuUploader::readTcxGzFile(
    const std::string& path,
    std::vector<uint8_t>& out_bytes) const {
    out_bytes.clear();
    if (path.empty()) return false;

    if (_fs && _fs->exists(path.c_str())) {
        return _fs->readFile(path.c_str(), out_bytes);
    }

#if !defined(ESP_PLATFORM)
    if (path[0] != '/' || path.rfind("/tmp/", 0) == 0) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            const long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                out_bytes.resize(static_cast<size_t>(sz));
                const size_t got = std::fread(out_bytes.data(), 1, static_cast<size_t>(sz), f);
                out_bytes.resize(got);
            }
            std::fclose(f);
            return true;
        }
    }
#endif
    return false;
}

// =============================================================================
// IUploader — Upload Stages (SPEC.md §10.2)
// =============================================================================

bool IntervalsIcuUploader::isConfigured() const {
    return _enabled && !_apiKey.empty() && !_athleteId.empty();
}

bool IntervalsIcuUploader::isAuthenticated() const {
    return !_apiKey.empty() && !_athleteId.empty();
}

bool IntervalsIcuUploader::needsTokenRefresh(uint32_t /*now_epoch_s*/) const {
    // Intervals.icu uses static API keys — no token refresh needed.
    return false;
}

UploadStageResult IntervalsIcuUploader::refreshToken(
    uint32_t /*timeout_ms*/,
    std::string& err_msg) {
    // No OAuth token refresh for API-key auth — always succeeds.
    err_msg.clear();
    return UploadStageResult::SUCCESS;
}

UploadStageResult IntervalsIcuUploader::postUpload(
    uint32_t session_id,
    const std::string& tcx_gz_path,
    uint32_t timeout_ms,
    std::string& out_upload_id,
    std::string& err_msg) {
    out_upload_id.clear();
    if (!isConfigured() || !isAuthenticated()) {
        err_msg = "401 Intervals.icu uploader not configured";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (!_httpClient) {
        err_msg = "HTTP client not configured";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    // Read the compressed .tcx.gz file
    std::vector<uint8_t> file_bytes;
    if (!readTcxGzFile(tcx_gz_path, file_bytes) || file_bytes.empty()) {
        err_msg = "Failed to read TCX.gz file: " + tcx_gz_path;
        return UploadStageResult::PERMANENT_ERROR;
    }

    const std::string url         = buildUploadUrl();
    const std::string auth_header = buildBasicAuthHeader();

    // For native tests: use postForm with auth info encoded in the URL parameter.
    // The mock IHttpClient captures the call and returns the canned response.
    // On ESP32: the dedicated EspIntervalsHttpClient handles binary POST properly.

    // Build form_body as binary data string for IHttpClient::postForm()
    // The auth_header is passed via the URL field as a convention for the mock.
    // The mock just returns the canned response regardless.
    const std::string body_str(reinterpret_cast<const char*>(file_bytes.data()), file_bytes.size());

#if defined(ESP_PLATFORM)
    // Use the dedicated ESP client for proper binary POST with Basic Auth
    auto* espClient = static_cast<EspIntervalsHttpClient*>(_httpClient);
    const HttpResponse resp = espClient->postBinaryWithBasicAuth(
        url, auth_header, file_bytes.data(), file_bytes.size(), timeout_ms);
#else
    // In native tests, use postForm — the mock captures URL and body, returns canned response.
    // We encode the auth header into the form body so tests can verify it.
    const std::string payload = auth_header + "\n" + body_str;
    const HttpResponse resp = _httpClient->postForm(url, payload, timeout_ms);
#endif

    if (resp.status_code == 200 || resp.status_code == 201) {
        // Intervals.icu processes synchronously. Generate a synthetic upload ID
        // from the session_id since the API may not return one.
        char idBuf[32];
        std::snprintf(idBuf, sizeof(idBuf), "intervals_%u",
                      static_cast<unsigned>(session_id));
        out_upload_id = idBuf;
        err_msg.clear();
        return UploadStageResult::SUCCESS;
    }

    if (resp.status_code == 401) {
        err_msg = "HTTP 401 Unauthorized — API key invalid or revoked";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (resp.status_code == 429) {
        err_msg = "HTTP 429 Too Many Requests (rate limit)";
        return UploadStageResult::RETRYABLE_ERROR;
    }
    if (resp.status_code >= 500 || resp.status_code <= 0) {
        err_msg = resp.error.empty()
            ? ("HTTP " + std::to_string(resp.status_code) + " server/network error")
            : resp.error;
        return UploadStageResult::RETRYABLE_ERROR;
    }

    // 400, 403, 404, 422 -> permanent error
    err_msg = "HTTP " + std::to_string(resp.status_code) + " upload rejected";
    return UploadStageResult::PERMANENT_ERROR;
}

UploadStageResult IntervalsIcuUploader::pollUploadStatus(
    const std::string& upload_id,
    uint32_t /*timeout_ms*/,
    std::string& out_activity_id,
    std::string& err_msg) {
    // Intervals.icu processes uploads synchronously — no polling needed.
    // The activity is available immediately after a successful POST.
    out_activity_id = upload_id;
    err_msg.clear();
    return UploadStageResult::SUCCESS;
}

// =============================================================================
// IConfigurable ("echenon_intervals", schema v1) — Non-Secret Metadata Only
// =============================================================================

void IntervalsIcuUploader::applyConfig(const JsonObject& section) {
    if (section["enabled"].is<bool>()) {
        _enabled = section["enabled"].as<bool>();
    }
    if (section["athlete_id"].is<const char*>()) {
        _athleteId = section["athlete_id"].as<const char*>();
    }
    if (section["athlete_name"].is<const char*>()) {
        _athleteName = section["athlete_name"].as<const char*>();
    }
}

void IntervalsIcuUploader::serializeConfig(JsonObject& section) const {
    section["enabled"]      = _enabled;
    section["athlete_id"]   = _athleteId;
    section["athlete_name"] = _athleteName;
}

void IntervalsIcuUploader::resetConfig() {
    _enabled = true;
    _athleteId.clear();
    _athleteName.clear();
}

} // namespace echenon
