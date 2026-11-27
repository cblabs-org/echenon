/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_diagnostics_module.cpp
 * Description: Native unit tests for DiagnosticsModule event recording, ring buffer, and query functionality.
 */

/// @file test_diagnostics_module.cpp
/// @brief Native unit tests for DiagnosticsModule event recording, ring buffer,
///        and query functionality.

#include <gtest/gtest.h>
#include "interfaces/ITickable.hpp"
#include "echenon/DiagnosticsModule.h"
#include "echenon/DiagnosticsModule.cpp"

using namespace echenon;

// =============================================================================
// 1. Lifecycle — begin() allocates buffer, getEventCount() starts at 0
// =============================================================================

TEST(DiagnosticsModuleTest, BeginAllocatesBuffer) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());
    EXPECT_EQ(mod.getEventCount(), 0u);
    mod.end();
}

// =============================================================================
// 2. Boot Event Recording — uptime encoding in 24-bit LE payload
// =============================================================================

TEST(DiagnosticsModuleTest, RecordBootEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordBoot(1710000000u, 3600u);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->epoch_s, 1710000000u);
    EXPECT_EQ(evt->type, DiagEventType::BOOT);

    // Decode 24-bit LE uptime: 3600 = 0x000E10
    uint32_t uptime = static_cast<uint32_t>(evt->payload[0])
                    | (static_cast<uint32_t>(evt->payload[1]) << 8)
                    | (static_cast<uint32_t>(evt->payload[2]) << 16);
    EXPECT_EQ(uptime, 3600u);

    mod.end();
}

// =============================================================================
// 3. Session Start Event — session_id in low 16-bit LE payload
// =============================================================================

TEST(DiagnosticsModuleTest, RecordSessionStartEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordSessionStart(1710001000u, 42u);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->type, DiagEventType::SESSION_START);
    EXPECT_EQ(evt->epoch_s, 1710001000u);

    uint32_t sid = static_cast<uint32_t>(evt->payload[0])
                 | (static_cast<uint32_t>(evt->payload[1]) << 8);
    EXPECT_EQ(sid, 42u);
    EXPECT_EQ(evt->payload[2], 0u);

    mod.end();
}

// =============================================================================
// 4. Session End Event — duration + record_count/64 payload
// =============================================================================

TEST(DiagnosticsModuleTest, RecordSessionEndEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordSessionEnd(1710002000u, 1800u, 1800u);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->type, DiagEventType::SESSION_END);

    uint16_t dur = static_cast<uint16_t>(evt->payload[0])
                 | (static_cast<uint16_t>(evt->payload[1]) << 8);
    EXPECT_EQ(dur, 1800u);
    // 1800 / 64 = 28 (truncated)
    EXPECT_EQ(evt->payload[2], 28u);

    mod.end();
}

// =============================================================================
// 5. Runaway Alert Event — duration_ms/100 as 16-bit LE
// =============================================================================

TEST(DiagnosticsModuleTest, RecordRunawayAlertEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordRunawayAlert(1710003000u, 15000u);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->type, DiagEventType::RUNAWAY_ALERT);

    uint32_t scaled = static_cast<uint32_t>(evt->payload[0])
                    | (static_cast<uint32_t>(evt->payload[1]) << 8);
    EXPECT_EQ(scaled, 150u); // 15000 / 100 = 150
    EXPECT_EQ(evt->payload[2], 0u);

    mod.end();
}

// =============================================================================
// 6. Calibration Done Event — calibration_type in payload[0]
// =============================================================================

TEST(DiagnosticsModuleTest, RecordCalibrationDoneEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordCalibrationDone(1710004000u, 1u);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->type, DiagEventType::CALIBRATION_DONE);
    EXPECT_EQ(evt->payload[0], 1u);
    EXPECT_EQ(evt->payload[1], 0u);
    EXPECT_EQ(evt->payload[2], 0u);

    mod.end();
}

// =============================================================================
// 7. Firmware Change Event — major/minor/patch in payload[0..2]
// =============================================================================

TEST(DiagnosticsModuleTest, RecordFirmwareChangeEvent) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordFirmwareChange(1710005000u, 2, 5, 12);
    ASSERT_EQ(mod.getEventCount(), 1u);

    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    EXPECT_EQ(evt->type, DiagEventType::FIRMWARE_CHANGE);
    EXPECT_EQ(evt->payload[0], 2u);
    EXPECT_EQ(evt->payload[1], 5u);
    EXPECT_EQ(evt->payload[2], 12u);

    mod.end();
}

// =============================================================================
// 8. Ring Buffer Wraps — oldest event evicted when > capacity
// =============================================================================

TEST(DiagnosticsModuleTest, RingBufferWraps) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    // Fill to capacity
    for (size_t i = 0; i < DiagnosticsModule::EVENT_LOG_CAPACITY; ++i) {
        mod.recordBoot(static_cast<uint32_t>(1000 + i), static_cast<uint32_t>(i));
    }
    EXPECT_EQ(mod.getEventCount(), DiagnosticsModule::EVENT_LOG_CAPACITY);

    // Oldest event should have epoch 1000
    const DiagnosticEvent* oldest = mod.getEvent(0);
    ASSERT_NE(oldest, nullptr);
    EXPECT_EQ(oldest->epoch_s, 1000u);

    // Add one more — oldest (epoch 1000) should be evicted
    mod.recordBoot(2000u, 999u);
    EXPECT_EQ(mod.getEventCount(), DiagnosticsModule::EVENT_LOG_CAPACITY);

    const DiagnosticEvent* newOldest = mod.getEvent(0);
    ASSERT_NE(newOldest, nullptr);
    EXPECT_EQ(newOldest->epoch_s, 1001u);

    // Newest should be our just-added event
    const DiagnosticEvent* newest = mod.getEvent(DiagnosticsModule::EVENT_LOG_CAPACITY - 1);
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(newest->epoch_s, 2000u);

    mod.end();
}

// =============================================================================
// 9. getEvent Returns Oldest First — ordering after wrap
// =============================================================================

TEST(DiagnosticsModuleTest, GetEventReturnsOldestFirst) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    // Record 3 events with different epochs
    mod.recordBoot(100u, 0u);
    mod.recordBoot(200u, 0u);
    mod.recordBoot(300u, 0u);

    ASSERT_EQ(mod.getEventCount(), 3u);
    EXPECT_EQ(mod.getEvent(0)->epoch_s, 100u);
    EXPECT_EQ(mod.getEvent(1)->epoch_s, 200u);
    EXPECT_EQ(mod.getEvent(2)->epoch_s, 300u);

    // Out-of-bounds returns nullptr
    EXPECT_EQ(mod.getEvent(3), nullptr);

    mod.end();
}

// =============================================================================
// 10. clearEvents Resets Count
// =============================================================================

TEST(DiagnosticsModuleTest, ClearEventsResetsCount) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordBoot(100u, 0u);
    mod.recordBoot(200u, 0u);
    EXPECT_EQ(mod.getEventCount(), 2u);

    mod.clearEvents();
    EXPECT_EQ(mod.getEventCount(), 0u);
    EXPECT_EQ(mod.getEvent(0), nullptr);

    // Can record again after clear
    mod.recordBoot(300u, 0u);
    EXPECT_EQ(mod.getEventCount(), 1u);
    EXPECT_EQ(mod.getEvent(0)->epoch_s, 300u);

    mod.end();
}

// =============================================================================
// 11. queryEvents with Time Filter
// =============================================================================

TEST(DiagnosticsModuleTest, QueryEventsTimeFilter) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordBoot(1000u, 0u);
    mod.recordBoot(2000u, 0u);
    mod.recordBoot(3000u, 0u);
    mod.recordBoot(4000u, 0u);
    mod.recordBoot(5000u, 0u);
    ASSERT_EQ(mod.getEventCount(), 5u);

    // Query events between 2000 and 4000 inclusive
    EventQuery query;
    query.from_epoch = 2000u;
    query.to_epoch   = 4000u;
    query.limit      = 100;

    DiagnosticEvent results[10];
    size_t count = mod.queryEvents(query, results, 10);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(results[0].epoch_s, 2000u);
    EXPECT_EQ(results[1].epoch_s, 3000u);
    EXPECT_EQ(results[2].epoch_s, 4000u);

    mod.end();
}

// =============================================================================
// 12. queryEvents with Limit Filter
// =============================================================================

TEST(DiagnosticsModuleTest, QueryEventsLimitFilter) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    for (uint32_t i = 0; i < 10; ++i) {
        mod.recordBoot(1000u + i * 100u, 0u);
    }
    ASSERT_EQ(mod.getEventCount(), 10u);

    EventQuery query;
    query.limit = 3;

    DiagnosticEvent results[10];
    size_t count = mod.queryEvents(query, results, 10);
    EXPECT_EQ(count, 3u);
    // Should return first 3 (oldest)
    EXPECT_EQ(results[0].epoch_s, 1000u);
    EXPECT_EQ(results[1].epoch_s, 1100u);
    EXPECT_EQ(results[2].epoch_s, 1200u);

    mod.end();
}

// =============================================================================
// 13. queryEvents with Empty Range — no events in range
// =============================================================================

TEST(DiagnosticsModuleTest, QueryEventsEmptyRange) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    mod.recordBoot(1000u, 0u);
    mod.recordBoot(2000u, 0u);

    EventQuery query;
    query.from_epoch = 5000u;
    query.to_epoch   = 6000u;
    query.limit      = 100;

    DiagnosticEvent results[10];
    size_t count = mod.queryEvents(query, results, 10);
    EXPECT_EQ(count, 0u);

    mod.end();
}

// =============================================================================
// 14. Event Payload 24-Bit Uptime — max 24-bit value encoding + clamping
// =============================================================================

TEST(DiagnosticsModuleTest, EventPayload24BitUptime) {
    DiagnosticsModule mod;
    ASSERT_TRUE(mod.begin());

    // Test max 24-bit value (0xFFFFFF = 16,777,215)
    mod.recordBoot(100u, 0x00FFFFFFu);
    const DiagnosticEvent* evt = mod.getEvent(0);
    ASSERT_NE(evt, nullptr);
    uint32_t uptime = static_cast<uint32_t>(evt->payload[0])
                    | (static_cast<uint32_t>(evt->payload[1]) << 8)
                    | (static_cast<uint32_t>(evt->payload[2]) << 16);
    EXPECT_EQ(uptime, 0x00FFFFFFu);

    // Test clamping: value > 0xFFFFFF should be clamped to 0xFFFFFF
    mod.recordBoot(200u, 0x01000000u);
    const DiagnosticEvent* evt2 = mod.getEvent(1);
    ASSERT_NE(evt2, nullptr);
    uint32_t uptime2 = static_cast<uint32_t>(evt2->payload[0])
                     | (static_cast<uint32_t>(evt2->payload[1]) << 8)
                     | (static_cast<uint32_t>(evt2->payload[2]) << 16);
    EXPECT_EQ(uptime2, 0x00FFFFFFu);

    // Test zero uptime
    mod.recordBoot(300u, 0u);
    const DiagnosticEvent* evt3 = mod.getEvent(2);
    ASSERT_NE(evt3, nullptr);
    uint32_t uptime3 = static_cast<uint32_t>(evt3->payload[0])
                     | (static_cast<uint32_t>(evt3->payload[1]) << 8)
                     | (static_cast<uint32_t>(evt3->payload[2]) << 16);
    EXPECT_EQ(uptime3, 0u);

    mod.end();
}
