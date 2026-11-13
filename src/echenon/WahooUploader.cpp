/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/WahooUploader.cpp
 * Description: Implementation of the Wahoo Cloud OAuth 2.0 BYOA uploader with token rotation and CSRF protection.
 */

#include "echenon/WahooUploader.h"

#include <ArduinoJson.h>

#if defined(ESP_PLATFORM)
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_random.h>
#include "modules/KeyManager/KeyManager.hpp"
#include "types/FrameworkSecureClient.hpp"
#else
#include <random>
#endif

#include <cstdio>
#include <cstring>
#include <ctime>

namespace echenon {

namespace {

#if defined(ESP_PLATFORM)
static constexpr const char* TAG = "WahooUploader";

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
        if (key.rfind("wahoo.", 0) == 0) {
            return key;
        }
        return "wahoo." + key;
    }

    KeyManager* _keyManager = nullptr;
};

class EspWahooHttpClient : public IHttpClient {
public:
    void setTransportContext(void* client_ctx) override {
        _sharedClient = static_cast<WiFiClient*>(client_ctx);
    }

    HttpResponse postForm(
        const std::string& url,
        const std::string& form_body,
        uint32_t timeout_ms) override {
        HttpResponse out{};
        FrameworkSecureClient localClient;
        WiFiClient* activeClient = _sharedClient;
        if (!activeClient) {
            localClient.useDefaultCertBundle();
            localClient.setTimeout(timeout_ms / 1000u + 1u);
            activeClient = &localClient;
        }

        HTTPClient http;
        http.setTimeout(static_cast<uint16_t>(timeout_ms > 0 ? timeout_ms : 8000u));
        if (!http.begin(*activeClient, url.c_str())) {
            out.status_code = 0;
            out.error       = "HTTPClient begin failed";
            return out;
        }

        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        out.status_code = http.POST(
            reinterpret_cast<uint8_t*>(const_cast<char*>(form_body.data())),
            form_body.size());
        if (out.status_code > 0) {
            out.body = http.getString().c_str();
        } else {
            out.error = http.errorToString(out.status_code).c_str();
        }
        http.end();
        return out;
    }

    HttpResponse postMultipart(
        const MultipartUploadRequest& req,
        uint32_t timeout_ms) override {
        HttpResponse out{};
        std::string payload = WahooUploader::assembleMultipartBody(req);
        if (req.file_data.empty() && !req.file_path.empty()) {
            if (!LittleFS.exists(req.file_path.c_str())) {
                out.status_code = 0;
                out.error       = "TCX.gz file not found on LittleFS";
                return out;
            }
            File f = LittleFS.open(req.file_path.c_str(), "r");
            if (!f) {
                out.status_code = 0;
                out.error       = "Failed to open TCX.gz file on LittleFS";
                return out;
            }
            payload.clear();
            payload.reserve(req.multipart_header.size() + f.size() + req.multipart_footer.size());
            payload.append(req.multipart_header);
            uint8_t buf[1024];
            while (f.available()) {
                const size_t n = f.read(buf, sizeof(buf));
                if (n == 0) break;
                payload.append(reinterpret_cast<const char*>(buf), n);
            }
            f.close();
            payload.append(req.multipart_footer);
        }

        FrameworkSecureClient localClient;
        WiFiClient* activeClient = _sharedClient;
        if (!activeClient) {
            localClient.useDefaultCertBundle();
            localClient.setTimeout(timeout_ms / 1000u + 1u);
            activeClient = &localClient;
        }

        HTTPClient http;
        http.setTimeout(static_cast<uint16_t>(timeout_ms > 0 ? timeout_ms : 10000u));
        if (!http.begin(*activeClient, req.url.c_str())) {
            out.status_code = 0;
            out.error       = "HTTPClient begin failed";
            return out;
        }

        const std::string authHeader = "Bearer " + req.bearer_token;
        http.addHeader("Authorization", authHeader.c_str());
        http.addHeader("Content-Type", req.content_type.c_str());

        out.status_code = http.POST(
            reinterpret_cast<uint8_t*>(const_cast<char*>(payload.data())),
            payload.size());
        if (out.status_code > 0) {
            out.body = http.getString().c_str();
        } else {
            out.error = http.errorToString(out.status_code).c_str();
        }
        http.end();
        return out;
    }

    HttpResponse getJson(
        const std::string& url,
        const std::string& bearer_token,
        uint32_t timeout_ms) override {
        HttpResponse out{};
        FrameworkSecureClient localClient;
        WiFiClient* activeClient = _sharedClient;
        if (!activeClient) {
            localClient.useDefaultCertBundle();
            localClient.setTimeout(timeout_ms / 1000u + 1u);
            activeClient = &localClient;
        }

        HTTPClient http;
        http.setTimeout(static_cast<uint16_t>(timeout_ms > 0 ? timeout_ms : 8000u));
        if (!http.begin(*activeClient, url.c_str())) {
            out.status_code = 0;
            out.error       = "HTTPClient begin failed";
            return out;
        }

        const std::string authHeader = "Bearer " + bearer_token;
        http.addHeader("Authorization", authHeader.c_str());
        http.addHeader("Accept", "application/json");

        out.status_code = http.GET();
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

KeyManagerSecretStore s_espKeyManagerStore;
EspWahooHttpClient   s_espHttpClient;
#endif

} // namespace

// =============================================================================
// Constructors & Dependency Wiring
// =============================================================================

WahooUploader::WahooUploader()
    : _secretStore(&_defaultSecretStore)
    , _secretRegistrar(&WahooUploader::defaultRegisterSecret) {
#if defined(ESP_PLATFORM)
    _httpClient = &s_espHttpClient;
#endif
}

WahooUploader::WahooUploader(
    ISecretStore* secretStore,
    IHttpClient* httpClient,
    SecretRegistrarFn secretRegistrar)
    : _secretStore(secretStore ? secretStore : &_defaultSecretStore)
    , _httpClient(httpClient)
    , _secretRegistrar(secretRegistrar ? std::move(secretRegistrar) : &WahooUploader::defaultRegisterSecret) {
#if defined(ESP_PLATFORM)
    if (!_httpClient) {
        _httpClient = &s_espHttpClient;
    }
#endif
}

void WahooUploader::setSecretStore(ISecretStore* store) {
    _secretStore = store ? store : &_defaultSecretStore;
}

void WahooUploader::setKeyManager(KeyManager* keyManager) {
    _keyManager = keyManager;
#if defined(ESP_PLATFORM)
    if (_keyManager) {
        s_espKeyManagerStore.setKeyManager(_keyManager);
        if (_secretStore == &_defaultSecretStore) {
            _secretStore = &s_espKeyManagerStore;
        }
    }
#endif
}

void WahooUploader::setHttpClient(IHttpClient* httpClient) {
    _httpClient = httpClient;
}

void WahooUploader::setSecretRegistrar(SecretRegistrarFn fn) {
    _secretRegistrar = fn ? std::move(fn) : &WahooUploader::defaultRegisterSecret;
}

void WahooUploader::setTransportContext(void* client_ctx) {
    if (_httpClient) {
        _httpClient->setTransportContext(client_ctx);
    }
}

bool WahooUploader::begin() {
    return loadCredentials();
}

bool WahooUploader::loadCredentials() {
    if (!_secretStore) {
        return false;
    }

    _clientSecret = _secretStore->getSecret(KEY_CLIENT_SECRET);
    if (_clientSecret.empty()) {
        _clientSecret = _secretStore->getSecret("client_secret");
    }

    _refreshToken = _secretStore->getSecret(KEY_REFRESH_TOKEN);
    if (_refreshToken.empty()) {
        _refreshToken = _secretStore->getSecret("refresh_token");
    }

    _accessToken = _secretStore->getSecret(KEY_ACCESS_TOKEN);
    if (_accessToken.empty()) {
        _accessToken = _secretStore->getSecret("access_token");
    }

    if (!_clientSecret.empty()) {
        registerSecretWithRedactor(_clientSecret, KEY_CLIENT_SECRET);
    }
    if (!_refreshToken.empty()) {
        registerSecretWithRedactor(_refreshToken, KEY_REFRESH_TOKEN);
    }
    if (!_accessToken.empty()) {
        registerSecretWithRedactor(_accessToken, KEY_ACCESS_TOKEN);
    }

    return true;
}

// =============================================================================
// Redactor Registration & Warning Handling (SPEC.md §10.4)
// =============================================================================

bool WahooUploader::defaultRegisterSecret(const std::string& secret) {
    if (secret.empty()) {
        return false;
    }
    const Redactor::RegisterResult res = Redactor::instance().registerSecret(secret);
    return (res == Redactor::RegisterResult::Registered ||
            res == Redactor::RegisterResult::AlreadyPresent);
}

bool WahooUploader::registerSecretWithRedactor(
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
                               (secret_label ? secret_label : "wahoo_secret") +
                               "' — secret will NOT be redacted from logs";
#if defined(ESP_PLATFORM)
        ESP_LOGW(TAG, "%s", _lastRedactorWarning.c_str());
#endif
        return false;
    }
    return true;
}

// =============================================================================
// BYOA Client Credentials & Single-Use OAuth State Nonce (SPEC.md §10.4)
// =============================================================================

bool WahooUploader::setClientCredentials(
    const std::string& client_id,
    const std::string& client_secret) {
    if (client_id.empty() || client_secret.empty() || !_secretStore) {
        return false;
    }

    if (!_secretStore->putSecret(KEY_CLIENT_SECRET, client_secret)) {
        return false;
    }

    _clientId     = client_id;
    _clientSecret = client_secret;
    registerSecretWithRedactor(_clientSecret, KEY_CLIENT_SECRET);
    return true;
}

uint32_t WahooUploader::nextRandomU32() {
    if (_randomU32Fn) {
        return _randomU32Fn();
    }
#if defined(ESP_PLATFORM)
    return esp_random();
#else
    static std::mt19937 s_rng{std::random_device{}()};
    return s_rng();
#endif
}

std::string WahooUploader::generateRandomStateHex() {
    char buf[STATE_HEX_LEN + 1];
    const uint32_t w0 = nextRandomU32();
    const uint32_t w1 = nextRandomU32();
    const uint32_t w2 = nextRandomU32();
    const uint32_t w3 = nextRandomU32();
    std::snprintf(buf, sizeof(buf), "%08x%08x%08x%08x",
                  static_cast<unsigned>(w0),
                  static_cast<unsigned>(w1),
                  static_cast<unsigned>(w2),
                  static_cast<unsigned>(w3));
    return std::string(buf, STATE_HEX_LEN);
}

std::string WahooUploader::urlEncode(const std::string& value) {
    std::string encoded;
    encoded.reserve(value.size() + 16u);
    static constexpr const char* HEX = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(HEX[(c >> 4) & 0x0Fu]);
            encoded.push_back(HEX[c & 0x0Fu]);
        }
    }
    return encoded;
}

std::string WahooUploader::buildAuthorizeUrl(
    const std::string& redirect_uri,
    uint32_t now_ms) {
    if (_clientId.empty()) return {};
    _pendingState  = generateRandomStateHex();
    _stateIssuedMs = now_ms;
    _stateActive   = true;
    _redirectUri   = redirect_uri;

    std::string url = AUTHORIZE_URL;
    url += "?client_id=" + urlEncode(_clientId);
    url += "&response_type=code";
    url += "&redirect_uri=" + urlEncode(redirect_uri);
    url += "&scope=" + urlEncode("user_read workouts_write");
    url += "&state=" + _pendingState;
    return url;
}

bool WahooUploader::validateAndConsumeState(
    const std::string& state,
    uint32_t now_ms) {
    if (!_stateActive || state.empty() || _pendingState.empty()) {
        return false;
    }

    // Wraparound-safe signed 32-bit millis() delta check (10-minute / 600,000 ms TTL)
    const int32_t elapsed_ms = static_cast<int32_t>(now_ms - _stateIssuedMs);
    if (elapsed_ms < 0 || elapsed_ms > static_cast<int32_t>(STATE_TTL_MS)) {
        _pendingState.clear();
        _stateActive = false;
        return false;
    }

    if (state != _pendingState) {
        return false;
    }

    // Single-use: immediately consume and invalidate the nonce so replay attempts fail
    _pendingState.clear();
    _stateActive = false;
    return true;
}

// =============================================================================
// OAuth Token Exchange & Rotated Refresh-Token-First Persistence (SPEC.md §10.4)
// =============================================================================

UploadStageResult WahooUploader::exchangeAuthCode(
    const std::string& code,
    uint32_t timeout_ms,
    std::string& err_msg) {
    return exchangeAuthCode(code, "", timeout_ms, err_msg);
}

UploadStageResult WahooUploader::exchangeAuthCode(
    const std::string& code,
    const std::string& redirect_uri,
    uint32_t timeout_ms,
    std::string& err_msg) {
    if (!isConfigured()) {
        err_msg = "Wahoo client credentials not configured";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (code.empty()) {
        err_msg = "Authorization code is empty";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (!_httpClient) {
        err_msg = "HTTP client not configured";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    const std::string effectiveRedirect = !redirect_uri.empty() ? redirect_uri : _redirectUri;

    std::string form_body = "client_id=" + urlEncode(_clientId) +
                            "&client_secret=" + urlEncode(_clientSecret) +
                            "&code=" + urlEncode(code) +
                            "&grant_type=authorization_code";
    if (!effectiveRedirect.empty()) {
        form_body += "&redirect_uri=" + urlEncode(effectiveRedirect);
    }

    const HttpResponse resp = _httpClient->postForm(TOKEN_URL, form_body, timeout_ms);
    if (resp.status_code == 200 || resp.status_code == 201) {
        return handleTokenResponse(resp.body, err_msg);
    }
    if (resp.status_code == 401) {
        err_msg = "HTTP 401 Unauthorized";
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

    err_msg = "HTTP " + std::to_string(resp.status_code) + " authorization code rejected";
    return UploadStageResult::PERMANENT_ERROR;
}

UploadStageResult WahooUploader::refreshToken(
    uint32_t timeout_ms,
    std::string& err_msg) {
    if (!isConfigured()) {
        err_msg = "401 Wahoo client credentials not configured";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (_refreshToken.empty()) {
        err_msg = "401 Wahoo refresh token missing";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (!_httpClient) {
        err_msg = "HTTP client not configured";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    std::string form_body = "client_id=" + urlEncode(_clientId) +
                            "&client_secret=" + urlEncode(_clientSecret) +
                            "&grant_type=refresh_token" +
                            "&refresh_token=" + urlEncode(_refreshToken);

    const HttpResponse resp = _httpClient->postForm(TOKEN_URL, form_body, timeout_ms);
    if (resp.status_code == 200 || resp.status_code == 201) {
        return handleTokenResponse(resp.body, err_msg);
    }
    if (resp.status_code == 401) {
        err_msg = "HTTP 401 Unauthorized";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (resp.status_code == 429) {
        err_msg = "HTTP 429 Too Many Requests (rate limit)";
        return UploadStageResult::RETRYABLE_ERROR;
    }
    if (resp.status_code >= 500 || resp.status_code <= 0) {
        err_msg = resp.error.empty()
            ? ("HTTP " + std::to_string(resp.status_code) + " token refresh network error")
            : resp.error;
        return UploadStageResult::RETRYABLE_ERROR;
    }

    err_msg = "HTTP " + std::to_string(resp.status_code) + " auth token refresh rejected";
    return UploadStageResult::PERMANENT_ERROR;
}

UploadStageResult WahooUploader::handleTokenResponse(
    const std::string& json_body,
    std::string& err_msg) {
    if (json_body.empty()) {
        err_msg = "Empty token response body";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json_body);
    if (err) {
        err_msg = std::string("Invalid token JSON: ") + err.c_str();
        return UploadStageResult::RETRYABLE_ERROR;
    }

    const char* new_refresh_cstr = doc["refresh_token"] | "";
    const char* new_access_cstr  = doc["access_token"]  | "";
    uint32_t new_expires         = doc["expires_at"]    | 0u;

    if (new_expires == 0u) {
        const uint32_t expires_in = doc["expires_in"] | 0u;
        const uint32_t created_at = doc["created_at"] | 0u;
        if (expires_in > 0u) {
            if (created_at > 0u) {
                new_expires = created_at + expires_in;
            } else {
                const std::time_t now = std::time(nullptr);
                if (now > 1000000) {
                    new_expires = static_cast<uint32_t>(now) + expires_in;
                } else {
                    new_expires = expires_in;
                }
            }
        }
    }

    if (std::strlen(new_refresh_cstr) == 0 ||
        std::strlen(new_access_cstr) == 0 ||
        new_expires == 0u) {
        err_msg = "Token response missing refresh_token, access_token, or expires_at";
        return UploadStageResult::PERMANENT_ERROR;
    }

    const std::string new_refresh_token(new_refresh_cstr);
    const std::string new_access_token(new_access_cstr);

    // -------------------------------------------------------------------------
    // CRITICAL INVARIANT (SPEC.md §10.4):
    // 1. Persist rotated `refresh_token` FIRST. If this write fails, abort
    //    immediately so the previous refresh token is never overwritten with
    //    an inconsistent state.
    // 2. Persist `access_token` SECOND.
    // 3. Persist `expires_at` (and athlete metadata) THIRD.
    // 4. Register both new tokens with Redactor.
    // -------------------------------------------------------------------------
    if (!_secretStore || !_secretStore->putSecret(KEY_REFRESH_TOKEN, new_refresh_token)) {
        err_msg = "Failed to persist rotated refresh_token to secret store";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    if (!_secretStore->putSecret(KEY_ACCESS_TOKEN, new_access_token)) {
        err_msg = "Failed to persist access_token to secret store";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    _refreshToken   = new_refresh_token;
    _accessToken    = new_access_token;
    _tokenExpiresAt = new_expires;

    // Extract user/athlete name if present
    std::string athlete_name;
    if (doc["user"].is<JsonObject>()) {
        JsonObject user = doc["user"].as<JsonObject>();
        const char* first = user["first_name"] | (user["firstname"] | "");
        const char* last  = user["last_name"]  | (user["lastname"]  | "");
        const char* uname = user["username"]   | (user["user_name"] | "");
        if (std::strlen(first) > 0 && std::strlen(last) > 0) {
            athlete_name = std::string(first) + " " + std::string(last);
        } else if (std::strlen(first) > 0) {
            athlete_name = first;
        } else if (std::strlen(last) > 0) {
            athlete_name = last;
        } else if (std::strlen(uname) > 0) {
            athlete_name = uname;
        }
    } else if (doc["athlete"].is<JsonObject>()) {
        JsonObject athlete = doc["athlete"].as<JsonObject>();
        const char* first = athlete["firstname"] | (athlete["first_name"] | "");
        const char* last  = athlete["lastname"]  | (athlete["last_name"]  | "");
        const char* uname = athlete["username"]  | (athlete["user_name"] | "");
        if (std::strlen(first) > 0 && std::strlen(last) > 0) {
            athlete_name = std::string(first) + " " + std::string(last);
        } else if (std::strlen(first) > 0) {
            athlete_name = first;
        } else if (std::strlen(last) > 0) {
            athlete_name = last;
        } else if (std::strlen(uname) > 0) {
            athlete_name = uname;
        }
    } else if (doc["athlete_name"].is<const char*>()) {
        const char* name = doc["athlete_name"].as<const char*>();
        if (name && std::strlen(name) > 0) {
            athlete_name = name;
        }
    } else if (doc["user_name"].is<const char*>()) {
        const char* name = doc["user_name"].as<const char*>();
        if (name && std::strlen(name) > 0) {
            athlete_name = name;
        }
    } else if (doc["name"].is<const char*>()) {
        const char* name = doc["name"].as<const char*>();
        if (name && std::strlen(name) > 0) {
            athlete_name = name;
        }
    }

    if (!athlete_name.empty()) {
        _athleteName = athlete_name;
    }

    if (_metadataSaveFn) {
        _metadataSaveFn(_tokenExpiresAt, _athleteName);
    }

    registerSecretWithRedactor(_refreshToken, KEY_REFRESH_TOKEN);
    registerSecretWithRedactor(_accessToken, KEY_ACCESS_TOKEN);

    err_msg.clear();
    return UploadStageResult::SUCCESS;
}

// =============================================================================
// Multipart TCX.gz Upload & Processing Status Polling (SPEC.md §10.2–§10.5)
// =============================================================================

void WahooUploader::buildMultipartRequest(
    uint32_t session_id,
    const std::string& tcx_gz_path,
    const std::string& bearer_token,
    const uint8_t* file_data,
    size_t file_len,
    MultipartUploadRequest& out_req) {
    char extBuf[64];
    std::snprintf(extBuf, sizeof(extBuf), "echenon_%u.tcx.gz",
                  static_cast<unsigned>(session_id));

    char boundaryBuf[64];
    std::snprintf(boundaryBuf, sizeof(boundaryBuf), "----EchenonWahooBoundary%u",
                  static_cast<unsigned>(session_id));

    out_req.url          = WORKOUTS_URL;
    out_req.bearer_token = bearer_token;
    out_req.boundary     = boundaryBuf;
    out_req.content_type = "multipart/form-data; boundary=" + out_req.boundary;
    out_req.data_type    = "tcx.gz";
    out_req.external_id  = extBuf;
    out_req.trainer      = "1";
    out_req.file_path    = tcx_gz_path;
    out_req.file_name    = extBuf;

    out_req.file_data.clear();
    if (file_data && file_len > 0) {
        out_req.file_data.assign(file_data, file_data + file_len);
    }

    std::string hdr;
    hdr.reserve(512);
    auto appendField = [&](const char* name, const std::string& value) {
        hdr += "--" + out_req.boundary + "\r\n";
        hdr += "Content-Disposition: form-data; name=\"";
        hdr += name;
        hdr += "\"\r\n\r\n";
        hdr += value;
        hdr += "\r\n";
    };

    appendField("workout[workout_type_id]", "1");

    hdr += "--" + out_req.boundary + "\r\n";
    hdr += "Content-Disposition: form-data; name=\"workout[file]\"; filename=\"" + out_req.file_name + "\"\r\n";
    hdr += "Content-Type: application/gzip\r\n\r\n";

    out_req.multipart_header = std::move(hdr);
    out_req.multipart_footer = "\r\n--" + out_req.boundary + "--\r\n";
}

std::string WahooUploader::assembleMultipartBody(const MultipartUploadRequest& req) {
    std::string body;
    body.reserve(req.multipart_header.size() + req.file_data.size() + req.multipart_footer.size());
    body.append(req.multipart_header);
    if (!req.file_data.empty()) {
        body.append(reinterpret_cast<const char*>(req.file_data.data()), req.file_data.size());
    }
    body.append(req.multipart_footer);
    return body;
}

bool WahooUploader::readTcxGzFile(
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

UploadStageResult WahooUploader::postUpload(
    uint32_t session_id,
    const std::string& tcx_gz_path,
    uint32_t timeout_ms,
    std::string& out_upload_id,
    std::string& err_msg) {
    out_upload_id.clear();
    if (!isConfigured() || !isAuthenticated()) {
        err_msg = "401 Wahoo uploader not authenticated";
        return UploadStageResult::PERMANENT_ERROR;
    }
    if (!_httpClient) {
        err_msg = "HTTP client not configured";
        return UploadStageResult::RETRYABLE_ERROR;
    }

    std::vector<uint8_t> file_bytes;
    readTcxGzFile(tcx_gz_path, file_bytes);

    MultipartUploadRequest req{};
    buildMultipartRequest(
        session_id,
        tcx_gz_path,
        _accessToken,
        file_bytes.empty() ? nullptr : file_bytes.data(),
        file_bytes.size(),
        req);

    const HttpResponse resp = _httpClient->postMultipart(req, timeout_ms);

    if (resp.status_code == 200 || resp.status_code == 201) {
        JsonDocument doc;
        const DeserializationError jsonErr = deserializeJson(doc, resp.body);
        if (jsonErr) {
            err_msg = std::string("Invalid upload JSON: ") + jsonErr.c_str();
            return UploadStageResult::RETRYABLE_ERROR;
        }

        if (doc["error"].is<const char*>()) {
            const char* apiErr = doc["error"].as<const char*>();
            if (apiErr && std::strlen(apiErr) > 0) {
                err_msg = apiErr;
                return UploadStageResult::PERMANENT_ERROR;
            }
        }

        if (doc["id_str"].is<const char*>()) {
            const char* idStr = doc["id_str"].as<const char*>();
            if (idStr && std::strlen(idStr) > 0) {
                out_upload_id = idStr;
            }
        }
        if (out_upload_id.empty()) {
            if (doc["id"].is<uint64_t>()) {
                const uint64_t idNum = doc["id"].as<uint64_t>();
                if (idNum > 0) {
                    out_upload_id = std::to_string(idNum);
                }
            } else if (doc["id"].is<const char*>()) {
                const char* idStr = doc["id"].as<const char*>();
                if (idStr && std::strlen(idStr) > 0) {
                    out_upload_id = idStr;
                }
            } else if (doc["workout"].is<JsonObject>()) {
                JsonObject w = doc["workout"].as<JsonObject>();
                if (w["id"].is<uint64_t>()) {
                    out_upload_id = std::to_string(w["id"].as<uint64_t>());
                } else if (w["id"].is<const char*>()) {
                    out_upload_id = w["id"].as<const char*>();
                }
            }
        }

        if (out_upload_id.empty()) {
            err_msg = "Missing workout ID in Wahoo response";
            return UploadStageResult::PERMANENT_ERROR;
        }

        err_msg.clear();
        return UploadStageResult::SUCCESS;
    }

    if (resp.status_code == 401) {
        err_msg = "HTTP 401 Unauthorized";
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

UploadStageResult WahooUploader::pollUploadStatus(
    const std::string& upload_id,
    uint32_t /*timeout_ms*/,
    std::string& out_activity_id,
    std::string& err_msg) {
    if (upload_id.empty()) {
        err_msg = "Empty upload_id for status poll";
        return UploadStageResult::PERMANENT_ERROR;
    }
    out_activity_id = upload_id;
    err_msg.clear();
    return UploadStageResult::SUCCESS;
}

// =============================================================================
// Capability State Queries & Disconnection
// =============================================================================

bool WahooUploader::isConfigured() const {
    return _enabled && !_clientId.empty() && !_clientSecret.empty();
}

bool WahooUploader::isAuthenticated() const {
    return !_accessToken.empty() && !_refreshToken.empty();
}

bool WahooUploader::needsTokenRefresh(uint32_t now_epoch_s) const {
    if (_accessToken.empty() || _tokenExpiresAt == 0u) {
        return true;
    }
    if (now_epoch_s == 0u) {
        return false;
    }
    return (now_epoch_s + TOKEN_EXPIRY_MARGIN_S) >= _tokenExpiresAt;
}

bool WahooUploader::disconnect() {
    if (_secretStore) {
        _secretStore->deleteSecret(KEY_ACCESS_TOKEN);
        _secretStore->deleteSecret(KEY_REFRESH_TOKEN);
    }
    _accessToken.clear();
    _refreshToken.clear();
    _tokenExpiresAt = 0;
    _athleteName.clear();
    _pendingState.clear();
    _stateActive = false;
    _redirectUri.clear();
    return true;
}

bool WahooUploader::clearCredentials() {
    disconnect();
    if (_secretStore) {
        _secretStore->deleteSecret(KEY_CLIENT_SECRET);
    }
    _clientId.clear();
    _clientSecret.clear();
    return true;
}

// =============================================================================
// IConfigurable ("echenon_wahoo", schema v1) — Non-Secret Metadata Only
// =============================================================================

void WahooUploader::applyConfig(const JsonObject& section) {
    if (section["enabled"].is<bool>()) {
        _enabled = section["enabled"].as<bool>();
    }
    if (section["client_id"].is<const char*>()) {
        _clientId = section["client_id"].as<const char*>();
    }
    if (section["expires_at"].is<uint32_t>()) {
        _tokenExpiresAt = section["expires_at"].as<uint32_t>();
    } else if (section["token_expires_at"].is<uint32_t>()) {
        _tokenExpiresAt = section["token_expires_at"].as<uint32_t>();
    }
    if (section["user_name"].is<const char*>()) {
        _athleteName = section["user_name"].as<const char*>();
    } else if (section["athlete_name"].is<const char*>()) {
        _athleteName = section["athlete_name"].as<const char*>();
    }
}

void WahooUploader::serializeConfig(JsonObject& section) const {
    section["enabled"]          = _enabled;
    section["client_id"]        = _clientId;
    section["expires_at"]       = _tokenExpiresAt;
    section["token_expires_at"] = _tokenExpiresAt;
    section["user_name"]        = _athleteName;
    section["athlete_name"]     = _athleteName;
}

void WahooUploader::resetConfig() {
    _enabled        = true;
    _clientId.clear();
    _tokenExpiresAt = 0;
    _athleteName.clear();
}

} // namespace echenon
