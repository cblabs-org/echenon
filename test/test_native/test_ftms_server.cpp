#include <gtest/gtest.h>
#include "echenon/FTMSData.h"
#include <cmath>

using namespace echenon;

// =============================================================================
// WO-05.1: FTMSData Packet Encoder Unit Tests
// =============================================================================

// ---------------------------------------------------------------------------
// encodeFitnessMachineFeature
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeFitnessMachineFeature_DefaultV1) {
    uint8_t buf[8] = {};
    encodeFitnessMachineFeature(buf);

    // FTMS_MACHINE_FEATURES_V1 = 0x00004482 → little-endian: [0x82, 0x44, 0x00, 0x00]
    EXPECT_EQ(buf[0], 0x82);
    EXPECT_EQ(buf[1], 0x44);
    EXPECT_EQ(buf[2], 0x00);
    EXPECT_EQ(buf[3], 0x00);
    // FTMS_TARGET_FEATURES_V1 = 0x00000000 → [0x00, 0x00, 0x00, 0x00]
    EXPECT_EQ(buf[4], 0x00);
    EXPECT_EQ(buf[5], 0x00);
    EXPECT_EQ(buf[6], 0x00);
    EXPECT_EQ(buf[7], 0x00);
}

TEST(FTMSDataTest, EncodeFitnessMachineFeature_CustomValues) {
    uint8_t buf[8] = {};
    encodeFitnessMachineFeature(buf, 0xDEADBEEFu, 0xCAFEBABEu);

    EXPECT_EQ(buf[0], 0xEF);
    EXPECT_EQ(buf[1], 0xBE);
    EXPECT_EQ(buf[2], 0xAD);
    EXPECT_EQ(buf[3], 0xDE);
    EXPECT_EQ(buf[4], 0xBE);
    EXPECT_EQ(buf[5], 0xBA);
    EXPECT_EQ(buf[6], 0xFE);
    EXPECT_EQ(buf[7], 0xCA);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Normal Riding
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_NormalRiding) {
    IndoorBikeTelemetry t;
    t.speed_kph           = 25.5f;    // → 2550 = 0x09F6
    t.cadence_rpm         = 85.0f;    // → 170 = 0x00AA
    t.internal_resistance = 127;      // → 127 = 0x007F
    t.power_watts         = 200;      // → 200 = 0x00C8
    t.heart_rate_bpm      = 142;

    uint8_t buf[11] = {};
    size_t len = encodeIndoorBikeData(t, buf);
    EXPECT_EQ(len, 11u);

    // Flags: 0x0264 → [0x64, 0x02]
    EXPECT_EQ(buf[0], 0x64);
    EXPECT_EQ(buf[1], 0x02);

    // Speed: 25.5 × 100 = 2550 → [0xF6, 0x09]
    EXPECT_EQ(buf[2], 0xF6);
    EXPECT_EQ(buf[3], 0x09);

    // Cadence: 85.0 × 2 = 170 → [0xAA, 0x00]
    EXPECT_EQ(buf[4], 0xAA);
    EXPECT_EQ(buf[5], 0x00);

    // Resistance: 127 → [0x7F, 0x00]
    EXPECT_EQ(buf[6], 0x7F);
    EXPECT_EQ(buf[7], 0x00);

    // Power: 200 → [0xC8, 0x00]
    EXPECT_EQ(buf[8], 0xC8);
    EXPECT_EQ(buf[9], 0x00);

    // Heart Rate: 142
    EXPECT_EQ(buf[10], 142);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Coasting (0 RPM, 0 W)
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_Coasting) {
    IndoorBikeTelemetry t;
    t.speed_kph           = 0.0f;
    t.cadence_rpm         = 0.0f;
    t.internal_resistance = 100;
    t.power_watts         = 0;
    t.heart_rate_bpm      = 75;

    uint8_t buf[11] = {};
    encodeIndoorBikeData(t, buf);

    // Speed: 0 → [0x00, 0x00]
    EXPECT_EQ(buf[2], 0x00);
    EXPECT_EQ(buf[3], 0x00);

    // Cadence: 0 → [0x00, 0x00]
    EXPECT_EQ(buf[4], 0x00);
    EXPECT_EQ(buf[5], 0x00);

    // Resistance: 100 → [0x64, 0x00]
    EXPECT_EQ(buf[6], 0x64);
    EXPECT_EQ(buf[7], 0x00);

    // Power: 0 → [0x00, 0x00]
    EXPECT_EQ(buf[8], 0x00);
    EXPECT_EQ(buf[9], 0x00);

    // HR: 75
    EXPECT_EQ(buf[10], 75);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Uncalibrated Resistance (255 → 0)
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_UncalibratedResistance) {
    IndoorBikeTelemetry t;
    t.speed_kph           = 10.0f;
    t.cadence_rpm         = 60.0f;
    t.internal_resistance = 255;  // UNCALIBRATED sentinel
    t.power_watts         = 50;
    t.heart_rate_bpm      = 0;

    uint8_t buf[11] = {};
    encodeIndoorBikeData(t, buf);

    // Resistance: 255 → encode as 0 → [0x00, 0x00]
    EXPECT_EQ(buf[6], 0x00);
    EXPECT_EQ(buf[7], 0x00);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Missing HR (0 BPM)
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_MissingHR) {
    IndoorBikeTelemetry t;
    t.speed_kph           = 30.0f;
    t.cadence_rpm         = 90.0f;
    t.internal_resistance = 200;
    t.power_watts         = 300;
    t.heart_rate_bpm      = 0;   // No HR strap

    uint8_t buf[11] = {};
    encodeIndoorBikeData(t, buf);

    // HR: 0
    EXPECT_EQ(buf[10], 0);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Power Clamping Bounds
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_PowerClampingBounds) {
    IndoorBikeTelemetry t;
    t.speed_kph           = 20.0f;
    t.cadence_rpm         = 80.0f;
    t.internal_resistance = 100;
    t.heart_rate_bpm      = 100;

    uint8_t buf[11] = {};

    // Negative power should clamp to 0
    t.power_watts = -50;
    encodeIndoorBikeData(t, buf);
    // int16 LE of 0: [0x00, 0x00]
    EXPECT_EQ(buf[8], 0x00);
    EXPECT_EQ(buf[9], 0x00);

    // Power at max (4000 W)
    t.power_watts = 4000;
    encodeIndoorBikeData(t, buf);
    // 4000 = 0x0FA0 → [0xA0, 0x0F]
    EXPECT_EQ(buf[8], 0xA0);
    EXPECT_EQ(buf[9], 0x0F);

    // Power above max (5000 W) clamps to 4000
    t.power_watts = 5000;
    encodeIndoorBikeData(t, buf);
    EXPECT_EQ(buf[8], 0xA0);
    EXPECT_EQ(buf[9], 0x0F);
}

// ---------------------------------------------------------------------------
// encodeIndoorBikeData — Speed Clamping
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, EncodeIndoorBikeData_SpeedClamping) {
    IndoorBikeTelemetry t;
    t.cadence_rpm         = 80.0f;
    t.internal_resistance = 100;
    t.power_watts         = 200;
    t.heart_rate_bpm      = 100;

    uint8_t buf[11] = {};

    // Negative speed should clamp to 0
    t.speed_kph = -5.0f;
    encodeIndoorBikeData(t, buf);
    EXPECT_EQ(buf[2], 0x00);
    EXPECT_EQ(buf[3], 0x00);

    // Max speed 655.35 km/h → 65535 = 0xFFFF
    t.speed_kph = 655.35f;
    encodeIndoorBikeData(t, buf);
    EXPECT_EQ(buf[2], 0xFF);
    EXPECT_EQ(buf[3], 0xFF);

    // Above max clamps
    t.speed_kph = 700.0f;
    encodeIndoorBikeData(t, buf);
    EXPECT_EQ(buf[2], 0xFF);
    EXPECT_EQ(buf[3], 0xFF);
}

// ---------------------------------------------------------------------------
// Constants sanity
// ---------------------------------------------------------------------------

TEST(FTMSDataTest, Constants) {
    EXPECT_EQ(FTMS_SERVICE_UUID, 0x1826);
    EXPECT_EQ(FTMS_FEATURE_CHAR_UUID, 0x2ACC);
    EXPECT_EQ(FTMS_INDOOR_BIKE_DATA_UUID, 0x2AD2);
    EXPECT_EQ(FTMS_CONTROL_POINT_UUID, 0x2AD9);
    EXPECT_EQ(FTMS_MACHINE_FEATURES_V1, 0x00004482u);
    EXPECT_EQ(FTMS_TARGET_FEATURES_V1, 0x00000000u);
    EXPECT_EQ(FTMS_INDOOR_BIKE_FLAGS_V1, 0x0264);
    EXPECT_EQ(FTMS_FEATURE_PACKET_SIZE, 8u);
    EXPECT_EQ(FTMS_INDOOR_BIKE_PACKET_SIZE, 11u);
}
