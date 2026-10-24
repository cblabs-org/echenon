#include <gtest/gtest.h>
#include "echenon/CadenceSensor.h"
#include "echenon/ResistanceSensor.h"
#include "echenon/LevelMapping.h"
#include "echenon/CadenceSensor.cpp"
#include "echenon/ResistanceSensor.cpp"

using namespace echenon;

// =============================================================================
// CadenceSensor Unit Tests
// =============================================================================

TEST(CadenceSensorTest, InitialState) {
    CadenceSensor sensor(5);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 0u);
    EXPECT_FALSE(sensor.isActive());
}

TEST(CadenceSensorTest, SinglePulseDoesNotComputeRPM) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    sensor.tick(1000);

    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 0u);
    EXPECT_FALSE(sensor.isActive());
}

TEST(CadenceSensorTest, ValidTwoPulsesCompute120RPM) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    sensor.injectPulse(1500000); // 1.5s -> Δt = 500,000 µs
    sensor.tick(1500);

    EXPECT_NEAR(sensor.getRPM(), 120.0f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 500000u);
    EXPECT_TRUE(sensor.isActive());
}

TEST(CadenceSensorTest, ValidTwoPulsesCompute60RPM) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    sensor.injectPulse(2000000); // 2.0s -> Δt = 1,000,000 µs
    sensor.tick(2000);

    EXPECT_NEAR(sensor.getRPM(), 60.0f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 1000000u);
    EXPECT_TRUE(sensor.isActive());
}

TEST(CadenceSensorTest, DebounceLockout25ms) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s

    // Bounce pulse 10 ms later (< 25 ms debounce lockout)
    sensor.injectPulse(1010000);
    sensor.tick(1010);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Another bounce pulse 20 ms after the first (< 25 ms)
    sensor.injectPulse(1020000);
    sensor.tick(1020);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Valid second pulse 500 ms after the first pulse
    sensor.injectPulse(1500000);
    sensor.tick(1500);

    // Interval should be from 1000000 to 1500000 (500,000 µs), not corrupted by bounces
    EXPECT_NEAR(sensor.getRPM(), 120.0f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 500000u);
    EXPECT_TRUE(sensor.isActive());
}

TEST(CadenceSensorTest, ClampMaxRPM200) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    // 150 ms interval = 400 RPM (> 200 max)
    sensor.injectPulse(1150000);
    sensor.tick(1150);

    EXPECT_FLOAT_EQ(sensor.getRPM(), 200.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 150000u);
    EXPECT_TRUE(sensor.isActive());
}

TEST(CadenceSensorTest, TimeoutAfter3Seconds) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    sensor.injectPulse(1500000); // 1.5s
    sensor.tick(1500);

    EXPECT_TRUE(sensor.isActive());
    EXPECT_NEAR(sensor.getRPM(), 120.0f, 0.01f);

    // 2.9 seconds after last pulse (at t=4.4s) -> still active
    sensor.tick(4400);
    EXPECT_TRUE(sensor.isActive());
    EXPECT_NEAR(sensor.getRPM(), 120.0f, 0.01f);

    // 3.0 seconds after last pulse (at t=4.5s) -> timeout triggers
    sensor.tick(4500);
    EXPECT_FALSE(sensor.isActive());
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
}

TEST(CadenceSensorTest, ResumeAfterTimeoutDoesNotProduceSpuriousRPM) {
    CadenceSensor sensor(5);
    sensor.injectPulse(1000000); // 1.0s
    sensor.injectPulse(1500000); // 1.5s
    sensor.tick(1500);
    EXPECT_TRUE(sensor.isActive());

    // Timeout
    sensor.tick(5000);
    EXPECT_FALSE(sensor.isActive());
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Pedaling resumes at t = 10.0s (8.5s gap). This is the first edge of the new bout.
    sensor.injectPulse(10000000);
    sensor.tick(10000);
    EXPECT_FALSE(sensor.isActive());
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Second edge of new bout arrives at t = 10.6s (600 ms = 100 RPM)
    sensor.injectPulse(10600000);
    sensor.tick(10600);
    EXPECT_TRUE(sensor.isActive());
    EXPECT_NEAR(sensor.getRPM(), 100.0f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 600000u);
}

// =============================================================================
// ResistanceSensor Unit Tests
// =============================================================================

TEST(ResistanceSensorTest, InitialState) {
    ResistanceSensor sensor(1);
    EXPECT_EQ(sensor.getRawWiper(), 0u);
    EXPECT_EQ(sensor.getFilteredWiper(), 0u);
    EXPECT_FALSE(sensor.isInWorkingRange());
}

TEST(ResistanceSensorTest, FirstSampleSnapsFiltered) {
    ResistanceSensor sensor(1);
    sensor.injectSample(500);

    EXPECT_EQ(sensor.getRawWiper(), 500u);
    EXPECT_EQ(sensor.getFilteredWiper(), 500u);
    EXPECT_TRUE(sensor.isInWorkingRange());
}

TEST(ResistanceSensorTest, EMAConvergence) {
    ResistanceSensor sensor(1);
    sensor.reset(0.0f); // Start explicitly at 0

    // Inject sample 1000 50 times with alpha = 0.15
    for (int i = 0; i < 50; ++i) {
        sensor.injectSample(1000);
    }

    EXPECT_NEAR(sensor.getFilteredWiper(), 1000u, 1);
}

TEST(ResistanceSensorTest, WorkingRangeBoundaries) {
    ResistanceSensor sensor(1);

    // Exact min boundary (165)
    sensor.injectSample(LevelMapping::WORKING_RANGE_MIN);
    EXPECT_TRUE(sensor.isInWorkingRange());

    // Exact max boundary (1327)
    sensor.reset();
    sensor.injectSample(LevelMapping::WORKING_RANGE_MAX);
    EXPECT_TRUE(sensor.isInWorkingRange());

    // Just below min (164)
    sensor.reset();
    sensor.injectSample(LevelMapping::WORKING_RANGE_MIN - 1);
    EXPECT_FALSE(sensor.isInWorkingRange());

    // Just above max (1328)
    sensor.reset();
    sensor.injectSample(LevelMapping::WORKING_RANGE_MAX + 1);
    EXPECT_FALSE(sensor.isInWorkingRange());

    // Out of range extremes
    sensor.reset();
    sensor.injectSample(0);
    EXPECT_FALSE(sensor.isInWorkingRange());

    sensor.reset();
    sensor.injectSample(4095);
    EXPECT_FALSE(sensor.isInWorkingRange());
}
