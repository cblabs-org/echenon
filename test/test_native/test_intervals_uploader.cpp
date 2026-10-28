/// @file test_intervals_uploader.cpp
/// @brief Native unit tests for IntervalsIcuUploader API-key auth, upload, and config.
///
/// IntervalsIcuUploader.cpp symbols are provided via test_session_manager.o
/// (SessionOverlay.cpp → #ifdef UNIT_TEST → IntervalsIcuUploader.cpp).
/// Redactor.cpp symbols are provided via test_strava_uploader.o.
/// We only include headers here to avoid duplicate symbol errors.

#include <gtest/gtest.h>

#include "echenon/IUploader.h"
#include "echenon/StravaUploader.h"       // ISecretStore, IHttpClient, HttpResponse, InMemorySecretStore
#include "echenon/IntervalsIcuUploader.h"

#include <string>
#include <vector>

using namespace echenon;

namespace {

/**
 * @brief Deterministic mock HTTP client for Intervals.icu API endpoints.
 *        Captures postForm() calls (used by IntervalsIcuUploader on native builds).
 */
class MockIntervalsHttpClient : public IHttpClient {
public:
    HttpResponse next_form_response{200, "", ""};
    HttpResponse next_multipart_response{200, "", ""};
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
// 1. isConfigured() — true/false based on athlete_id and api_key
// =============================================================================

TEST(IntervalsIcuUploaderTest, IsConfiguredRequiresBothAthleteIdAndApiKey) {
    InMemorySecretStore secretStore;
    MockIntervalsHttpClient httpClient;
    IntervalsIcuUploader uploader(&secretStore, &httpClient);

    // Initially: no credentials → not configured
    EXPECT_FALSE(uploader.isConfigured());
    EXPECT_STREQ(uploader.serviceId(), "intervals");
    EXPECT_STREQ(uploader.name(), "Intervals.icu");

    // Only api_key → not configured (missing athlete_id)
    ASSERT_TRUE(secretStore.putSecret(IntervalsIcuUploader::KEY_API_KEY, "test_api_key"));
    ASSERT_TRUE(uploader.loadCredentials());
    EXPECT_FALSE(uploader.isConfigured());

    // Both api_key and athlete_id → configured
    ASSERT_TRUE(uploader.setApiCredentials("test_api_key", "i12345"));
    EXPECT_TRUE(uploader.isConfigured());
    EXPECT_TRUE(uploader.isAuthenticated());

    // Disabled → not configured even with credentials
    uploader.setEnabled(false);
    EXPECT_FALSE(uploader.isConfigured());
    EXPECT_TRUE(uploader.isAuthenticated()); // authenticated != configured

    uploader.setEnabled(true);
    EXPECT_TRUE(uploader.isConfigured());
}

// =============================================================================
// 2. needsTokenRefresh() — always false for API-key auth
// =============================================================================

TEST(IntervalsIcuUploaderTest, NeedsTokenRefreshAlwaysFalse) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    EXPECT_FALSE(uploader.needsTokenRefresh(0));
    EXPECT_FALSE(uploader.needsTokenRefresh(1800000000u));
    EXPECT_FALSE(uploader.needsTokenRefresh(UINT32_MAX));
}

// =============================================================================
// 3. refreshToken() — always SUCCESS for API-key auth
// =============================================================================

TEST(IntervalsIcuUploaderTest, RefreshTokenAlwaysSuccess) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    std::string err;
    EXPECT_EQ(uploader.refreshToken(5000u, err), UploadStageResult::SUCCESS);
    EXPECT_TRUE(err.empty());
}

// =============================================================================
// 4. postUpload() success — verify Basic Auth header, URL path, content type
// =============================================================================

TEST(IntervalsIcuUploaderTest, PostUploadSuccessVerifiesBasicAuthAndUrl) {
    InMemorySecretStore secretStore;
    MockIntervalsHttpClient httpClient;
    InMemoryRideFileSystem fs;
    IntervalsIcuUploader uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setApiCredentials("my_api_key_123", "i99887"));

    // Write a synthetic .tcx.gz payload
    const uint8_t gzPayload[] = {0x1F, 0x8B, 0x08, 0x00, 0xAA, 0xBB};
    const std::string tcxPath = "/rides/ride_42.tcx.gz";
    ASSERT_TRUE(fs.writeFile(tcxPath.c_str(), gzPayload, sizeof(gzPayload)));

    httpClient.next_form_response = {200, R"({"id":"abc123"})", ""};

    std::string uploadId, err;
    EXPECT_EQ(uploader.postUpload(42u, tcxPath, 10000u, uploadId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(uploadId, "intervals_42");
    EXPECT_TRUE(err.empty());

    // Verify URL contains athlete_id
    EXPECT_EQ(httpClient.last_form_url,
              "https://intervals.icu/api/v1/athlete/i99887/activities");

    // Verify Basic Auth header is in the payload (native test convention: auth + \n + body)
    const std::string expectedAuth = "Basic " +
        IntervalsIcuUploader::base64Encode("my_api_key_123:");
    EXPECT_NE(httpClient.last_form_body.find(expectedAuth), std::string::npos);
}

// =============================================================================
// 5. postUpload() 401 → PERMANENT_ERROR
// =============================================================================

TEST(IntervalsIcuUploaderTest, PostUpload401ReturnsPermanentError) {
    InMemorySecretStore secretStore;
    MockIntervalsHttpClient httpClient;
    InMemoryRideFileSystem fs;
    IntervalsIcuUploader uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setApiCredentials("key", "athlete"));

    const uint8_t gzPayload[] = {0x1F, 0x8B};
    ASSERT_TRUE(fs.writeFile("/rides/ride_10.tcx.gz", gzPayload, sizeof(gzPayload)));

    httpClient.next_form_response = {401, R"({"message":"Unauthorized"})", ""};

    std::string uploadId, err;
    EXPECT_EQ(uploader.postUpload(10u, "/rides/ride_10.tcx.gz", 10000u, uploadId, err),
              UploadStageResult::PERMANENT_ERROR);
    EXPECT_NE(err.find("401"), std::string::npos);
}

// =============================================================================
// 6. postUpload() 429 → RETRYABLE_ERROR
// =============================================================================

TEST(IntervalsIcuUploaderTest, PostUpload429ReturnsRetryableError) {
    InMemorySecretStore secretStore;
    MockIntervalsHttpClient httpClient;
    InMemoryRideFileSystem fs;
    IntervalsIcuUploader uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setApiCredentials("key", "athlete"));

    const uint8_t gzPayload[] = {0x1F, 0x8B};
    ASSERT_TRUE(fs.writeFile("/rides/ride_11.tcx.gz", gzPayload, sizeof(gzPayload)));

    httpClient.next_form_response = {429, R"({"message":"Rate Limit"})", ""};

    std::string uploadId, err;
    EXPECT_EQ(uploader.postUpload(11u, "/rides/ride_11.tcx.gz", 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);
    EXPECT_NE(err.find("429"), std::string::npos);
}

// =============================================================================
// 7. postUpload() 500 → RETRYABLE_ERROR
// =============================================================================

TEST(IntervalsIcuUploaderTest, PostUpload500ReturnsRetryableError) {
    InMemorySecretStore secretStore;
    MockIntervalsHttpClient httpClient;
    InMemoryRideFileSystem fs;
    IntervalsIcuUploader uploader(&secretStore, &httpClient);
    uploader.setFileSystem(&fs);

    ASSERT_TRUE(uploader.setApiCredentials("key", "athlete"));

    const uint8_t gzPayload[] = {0x1F, 0x8B};
    ASSERT_TRUE(fs.writeFile("/rides/ride_12.tcx.gz", gzPayload, sizeof(gzPayload)));

    httpClient.next_form_response = {500, R"({"message":"Internal Error"})", ""};

    std::string uploadId, err;
    EXPECT_EQ(uploader.postUpload(12u, "/rides/ride_12.tcx.gz", 10000u, uploadId, err),
              UploadStageResult::RETRYABLE_ERROR);
}

// =============================================================================
// 8. pollUploadStatus() — always SUCCESS (synchronous processing)
// =============================================================================

TEST(IntervalsIcuUploaderTest, PollUploadStatusAlwaysSuccess) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    std::string activityId, err;
    EXPECT_EQ(uploader.pollUploadStatus("intervals_42", 8000u, activityId, err),
              UploadStageResult::SUCCESS);
    EXPECT_EQ(activityId, "intervals_42");
    EXPECT_TRUE(err.empty());
}

// =============================================================================
// 9. setApiCredentials() — persists via ISecretStore, registers with Redactor
// =============================================================================

TEST(IntervalsIcuUploaderTest, SetApiCredentialsPersistsAndRegistersWithRedactor) {
    InMemorySecretStore secretStore;

    std::vector<std::string> registered_secrets;
    IntervalsIcuUploader uploader(
        &secretStore,
        nullptr,
        [&](const std::string& secret) -> bool {
            registered_secrets.push_back(secret);
            return true;
        });

    ASSERT_TRUE(uploader.setApiCredentials("secret_api_key_999", "i77"));

    // Verify secret store persistence
    EXPECT_EQ(secretStore.getSecret(IntervalsIcuUploader::KEY_API_KEY), "secret_api_key_999");
    EXPECT_EQ(secretStore.getSecret(IntervalsIcuUploader::KEY_ATHLETE_ID), "i77");

    // Verify Redactor registration of the api_key
    ASSERT_EQ(registered_secrets.size(), 1u);
    EXPECT_EQ(registered_secrets[0], "secret_api_key_999");

    EXPECT_EQ(uploader.getApiKey(), "secret_api_key_999");
    EXPECT_EQ(uploader.getAthleteId(), "i77");
}

// =============================================================================
// 10. disconnect() — clears credentials from ISecretStore
// =============================================================================

TEST(IntervalsIcuUploaderTest, DisconnectClearsCredentials) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    ASSERT_TRUE(uploader.setApiCredentials("key_to_clear", "athlete_to_clear"));
    EXPECT_TRUE(uploader.isConfigured());

    EXPECT_TRUE(uploader.disconnect());
    EXPECT_FALSE(uploader.isConfigured());
    EXPECT_FALSE(uploader.isAuthenticated());
    EXPECT_TRUE(uploader.getApiKey().empty());
    EXPECT_TRUE(uploader.getAthleteId().empty());
    EXPECT_TRUE(secretStore.getSecret(IntervalsIcuUploader::KEY_API_KEY).empty());
    EXPECT_TRUE(secretStore.getSecret(IntervalsIcuUploader::KEY_ATHLETE_ID).empty());
}

// =============================================================================
// 11. Config serialization: applyConfig()/serializeConfig() round-trip
// =============================================================================

TEST(IntervalsIcuUploaderTest, ConfigSerializationRoundTrip) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    // Apply config
    JsonDocument doc;
    JsonObject section = doc.to<JsonObject>();
    section["enabled"]      = false;
    section["athlete_id"]   = "i42";
    section["athlete_name"] = "Eddy Merckx";
    uploader.applyConfig(section);

    EXPECT_FALSE(uploader.isEnabled());
    EXPECT_EQ(uploader.getAthleteId(), "i42");
    EXPECT_EQ(uploader.getAthleteName(), "Eddy Merckx");

    // Serialize and verify round-trip
    JsonDocument outDoc;
    JsonObject outSection = outDoc.to<JsonObject>();
    uploader.serializeConfig(outSection);

    EXPECT_FALSE(outSection["enabled"].as<bool>());
    EXPECT_EQ(std::string(outSection["athlete_id"].as<const char*>()), "i42");
    EXPECT_EQ(std::string(outSection["athlete_name"].as<const char*>()), "Eddy Merckx");

    // resetConfig() restores defaults
    uploader.resetConfig();
    EXPECT_TRUE(uploader.isEnabled());
    EXPECT_TRUE(uploader.getAthleteId().empty());
    EXPECT_TRUE(uploader.getAthleteName().empty());
}

// =============================================================================
// 12. base64Encode() — known test vectors (RFC 4648)
// =============================================================================

TEST(IntervalsIcuUploaderTest, Base64EncodeTestVectors) {
    // RFC 4648 test vectors
    EXPECT_EQ(IntervalsIcuUploader::base64Encode(""), "");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("f"), "Zg==");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("fo"), "Zm8=");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("foo"), "Zm9v");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("foob"), "Zm9vYg==");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("fooba"), "Zm9vYmE=");
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("foobar"), "Zm9vYmFy");

    // Intervals.icu Basic Auth format: "API_KEY:"
    EXPECT_EQ(IntervalsIcuUploader::base64Encode("my_key:"), "bXlfa2V5Og==");
}

// =============================================================================
// 13. testConnection() — success and failure
// =============================================================================

TEST(IntervalsIcuUploaderTest, TestConnectionSuccessAndFailure) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    // Not configured → failure
    EXPECT_FALSE(uploader.testConnection());

    // Configured → success
    ASSERT_TRUE(uploader.setApiCredentials("key", "athlete"));
    EXPECT_TRUE(uploader.testConnection());

    // After disconnect → failure
    uploader.disconnect();
    EXPECT_FALSE(uploader.testConnection());
}

// =============================================================================
// 14. buildBasicAuthHeader() — correct format
// =============================================================================

TEST(IntervalsIcuUploaderTest, BuildBasicAuthHeaderFormat) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    ASSERT_TRUE(uploader.setApiCredentials("my_secret_key", "i100"));

    const std::string header = uploader.buildBasicAuthHeader();
    EXPECT_EQ(header, "Basic " + IntervalsIcuUploader::base64Encode("my_secret_key:"));
    EXPECT_NE(header.find("Basic "), std::string::npos);
}

// =============================================================================
// 15. buildUploadUrl() — correct URL path with athlete ID
// =============================================================================

TEST(IntervalsIcuUploaderTest, BuildUploadUrlContainsAthleteId) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(&secretStore);

    ASSERT_TRUE(uploader.setApiCredentials("key", "i55555"));

    const std::string url = uploader.buildUploadUrl();
    EXPECT_EQ(url, "https://intervals.icu/api/v1/athlete/i55555/activities");
}

// =============================================================================
// 16. Redactor saturation warning
// =============================================================================

TEST(IntervalsIcuUploaderTest, RedactorSaturationWarning) {
    InMemorySecretStore secretStore;
    IntervalsIcuUploader uploader(
        &secretStore,
        nullptr,
        [](const std::string& /*secret*/) -> bool { return false; }); // Always reject

    ASSERT_TRUE(uploader.setApiCredentials("rejected_key", "i99"));
    EXPECT_EQ(uploader.getRedactorWarningCount(), 1u);
    EXPECT_TRUE(uploader.isRedactorSaturated());
    // Warning must not leak the actual secret value
    EXPECT_EQ(uploader.getLastRedactorWarning().find("rejected_key"), std::string::npos);
}
