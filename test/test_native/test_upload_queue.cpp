#include <gtest/gtest.h>

#include "echenon/IUploader.h"
#include "echenon/RideStore.h"
#include "echenon/TCXGenerator.h"
#include "echenon/UploadQueue.h"
#include "echenon/UploadQueue.cpp"
#include "modules/EventBus/EventBus.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace echenon;

namespace {

class MockUploader : public IUploader {
public:
    bool configured         = true;
    bool authenticated      = true;
    bool token_needs_refresh = true;

    UploadStageResult refresh_result = UploadStageResult::SUCCESS;
    UploadStageResult post_result    = UploadStageResult::SUCCESS;
    UploadStageResult poll_result    = UploadStageResult::SUCCESS;

    std::string refresh_err;
    std::string post_err;
    std::string poll_err;

    std::string returned_upload_id   = "up_987654";
    std::string returned_activity_id = "act_123456789";

    uint32_t refresh_calls      = 0;
    uint32_t post_calls         = 0;
    uint32_t poll_calls         = 0;
    uint32_t last_timeout_ms    = 0;
    uint32_t last_session_id    = 0;
    std::string last_tcx_path;
    std::string last_polled_upload_id;
    bool post_saw_existing_tcx_file = false;
    IRideFileSystem* fs_to_check = nullptr;

    const char* serviceId() const override { return "strava"; }
    bool isConfigured() const override { return configured; }
    bool isAuthenticated() const override { return authenticated; }
    bool needsTokenRefresh(uint32_t /*now_epoch_s*/) const override {
        return token_needs_refresh;
    }

    UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) override {
        ++refresh_calls;
        last_timeout_ms = timeout_ms;
        err_msg = refresh_err;
        if (refresh_result == UploadStageResult::SUCCESS) {
            token_needs_refresh = false;
        }
        return refresh_result;
    }

    UploadStageResult postUpload(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        uint32_t timeout_ms,
        std::string& out_upload_id,
        std::string& err_msg) override {
        ++post_calls;
        last_session_id = session_id;
        last_tcx_path   = tcx_gz_path;
        last_timeout_ms = timeout_ms;
        err_msg         = post_err;

        if (fs_to_check && fs_to_check->exists(tcx_gz_path.c_str())) {
            post_saw_existing_tcx_file = true;
        } else {
            FILE* f = std::fopen(tcx_gz_path.c_str(), "rb");
            if (f) {
                post_saw_existing_tcx_file = true;
                std::fclose(f);
            }
        }

        if (post_result == UploadStageResult::SUCCESS) {
            out_upload_id = returned_upload_id;
        }
        return post_result;
    }

    UploadStageResult pollUploadStatus(
        const std::string& upload_id,
        uint32_t timeout_ms,
        std::string& out_activity_id,
        std::string& err_msg) override {
        ++poll_calls;
        last_polled_upload_id = upload_id;
        last_timeout_ms       = timeout_ms;
        err_msg               = poll_err;
        if (poll_result == UploadStageResult::SUCCESS) {
            out_activity_id = returned_activity_id;
        }
        return poll_result;
    }
};

struct UploadEventSpy {
    std::vector<UploadEvent> events;

    static void onEvent(const char* topic, const void* payload, void* ctx) {
        if (!topic || !payload || !ctx) return;
        if (std::strcmp(topic, EVENT_UPLOAD_TOPIC) != 0) return;
        auto* self = static_cast<UploadEventSpy*>(ctx);
        self->events.push_back(*static_cast<const UploadEvent*>(payload));
    }
};

struct MockPreconditionEntry {
    std::string name;
    PreconditionRole role;
    IPreconditionRegistry::EvalFn evaluate;
    IPreconditionRegistry::DescribeFn describe;
    void* context;
};

class MockPreconditionRegistry : public IPreconditionRegistry {
public:
    std::vector<MockPreconditionEntry> entries;

    void add(const char* name,
             PreconditionRole role,
             EvalFn evaluate,
             DescribeFn describe = nullptr,
             void* context = nullptr,
             const Edge* /*edges*/ = nullptr,
             uint8_t /*edgeCount*/ = 0,
             const DampingPolicy* /*damping*/ = nullptr,
             const RetryPolicy* /*retry*/ = nullptr) override {
        entries.push_back(MockPreconditionEntry{
            name ? name : "",
            role,
            evaluate,
            describe,
            context
        });
    }

    const MockPreconditionEntry* find(const std::string& name) const {
        for (const auto& e : entries) {
            if (e.name == name) return &e;
        }
        return nullptr;
    }
};

void populateStoredRide(RideStore& store, uint32_t session_id, uint16_t count = 10) {
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
// 1. 4-Step Split-Stage Progression (GENERATE -> REFRESH -> POST -> POLL -> COMPLETE)
// =============================================================================

TEST(UploadQueueTest, FourStageProgressionRunsAtMostOneStagePerStepAndCleansUpTcxGz) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    populateStoredRide(store, 101u, 12);

    EventBus bus;
    bus.begin();
    UploadEventSpy eventSpy;
    ASSERT_TRUE(bus.subscribe(EVENT_UPLOAD_TOPIC, UploadEventSpy::onEvent, &eventSpy));

    MockUploader uploader;
    uploader.token_needs_refresh = true;
    uploader.fs_to_check = &fs;

    UploadQueue queue(&uploader, &store, &bus);
    queue.setBaseDir("./tmp/test_upload_queue");
    queue.setNowEpochCallback([]() -> uint32_t { return 1789680000u; });

    ASSERT_TRUE(queue.enqueueSession(101u, 1000u));
    ASSERT_EQ(queue.size(), 1u);
    ASSERT_EQ(queue.getPendingCount(), 1u);

    const UploadJob* job = queue.findJob(101u);
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(job->stage, UploadStage::STAGE_GENERATE_TCX_GZ);

    // Step 1: STAGE_GENERATE_TCX_GZ -> STAGE_REFRESH_TOKEN (no network calls)
    EXPECT_EQ(queue.stepQueue(15000u, 1000u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_REFRESH_TOKEN);
    EXPECT_EQ(uploader.refresh_calls, 0u);
    EXPECT_EQ(uploader.post_calls, 0u);
    EXPECT_EQ(uploader.poll_calls, 0u);
    EXPECT_TRUE(fs.exists(queue.getTcxGzPath(101u).c_str()));

    // Step 2: STAGE_REFRESH_TOKEN -> STAGE_POST_UPLOAD (only refreshToken called)
    EXPECT_EQ(queue.stepQueue(15000u, 1100u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POST_UPLOAD);
    EXPECT_EQ(uploader.refresh_calls, 1u);
    EXPECT_EQ(uploader.last_timeout_ms, UploadQueue::REFRESH_STAGE_BUDGET_MS);
    EXPECT_EQ(uploader.post_calls, 0u);
    EXPECT_EQ(uploader.poll_calls, 0u);

    // Step 3: STAGE_POST_UPLOAD -> STAGE_POLL_STATUS (scheduled +3000 ms later)
    EXPECT_EQ(queue.stepQueue(15000u, 1200u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POLL_STATUS);
    EXPECT_EQ(job->remote_upload_id, "up_987654");
    EXPECT_EQ(job->next_attempt_ms, 1200u + UploadQueue::POLL_DELAY_MS);
    EXPECT_EQ(uploader.refresh_calls, 1u);
    EXPECT_EQ(uploader.post_calls, 1u);
    EXPECT_TRUE(uploader.post_saw_existing_tcx_file);
    EXPECT_EQ(uploader.last_timeout_ms, UploadQueue::POST_STAGE_BUDGET_MS);
    EXPECT_EQ(uploader.poll_calls, 0u);

    // Calling stepQueue before the 3000 ms poll delay expires must skip
    EXPECT_EQ(queue.stepQueue(15000u, 3000u), UpdateStatus::Skipped);
    EXPECT_EQ(uploader.poll_calls, 0u);

    // Simulate IN_PROGRESS on first poll at t=4200 ms -> reschedules +3000 ms without failing
    uploader.poll_result = UploadStageResult::IN_PROGRESS;
    EXPECT_EQ(queue.stepQueue(15000u, 4200u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POLL_STATUS);
    EXPECT_EQ(job->attempt_count, 0u);
    EXPECT_EQ(job->next_attempt_ms, 4200u + UploadQueue::POLL_DELAY_MS);
    EXPECT_EQ(uploader.poll_calls, 1u);
    EXPECT_TRUE(eventSpy.events.empty());

    // Step 4: Poll completes at t=7200 ms -> STAGE_COMPLETE, .tcx.gz deleted, event published
    uploader.poll_result = UploadStageResult::SUCCESS;
    EXPECT_EQ(queue.stepQueue(15000u, 7200u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_COMPLETE);
    EXPECT_EQ(job->remote_activity_id, "act_123456789");
    EXPECT_EQ(uploader.poll_calls, 2u);
    EXPECT_EQ(queue.getPendingCount(), 0u);

    // Verify temporary .tcx.gz was deleted from both RideStore FS and POSIX temp path
    EXPECT_FALSE(fs.exists(queue.getTcxGzPath(101u).c_str()));
    EXPECT_FALSE(fs.exists(RideStore::getRideTcxGzPath(101u).c_str()));
    FILE* f = std::fopen(queue.getTcxGzPath(101u).c_str(), "rb");
    EXPECT_EQ(f, nullptr);
    if (f) std::fclose(f);

    // Verify EventBus notification
    ASSERT_EQ(eventSpy.events.size(), 1u);
    EXPECT_EQ(eventSpy.events[0].session_id, 101u);
    EXPECT_EQ(eventSpy.events[0].stage, UploadStage::STAGE_COMPLETE);
    EXPECT_STREQ(eventSpy.events[0].remote_activity_id, "act_123456789");
}

// =============================================================================
// 2. Token Refresh Skip When needsTokenRefresh() == false
// =============================================================================

TEST(UploadQueueTest, SkipsTokenRefreshStageWhenTokenStillValid) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    populateStoredRide(store, 202u, 8);

    MockUploader uploader;
    uploader.token_needs_refresh = false;

    UploadQueue queue(&uploader, &store);
    queue.setBaseDir("./tmp/test_upload_queue");

    ASSERT_TRUE(queue.enqueueSession(202u, 500u));
    const UploadJob* job = queue.findJob(202u);
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(job->stage, UploadStage::STAGE_GENERATE_TCX_GZ);

    // Step 1: STAGE_GENERATE_TCX_GZ jumps directly to STAGE_POST_UPLOAD
    EXPECT_EQ(queue.stepQueue(15000u, 500u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POST_UPLOAD);
    EXPECT_EQ(uploader.refresh_calls, 0u);

    // Step 2: STAGE_POST_UPLOAD -> STAGE_POLL_STATUS
    EXPECT_EQ(queue.stepQueue(15000u, 500u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POLL_STATUS);
    EXPECT_EQ(uploader.refresh_calls, 0u);
    EXPECT_EQ(uploader.post_calls, 1u);

    // Step 3: STAGE_POLL_STATUS -> STAGE_COMPLETE
    EXPECT_EQ(queue.stepQueue(15000u, 3500u), UpdateStatus::Success);
    EXPECT_EQ(job->stage, UploadStage::STAGE_COMPLETE);
    EXPECT_EQ(uploader.refresh_calls, 0u);
    EXPECT_EQ(uploader.poll_calls, 1u);
}

// =============================================================================
// 3. Exponential Backoff, Wraparound-Safe Timers & STAGE_FAILED Transition
// =============================================================================

TEST(UploadQueueTest, ExponentialBackoffAndFailureAfterFiveRetryableAttemptsOrPermanentError) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    populateStoredRide(store, 303u, 6);
    populateStoredRide(store, 304u, 6);

    EventBus bus;
    bus.begin();
    UploadEventSpy eventSpy;
    ASSERT_TRUE(bus.subscribe(EVENT_UPLOAD_TOPIC, UploadEventSpy::onEvent, &eventSpy));

    MockUploader uploader;
    uploader.token_needs_refresh = false;
    uploader.post_result         = UploadStageResult::RETRYABLE_ERROR;
    uploader.post_err            = "HTTP 503 Service Unavailable";

    UploadQueue queue(&uploader, &store, &bus);
    queue.setBaseDir("./tmp/test_upload_queue");

    // Start near 32-bit millis() wraparound boundary (0xFFFFFFF0u) to verify signed int32_t math
    uint32_t now_ms = 0xFFFFFFF0u;
    ASSERT_TRUE(queue.enqueueSession(303u, now_ms));

    // Generate TCX.gz -> advances to STAGE_POST_UPLOAD
    EXPECT_EQ(queue.stepQueue(15000u, now_ms), UpdateStatus::Success);
    const UploadJob* job = queue.findJob(303u);
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(job->stage, UploadStage::STAGE_POST_UPLOAD);

    // Expected backoff schedule for attempts 1..4: 30s, 60s, 120s, 300s
    const uint32_t expected_backoffs[4] = {30000u, 60000u, 120000u, 300000u};
    for (uint8_t attempt = 1; attempt <= 4; ++attempt) {
        EXPECT_EQ(queue.stepQueue(15000u, now_ms), UpdateStatus::FetchError);
        EXPECT_EQ(job->stage, UploadStage::STAGE_POST_UPLOAD);
        EXPECT_EQ(job->attempt_count, attempt);
        EXPECT_EQ(job->next_attempt_ms, now_ms + expected_backoffs[attempt - 1]);

        // 1 ms before backoff expiry -> must not run
        EXPECT_EQ(queue.stepQueue(15000u, now_ms + expected_backoffs[attempt - 1] - 1u),
                  UpdateStatus::Skipped);
        EXPECT_EQ(uploader.post_calls, attempt);

        now_ms += expected_backoffs[attempt - 1];
    }

    // 5th retryable failure transitions job to STAGE_FAILED and publishes failure event
    EXPECT_EQ(queue.stepQueue(15000u, now_ms), UpdateStatus::FetchError);
    EXPECT_EQ(job->stage, UploadStage::STAGE_FAILED);
    EXPECT_EQ(job->attempt_count, 5u);
    EXPECT_EQ(uploader.post_calls, 5u);
    ASSERT_EQ(eventSpy.events.size(), 1u);
    EXPECT_EQ(eventSpy.events[0].session_id, 303u);
    EXPECT_EQ(eventSpy.events[0].stage, UploadStage::STAGE_FAILED);

    // Enqueue second session (304) and verify 1 PERMANENT_ERROR immediately enters STAGE_FAILED
    ASSERT_TRUE(queue.enqueueSession(304u, now_ms));
    EXPECT_EQ(queue.stepQueue(15000u, now_ms), UpdateStatus::Success); // GENERATE_TCX_GZ
    uploader.post_result = UploadStageResult::PERMANENT_ERROR;
    uploader.post_err    = "Duplicate activity";
    EXPECT_EQ(queue.stepQueue(15000u, now_ms), UpdateStatus::ParseError);

    const UploadJob* job2 = queue.findJob(304u);
    ASSERT_NE(job2, nullptr);
    EXPECT_EQ(job2->stage, UploadStage::STAGE_FAILED);
    EXPECT_EQ(job2->attempt_count, 1u);
    ASSERT_EQ(eventSpy.events.size(), 2u);
    EXPECT_EQ(eventSpy.events[1].session_id, 304u);
    EXPECT_EQ(eventSpy.events[1].stage, UploadStage::STAGE_FAILED);
}

// =============================================================================
// 4. Discard Session, 5-Entry Capacity & JSON Persistence Round-Trip
// =============================================================================

TEST(UploadQueueTest, DiscardSessionQueueCapacityAndJsonRoundTrip) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    for (uint32_t id = 1; id <= 5; ++id) {
        populateStoredRide(store, id, 5);
    }

    MockUploader uploader;
    uploader.token_needs_refresh = true;

    UploadQueue queue(&uploader, &store);
    queue.setBaseDir("./tmp/test_upload_queue");

    // Enqueue 5 active sessions (fills MAX_QUEUE_ENTRIES = 5)
    for (uint32_t id = 1; id <= 5; ++id) {
        EXPECT_TRUE(queue.enqueueSession(id, 100u * id));
    }
    EXPECT_EQ(queue.size(), 5u);

    // 6th active session must be rejected while all 5 slots hold active jobs
    EXPECT_FALSE(queue.enqueueSession(6u, 600u));
    EXPECT_EQ(queue.size(), 5u);

    // Advance session 1 to STAGE_REFRESH_TOKEN so it generates .tcx.gz and custom fields
    EXPECT_EQ(queue.stepQueue(15000u, 100u), UpdateStatus::Success);
    UploadJob* job1 = queue.findJobMutable(1u);
    ASSERT_NE(job1, nullptr);
    EXPECT_EQ(job1->stage, UploadStage::STAGE_REFRESH_TOKEN);
    job1->remote_upload_id = "up_persist_1";
    job1->last_error       = "transient_warn";
    ASSERT_TRUE(queue.saveQueue());

    // Verify .tcx.gz and .bin exist for session 1 before discard
    EXPECT_TRUE(fs.exists(RideStore::getRideFilePath(1u).c_str()));
    EXPECT_TRUE(fs.exists(RideStore::getRideTcxGzPath(1u).c_str()));

    // Verify JSON round-trip into a brand-new UploadQueue instance before discarding
    UploadQueue restoredQueue(&uploader, &store);
    restoredQueue.setBaseDir("./tmp/test_upload_queue");
    ASSERT_TRUE(restoredQueue.loadQueue());
    ASSERT_EQ(restoredQueue.size(), 5u);
    const UploadJob* restoredJob1 = restoredQueue.findJob(1u);
    ASSERT_NE(restoredJob1, nullptr);
    EXPECT_EQ(restoredJob1->stage, UploadStage::STAGE_REFRESH_TOKEN);
    EXPECT_EQ(restoredJob1->remote_upload_id, "up_persist_1");
    EXPECT_EQ(restoredJob1->last_error, "transient_warn");

    // Discard session 1: removes from queue and deletes .tcx.gz and .bin artifacts
    EXPECT_TRUE(queue.discardSession(1u));
    EXPECT_EQ(queue.size(), 4u);
    EXPECT_EQ(queue.findJob(1u), nullptr);
    EXPECT_EQ(store.findStoredRide(1u), nullptr);
    EXPECT_FALSE(fs.exists(RideStore::getRideFilePath(1u).c_str()));
    EXPECT_FALSE(fs.exists(RideStore::getRideTcxGzPath(1u).c_str()));
    EXPECT_FALSE(fs.exists(queue.getTcxGzPath(1u).c_str()));

    // Now enqueuing session 6 succeeds (4 active + 1 new = 5)
    EXPECT_TRUE(queue.enqueueSession(6u, 600u));
    EXPECT_EQ(queue.size(), 5u);

    // Mark session 2 as STAGE_COMPLETE and verify a 7th session evicts the completed entry
    UploadJob* job2 = queue.findJobMutable(2u);
    ASSERT_NE(job2, nullptr);
    job2->stage = UploadStage::STAGE_COMPLETE;
    EXPECT_TRUE(queue.enqueueSession(7u, 700u));
    EXPECT_EQ(queue.size(), 5u);
    EXPECT_EQ(queue.findJob(2u), nullptr);
    EXPECT_NE(queue.findJob(7u), nullptr);
}

// =============================================================================
// 5. Preconditions ("strava_configured", "ntp_synced") & IConfigurable
// =============================================================================

TEST(UploadQueueTest, PreconditionsAndConfigurableSettings) {
    MockUploader uploader;
    uploader.configured    = false;
    uploader.authenticated = false;

    bool ntp_synced = false;
    UploadQueue queue(&uploader);
    queue.setNtpSyncedCallback([&ntp_synced]() { return ntp_synced; });

    MockPreconditionRegistry reg;
    queue.registerPreconditions(reg);
    ASSERT_EQ(reg.entries.size(), 2u);

    const auto* stravaPre = reg.find("strava_configured");
    const auto* ntpPre    = reg.find("ntp_synced");
    ASSERT_NE(stravaPre, nullptr);
    ASSERT_NE(ntpPre, nullptr);
    EXPECT_EQ(stravaPre->role, PreconditionRole::Operational);
    EXPECT_EQ(ntpPre->role, PreconditionRole::Operational);

    // Initially neither is met
    EXPECT_FALSE(stravaPre->evaluate(stravaPre->context).met);
    EXPECT_EQ(stravaPre->evaluate(stravaPre->context).cause, Cause::NotConfigured);
    EXPECT_FALSE(ntpPre->evaluate(ntpPre->context).met);

    // Configured but not authenticated -> Rejected
    uploader.configured = true;
    EXPECT_FALSE(stravaPre->evaluate(stravaPre->context).met);
    EXPECT_EQ(stravaPre->evaluate(stravaPre->context).cause, Cause::Rejected);

    // Authenticated and NTP synced -> both Met
    uploader.authenticated = true;
    ntp_synced             = true;
    EXPECT_TRUE(stravaPre->evaluate(stravaPre->context).met);
    EXPECT_TRUE(ntpPre->evaluate(ntpPre->context).met);

    // IConfigurable round-trip
    JsonDocument doc;
    JsonObject section = doc.to<JsonObject>();
    section["auto_upload_enabled"] = false;
    queue.applyConfig(section);
    EXPECT_FALSE(queue.isAutoUploadEnabled());

    JsonDocument outDoc;
    JsonObject outSection = outDoc.to<JsonObject>();
    queue.serializeConfig(outSection);
    EXPECT_FALSE(outSection["auto_upload_enabled"].as<bool>());

    queue.resetConfig();
    EXPECT_TRUE(queue.isAutoUploadEnabled());
}
