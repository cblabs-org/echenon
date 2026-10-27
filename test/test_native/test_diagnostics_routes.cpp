/// @file test_diagnostics_routes.cpp
/// @brief Native unit tests for DiagnosticsRoutes helper functions:
///        diagEventTypeToString(), payload decode helpers, and formatPayloadJson().

#include <gtest/gtest.h>
#include "interfaces/ITickable.hpp"
#include "echenon/DiagnosticsModule.h"
// NOTE: DiagnosticsModule.cpp is unity-included by test_diagnostics_module.cpp
#include "echenon/RideStore.h"
// NOTE: RideStore.cpp is unity-included by test_session_manager.cpp
#include "echenon/RidePersistence.h"
// NOTE: RidePersistence.cpp is unity-included by test_ride_persistence.cpp
#include "echenon/DiagnosticsRoutes.h"
#include "echenon/DiagnosticsRoutes.cpp"

#include <cstring>

using namespace echenon;

// =============================================================================
// 1. Event Type to String — all 6 types + unknown
// =============================================================================

TEST(DiagnosticsRoutesTest, EventTypeToString) {
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::BOOT), "BOOT");
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::SESSION_START), "SESSION_START");
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::SESSION_END), "SESSION_END");
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::RUNAWAY_ALERT), "RUNAWAY_ALERT");
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::CALIBRATION_DONE), "CALIBRATION_DONE");
    EXPECT_STREQ(diagEventTypeToString(DiagEventType::FIRMWARE_CHANGE), "FIRMWARE_CHANGE");
    // Unknown type
    EXPECT_STREQ(diagEventTypeToString(static_cast<DiagEventType>(99)), "UNKNOWN");
}

// =============================================================================
// 2. Boot Payload Decode — decode boot event payload to uptime_s
// =============================================================================

TEST(DiagnosticsRoutesTest, BootPayloadDecode) {
    // Encode 3600 = 0x000E10 as LE: [0x10, 0x0E, 0x00]
    uint8_t payload[3] = {0x10, 0x0E, 0x00};
    EXPECT_EQ(decodeBootPayload(payload), 3600u);

    // Max 24-bit: 0xFFFFFF = 16,777,215
    uint8_t maxPayload[3] = {0xFF, 0xFF, 0xFF};
    EXPECT_EQ(decodeBootPayload(maxPayload), 0x00FFFFFFu);

    // Zero
    uint8_t zeroPayload[3] = {0, 0, 0};
    EXPECT_EQ(decodeBootPayload(zeroPayload), 0u);
}

// =============================================================================
// 3. Session End Payload Decode — duration_s and record_count_approx
// =============================================================================

TEST(DiagnosticsRoutesTest, SessionEndPayloadDecode) {
    // duration = 1800 (0x0708), record_count_approx = 28 * 64 = 1792
    // 1800 = 0x0708 → LE: [0x08, 0x07], coarse_records = 28
    uint8_t payload[3] = {0x08, 0x07, 28};
    uint16_t dur = 0, rec = 0;
    decodeSessionEndPayload(payload, dur, rec);
    EXPECT_EQ(dur, 1800u);
    EXPECT_EQ(rec, 1792u); // 28 * 64

    // Zero values
    uint8_t zeroPayload[3] = {0, 0, 0};
    decodeSessionEndPayload(zeroPayload, dur, rec);
    EXPECT_EQ(dur, 0u);
    EXPECT_EQ(rec, 0u);

    // Max duration (0xFFFF = 65535), max coarse (255 * 64 = 16320)
    uint8_t maxPayload[3] = {0xFF, 0xFF, 0xFF};
    decodeSessionEndPayload(maxPayload, dur, rec);
    EXPECT_EQ(dur, 0xFFFFu);
    EXPECT_EQ(rec, 255u * 64u);
}

// =============================================================================
// 4. Runaway Payload Decode — duration_ms from scaled value
// =============================================================================

TEST(DiagnosticsRoutesTest, RunawayPayloadDecode) {
    // Scaled = 150 → duration_ms = 15000
    // 150 = 0x0096 → LE: [0x96, 0x00]
    uint8_t payload[3] = {0x96, 0x00, 0x00};
    EXPECT_EQ(decodeRunawayPayload(payload), 15000u);

    // Max: 0xFFFF * 100 = 6,553,500
    uint8_t maxPayload[3] = {0xFF, 0xFF, 0x00};
    EXPECT_EQ(decodeRunawayPayload(maxPayload), 6553500u);

    // Zero
    uint8_t zeroPayload[3] = {0, 0, 0};
    EXPECT_EQ(decodeRunawayPayload(zeroPayload), 0u);
}

// =============================================================================
// 5. Firmware Change Payload Decode — major.minor.patch
// =============================================================================

TEST(DiagnosticsRoutesTest, FirmwareChangePayloadDecode) {
    uint8_t payload[3] = {2, 5, 12};
    uint8_t major = 0, minor = 0, patch = 0;
    decodeFirmwarePayload(payload, major, minor, patch);
    EXPECT_EQ(major, 2u);
    EXPECT_EQ(minor, 5u);
    EXPECT_EQ(patch, 12u);

    // Max values (255.255.255)
    uint8_t maxPayload[3] = {0xFF, 0xFF, 0xFF};
    decodeFirmwarePayload(maxPayload, major, minor, patch);
    EXPECT_EQ(major, 255u);
    EXPECT_EQ(minor, 255u);
    EXPECT_EQ(patch, 255u);
}

// =============================================================================
// 6. Calibration Payload Decode
// =============================================================================

TEST(DiagnosticsRoutesTest, CalibrationPayloadDecode) {
    uint8_t payload[3] = {0, 0, 0};
    EXPECT_EQ(decodeCalibrationPayload(payload), 0u); // "initial"

    uint8_t payload2[3] = {1, 0, 0};
    EXPECT_EQ(decodeCalibrationPayload(payload2), 1u); // "recalibration"
}

// =============================================================================
// 7. Session Start Payload Decode
// =============================================================================

TEST(DiagnosticsRoutesTest, SessionStartPayloadDecode) {
    // session_id = 42 → LE: [42, 0]
    uint8_t payload[3] = {42, 0, 0};
    EXPECT_EQ(decodeSessionStartPayload(payload), 42u);

    // session_id = 0xABCD → LE: [0xCD, 0xAB]
    uint8_t payload2[3] = {0xCD, 0xAB, 0};
    EXPECT_EQ(decodeSessionStartPayload(payload2), 0xABCDu);
}

// =============================================================================
// 8. Format Payload JSON — boot event
// =============================================================================

TEST(DiagnosticsRoutesTest, FormatPayloadJsonBoot) {
    uint8_t payload[3] = {0x10, 0x0E, 0x00}; // uptime = 3600
    char buf[128];
    int n = formatPayloadJson(DiagEventType::BOOT, payload, buf, sizeof(buf));
    EXPECT_GT(n, 0);
    EXPECT_NE(std::strstr(buf, "\"uptime_s\":3600"), nullptr);
}

// =============================================================================
// 9. Format Payload JSON — firmware change event
// =============================================================================

TEST(DiagnosticsRoutesTest, FormatPayloadJsonFirmware) {
    uint8_t payload[3] = {1, 2, 3}; // v1.2.3
    char buf[128];
    int n = formatPayloadJson(DiagEventType::FIRMWARE_CHANGE, payload, buf, sizeof(buf));
    EXPECT_GT(n, 0);
    EXPECT_NE(std::strstr(buf, "\"version\":\"1.2.3\""), nullptr);
}

// =============================================================================
// 10. Format Payload JSON — buffer too small returns 0
// =============================================================================

TEST(DiagnosticsRoutesTest, FormatPayloadJsonBufferTooSmall) {
    uint8_t payload[3] = {0x10, 0x0E, 0x00};
    char buf[5]; // too small
    int n = formatPayloadJson(DiagEventType::BOOT, payload, buf, sizeof(buf));
    EXPECT_EQ(n, 0);
}

// =============================================================================
// 11. Format Payload JSON — null buffer returns 0
// =============================================================================

TEST(DiagnosticsRoutesTest, FormatPayloadJsonNullBuffer) {
    uint8_t payload[3] = {0, 0, 0};
    int n = formatPayloadJson(DiagEventType::BOOT, payload, nullptr, 0);
    EXPECT_EQ(n, 0);
}
