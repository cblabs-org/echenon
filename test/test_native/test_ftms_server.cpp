#include <gtest/gtest.h>
#include "echenon/FTMSData.h"
#include "echenon/PowerEngine.h"
#include <cmath>

// FTMSServer lifecycle test dependencies — native test pattern: include .cpp directly
// Native env has -I framework, so module paths must be prefixed with modules/
#include "modules/EventBus/EventBus.hpp"
#include "modules/EventBus/EventBus.cpp"
#include "echenon/HREvents.h"
#include "echenon/FTMSServer.h"
#include "echenon/FTMSServer.cpp"

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


// =============================================================================
// WO-05.2: FTMSServer Lifecycle Unit Tests
// =============================================================================

using namespace echenon;

// ---------------------------------------------------------------------------
// Construction & begin/end
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, ConstructionAndBegin) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    EXPECT_EQ(server.getNotifyCount(), 0u);

    server.end();
}

// ---------------------------------------------------------------------------
// updateTelemetry + tick encodes correct packet
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, UpdateTelemetryEncodesPacket) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    server.updateTelemetry(25.0f, 80.0f, 127, 200);
    server.tick(1000);

    uint8_t pkt[11] = {};
    server.getLastPacket(pkt);

    // Flags: 0x0264 → [0x64, 0x02]
    EXPECT_EQ(pkt[0], 0x64);
    EXPECT_EQ(pkt[1], 0x02);

    // Speed: 25.0 × 100 = 2500 → [0xC4, 0x09]
    EXPECT_EQ(pkt[2], 0xC4);
    EXPECT_EQ(pkt[3], 0x09);

    // Cadence: 80.0 × 2 = 160 → [0xA0, 0x00]
    EXPECT_EQ(pkt[4], 0xA0);
    EXPECT_EQ(pkt[5], 0x00);

    // Resistance: 127 → [0x7F, 0x00]
    EXPECT_EQ(pkt[6], 0x7F);
    EXPECT_EQ(pkt[7], 0x00);

    // Power: 200 → [0xC8, 0x00]
    EXPECT_EQ(pkt[8], 0xC8);
    EXPECT_EQ(pkt[9], 0x00);

    // HR: 0 (no HR subscription event yet)
    EXPECT_EQ(pkt[10], 0);

    EXPECT_EQ(server.getNotifyCount(), 1u);

    server.end();
}

// ---------------------------------------------------------------------------
// HR EventBus delivery
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, HREventBusDelivery) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    HrSample sample{72, HRContactState::ACTIVE, 1000};
    bus.publish(echenon::HR_TOPIC, &sample);

    EXPECT_EQ(server.getEffectiveHrBpm(1000), 72);

    server.end();
}

// ---------------------------------------------------------------------------
// HR staleness timeout
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, HRStalenessTimeout) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    HrSample sample{72, HRContactState::ACTIVE, 1000};
    bus.publish(echenon::HR_TOPIC, &sample);

    // Still fresh at 5999 ms (5999 - 1000 = 4999 ≤ 5000)
    EXPECT_EQ(server.getEffectiveHrBpm(5999), 72);

    // Exactly at boundary (6000 - 1000 = 5000 ≤ 5000) — still valid
    EXPECT_EQ(server.getEffectiveHrBpm(6000), 72);

    // Stale at 6001 ms (6001 - 1000 = 5001 > 5000)
    EXPECT_EQ(server.getEffectiveHrBpm(6001), 0);

    server.end();
}

// ---------------------------------------------------------------------------
// HR disconnection clears BPM
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, HRDisconnection) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    // First establish an active HR
    HrSample active{72, HRContactState::ACTIVE, 1000};
    bus.publish(echenon::HR_TOPIC, &active);
    EXPECT_EQ(server.getEffectiveHrBpm(1000), 72);

    // Now disconnect
    HrSample disc{0, HRContactState::DISCONNECTED, 2000};
    bus.publish(echenon::HR_TOPIC, &disc);
    EXPECT_EQ(server.getEffectiveHrBpm(2000), 0);

    server.end();
}

// ---------------------------------------------------------------------------
// Notification cadence (500 ms)
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, NotificationCadence) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    server.updateTelemetry(20.0f, 70.0f, 100, 150);

    // First tick at t=0 encodes packet (0 - 0 >= 500 is false if _lastNotifyMs starts at 0)
    // Actually: currentMillis(0) - _lastNotifyMs(0) = 0 < 500 → skipped
    // The first tick that fires must be at >= 500 ms OR on initial state
    // Let's check: _lastNotifyMs starts at 0, so tick(0): 0 - 0 = 0, which is < 500 → skip
    // tick(500): 500 - 0 = 500, which is >= 500 → fires!
    server.tick(0);
    EXPECT_EQ(server.getNotifyCount(), 0u);

    server.tick(499);
    EXPECT_EQ(server.getNotifyCount(), 0u);

    server.tick(500);
    EXPECT_EQ(server.getNotifyCount(), 1u);

    // Next notification at 1000
    server.tick(999);
    EXPECT_EQ(server.getNotifyCount(), 1u);

    server.tick(1000);
    EXPECT_EQ(server.getNotifyCount(), 2u);

    server.end();
}

// ---------------------------------------------------------------------------
// end() unsubscription — HR events no longer reach the server
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, EndUnsubscription) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    // Establish HR
    HrSample active{72, HRContactState::ACTIVE, 1000};
    bus.publish(echenon::HR_TOPIC, &active);
    EXPECT_EQ(server.getEffectiveHrBpm(1000), 72);

    // Unsubscribe
    server.end();

    // Publish new HR — should NOT reach server
    HrSample updated{120, HRContactState::ACTIVE, 2000};
    bus.publish(echenon::HR_TOPIC, &updated);

    // HR should still be 72 (not 120), and stale check still uses old timestamp
    EXPECT_EQ(server.getEffectiveHrBpm(2000), 72);
}

// ---------------------------------------------------------------------------
// HR included in packet when active
// ---------------------------------------------------------------------------

TEST(FTMSServerTest, HRIncludedInPacket) {
    EventBus bus;
    bus.begin();

    FTMSServer server(bus);
    server.begin();

    // Set telemetry and HR
    server.updateTelemetry(30.0f, 90.0f, 200, 300);
    HrSample sample{142, HRContactState::ACTIVE, 500};
    bus.publish(echenon::HR_TOPIC, &sample);

    // Tick at 500 ms (should fire notification)
    server.tick(500);

    uint8_t pkt[11] = {};
    server.getLastPacket(pkt);

    // HR byte should be 142
    EXPECT_EQ(pkt[10], 142);

    server.end();
}

// =============================================================================
// WO-05.3: Peloton Resistance & Virtual Speed Integration Tests
// =============================================================================

// ---------------------------------------------------------------------------
// Peloton resistance is a pure function of cam position, not cadence.
// It must produce non-zero, monotonically increasing values at 0 RPM.
// ---------------------------------------------------------------------------

TEST(PelotonResistanceTest, UpdatesAtZeroRPM) {
    // Peloton resistance is purely a function of normalized position,
    // NOT cadence. It should produce non-zero values even at 0 RPM.
    uint8_t r1 = echenon::PowerEngine::computePelotonResistance(1.0f);
    uint8_t r16 = echenon::PowerEngine::computePelotonResistance(16.0f);
    uint8_t r32 = echenon::PowerEngine::computePelotonResistance(32.0f);
    EXPECT_GT(r1, 0u);    // Level 1 should map to non-zero Peloton
    EXPECT_GT(r16, r1);   // Monotonically increasing
    EXPECT_GT(r32, r16);
}

TEST(PelotonResistanceTest, ZeroSpeedAtZeroPower) {
    float speed = echenon::PowerEngine::computeVirtualSpeedKph(0.0f);
    EXPECT_FLOAT_EQ(speed, 0.0f);
}

TEST(PelotonResistanceTest, PositiveSpeedAtPositivePower) {
    float speed = echenon::PowerEngine::computeVirtualSpeedKph(200.0f);
    EXPECT_GT(speed, 0.0f);
    EXPECT_LT(speed, 100.0f);  // Sanity: 200 W should not exceed 100 km/h
}
