/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_wahoo_uploader.cpp
 * Description: Native unit tests for WahooUploader OAuth flows, token rotation, and UploadQueue progression.
 */

/// @file test_wahoo_uploader.cpp
/// @brief Native unit tests for WahooUploader OAuth flows, token rotation, and UploadQueue progression.
///
/// WahooUploader.cpp symbols are provided via test_session_manager.o
/// (SessionOverlay.cpp -> #ifdef UNIT_TEST -> WahooUploader.cpp).
/// IntervalsIcuUploader.cpp symbols are provided via test_session_manager.o.
/// StravaUploader.cpp symbols are provided via test_strava_uploader.o.
/// UploadQueue.cpp symbols are provided via test_upload_queue.o.
/// Redactor.cpp symbols are provided via test_strava_uploader.o.
/// RideStore.cpp symbols are provided via test_ride_persistence.o.
/// TCXGenerator.cpp symbols are provided via test_tcx_generator.o.
/// We ONLY include headers here to prevent duplicate symbol linker errors.

#include <gtest/gtest.h>

#include "echenon/IUploader.h"
#include "echenon/IntervalsIcuUploader.h"
#include "echenon/RideStore.h"
#include "echenon/StravaUploader.h"
#include "echenon/TCXGenerator.h"
#include "echenon/UploadQueue.h"
#include "echenon/WahooUploader.h"
#include "lib/Redactor/Redactor.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace echenon;

namespace {

/**
 * @brief Spy secret store that records chronological write/delete operations
 *        and supports simulated write failures for NVS atomic abort testing.
 */
class OrderedSpySecretStore : public ISecretStore {
public:
    struct WriteEntry {
        std::string key;
        std::string value;
    };

    std::vector<WriteEntry>  write_log;
    std::vector<std::string> events;
    std::map<std::string, std::string> store;

    bool fail_refresh_token_write = false;
    bool fail_access_token_write  = false;
    bool fail_client_secret_write = false;

    bool putSecret(const std::string& key, const std::string& value) override {
        if (key == WahooUploader::KEY_REFRESH_TOKEN && fail_refresh_token_write) {
            events.push_back("fail:" + key);
            return false;
        }
        if (key == WahooUploader::KEY_ACCESS_TOKEN && fail_access_token_write) {
            events.push_back("fail:" + key);
            return false;
        }
        if (key == WahooUploader::KEY_CLIENT_SECRET && fail_client_secret_write) {
            events.push_back("fail:" + key);
            return false;
        }
        write_log.push_back({key, value});
        events.push_back("write:" + key);
        store[key] = value;
        return true;
    }

    std::string getSecret(const std::string& key) const override {
        auto it = store.find(key);
        return (it != store.end()) ? it->second : std::string{};
    }

    bool deleteSecret(const std::string& key) override {
        events.push_back("delete:" + key);
        store.erase(key);
        return true;
    }

    void clearLog() {
        write_log.clear();
        events.clear();
    }
};

/**
 * @brief Deterministic mock HTTP client capturing form POSTs, multipart requests, and GET requests.
 */
class MockWahooHttpClient : public IHttpClient {
public:
    HttpResponse next_form_response{200, "", ""};
    HttpResponse next_multipart_response{201, "", ""};
    HttpResponse next_get_response{200, "", ""};

    std::string last_form_url;
    std::string last_form_body;
    uint32_t    last_form_timeout_ms = 0;

    MultipartUploadRequest last_multipart_req;
    uint32_t               last_multipart_timeout_ms = 0;

    std::string last_get_url;
    std::string last_get_bearer;
    uint32_t    last_get_timeout_ms = 0;

    HttpResponse postForm(
        const std::string& url,
        const std::string& form_body,
        uint32_t timeout_ms) override {
        last_form_url        = url;
        last_form_body       = form_body;
        last_form_timeout_ms = timeout_ms;
        return next_form_response;
    }

    HttpResponse postMultipart(
        const MultipartUploadRequest& req,
        uint32_t timeout_ms) override {
        last_multipart_req        = req;
        last_multipart_timeout_ms = timeout_ms;
        return next_multipart_response;
    }

    HttpResponse getJson(
        const std::string& url,
        const std::string& bearer_token,
        uint32_t timeout_ms) override {
        last_get_url        = url;
        last_get_bearer     = bearer_token;
        last_get_timeout_ms = timeout_ms;
        return next_get_response;
    }
};

/**
 * @brief Lightweight mock uploader sink for multi-sink progression verification.
 */
class MockProgressionSink : public IUploader {
public:
    std::string mock_service_id;
    std::string mock_name;
    bool configured          = true;
    bool authenticated       = true;
    bool token_needs_refresh = false;
    bool enabled             = true;

    UploadStageResult refresh_result = UploadStageResult::SUCCESS;
    UploadStageResult post_result    = UploadStageResult::SUCCESS;
    UploadStageResult poll_result    = UploadStageResult::SUCCESS;

    std::string refresh_err;
    std::string post_err;
    std::string poll_err;

    std::string returned_upload_id   = "up_default";
    std::string returned_activity_id = "act_default";

    uint32_t post_calls = 0;
    uint32_t poll_calls = 0;

    const char* serviceId() const override { return mock_service_id.c_str(); }
    const char* name() const override { return mock_name.c_str(); }
    bool isEnabled() const override { return enabled; }
    bool isConfigured() const override { return configured; }
    bool isAuthenticated() const override { return authenticated; }
    bool needsTokenRefresh(uint32_t) const override { return token_needs_refresh; }

    UploadStageResult refreshToken(uint32_t, std::string& err) override {
        err = refresh_err;
        return refresh_result;
    }

    UploadStageResult postUpload(
        uint32_t,
        const std::string&,
        uint32_t,
        std::string& out_upload_id,
        std::string& err) override {
        ++post_calls;
        out_upload_id = returned_upload_id;
        err = post_err;
        return post_result;
    }

    UploadStageResult pollUploadStatus(
        const std::string&,
        uint32_t,
        std::string& out_activity_id,
        std::string& err) override {
        ++poll_calls;
        out_activity_id = returned_activity_id;
        err = poll_err;
        return poll_result;
    }
};

/**
 * @brief Helper to write synthetic ride records to RideStore for TCX generation.
 */
void populateTestRide(RideStore& store, uint32_t session_id, uint16_t count = 5) {
    ASSERT_TRUE(store.startNewSession(session_id, 1789678800u, true));
    for (uint16_t i = 0; i < count; ++i) {
        RideRecord rec{};
        rec.timestamp_offset_s  = i;
        rec.power_watts         = static_cast<uint16_t>(180 + (i % 20));
        rec.cadence_rpm         = 85;
        rec.heart_rate_bpm      = 145;
        rec.internal_resistance = 120;
        rec.flags               = RIDE_FLAG_HR_VALID | RIDE_FLAG_RESISTANCE_CALIBRATED;
        ASSERT_TRUE(store.appendRecord(rec));
    }
    ASSERT_TRUE(store.finalizeSession());
}

} // namespace

// =============================================================================
// 1. BuildAuthorizeUrlAndValidateSingleUseStateNonce (SPEC.md §10.4)
// =============================================================================

TEST(WahooUploaderTest, BuildAuthorizeUrlAndValidateSingleUseStateNonce) {
    OrderedSpySecretStore secretStore;
    MockWahooHttpClient  httpClient;
    WahooUploader        uploader(&secretStore, &httpClient);

    EXPECT_STREQ(uploader.serviceId(), "wahoo");
    EXPECT_STREQ(uploader.name(), "Wahoo Cloud");
    EXPECT_FALSE(uploader.isConfigured());

    ASSERT_TRUE(uploader.setClientCredentials("client_wahoo_123", "secret_wahoo_456"));
    EXPECT_TRUE(uploader.isConfigured());

    // Deterministic RNG for predictable 32-hex state check
    uint32_t counter = 0x11223344u;
    uploader.setRandomGenerator([&counter]() -> uint32_t {
        const uint32_t val = counter;
        counter += 0x11111111u;
        return val;
    });

    const std::string authUrl = uploader.buildAuthorizeUrl(
        "http://echenon.local/api/wahoo/callback", 1000u);

    EXPECT_TRUE(uploader.isStateActive());
    const std::string nonce = uploader.getPendingState();
    EXPECT_EQ(nonce.size(), 32u);
    EXPECT_EQ(nonce, "11223344223344553344556644556677");

    // Verify URL components
    EXPECT_NE(authUrl.find("https://api.wahooligan.com/oauth/authorize?"), std::string::npos);
    EXPECT_NE(authUrl.find("client_id=client_wahoo_123"), std::string::npos);
    EXPECT_NE(authUrl.find("response_type=code"), std::string::npos);
    EXPECT_NE(authUrl.find("redirect_uri=http%3A%2F%2Fechenon.local%2Fapi%2Fwahoo%2Fcallback"),
              std::string::npos);
    EXPECT_NE(authUrl.find("scope=user_read%20workouts_write"), std::string::npos);
    EXPECT_NE(authUrl.find("&state=" + nonce), std::string::npos);

    // Mismatched state is rejected without consuming active state
    EXPECT_FALSE(uploader.validateAndConsumeState("wrong_state_val", 2000u));
    EXPECT_FALSE(uploader.validateAndConsumeState("", 2000u));
    EXPECT_TRUE(uploader.isStateActive());

    // Valid match within 10-minute (600,000 ms) TTL succeeds and immediately consumes nonce
    EXPECT_TRUE(uploader.validateAndConsumeState(nonce, 1000u + 599999u));
    EXPECT_FALSE(uploader.isStateActive());
    EXPECT_TRUE(uploader.getPendingState().empty());

    // Single-use replay with the same nonce MUST be rejected
    EXPECT_FALSE(uploader.validateAndConsumeState(nonce, 1000u + 600000u));
}

// =============================================================================
// 2. StateNonceTtlExpiryAndMillisWraparound (SPEC.md §10.4)
// =============================================================================

TEST(WahooUploaderTest, StateNonceTtlExpiryAndMillisWraparound) {
    OrderedSpySecretStore secretStore;
    WahooUploader        uploader(&secretStore);
    ASSERT_TRUE(uploader.setClientCredentials("client_wahoo_123", "secret_wahoo_456"));

    // 1. Issue state at t = 10,000 ms; expire at t = 610,001 ms (> 600,000 ms TTL)
    uploader.buildAuthorizeUrl("http://echenon.local/api/wahoo/callback", 10000u);
    const std::string expiredNonce = uploader.getPendingState();
    ASSERT_EQ(expiredNonce.size(), 32u);

    EXPECT_FALSE(uploader.validateAndConsumeState(expiredNonce, 10000u + 600001u));
    EXPECT_FALSE(uploader.isStateActive());

    // 2. Wraparound-safe signed 32-bit millis() arithmetic across 0xFFFFFFFF -> 0x00000200
    const uint32_t issuedNearWrap = 0xFFFFFF00u;
    uploader.buildAuthorizeUrl("http://echenon.local/api/wahoo/callback", issuedNearWrap);
    const std::string wrapNonce = uploader.getPendingState();

    // 512 ms after issuedNearWrap (wraps past 0)
    const uint32_t afterWrapMs = issuedNearWrap + 512u;
    EXPECT_TRUE(uploader.validateAndConsumeState(wrapNonce, afterWrapMs));
    EXPECT_FALSE(uploader.isStateActive());

    // 3. Wraparound past TTL
    uploader.buildAuthorizeUrl("http://echenon.local/api/wahoo/callback", issuedNearWrap);
    const std::string wrapExpiredNonce = uploader.getPendingState();
    const uint32_t expiredWrapMs = issuedNearWrap + 600001u;
    EXPECT_FALSE(uploader.validateAndConsumeState(wrapExpiredNonce, expiredWrapMs));
    EXPECT_FALSE(uploader.isStateActive());
}

// =============================================================================
// 3. RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt (SPEC.md §10.4)
// =============================================================================

TEST(WahooUploaderTest, RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt) {
    OrderedSpySecretStore secretStore;
    MockWahooHttpClient  httpClient;
    WahooUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("client_wahoo_1", "client_sec_wahoo"));
    secretStore.clearLog();

    // Hook metadata persistence callback into spy secret store event log
    uploader.setMetadataSaveCallback([&secretStore](uint32_t expires_at, const std::string& athlete) {
        secretStore.events.push_back("meta:expires_at=" + std::to_string(expires_at) + ":" + athlete);
        return true;
    });

    // Test 1: OAuth authorization code exchange persistence ordering
    httpClient.next_form_response = {
        200,
        R"({"token_type":"Bearer","expires_at":1760021600,"expires_in":21600,)"
        R"("refresh_token":"rot_refresh_001","access_token":"acc_tok_001",)"
        R"("user":{"id":42,"first_name":"Peter","last_name":"Sagan"}})",
        ""
    };

    std::string err;
    EXPECT_EQ(uploader.exchangeAuthCode("auth_code_wahoo", 8000u, err),
              UploadStageResult::SUCCESS);
    EXPECT_TRUE(err.empty());

    // Verify exact write ordering:
    // index 0 -> write("wahoo.refresh_token")
    // index 1 -> write("wahoo.access_token")
    // index 2 -> metadata expires_at
    ASSERT_EQ(secretStore.write_log.size(), 2u);
    EXPECT_EQ(secretStore.write_log[0].key, WahooUploader::KEY_REFRESH_TOKEN);
    EXPECT_EQ(secretStore.write_log[0].value, "rot_refresh_001");
    EXPECT_EQ(secretStore.write_log[1].key, WahooUploader::KEY_ACCESS_TOKEN);
    EXPECT_EQ(secretStore.write_log[1].value, "acc_tok_001");

    ASSERT_EQ(secretStore.events.size(), 3u);
    EXPECT_EQ(secretStore.events[0], "write:" + std::string(WahooUploader::KEY_REFRESH_TOKEN));
    EXPECT_EQ(secretStore.events[1], "write:" + std::string(WahooUploader::KEY_ACCESS_TOKEN));
    EXPECT_EQ(secretStore.events[2], "meta:expires_at=1760021600:Peter Sagan");

    EXPECT_EQ(uploader.getRefreshToken(), "rot_refresh_001");
    EXPECT_EQ(uploader.getAccessToken(), "acc_tok_001");
    EXPECT_EQ(uploader.getTokenExpiresAt(), 1760021600u);
    EXPECT_EQ(uploader.getAthleteName(), "Peter Sagan");

    // Test 2: Token refresh (grant_type=refresh_token) persistence ordering
    secretStore.clearLog();
    httpClient.next_form_response = {
        200,
        R"({"token_type":"Bearer","expires_at":1760043200,"expires_in":21600,)"
        R"("refresh_token":"rot_refresh_002","access_token":"acc_tok_002"})",
        ""
    };

    EXPECT_EQ(uploader.refreshToken(8000u, err), UploadStageResult::SUCCESS);
    EXPECT_NE(httpClient.last_form_body.find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(httpClient.last_form_body.find("refresh_token=rot_refresh_001"), std::string::npos);

    ASSERT_EQ(secretStore.write_log.size(), 2u);
    EXPECT_EQ(secretStore.write_log[0].key, WahooUploader::KEY_REFRESH_TOKEN);
    EXPECT_EQ(secretStore.write_log[0].value, "rot_refresh_002");
    EXPECT_EQ(secretStore.write_log[1].key, WahooUploader::KEY_ACCESS_TOKEN);
    EXPECT_EQ(secretStore.write_log[1].value, "acc_tok_002");

    ASSERT_EQ(secretStore.events.size(), 3u);
    EXPECT_EQ(secretStore.events[0], "write:" + std::string(WahooUploader::KEY_REFRESH_TOKEN));
    EXPECT_EQ(secretStore.events[1], "write:" + std::string(WahooUploader::KEY_ACCESS_TOKEN));
    EXPECT_EQ(secretStore.events[2], "meta:expires_at=1760043200:Peter Sagan");
}

// =============================================================================
// 4. RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens (SPEC.md §10.4)
// =============================================================================

TEST(WahooUploaderTest, RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens) {
    OrderedSpySecretStore secretStore;
    MockWahooHttpClient  httpClient;
    WahooUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("client_wahoo_1", "client_sec_wahoo"));

    // Seed initial valid tokens
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1760000000,"refresh_token":"initial_refresh_tok","access_token":"initial_access_tok"})",
        ""
    };
    std::string err;
    ASSERT_EQ(uploader.exchangeAuthCode("code_init", 5000u, err), UploadStageResult::SUCCESS);

    secretStore.clearLog();
    secretStore.fail_refresh_token_write = true;

    bool metadataCalled = false;
    uploader.setMetadataSaveCallback([&metadataCalled](uint32_t, const std::string&) {
        metadataCalled = true;
        return true;
    });

    // Simulate rotated token response when NVS write for refresh_token fails
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1760099999,"refresh_token":"new_rotated_refresh","access_token":"new_access_tok"})",
        ""
    };

    const UploadStageResult res = uploader.refreshToken(5000u, err);
    EXPECT_EQ(res, UploadStageResult::RETRYABLE_ERROR);
    EXPECT_FALSE(err.empty());

    // Verify access_token was NEVER written and metadata was NEVER updated
    EXPECT_TRUE(secretStore.write_log.empty());
    ASSERT_EQ(secretStore.events.size(), 1u);
    EXPECT_EQ(secretStore.events[0], "fail:" + std::string(WahooUploader::KEY_REFRESH_TOKEN));
    EXPECT_FALSE(metadataCalled);

    // Previous tokens and expiry remain intact
    EXPECT_EQ(uploader.getRefreshToken(), "initial_refresh_tok");
    EXPECT_EQ(uploader.getAccessToken(), "initial_access_tok");
    EXPECT_EQ(uploader.getTokenExpiresAt(), 1760000000u);
    EXPECT_EQ(secretStore.getSecret(WahooUploader::KEY_REFRESH_TOKEN), "initial_refresh_tok");
    EXPECT_EQ(secretStore.getSecret(WahooUploader::KEY_ACCESS_TOKEN), "initial_access_tok");
}

// =============================================================================
// 5. RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor (SPEC.md §10.4)
// =============================================================================

TEST(WahooUploaderTest, RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor) {
    OrderedSpySecretStore secretStore;
    MockWahooHttpClient  httpClient;

    std::vector<std::string> registered_secrets;
    bool registrar_return_value = true;

    WahooUploader uploader(
        &secretStore,
        &httpClient,
        [&](const std::string& secret) -> bool {
            registered_secrets.push_back(secret);
            return registrar_return_value;
        });

    // 1. Saving BYOA client credentials registers client_secret
    ASSERT_TRUE(uploader.setClientCredentials("555", "my_wahoo_secret_1234"));
    ASSERT_EQ(registered_secrets.size(), 1u);
    EXPECT_EQ(registered_secrets[0], "my_wahoo_secret_1234");
    EXPECT_EQ(uploader.getRedactorWarningCount(), 0u);

    // 2. OAuth code exchange registers refresh_token and access_token
    registered_secrets.clear();
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1760010000,"refresh_token":"exch_refresh_secret","access_token":"exch_access_secret"})",
        ""
    };
    std::string err;
    ASSERT_EQ(uploader.exchangeAuthCode("code_1", 5000u, err), UploadStageResult::SUCCESS);
    ASSERT_EQ(registered_secrets.size(), 2u);
    EXPECT_EQ(registered_secrets[0], "exch_refresh_secret");
    EXPECT_EQ(registered_secrets[1], "exch_access_secret");

    // 3. Token refresh registers new rotated refresh_token and access_token
    registered_secrets.clear();
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1760020000,"refresh_token":"refr_refresh_secret","access_token":"refr_access_secret"})",
        ""
    };
    ASSERT_EQ(uploader.refreshToken(5000u, err), UploadStageResult::SUCCESS);
    ASSERT_EQ(registered_secrets.size(), 2u);
    EXPECT_EQ(registered_secrets[0], "refr_refresh_secret");
    EXPECT_EQ(registered_secrets[1], "refr_access_secret");

    // 4. Boot loadCredentials() registers all 3 stored secrets (client_secret, refresh_token, access_token)
    registered_secrets.clear();
    WahooUploader bootUploader(
        &secretStore,
        &httpClient,
        [&](const std::string& secret) -> bool {
            registered_secrets.push_back(secret);
            return registrar_return_value;
        });
    bootUploader.setClientId("555");
    ASSERT_TRUE(bootUploader.begin());
    ASSERT_EQ(registered_secrets.size(), 3u);
    EXPECT_EQ(registered_secrets[0], "my_wahoo_secret_1234");
    EXPECT_EQ(registered_secrets[1], "refr_refresh_secret");
    EXPECT_EQ(registered_secrets[2], "refr_access_secret");

    // 5. Simulate Redactor slot table full (registerSecret returns false)
    registrar_return_value = false;
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1760030000,"refresh_token":"unprotected_refresh_99","access_token":"unprotected_access_99"})",
        ""
    };
    EXPECT_EQ(bootUploader.refreshToken(5000u, err), UploadStageResult::SUCCESS);
    EXPECT_EQ(bootUploader.getRedactorWarningCount(), 2u);
    EXPECT_TRUE(bootUploader.isRedactorSaturated());
    // Warning message MUST NOT contain the secret values
    EXPECT_EQ(bootUploader.getLastRedactorWarning().find("unprotected_refresh_99"), std::string::npos);
    EXPECT_EQ(bootUploader.getLastRedactorWarning().find("unprotected_access_99"), std::string::npos);

    // 6. Verify default Redactor::instance() integration directly
    Redactor::instance().clearSecrets();
    WahooUploader defaultRedactorUploader(&secretStore, &httpClient);
    ASSERT_TRUE(defaultRedactorUploader.setClientCredentials("777", "real_wahoo_secret_val"));
    char logBuf[128];
    std::snprintf(logBuf, sizeof(logBuf), "Wahoo token leak test: real_wahoo_secret_val end");
    Redactor::instance().redact(logBuf, sizeof(logBuf));
    EXPECT_EQ(std::string(logBuf).find("real_wahoo_secret_val"), std::string::npos);
    EXPECT_NE(std::string(logBuf).find("*********************"), std::string::npos);
    Redactor::instance().clearSecrets();
}

// =============================================================================
// 6. PostUploadMultipartFramingAndStatusResponses (SPEC.md §10.2–§10.5)
// =============================================================================

TEST(WahooUploaderTest, PostUploadMultipartFramingAndStatusResponses) {
    OrderedSpySecretStore secretStore;
    MockWahooHttpClient  httpClient;
    InMemoryRideFileSystem fs;
    WahooUploader        uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setClientCredentials("12345", "client_secret_val"));
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1800000000,"refresh_token":"ref_tok_1","access_token":"bearer_tok_wahoo"})",
        ""
    };
    std::string err;
    ASSERT_EQ(uploader.exchangeAuthCode("code_ok", 5000u, err), UploadStageResult::SUCCESS);

    // Write a synthetic compressed .tcx.gz payload into the in-memory filesystem
    const uint8_t gzPayload[] = {0x1F, 0x8B, 0x08, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
    const std::string tcxPath = "/rides/ride_88.tcx.gz";
    ASSERT_TRUE(fs.writeFile(tcxPath.c_str(), gzPayload, sizeof(gzPayload)));

    // Case A: 201 Created -> SUCCESS with upload_id extracted
    httpClient.next_multipart_response = {
        201,
        R"({"id":99887766,"id_str":"99887766","workout":{"id":99887766}})",
        ""
    };

    std::string uploadId;
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(uploadId, "99887766");
    EXPECT_TRUE(err.empty());

    // Verify multipart framing fields (SPEC.md §10.2)
    EXPECT_EQ(httpClient.last_multipart_req.url, WahooUploader::WORKOUTS_URL);
    EXPECT_EQ(httpClient.last_multipart_req.bearer_token, "bearer_tok_wahoo");
    EXPECT_EQ(httpClient.last_multipart_req.data_type, "tcx.gz");
    EXPECT_EQ(httpClient.last_multipart_req.external_id, "echenon_88.tcx.gz");
    EXPECT_EQ(httpClient.last_multipart_req.file_name, "echenon_88.tcx.gz");
    EXPECT_EQ(httpClient.last_multipart_req.file_data.size(), sizeof(gzPayload));

    const std::string fullBody = WahooUploader::assembleMultipartBody(httpClient.last_multipart_req);
    EXPECT_NE(fullBody.find("name=\"workout[workout_type_id]\"\r\n\r\n1"), std::string::npos);
    EXPECT_NE(fullBody.find("name=\"workout[file]\"; filename=\"echenon_88.tcx.gz\""), std::string::npos);
    EXPECT_NE(fullBody.find("Content-Type: application/gzip"), std::string::npos);

    // Case B: 200 OK -> SUCCESS
    httpClient.next_multipart_response = {200, R"({"id":554433})", ""};
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(uploadId, "554433");

    // Case C: 401 Unauthorized -> PERMANENT_ERROR
    httpClient.next_multipart_response = {401, R"({"message":"Unauthorized"})", ""};
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("401"), std::string::npos);

    // Case D: 429 Too Many Requests -> RETRYABLE_ERROR
    httpClient.next_multipart_response = {429, R"({"message":"Rate limit exceeded"})", ""};
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);
    EXPECT_NE(err.find("429"), std::string::npos);

    // Case E: 500 Server Error -> RETRYABLE_ERROR
    httpClient.next_multipart_response = {500, R"({"message":"Internal server error"})", ""};
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);
    EXPECT_NE(err.find("500"), std::string::npos);

    // Case F: 422 Unprocessable Entity -> PERMANENT_ERROR
    httpClient.next_multipart_response = {422, R"({"message":"Unprocessable entity"})", ""};
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("422"), std::string::npos);

    // Case G: Explicit error field in 201 JSON -> PERMANENT_ERROR
    httpClient.next_multipart_response = {
        201,
        R"({"id":99887767,"error":"Duplicate workout detected"})",
        ""
    };
    EXPECT_EQ(uploader.postUpload(88u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("Duplicate"), std::string::npos);
}

// =============================================================================
// 7. PollUploadStatusReturnsSuccessImmediately (SPEC.md §10.2)
// =============================================================================

TEST(WahooUploaderTest, PollUploadStatusReturnsSuccessImmediately) {
    WahooUploader uploader;
    std::string activityId;
    std::string err;

    // Synchronous immediate success mapping remote workout ID
    EXPECT_EQ(uploader.pollUploadStatus("12345678", 5000u, activityId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(activityId, "12345678");
    EXPECT_TRUE(err.empty());

    // Empty upload_id returns PERMANENT_ERROR
    EXPECT_EQ(uploader.pollUploadStatus("", 5000u, activityId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_FALSE(err.empty());
}

// =============================================================================
// 8. ThreeSinkQueueProgression (UploadQueue Fan-Out & Multi-Sink Isolation)
// =============================================================================

TEST(WahooUploaderTest, ThreeSinkQueueProgression) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    populateTestRide(store, 2101u, 5);

    // Sink 1: Strava (Mock progression sink, succeeds all stages)
    MockProgressionSink stravaSink;
    stravaSink.mock_service_id       = "strava";
    stravaSink.mock_name             = "Strava";
    stravaSink.returned_upload_id    = "strava_up_1";
    stravaSink.returned_activity_id  = "strava_act_1";
    stravaSink.token_needs_refresh   = false;

    // Sink 2: Intervals.icu (Mock progression sink, simulated isolated failure)
    MockProgressionSink intervalsSink;
    intervalsSink.mock_service_id       = "intervals";
    intervalsSink.mock_name             = "Intervals.icu";
    intervalsSink.token_needs_refresh   = false;
    intervalsSink.post_result           = UploadStageResult::PERMANENT_ERROR;
    intervalsSink.post_err              = "HTTP 401 Unauthorized";

    // Sink 3: Wahoo Cloud (Real WahooUploader instance with mock HTTP client)
    InMemorySecretStore wahooSecretStore;
    MockWahooHttpClient wahooHttpClient;
    WahooUploader wahooSink(&wahooSecretStore, &wahooHttpClient);
    wahooSink.setFileSystem(&fs);
    ASSERT_TRUE(wahooSink.setClientCredentials("wahoo_client_1", "wahoo_sec_1"));
    wahooSecretStore.putSecret(WahooUploader::KEY_ACCESS_TOKEN, "wahoo_tok_acc");
    wahooSecretStore.putSecret(WahooUploader::KEY_REFRESH_TOKEN, "wahoo_tok_ref");
    wahooSink.setTokenExpiresAt(2000000000u);
    ASSERT_TRUE(wahooSink.loadCredentials());
    wahooHttpClient.next_multipart_response = {201, R"({"id": 889900})", ""};

    // Queue wiring
    UploadQueue queue(nullptr, &store);
    queue.setBaseDir("./tmp/test_upload_queue");
    ASSERT_TRUE(queue.registerSink(&stravaSink));
    ASSERT_TRUE(queue.registerSink(&intervalsSink));
    ASSERT_TRUE(queue.registerSink(&wahooSink));
    EXPECT_EQ(queue.getSinkCount(), 3u);

    // 1. Fan-Out produces 3 jobs
    ASSERT_TRUE(queue.enqueueSession(2101u, 0u));
    EXPECT_EQ(queue.size(), 3u);

    const UploadJob* jobStrava    = queue.findJob(2101u, "strava");
    const UploadJob* jobIntervals = queue.findJob(2101u, "intervals");
    const UploadJob* jobWahoo     = queue.findJob(2101u, "wahoo");

    ASSERT_NE(jobStrava, nullptr);
    ASSERT_NE(jobIntervals, nullptr);
    ASSERT_NE(jobWahoo, nullptr);
    EXPECT_EQ(jobStrava->stage, UploadStage::STAGE_GENERATE_TCX_GZ);
    EXPECT_EQ(jobIntervals->stage, UploadStage::STAGE_GENERATE_TCX_GZ);
    EXPECT_EQ(jobWahoo->stage, UploadStage::STAGE_GENERATE_TCX_GZ);

    // 2. Sequential execution across steps
    // Step 1: Strava generates .tcx.gz -> advances to POST_UPLOAD
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Success);
    EXPECT_EQ(jobStrava->stage, UploadStage::STAGE_POST_UPLOAD);
    EXPECT_EQ(jobIntervals->stage, UploadStage::STAGE_GENERATE_TCX_GZ);
    EXPECT_EQ(jobWahoo->stage, UploadStage::STAGE_GENERATE_TCX_GZ);
    EXPECT_TRUE(fs.exists(queue.getTcxGzPath(2101u).c_str()));

    // Step 2: Strava executes POST_UPLOAD -> advances to STAGE_POLL_STATUS (next attempt at 3000 ms)
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Success);
    EXPECT_EQ(jobStrava->stage, UploadStage::STAGE_POLL_STATUS);

    // Step 3: Intervals generates .tcx.gz (file already exists, skips generation) -> advances to POST_UPLOAD
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Success);
    EXPECT_EQ(jobIntervals->stage, UploadStage::STAGE_POST_UPLOAD);

    // Step 4: Intervals executes POST_UPLOAD -> hits simulated 401 permanent error -> STAGE_FAILED
    queue.stepQueue(15000u, 0u);
    EXPECT_EQ(jobIntervals->stage, UploadStage::STAGE_FAILED);

    // 3. Isolated sink failure verification:
    // Intervals failed, but Strava and Wahoo are still active -> .tcx.gz must NOT be deleted yet!
    EXPECT_FALSE(queue.allSinkJobsTerminal(2101u));
    EXPECT_TRUE(fs.exists(queue.getTcxGzPath(2101u).c_str()));

    // Step 5: Wahoo generates .tcx.gz (file already exists) -> advances to POST_UPLOAD
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Success);
    EXPECT_EQ(jobWahoo->stage, UploadStage::STAGE_POST_UPLOAD);

    // Step 6: Wahoo executes POST_UPLOAD -> real Wahoo uploader sends multipart -> STAGE_POLL_STATUS
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Success);
    EXPECT_EQ(jobWahoo->stage, UploadStage::STAGE_POLL_STATUS);
    EXPECT_EQ(jobWahoo->remote_upload_id, "889900");
    EXPECT_FALSE(queue.allSinkJobsTerminal(2101u));
    EXPECT_TRUE(fs.exists(queue.getTcxGzPath(2101u).c_str()));

    // Step 7: At t = 0 ms, Strava and Wahoo are waiting for poll delay (3000 ms) -> Skipped
    EXPECT_EQ(queue.stepQueue(15000u, 0u), UpdateStatus::Skipped);

    // Advance clock past 3000 ms poll delay to t = 3100 ms
    // Step 8: Strava executes POLL_STATUS -> STAGE_COMPLETE
    EXPECT_EQ(queue.stepQueue(15000u, 3100u), UpdateStatus::Success);
    EXPECT_EQ(jobStrava->stage, UploadStage::STAGE_COMPLETE);
    EXPECT_EQ(jobStrava->remote_activity_id, "strava_act_1");
    // Wahoo is still at STAGE_POLL_STATUS -> .tcx.gz still exists
    EXPECT_FALSE(queue.allSinkJobsTerminal(2101u));
    EXPECT_TRUE(fs.exists(queue.getTcxGzPath(2101u).c_str()));

    // Step 9: Wahoo executes POLL_STATUS -> synchronous completion -> STAGE_COMPLETE
    EXPECT_EQ(queue.stepQueue(15000u, 3100u), UpdateStatus::Success);
    EXPECT_EQ(jobWahoo->stage, UploadStage::STAGE_COMPLETE);
    EXPECT_EQ(jobWahoo->remote_activity_id, "889900");

    // 4. Single .tcx.gz deletion only after all three sinks reach terminal state
    EXPECT_TRUE(queue.allSinkJobsTerminal(2101u));
    EXPECT_FALSE(fs.exists(queue.getTcxGzPath(2101u).c_str()));
}
