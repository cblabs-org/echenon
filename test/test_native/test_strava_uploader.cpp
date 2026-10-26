/// @file test_strava_uploader.cpp
/// @brief Native unit tests for StravaUploader OAuth flows, token refresh, and upload handling.

#include <gtest/gtest.h>

#include "lib/Redactor/Redactor.cpp"
#include "echenon/IUploader.h"
#include "echenon/StravaUploader.h"
#include "echenon/StravaUploader.cpp"

#include <string>
#include <utility>
#include <vector>

using namespace echenon;

namespace {

/**
 * @brief Spy secret store that records the exact chronological order of all
 *        putSecret() / deleteSecret() operations and can simulate write failures.
 */
class OrderedSpySecretStore : public ISecretStore {
public:
    struct WriteEntry {
        std::string key;
        std::string value;
    };

    std::vector<WriteEntry>  write_log;
    std::vector<std::string> events; // e.g. "write:refresh_token", "write:access_token", "meta:expires_at"
    std::map<std::string, std::string> store;

    bool fail_refresh_token_write = false;
    bool fail_access_token_write  = false;
    bool fail_client_secret_write = false;

    bool putSecret(const std::string& key, const std::string& value) override {
        if (key == StravaUploader::KEY_REFRESH_TOKEN && fail_refresh_token_write) {
            events.push_back("fail:" + key);
            return false;
        }
        if (key == StravaUploader::KEY_ACCESS_TOKEN && fail_access_token_write) {
            events.push_back("fail:" + key);
            return false;
        }
        if (key == StravaUploader::KEY_CLIENT_SECRET && fail_client_secret_write) {
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
 * @brief Deterministic mock HTTP client for Strava OAuth & v3 API endpoints.
 */
class MockStravaHttpClient : public IHttpClient {
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

} // namespace

// =============================================================================
// 1. OAuth Authorize URL & Single-Use CSRF State Nonce (SPEC.md §10.4)
// =============================================================================

TEST(StravaUploaderTest, BuildAuthorizeUrlAndValidateSingleUseStateNonce) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;
    StravaUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("12345", "super_secret_client_key"));
    EXPECT_TRUE(uploader.isConfigured());
    EXPECT_STREQ(uploader.serviceId(), "strava");
    EXPECT_STREQ(uploader.name(), "Strava");

    // Deterministic RNG for predictable 32-hex state check
    uint32_t counter = 0x11223344u;
    uploader.setRandomGenerator([&counter]() -> uint32_t {
        const uint32_t val = counter;
        counter += 0x11111111u;
        return val;
    });

    const std::string authUrl = uploader.buildAuthorizeUrl(
        "http://echenon.local/api/strava/callback", 1000u);

    EXPECT_TRUE(uploader.isStateActive());
    const std::string nonce = uploader.getPendingState();
    EXPECT_EQ(nonce.size(), 32u);
    EXPECT_EQ(nonce, "11223344223344553344556644556677");

    // Verify URL components
    EXPECT_NE(authUrl.find("https://www.strava.com/oauth/authorize?"), std::string::npos);
    EXPECT_NE(authUrl.find("client_id=12345"), std::string::npos);
    EXPECT_NE(authUrl.find("response_type=code"), std::string::npos);
    EXPECT_NE(authUrl.find("redirect_uri=http%3A%2F%2Fechenon.local%2Fapi%2Fstrava%2Fcallback"),
              std::string::npos);
    EXPECT_NE(authUrl.find("approval_prompt=auto"), std::string::npos);
    EXPECT_NE(authUrl.find("scope=activity:write"), std::string::npos);
    EXPECT_NE(authUrl.find("&state=" + nonce), std::string::npos);

    // Mismatched state is rejected without consuming the active state
    EXPECT_FALSE(uploader.validateAndConsumeState("wrong_state_value", 2000u));
    EXPECT_FALSE(uploader.validateAndConsumeState("", 2000u));
    EXPECT_TRUE(uploader.isStateActive());

    // Valid match within 10-minute (600,000 ms) TTL succeeds and consumes nonce
    EXPECT_TRUE(uploader.validateAndConsumeState(nonce, 1000u + 599999u));
    EXPECT_FALSE(uploader.isStateActive());
    EXPECT_TRUE(uploader.getPendingState().empty());

    // Single-use replay with the same nonce MUST be rejected (HTTP 403 path)
    EXPECT_FALSE(uploader.validateAndConsumeState(nonce, 1000u + 600000u));
}

TEST(StravaUploaderTest, StateNonceTtlExpiryAndMillisWraparound) {
    OrderedSpySecretStore secretStore;
    StravaUploader        uploader(&secretStore);
    ASSERT_TRUE(uploader.setClientCredentials("12345", "super_secret_client_key"));

    // 1. Issue state at t = 10,000 ms; expire at t = 610,001 ms (> 600,000 ms TTL)
    uploader.buildAuthorizeUrl("http://echenon.local/api/strava/callback", 10000u);
    const std::string expiredNonce = uploader.getPendingState();
    ASSERT_EQ(expiredNonce.size(), 32u);

    EXPECT_FALSE(uploader.validateAndConsumeState(expiredNonce, 10000u + 600001u));
    EXPECT_FALSE(uploader.isStateActive());

    // 2. Wraparound-safe 32-bit millis() arithmetic across 0xFFFFFFFF -> 0x00000200
    const uint32_t issuedNearWrap = 0xFFFFFF00u;
    uploader.buildAuthorizeUrl("http://echenon.local/api/strava/callback", issuedNearWrap);
    const std::string wrapNonce = uploader.getPendingState();

    // 512 ms after issuedNearWrap (wraps past 0)
    const uint32_t afterWrapMs = issuedNearWrap + 512u;
    EXPECT_TRUE(uploader.validateAndConsumeState(wrapNonce, afterWrapMs));
}

// =============================================================================
// 2. Rotated refresh_token-First Persistence Order & Failure Abort (SPEC.md §10.4)
// =============================================================================

TEST(StravaUploaderTest, RotatedRefreshTokenPersistedFirstBeforeAccessTokenAndExpiresAt) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;
    StravaUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("99887", "client_sec_xyz"));
    secretStore.clearLog();

    // Hook metadata persistence callback into the same ordered spy event log
    uploader.setMetadataSaveCallback([&secretStore](uint32_t expires_at, const std::string& athlete) {
        secretStore.events.push_back("meta:expires_at=" + std::to_string(expires_at) + ":" + athlete);
        return true;
    });

    // Test 1: OAuth authorization code exchange persistence ordering
    httpClient.next_form_response = {
        200,
        R"({"token_type":"Bearer","expires_at":1760021600,"expires_in":21600,)"
        R"("refresh_token":"rot_refresh_001","access_token":"acc_tok_001",)"
        R"("athlete":{"id":42,"firstname":"Eddy","lastname":"Merckx"}})",
        ""
    };

    std::string err;
    EXPECT_EQ(uploader.exchangeAuthCode("auth_code_abc", 8000u, err),
              UploadStageResult::SUCCESS);
    EXPECT_TRUE(err.empty());

    // Verify exact write ordering:
    // index 0 -> write("refresh_token")
    // index 1 -> write("access_token")
    // index 2 -> metadata expires_at
    ASSERT_EQ(secretStore.write_log.size(), 2u);
    EXPECT_EQ(secretStore.write_log[0].key, "refresh_token");
    EXPECT_EQ(secretStore.write_log[0].value, "rot_refresh_001");
    EXPECT_EQ(secretStore.write_log[1].key, "access_token");
    EXPECT_EQ(secretStore.write_log[1].value, "acc_tok_001");

    ASSERT_EQ(secretStore.events.size(), 3u);
    EXPECT_EQ(secretStore.events[0], "write:refresh_token");
    EXPECT_EQ(secretStore.events[1], "write:access_token");
    EXPECT_EQ(secretStore.events[2], "meta:expires_at=1760021600:Eddy Merckx");

    EXPECT_EQ(uploader.getRefreshToken(), "rot_refresh_001");
    EXPECT_EQ(uploader.getAccessToken(), "acc_tok_001");
    EXPECT_EQ(uploader.getTokenExpiresAt(), 1760021600u);
    EXPECT_EQ(uploader.getAthleteName(), "Eddy Merckx");

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
    EXPECT_EQ(secretStore.write_log[0].key, "refresh_token");
    EXPECT_EQ(secretStore.write_log[0].value, "rot_refresh_002");
    EXPECT_EQ(secretStore.write_log[1].key, "access_token");
    EXPECT_EQ(secretStore.write_log[1].value, "acc_tok_002");

    ASSERT_EQ(secretStore.events.size(), 3u);
    EXPECT_EQ(secretStore.events[0], "write:refresh_token");
    EXPECT_EQ(secretStore.events[1], "write:access_token");
    EXPECT_EQ(secretStore.events[2], "meta:expires_at=1760043200:Eddy Merckx");
}

TEST(StravaUploaderTest, RefreshTokenWriteFailureAbortsImmediatelyWithoutOverwritingTokens) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;
    StravaUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("99887", "client_sec_xyz"));

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

    // Simulate rotated token response when NVS / KeyManager write for refresh_token fails
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
    EXPECT_EQ(secretStore.events[0], "fail:refresh_token");
    EXPECT_FALSE(metadataCalled);

    // Previous tokens and expiry remain intact
    EXPECT_EQ(uploader.getRefreshToken(), "initial_refresh_tok");
    EXPECT_EQ(uploader.getAccessToken(), "initial_access_tok");
    EXPECT_EQ(uploader.getTokenExpiresAt(), 1760000000u);
    EXPECT_EQ(secretStore.getSecret("refresh_token"), "initial_refresh_tok");
    EXPECT_EQ(secretStore.getSecret("access_token"), "initial_access_tok");
}

// =============================================================================
// 3. Redactor Secret Registration & Saturation Warning Verification (SPEC.md §10.4)
// =============================================================================

TEST(StravaUploaderTest, RegistersAllSecretsOnLoadSaveExchangeAndRefreshAndHandlesFullRedactor) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;

    std::vector<std::string> registered_secrets;
    bool registrar_return_value = true;

    StravaUploader uploader(
        &secretStore,
        &httpClient,
        [&](const std::string& secret) -> bool {
            registered_secrets.push_back(secret);
            return registrar_return_value;
        });

    // 1. Saving BYOA client credentials registers client_secret
    ASSERT_TRUE(uploader.setClientCredentials("555", "my_client_secret_1234"));
    ASSERT_EQ(registered_secrets.size(), 1u);
    EXPECT_EQ(registered_secrets[0], "my_client_secret_1234");
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
    StravaUploader bootUploader(
        &secretStore,
        &httpClient,
        [&](const std::string& secret) -> bool {
            registered_secrets.push_back(secret);
            return registrar_return_value;
        });
    bootUploader.setClientId("555");
    ASSERT_TRUE(bootUploader.begin());
    ASSERT_EQ(registered_secrets.size(), 3u);
    EXPECT_EQ(registered_secrets[0], "my_client_secret_1234");
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
    // Warning message MUST NOT contain the secret values!
    EXPECT_EQ(bootUploader.getLastRedactorWarning().find("unprotected_refresh_99"), std::string::npos);
    EXPECT_EQ(bootUploader.getLastRedactorWarning().find("unprotected_access_99"), std::string::npos);

    // 6. Verify default Redactor::instance() integration directly
    Redactor::instance().clearSecrets();
    StravaUploader defaultRedactorUploader(&secretStore, &httpClient);
    ASSERT_TRUE(defaultRedactorUploader.setClientCredentials("777", "real_redactor_secret_val"));
    char logBuf[128];
    std::snprintf(logBuf, sizeof(logBuf), "Token leak test: real_redactor_secret_val end");
    Redactor::instance().redact(logBuf, sizeof(logBuf));
    EXPECT_EQ(std::string(logBuf).find("real_redactor_secret_val"), std::string::npos);
    EXPECT_NE(std::string(logBuf).find("************************"), std::string::npos);
    Redactor::instance().clearSecrets();
}

// =============================================================================
// 4. Multipart Upload & Poll Status Parsing (201, 200 IN_PROGRESS, 200 SUCCESS, 429, Duplicate)
// =============================================================================

TEST(StravaUploaderTest, PostUploadMultipartFramingAndStatusResponses) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;
    InMemoryRideFileSystem fs;
    StravaUploader        uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setClientCredentials("12345", "client_secret_val"));
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1800000000,"refresh_token":"ref_tok_1","access_token":"bearer_tok_123"})",
        ""
    };
    std::string err;
    ASSERT_EQ(uploader.exchangeAuthCode("code_ok", 5000u, err), UploadStageResult::SUCCESS);

    // Write a synthetic compressed .tcx.gz payload into the in-memory filesystem
    const uint8_t gzPayload[] = {0x1F, 0x8B, 0x08, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
    const std::string tcxPath = "/rides/ride_77.tcx.gz";
    ASSERT_TRUE(fs.writeFile(tcxPath.c_str(), gzPayload, sizeof(gzPayload)));

    // Case A: 201 Created -> SUCCESS with upload_id extracted
    httpClient.next_multipart_response = {
        201,
        R"({"id":9876543210,"id_str":"9876543210","external_id":"echenon_77.tcx.gz","error":null,"status":"Your activity is still being processed.","activity_id":null})",
        ""
    };

    std::string uploadId;
    EXPECT_EQ(uploader.postUpload(77u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(uploadId, "9876543210");
    EXPECT_TRUE(err.empty());

    // Verify multipart framing fields (SPEC.md §10.4)
    EXPECT_EQ(httpClient.last_multipart_req.url, "https://www.strava.com/api/v3/uploads");
    EXPECT_EQ(httpClient.last_multipart_req.bearer_token, "bearer_tok_123");
    EXPECT_EQ(httpClient.last_multipart_req.data_type, "tcx.gz");
    EXPECT_EQ(httpClient.last_multipart_req.external_id, "echenon_77.tcx.gz");
    EXPECT_EQ(httpClient.last_multipart_req.trainer, "1");
    EXPECT_EQ(httpClient.last_multipart_req.file_data.size(), sizeof(gzPayload));
    const std::string fullBody = StravaUploader::assembleMultipartBody(httpClient.last_multipart_req);
    EXPECT_NE(fullBody.find("name=\"data_type\"\r\n\r\ntcx.gz"), std::string::npos);
    EXPECT_NE(fullBody.find("name=\"external_id\"\r\n\r\nechenon_77.tcx.gz"), std::string::npos);
    EXPECT_NE(fullBody.find("name=\"trainer\"\r\n\r\n1"), std::string::npos);
    EXPECT_NE(fullBody.find("name=\"file\"; filename=\"echenon_77.tcx.gz\""), std::string::npos);

    // Case B: 429 Too Many Requests on postUpload -> RETRYABLE_ERROR
    httpClient.next_multipart_response = {429, R"({"message":"Rate Limit Exceeded"})", ""};
    EXPECT_EQ(uploader.postUpload(77u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);
    EXPECT_NE(err.find("429"), std::string::npos);

    // Case C: 500 Server Error on postUpload -> RETRYABLE_ERROR
    httpClient.next_multipart_response = {500, R"({"message":"Internal Error"})", ""};
    EXPECT_EQ(uploader.postUpload(77u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);

    // Case D: 401 Unauthorized on postUpload -> PERMANENT_ERROR
    httpClient.next_multipart_response = {401, R"({"message":"Authorization Error"})", ""};
    EXPECT_EQ(uploader.postUpload(77u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);

    // Case E: Duplicate upload error in 200/201 response -> PERMANENT_ERROR
    httpClient.next_multipart_response = {
        201,
        R"({"id":9876543211,"id_str":"9876543211","error":"echenon_77.tcx.gz duplicate of activity 123456","status":"There was an error processing your activity."})",
        ""
    };
    EXPECT_EQ(uploader.postUpload(77u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("duplicate"), std::string::npos);
}

TEST(StravaUploaderTest, PollUploadStatusHandlesInProgressSuccessRateLimitAndDuplicateError) {
    OrderedSpySecretStore secretStore;
    MockStravaHttpClient  httpClient;
    StravaUploader        uploader(&secretStore, &httpClient);

    ASSERT_TRUE(uploader.setClientCredentials("12345", "client_secret_val"));
    httpClient.next_form_response = {
        200,
        R"({"expires_at":1800000000,"refresh_token":"ref_tok_1","access_token":"bearer_tok_123"})",
        ""
    };
    std::string err;
    ASSERT_EQ(uploader.exchangeAuthCode("code_ok", 5000u, err), UploadStageResult::SUCCESS);

    std::string activityId;

    // 1. 200 OK still processing (activity_id is null, error is null) -> IN_PROGRESS
    httpClient.next_get_response = {
        200,
        R"({"id":9876543210,"id_str":"9876543210","external_id":"echenon_77.tcx.gz","error":null,"status":"Your activity is still being processed.","activity_id":null})",
        ""
    };
    EXPECT_EQ(uploader.pollUploadStatus("9876543210", 8000u, activityId, err),
              UploadStageResult::IN_PROGRESS);
    EXPECT_TRUE(activityId.empty());
    EXPECT_EQ(httpClient.last_get_url, "https://www.strava.com/api/v3/uploads/9876543210");
    EXPECT_EQ(httpClient.last_get_bearer, "bearer_tok_123");

    // 2. 200 OK with activity_id populated -> SUCCESS
    httpClient.next_get_response = {
        200,
        R"({"id":9876543210,"id_str":"9876543210","external_id":"echenon_77.tcx.gz","error":null,"status":"Your activity is ready.","activity_id":1122334455})",
        ""
    };
    EXPECT_EQ(uploader.pollUploadStatus("9876543210", 8000u, activityId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(activityId, "1122334455");

    // 3. 429 Too Many Requests during poll -> RETRYABLE_ERROR
    httpClient.next_get_response = {429, R"({"message":"Rate Limit Exceeded"})", ""};
    EXPECT_EQ(uploader.pollUploadStatus("9876543210", 8000u, activityId, err),
              UploadStageResult::RETRYABLE_ERROR);

    // 4. 200 OK with duplicate upload error string -> PERMANENT_ERROR
    httpClient.next_get_response = {
        200,
        R"({"id":9876543210,"error":"echenon_77.tcx.gz duplicate of activity 1122334455","status":"There was an error processing your activity.","activity_id":null})",
        ""
    };
    EXPECT_EQ(uploader.pollUploadStatus("9876543210", 8000u, activityId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("duplicate"), std::string::npos);

    // 5. Disconnect clears OAuth tokens from secret store while retaining client credentials
    EXPECT_TRUE(uploader.disconnect());
    EXPECT_TRUE(uploader.isConfigured());
    EXPECT_FALSE(uploader.isAuthenticated());
    EXPECT_TRUE(secretStore.getSecret("access_token").empty());
    EXPECT_TRUE(secretStore.getSecret("refresh_token").empty());
    EXPECT_EQ(secretStore.getSecret("client_secret"), "client_secret_val");
}
