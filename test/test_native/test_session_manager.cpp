/// @file test_session_manager.cpp
/// @brief Native unit and integration tests for SessionManager lifecycle and telemetry sampling.

#include <gtest/gtest.h>

#include "modules/EventBus/EventBus.hpp"
#include "echenon/HREvents.h"
#include "echenon/RideStore.h"
#include "echenon/RideStore.cpp"
#include "echenon/SessionManager.h"
#include "echenon/SessionManager.cpp"
#include "echenon/TCXGenerator.h"
#include "echenon/UploadQueue.h"
#include "echenon/StravaUploader.h"
#include "echenon/SessionOverlay.h"
#include "echenon/SessionOverlay.cpp"

#include <vector>

using namespace echenon;

namespace {

struct CapturedStateEvent {
    SessionState oldState;
    SessionState newState;
    uint32_t     sessionId;
    uint32_t     timestampMs;
};

struct EventRecorder {
    std::vector<CapturedStateEvent> events;

    static void onEvent(const char* topic, const void* payload, void* ctx) {
        (void)topic;
        if (!payload || !ctx) return;
        auto* self = static_cast<EventRecorder*>(ctx);
        const auto* ev = static_cast<const SessionStateChangeEvent*>(payload);
        self->events.push_back(CapturedStateEvent{
            ev->oldState,
            ev->newState,
            ev->sessionId,
            ev->timestampMs
        });
    }
};

} // namespace

// =============================================================================
// 1. RideRecord Binary Layout & Helper Conversion Tests
// =============================================================================

TEST(RideRecordTest, PackedSizeAndConversionHelpers) {
    static_assert(sizeof(RideRecord) == 8, "RideRecord must be exactly 8 bytes");
    EXPECT_EQ(sizeof(RideRecord), 8u);

    // Uncalibrated sentinel (255) -> 0 for both Echelon and Peloton
    RideRecord rec{};
    rec.timestamp_offset_s  = 42;
    rec.power_watts         = 225;
    rec.cadence_rpm         = 90;
    rec.heart_rate_bpm      = 155;
    rec.internal_resistance = LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
    rec.flags               = RIDE_FLAG_HR_VALID;

    EXPECT_EQ(rec.elapsed_sec, 42u);
    EXPECT_FLOAT_EQ(rec.normalizedResistance(), 0.0f);
    EXPECT_EQ(rec.echelonLevel(), 0u);
    EXPECT_EQ(rec.pelotonLevel(), 0u);
    EXPECT_TRUE(rec.isHrValid());
    EXPECT_FALSE(rec.isResistanceCalibrated());
    EXPECT_FALSE(rec.isPauseBoundary());

    // Minimum calibrated resistance (0 -> Echelon level 1)
    rec.internal_resistance = 0;
    rec.flags |= RIDE_FLAG_RESISTANCE_CALIBRATED;
    EXPECT_FLOAT_EQ(rec.normalizedResistance(), 0.0f);
    EXPECT_EQ(rec.echelonLevel(), 1u);
    EXPECT_EQ(rec.pelotonLevel(), LevelMapping::internalToPeloton(0));
    EXPECT_TRUE(rec.isResistanceCalibrated());

    // Maximum calibrated resistance (254 -> Echelon level 32)
    rec.internal_resistance = 254;
    EXPECT_NEAR(rec.normalizedResistance(), 1.0f, 1e-5f);
    EXPECT_EQ(rec.echelonLevel(), 32u);
    EXPECT_EQ(rec.pelotonLevel(), LevelMapping::internalToPeloton(254));
    EXPECT_GT(rec.pelotonLevel(), 0u);

    // Mid-scale resistance (127 -> Echelon level ~16-17)
    rec.internal_resistance = 127;
    EXPECT_EQ(rec.echelonLevel(), LevelMapping::internalToEchelon(127));
    EXPECT_EQ(rec.pelotonLevel(), LevelMapping::internalToPeloton(127));
}

// =============================================================================
// 2. SessionManager State Machine & Auto-Start / Auto-Pause / Inactivity Tests
// =============================================================================

TEST(SessionManagerTest, AutoStartRequiresContinuousCadenceForDefault5Seconds) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    EventBus bus;
    EventRecorder recorder;
    bus.subscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);

    SessionManager mgr(store, bus);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    EXPECT_TRUE(mgr.isAutoStartEnabled());
    EXPECT_EQ(mgr.getAutoStartDelaySec(), 5u);

    // Non-zero cadence starts at t = 1000 ms
    mgr.updateTelemetry(150.0f, 80.0f, 140, true, 100, true);
    mgr.tick(1000);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);

    // Up to t = 5999 ms (< 5000 ms elapsed), remains IDLE
    mgr.tick(3000);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    mgr.tick(5999);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    EXPECT_TRUE(recorder.events.empty());

    // Drop cadence to 0 before 5000 ms -> resets auto-start timer
    mgr.updateTelemetry(0.0f, 0.0f, 140, true, 100, true);
    mgr.tick(6000);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);

    // Resume non-zero cadence at t = 10000 ms
    mgr.updateTelemetry(150.0f, 80.0f, 140, true, 100, true);
    mgr.tick(10000);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    mgr.tick(14999);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);

    // At t = 15000 ms (exactly 5000 ms continuous cadence), transitions IDLE -> ACTIVE
    mgr.tick(15000);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    ASSERT_EQ(recorder.events.size(), 1u);
    EXPECT_EQ(recorder.events[0].oldState, SessionState::IDLE);
    EXPECT_EQ(recorder.events[0].newState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[0].timestampMs, 15000u);

    bus.unsubscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);
}

TEST(SessionManagerTest, AutoStartDisabledStaysIdleUntilManualStart) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    EventBus bus;
    SessionManager mgr(store, bus);
    mgr.setAutoStartEnabled(false);

    mgr.updateTelemetry(200.0f, 90.0f, 145, true, 120, true);
    for (uint32_t t = 0; t <= 60000; t += 1000) {
        mgr.tick(t);
        EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    }

    // Manual startSession() transitions IDLE -> ACTIVE immediately
    EXPECT_TRUE(mgr.startSession(1710000000u, 61000u));
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    EXPECT_EQ(mgr.getStartEpochS(), 1710000000u);
}

TEST(SessionManagerTest, ConfigurableThresholdsViaIConfigurable) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);
    EXPECT_STREQ(mgr.getConfigNamespace(), "echenon_session");
    EXPECT_EQ(mgr.getSchemaVersion(), 1);

    JsonDocument doc;
    JsonObject cfg = doc.to<JsonObject>();
    cfg["session_auto_start"]   = true;
    cfg["session_auto_start_s"] = 10;
    cfg["session_inactivity_s"] = 60;
    mgr.applyConfig(cfg);

    EXPECT_TRUE(mgr.isAutoStartEnabled());
    EXPECT_EQ(mgr.getAutoStartDelaySec(), 10u);
    EXPECT_EQ(mgr.getInactivityTimeoutSec(), 60u);

    // Serialize round-trip verification
    JsonDocument outDoc;
    JsonObject outCfg = outDoc.to<JsonObject>();
    mgr.serializeConfig(outCfg);
    EXPECT_EQ(outCfg["session_auto_start"].as<bool>(), true);
    EXPECT_EQ(outCfg["session_auto_start_s"].as<int>(), 10);
    EXPECT_EQ(outCfg["session_inactivity_s"].as<int>(), 60);

    // Verify 10 s auto-start delay in action
    mgr.updateTelemetry(160.0f, 82.0f, 130, true, 90, true);
    mgr.tick(0);
    mgr.tick(5000);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    mgr.tick(9999);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    mgr.tick(10000);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);

    // Pause and verify 60 s inactivity timeout
    ASSERT_TRUE(mgr.pauseSession(20000));
    EXPECT_EQ(mgr.getState(), SessionState::PAUSED);
    mgr.updateTelemetry(0.0f, 0.0f, 130, true, 90, true);
    mgr.tick(79999);
    EXPECT_EQ(mgr.getState(), SessionState::PAUSED);
    mgr.tick(80000);
    EXPECT_EQ(mgr.getState(), SessionState::ENDED);

    // Reset config restores defaults (5 s, 180 s)
    mgr.resetConfig();
    EXPECT_TRUE(mgr.isAutoStartEnabled());
    EXPECT_EQ(mgr.getAutoStartDelaySec(), 5u);
    EXPECT_EQ(mgr.getInactivityTimeoutSec(), 180u);
}

TEST(SessionManagerTest, FullLifecycleTransitionsPauseBoundaryAndEventBus) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    EventBus bus;
    EventRecorder recorder;
    bus.subscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);

    SessionManager mgr(store, bus);
    mgr.updateTelemetry(180.0f, 85.0f, 145, true, 128, true);

    // 1. IDLE -> ACTIVE after 5000 ms
    mgr.tick(0);
    mgr.tick(5000);
    ASSERT_EQ(mgr.getState(), SessionState::ACTIVE);

    // Record 5 seconds of active riding (t = 5500 .. 10000 ms -> 5 records, offsets 0..4)
    for (uint32_t t = 5500; t <= 10000; t += 500) {
        mgr.tick(t);
    }
    EXPECT_EQ(store.getRecordCount(), 5u);
    EXPECT_EQ(store.getRecords()[4].timestamp_offset_s, 4u);

    // 2. Manual UI pause at t = 10000 ms -> PAUSED
    ASSERT_TRUE(mgr.pauseSession(10000));
    EXPECT_EQ(mgr.getState(), SessionState::PAUSED);

    size_t countAtPause = store.getRecordCount();
    uint16_t offsetBeforePause = mgr.getNextTimestampOffsetS();

    // While PAUSED for 45 seconds (t = 10000 -> 55000), no records are written even if telemetry updates
    for (uint32_t t = 15000; t <= 50000; t += 5000) {
        mgr.tick(t);
        EXPECT_EQ(mgr.getState(), SessionState::PAUSED);
        EXPECT_EQ(store.getRecordCount(), countAtPause);
    }

    // 3. Manual UI resume at t = 55000 (45 s pause gap) -> PAUSED -> ACTIVE
    mgr.updateTelemetry(200.0f, 90.0f, 150, true, 140, true);
    ASSERT_TRUE(mgr.resumeSession(55000));
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    EXPECT_EQ(mgr.getNextTimestampOffsetS(), static_cast<uint16_t>(offsetBeforePause + 45u));

    // Emit first resumed 1 Hz record (t = 55500, 56000)
    mgr.tick(55500);
    mgr.tick(56000);
    ASSERT_EQ(store.getRecordCount(), countAtPause + 1u);
    const RideRecord& resumedRec = store.getRecords()[countAtPause];
    EXPECT_TRUE(resumedRec.isPauseBoundary());
    EXPECT_EQ(resumedRec.timestamp_offset_s, static_cast<uint16_t>(offsetBeforePause + 45u));
    EXPECT_EQ(resumedRec.power_watts, 200u);
    EXPECT_EQ(resumedRec.cadence_rpm, 90u);

    // Emit second resumed 1 Hz record (t = 56500, 57000) — pause boundary flag cleared
    mgr.tick(56500);
    mgr.tick(57000);
    ASSERT_EQ(store.getRecordCount(), countAtPause + 2u);
    EXPECT_FALSE(store.getRecords()[countAtPause + 1].isPauseBoundary());
    EXPECT_EQ(store.getRecords()[countAtPause + 1].timestamp_offset_s,
              static_cast<uint16_t>(offsetBeforePause + 46u));

    // 4. Stop pedalling while ACTIVE at t = 57500 ms:
    //    Stays ACTIVE and records 1 Hz zero-cadence samples continuously during coasting,
    //    then auto-ends at 180,000 ms (t = 237500 ms) and trims all trailing zero-cadence records!
    mgr.updateTelemetry(0.0f, 0.0f, 130, true, 140, true);
    for (uint32_t t = 57500; t <= 237000; t += 500) {
        mgr.tick(t);
        EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    }
    // 180 seconds of 1 Hz zero-cadence samples were recorded prior to auto-end (7 active + 179 zero-cadence = 186)
    EXPECT_GT(store.getRecordCount(), 180u);

    // At t = 237500 ms (exactly 180,000 ms since zero cadence started at 57500 ms) -> ACTIVE -> ENDED
    mgr.tick(237500);
    EXPECT_EQ(mgr.getState(), SessionState::ENDED);

    // Verify all trailing zero-cadence records were stripped and excluded from summary metrics:
    // Only the 7 active pedalling records remain (5 records @ 180 W/85 RPM + 2 records @ 200 W/90 RPM)
    const RideSummary& summary = mgr.getLastSummary();
    EXPECT_EQ(summary.record_count, 7u);
    EXPECT_EQ(summary.duration_s, 7u);
    EXPECT_EQ(summary.active_duration_s, 7u);
    // Average power = round((5*180 + 2*200) / 7) = round(1300 / 7) = 186 W
    EXPECT_EQ(summary.avg_power_w, 186u);
    // Average cadence = round((5*85 + 2*90) / 7) = round(605 / 7) = 86 RPM
    EXPECT_EQ(summary.avg_cadence_rpm, 86u);

    // Verify EventBus received IDLE->ACTIVE, ACTIVE->PAUSED, PAUSED->ACTIVE, ACTIVE->ENDED
    ASSERT_EQ(recorder.events.size(), 4u);
    EXPECT_EQ(recorder.events[0].oldState, SessionState::IDLE);
    EXPECT_EQ(recorder.events[0].newState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[1].oldState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[1].newState, SessionState::PAUSED);
    EXPECT_EQ(recorder.events[2].oldState, SessionState::PAUSED);
    EXPECT_EQ(recorder.events[2].newState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[3].oldState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[3].newState, SessionState::ENDED);

    bus.unsubscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);
}

TEST(SessionManagerTest, DiscardSessionCleansUpAndReturnsToIdle) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    EventBus bus;
    EventRecorder recorder;
    bus.subscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);

    SessionManager mgr(store, bus);
    ASSERT_TRUE(mgr.startSession(1710000000u, 1000u));
    mgr.updateTelemetry(180.0f, 85.0f, 140, true, 100, true);

    // Record 35 seconds so a checkpoint file (/ride_active.bin) is created
    for (uint32_t t = 1500; t <= 36000; t += 500) {
        mgr.tick(t);
    }
    EXPECT_EQ(store.getRecordCount(), 35u);
    EXPECT_TRUE(fs.exists(RideStore::ACTIVE_FILE_PATH));

    // Discard active session
    EXPECT_TRUE(mgr.discardSession(36500));
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);
    EXPECT_EQ(store.getRecordCount(), 0u);
    EXPECT_FALSE(store.isSessionActive());
    EXPECT_FALSE(fs.exists(RideStore::ACTIVE_FILE_PATH));

    ASSERT_EQ(recorder.events.size(), 2u);
    EXPECT_EQ(recorder.events[1].oldState, SessionState::ACTIVE);
    EXPECT_EQ(recorder.events[1].newState, SessionState::IDLE);

    bus.unsubscribe(EVENT_SESSION_STATE, EventRecorder::onEvent, &recorder);
}

// =============================================================================
// 3. 32-bit millis() Wraparound & Deferred NTP Back-Computation Tests
// =============================================================================

TEST(SessionManagerTest, Millis32BitWraparoundAcrossZeroBoundary) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);

    // Start cadence near 32-bit rollover: 0xFFFFFFF0 (4294967280)
    const uint32_t t0 = 0xFFFFFFF0u;
    mgr.updateTelemetry(150.0f, 80.0f, 135, true, 100, true);
    mgr.tick(t0);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);

    // 4999 ms later (wraps past 0xFFFFFFFF -> 0x00001377)
    mgr.tick(t0 + 4999u);
    EXPECT_EQ(mgr.getState(), SessionState::IDLE);

    // 5000 ms later (0x00001378) -> transitions IDLE -> ACTIVE across rollover
    const uint32_t tActive = t0 + 5000u;
    mgr.tick(tActive);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);

    // Manual pause and resume across rollover boundary
    const uint32_t tPause = 0xFFFFFFFCu;
    ASSERT_TRUE(mgr.pauseSession(tPause));
    EXPECT_EQ(mgr.getState(), SessionState::PAUSED);

    const uint32_t tResume = tPause + 10000u; // wraps past 0xFFFFFFFF
    ASSERT_TRUE(mgr.resumeSession(tResume));
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    EXPECT_EQ(mgr.getNextTimestampOffsetS(), 10u);

    // Test ACTIVE 180 s inactivity auto-end across another rollover boundary
    const uint32_t tZeroStart = 0xFFFFFFF0u;
    mgr.updateTelemetry(0.0f, 0.0f, 135, true, 100, true);
    mgr.tick(tZeroStart);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);

    mgr.tick(tZeroStart + 179999u);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);

    mgr.tick(tZeroStart + 180000u);
    EXPECT_EQ(mgr.getState(), SessionState::ENDED);
}

TEST(SessionManagerTest, DeferredNtpWallClockBackComputation) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);

    // Start session before SNTP sync (start_epoch_s == 0) at t = 10,000 ms
    ASSERT_TRUE(mgr.startSession(0u, 10000u));
    EXPECT_EQ(mgr.getStartEpochS(), 0u);

    // Advance 100 seconds (t = 110,000 ms)
    mgr.updateTelemetry(175.0f, 85.0f, 140, true, 110, true);
    mgr.tick(110000u);

    // SNTP sync arrives at epoch 1710000100 after 100 s elapsed -> back-computes 1710000000
    mgr.updateWallClock(1710000100u);
    EXPECT_EQ(mgr.getStartEpochS(), 1710000000u);

    // Subsequent updateWallClock calls do not overwrite an already-valid start_epoch_s
    mgr.tick(120000u);
    mgr.updateWallClock(1710000500u);
    EXPECT_EQ(mgr.getStartEpochS(), 1710000000u);
}

// =============================================================================
// 4. 2 Hz -> 1 Hz Averaging & Active Coasting vs. Trailing Zero Strip Tests
// =============================================================================

TEST(SessionManagerTest, TwoHzToOneHzFundamentalAveraging) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);
    ASSERT_TRUE(mgr.startSession(1710000000u, 0u));

    // Sample 0 at t = 500 ms: power = 100 W, cadence = 80 RPM
    mgr.updateTelemetry(100.0f, 80.0f, 142, true, 120, true);
    mgr.tick(500);
    EXPECT_EQ(store.getRecordCount(), 0u);

    // Sample 1 at t = 1000 ms: power = 120 W, cadence = 84 RPM
    mgr.updateTelemetry(120.0f, 84.0f, 146, true, 124, true);
    mgr.tick(1000);
    ASSERT_EQ(store.getRecordCount(), 1u);

    const RideRecord& rec = store.getRecords()[0];
    EXPECT_EQ(rec.timestamp_offset_s, 0u);
    EXPECT_EQ(rec.power_watts, 110u);
    EXPECT_EQ(rec.cadence_rpm, 82u);
    EXPECT_EQ(rec.heart_rate_bpm, 146u);
    EXPECT_EQ(rec.internal_resistance, 124u);
    EXPECT_TRUE(rec.isHrValid());
    EXPECT_TRUE(rec.isResistanceCalibrated());
}

TEST(SessionManagerTest, ActiveCoastingPreservedInMiddleWhileTrailingZerosStripped) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);
    ASSERT_TRUE(mgr.startSession(1710000000u, 0u));

    // 1. Pedal for 5 seconds (t = 500 .. 5000 ms) -> 5 records (200 W, 85 RPM)
    mgr.updateTelemetry(200.0f, 85.0f, 145, true, 100, true);
    for (uint32_t t = 500; t <= 5000; t += 500) {
        mgr.tick(t);
    }
    EXPECT_EQ(store.getRecordCount(), 5u);

    // 2. Coast (0 RPM, 0 W) for 30 seconds (t = 5500 .. 35000 ms) — stays ACTIVE and records 30 1 Hz zero samples
    mgr.updateTelemetry(0.0f, 0.0f, 140, true, 100, true);
    for (uint32_t t = 5500; t <= 35000; t += 500) {
        mgr.tick(t);
        EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);
    }
    EXPECT_EQ(store.getRecordCount(), 35u);
    for (size_t i = 5; i < 35; ++i) {
        EXPECT_EQ(store.getRecords()[i].power_watts, 0u);
        EXPECT_EQ(store.getRecords()[i].cadence_rpm, 0u);
    }

    // 3. Resume pedalling for 5 seconds (t = 35500 .. 40000 ms) -> 5 records (200 W, 85 RPM)
    mgr.updateTelemetry(200.0f, 85.0f, 148, true, 100, true);
    for (uint32_t t = 35500; t <= 40000; t += 500) {
        mgr.tick(t);
    }
    EXPECT_EQ(store.getRecordCount(), 40u);

    // 4. Stop pedalling at the end for 4 seconds (t = 40500 .. 44000 ms) -> 4 trailing zero records
    mgr.updateTelemetry(0.0f, 0.0f, 138, true, 100, true);
    for (uint32_t t = 40500; t <= 44000; t += 500) {
        mgr.tick(t);
    }
    EXPECT_EQ(store.getRecordCount(), 44u);

    // 5. End session: trailing 4 zero-cadence records are stripped, but the 30 mid-ride coasting records remain!
    ASSERT_TRUE(mgr.endSession(44500));
    EXPECT_EQ(mgr.getLastSummary().duration_s, 40u);

    std::vector<RideRecord> savedRecords;
    RideSummary loadedSummary{};
    ASSERT_TRUE(store.loadStoredRide(mgr.getLastSummary().session_id, loadedSummary, savedRecords));
    ASSERT_EQ(savedRecords.size(), 40u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(savedRecords[i].cadence_rpm, 85u);
        EXPECT_EQ(savedRecords[i].power_watts, 200u);
    }
    for (size_t i = 5; i < 35; ++i) {
        EXPECT_EQ(savedRecords[i].cadence_rpm, 0u);
        EXPECT_EQ(savedRecords[i].power_watts, 0u);
    }
    for (size_t i = 35; i < 40; ++i) {
        EXPECT_EQ(savedRecords[i].cadence_rpm, 85u);
        EXPECT_EQ(savedRecords[i].power_watts, 200u);
    }
}

// =============================================================================
// 5. RideStore Metrics, Checkpointing, Crash Recovery & 5-Ride FIFO Eviction
// =============================================================================

TEST(RideStoreTest, Append100RecordsCheckpointsAndFinalizesSummary) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    ASSERT_TRUE(store.beginSession(1710000000u));

    // Append 100 records:
    // First 50 records: 200 W, 80 RPM, 140 BPM (valid HR)
    // Next 50 records:  300 W, 90 RPM, 160 BPM (valid HR)
    // Expected: avg_power = 250 W, max_power = 300 W, avg_cadence = 85 RPM,
    //           avg_hr = 150 BPM, max_hr = 160 BPM, total_work = 25,000 J = 25 kJ
    for (uint16_t i = 0; i < 100; ++i) {
        RideRecord r{};
        r.timestamp_offset_s  = i;
        r.power_watts         = (i < 50) ? 200u : 300u;
        r.cadence_rpm         = (i < 50) ? 80u : 90u;
        r.heart_rate_bpm      = (i < 50) ? 140u : 160u;
        r.internal_resistance = 120u;
        r.flags               = RIDE_FLAG_HR_VALID | RIDE_FLAG_RESISTANCE_CALIBRATED;
        ASSERT_TRUE(store.appendRecord(r));
    }

    // 100 records -> checkpoints at 30, 60, 90 (3 checkpoints)
    EXPECT_EQ(store.getCheckpointCount(), 3u);
    EXPECT_TRUE(fs.exists(RideStore::ACTIVE_FILE_PATH));

    RideSummary summary{};
    ASSERT_TRUE(store.finalizeSession(summary));
    EXPECT_FALSE(fs.exists(RideStore::ACTIVE_FILE_PATH));

    EXPECT_EQ(summary.start_epoch_s, 1710000000u);
    EXPECT_EQ(summary.duration_s, 100u);
    EXPECT_EQ(summary.avg_power_w, 250u);
    EXPECT_EQ(summary.max_power_w, 300u);
    EXPECT_EQ(summary.avg_cadence_rpm, 85u);
    EXPECT_EQ(summary.avg_hr_bpm, 150u);
    EXPECT_EQ(summary.max_hr_bpm, 160u);
    EXPECT_EQ(summary.total_work_kj, 25u);
}

TEST(RideStoreTest, CrashRecoveryDiscardsUnder60AndRecoversAtLeast60Records) {
    InMemoryRideFileSystem fs;

    // Case 1: Crash after 30 records (< 60 minimum) -> discarded on boot
    {
        RideStore writer(&fs);
        ASSERT_TRUE(writer.begin());
        ASSERT_TRUE(writer.beginSession(1710001000u));
        for (uint16_t i = 0; i < 30; ++i) {
            RideRecord r{i, 180, 80, 140, 100, RIDE_FLAG_HR_VALID};
            ASSERT_TRUE(writer.appendRecord(r));
        }
        EXPECT_TRUE(fs.exists(RideStore::ACTIVE_FILE_PATH));
    }
    {
        RideStore rebooted(&fs);
        ASSERT_TRUE(rebooted.begin());
        RideSummary recovered{};
        EXPECT_FALSE(rebooted.recoverInterruptedSession(60, &recovered));
        EXPECT_FALSE(fs.exists(RideStore::ACTIVE_FILE_PATH));
        EXPECT_EQ(rebooted.getCompletedRides().size(), 0u);
    }

    // Case 2: Crash after 60 records (>= 60 minimum) -> recovered and finalized on boot
    {
        RideStore writer(&fs);
        ASSERT_TRUE(writer.begin());
        ASSERT_TRUE(writer.beginSession(1710002000u));
        for (uint16_t i = 0; i < 60; ++i) {
            RideRecord r{i, 210, 85, 145, 110, RIDE_FLAG_HR_VALID | RIDE_FLAG_RESISTANCE_CALIBRATED};
            ASSERT_TRUE(writer.appendRecord(r));
        }
        EXPECT_TRUE(fs.exists(RideStore::ACTIVE_FILE_PATH));
    }
    {
        RideStore rebooted(&fs);
        ASSERT_TRUE(rebooted.begin());
        RideSummary recovered{};
        EXPECT_TRUE(rebooted.recoverInterruptedSession(60, &recovered));
        EXPECT_FALSE(fs.exists(RideStore::ACTIVE_FILE_PATH));
        EXPECT_EQ(recovered.start_epoch_s, 1710002000u);
        EXPECT_EQ(recovered.duration_s, 60u);
        EXPECT_EQ(recovered.avg_power_w, 210u);
        ASSERT_EQ(rebooted.getCompletedRides().size(), 1u);
    }
}

TEST(RideStoreTest, FiveRideFifoEvictionOnSixthCompletedRide) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    for (uint32_t rideIdx = 1; rideIdx <= 6; ++rideIdx) {
        ASSERT_TRUE(store.beginSession(1710000000u + rideIdx * 1000u));
        for (uint16_t s = 0; s < 10; ++s) {
            RideRecord r{s, static_cast<uint16_t>(100 + rideIdx * 10), 80, 140, 100, RIDE_FLAG_HR_VALID};
            ASSERT_TRUE(store.appendRecord(r));
        }
        RideSummary summary{};
        ASSERT_TRUE(store.finalizeSession(summary));
        EXPECT_EQ(summary.session_id, rideIdx);
    }

    // Only 5 most recent rides (sessions 2..6) remain; session 1 was evicted
    const auto& rides = store.getCompletedRides();
    ASSERT_EQ(rides.size(), 5u);
    EXPECT_EQ(rides[0].session_id, 2u);
    EXPECT_EQ(rides[1].session_id, 3u);
    EXPECT_EQ(rides[2].session_id, 4u);
    EXPECT_EQ(rides[3].session_id, 5u);
    EXPECT_EQ(rides[4].session_id, 6u);

    EXPECT_FALSE(fs.exists(RideStore::formatRidePath(1)));
    EXPECT_TRUE(fs.exists(RideStore::formatRidePath(2)));
    EXPECT_TRUE(fs.exists(RideStore::formatRidePath(6)));
}

// =============================================================================
// 7. SessionOverlay REST Handlers, Tab Registration & LiveFeed 13-Field JSON
// =============================================================================

namespace {

class MockTabRegistry : public ITabRegistry {
public:
    std::vector<TabEntry> tabs;
    void addTab(const char* name,
                const char* parent,
                int order = 100,
                const char* icon = nullptr,
                const char* script = nullptr) override {
        tabs.push_back(TabEntry{name, parent, order, icon, script});
    }
};

class OverlayMockHttpClient : public IHttpClient {
public:
    echenon::HttpResponse postForm(const std::string& /*url*/,
                                   const std::string& /*form_body*/,
                                   uint32_t /*timeout_ms*/) override {
        echenon::HttpResponse r;
        r.status_code = 200;
        r.body = "{\"access_token\":\"acc_tok_xyz\",\"refresh_token\":\"ref_tok_xyz\","
                 "\"expires_at\":1800000000,\"athlete\":{\"firstname\":\"Ada\",\"lastname\":\"Lovelace\"}}";
        return r;
    }

    echenon::HttpResponse postMultipart(const MultipartUploadRequest& /*req*/,
                                        uint32_t /*timeout_ms*/) override {
        return echenon::HttpResponse{201, "{\"id_str\":\"1001\"}", ""};
    }

    echenon::HttpResponse getJson(const std::string& /*url*/,
                                  const std::string& /*bearer_token*/,
                                  uint32_t /*timeout_ms*/) override {
        return echenon::HttpResponse{200, "{\"id_str\":\"1001\",\"activity_id\":9001}", ""};
    }
};

} // namespace

TEST(SessionOverlayTest, LifecycleStatusConfigHistoryDownloadAndUploadEndpoints) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());

    SessionManager mgr(store);
    mgr.begin();

    UploadQueue queue(store);
    ASSERT_TRUE(queue.begin());

    InMemorySecretStore secretStore;
    OverlayMockHttpClient httpClient;
    StravaUploader strava(&secretStore, &httpClient);
    ASSERT_TRUE(strava.begin());

    SessionOverlay overlay(&mgr, &store, &queue, &strava);

    // 1. Initial status is IDLE
    auto statusResp = overlay.handleGetStatus();
    EXPECT_EQ(statusResp.status_code, 200);
    {
        JsonDocument doc;
        ASSERT_FALSE(deserializeJson(doc, statusResp.body));
        EXPECT_STREQ(doc["state"].as<const char*>(), "IDLE");
        EXPECT_EQ(doc["session_id"].as<uint32_t>(), 0u);
    }

    // 2. Invalid pause while IDLE returns 409
    EXPECT_EQ(overlay.handlePause(1000).status_code, 409);

    // 3. Start -> Active -> 2s samples -> Pause -> Resume -> End
    auto startResp = overlay.handleStart(1710000000u, 1000u);
    EXPECT_EQ(startResp.status_code, 200);
    EXPECT_EQ(overlay.handleStart(1710000000u, 1100u).status_code, 409); // already active

    mgr.updateTelemetry(240.0f, 90.0f, 150, true, 100, true);
    mgr.tick(1500);
    mgr.tick(2000);
    mgr.tick(2500);
    mgr.tick(3000);

    auto pauseResp = overlay.handlePause(3000u);
    EXPECT_EQ(pauseResp.status_code, 200);
    EXPECT_EQ(mgr.getState(), SessionState::PAUSED);

    auto resumeResp = overlay.handleResume(5000u);
    EXPECT_EQ(resumeResp.status_code, 200);
    EXPECT_EQ(mgr.getState(), SessionState::ACTIVE);

    auto endResp = overlay.handleEnd(5000u);
    EXPECT_EQ(endResp.status_code, 200);
    {
        JsonDocument doc;
        ASSERT_FALSE(deserializeJson(doc, endResp.body));
        EXPECT_TRUE(doc["ok"].as<bool>());
        EXPECT_EQ(doc["session_id"].as<uint32_t>(), 1u);
        EXPECT_EQ(doc["avg_power_w"].as<uint16_t>(), 240u);
    }

    // 4. History includes completed ride #1 and auto-enqueued PENDING upload job
    auto histResp = overlay.handleGetHistory();
    EXPECT_EQ(histResp.status_code, 200);
    {
        JsonDocument doc;
        ASSERT_FALSE(deserializeJson(doc, histResp.body));
        JsonArray rides = doc["rides"].as<JsonArray>();
        ASSERT_EQ(rides.size(), 1u);
        EXPECT_EQ(rides[0]["session_id"].as<uint32_t>(), 1u);
        EXPECT_STREQ(rides[0]["upload_state"].as<const char*>(), "PENDING");
    }

    // 5. Download uncompressed TCX and compressed TCX.gz
    auto dlXml = overlay.handleDownload(1u, false);
    EXPECT_EQ(dlXml.status_code, 200);
    EXPECT_EQ(dlXml.content_type, "application/vnd.garmin.tcx+xml");
    EXPECT_EQ(dlXml.content_disposition, "attachment; filename=\"echenon_1.tcx\"");
    EXPECT_NE(dlXml.body.find("<TrainingCenterDatabase"), std::string::npos);
    EXPECT_NE(dlXml.body.find("<ns3:Watts>240</ns3:Watts>"), std::string::npos);

    auto dlGz = overlay.handleDownload(1u, true);
    EXPECT_EQ(dlGz.status_code, 200);
    EXPECT_EQ(dlGz.content_type, "application/gzip");
    EXPECT_EQ(dlGz.content_disposition, "attachment; filename=\"echenon_1.tcx.gz\"");
    ASSERT_GE(dlGz.body.size(), 18u);
    EXPECT_EQ(static_cast<uint8_t>(dlGz.body[0]), 0x1Fu);
    EXPECT_EQ(static_cast<uint8_t>(dlGz.body[1]), 0x8Bu);

    EXPECT_EQ(overlay.handleDownload(999u, false).status_code, 404);

    // 6. Manual upload trigger and config update
    EXPECT_EQ(overlay.handleUpload(1u, 6000u).status_code, 200);
    EXPECT_EQ(overlay.handleUpload(999u, 6000u).status_code, 404);

    auto cfgResp = overlay.handleConfig(
        "{\"auto_start_enabled\":false,\"auto_start_delay_sec\":5,"
        "\"inactivity_timeout_sec\":120,\"auto_upload_enabled\":false}");
    EXPECT_EQ(cfgResp.status_code, 200);
    EXPECT_FALSE(mgr.isAutoStartEnabled());
    EXPECT_EQ(mgr.getAutoStartDelaySec(), 5u);
    EXPECT_EQ(mgr.getInactivityTimeoutSec(), 120u);
    EXPECT_FALSE(queue.isAutoUploadEnabled());

    EXPECT_EQ(overlay.handleConfig("not-json").status_code, 400);

    // 7. Discard stored ride #1
    auto discResp = overlay.handleDiscard(1u, 7000u);
    EXPECT_EQ(discResp.status_code, 200);
    EXPECT_EQ(store.findStoredRide(1u), nullptr);
    EXPECT_EQ(overlay.handleDiscard(1u, 7000u).status_code, 404);
}

TEST(SessionOverlayTest, StravaCredentialsOAuthFlowTabsAndLiveFeedJson) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    SessionManager mgr(store);
    UploadQueue queue(store);
    InMemorySecretStore secretStore;
    OverlayMockHttpClient httpClient;
    StravaUploader strava(&secretStore, &httpClient);
    ASSERT_TRUE(strava.begin());

    SessionOverlay overlay(&mgr, &store, &queue, &strava);

    // 1. Save credentials (write-only secret never echoed)
    EXPECT_EQ(overlay.handleSaveStravaCredentials("{}").status_code, 400);
    auto saveResp = overlay.handleSaveStravaCredentials(
        "{\"client_id\":\"54321\",\"client_secret\":\"super_secret_val\"}");
    EXPECT_EQ(saveResp.status_code, 200);
    EXPECT_EQ(saveResp.body.find("super_secret_val"), std::string::npos);

    auto stResp = overlay.handleGetStravaStatus(1710000000u);
    EXPECT_EQ(stResp.status_code, 200);
    EXPECT_EQ(stResp.body.find("super_secret_val"), std::string::npos);

    // 2. Build OAuth authorize URL and validate CSRF state callback
    auto authUrlResp = overlay.handleGetStravaAuthUrl("http://echenon.local/api/strava/callback", 1000u);
    EXPECT_EQ(authUrlResp.status_code, 200);
    std::string stateToken = strava.getPendingState();
    ASSERT_FALSE(stateToken.empty());

    // Wrong state rejected with 403
    EXPECT_EQ(overlay.handleStravaCallback("code123", "bad_state", 2000u).status_code, 403);

    // Valid state exchanges code and authenticates athlete
    auto authUrlResp2 = overlay.handleGetStravaAuthUrl("http://echenon.local/api/strava/callback", 3000u);
    EXPECT_EQ(authUrlResp2.status_code, 200);
    stateToken = strava.getPendingState();
    auto cbResp = overlay.handleStravaCallback("valid_code_abc", stateToken, 4000u);
    EXPECT_EQ(cbResp.status_code, 200);
    EXPECT_TRUE(strava.isAuthenticated());
    EXPECT_EQ(strava.getAthleteName(), "Ada Lovelace");

    // Replaying consumed state rejected with 403
    EXPECT_EQ(overlay.handleStravaCallback("valid_code_abc", stateToken, 4001u).status_code, 403);

    // Disconnect clears tokens
    auto discStrava = overlay.handleStravaDisconnect();
    EXPECT_EQ(discStrava.status_code, 200);
    EXPECT_FALSE(strava.isAuthenticated());

    // 3. Verify portal tab registrations
    MockTabRegistry tabReg;
    overlay.registerTabs(tabReg);
    ASSERT_EQ(tabReg.tabs.size(), 2u);
    EXPECT_STREQ(tabReg.tabs[0].name, "Session");
    EXPECT_EQ(tabReg.tabs[0].order, 80);
    EXPECT_STREQ(tabReg.tabs[0].script, "/session.js");
    EXPECT_STREQ(tabReg.tabs[1].name, "Upload Services");
    EXPECT_EQ(tabReg.tabs[1].order, 90);
    EXPECT_STREQ(tabReg.tabs[1].script, "/upload_settings.js");

    // 4. Verify 13-field LiveFeed JSON includes session_state and session_elapsed_s
    std::string liveJson = SessionOverlay::formatLiveFeedJson(
        85.5f, 16, 100, 215.0f, 1450, 0, 0, false, 45, 31.2f, "ACTIVE", 125u, 98765u);
    JsonDocument liveDoc;
    ASSERT_FALSE(deserializeJson(liveDoc, liveJson));
    EXPECT_STREQ(liveDoc["session_state"].as<const char*>(), "ACTIVE");
    EXPECT_EQ(liveDoc["session_elapsed_s"].as<uint32_t>(), 125u);
    EXPECT_EQ(liveDoc["timestamp_ms"].as<uint32_t>(), 98765u);
}
