/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_cadence_sensor.cpp
 * Description: Native unit tests for CadenceSensor pulse/RPM/debounce logic and ResistanceSensor filtering.
 */

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

TEST(CadenceSensorTest, HardwareReedSwitchRejectsOpeningEdgeGlitch) {
    CadenceSensor sensor(5);

    // Revolution 1: falling edge at t = 1,000,000 µs (1.0 s) — commits immediately
    sensor.onFallingEdgeISR(1000000);
    sensor.tick(1000);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Reed switch stays closed (LOW) for 150 ms; at t = 1,150,000 µs it opens (LOW -> HIGH)
    // and RC ramp threshold oscillation triggers a spurious FALLING interrupt
    sensor.processHardwareState(1100000, true);
    sensor.onFallingEdgeISR(1150000); // Spurious opening-edge ISR blocked by _waitingForOpen and MIN_HW_INTERVAL_US
    sensor.processHardwareState(1155000, false); // Line is now HIGH
    sensor.tick(1155);
    // Must NOT compute a 200 RPM spike!
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 0u);

    // Line stays continuously HIGH for >= 50 ms (re-arms at t = 1,205,000 µs)
    sensor.processHardwareState(1205000, false);

    // Revolution 2: falling edge at t = 2,460,000 µs (dt = 1,460,000 µs -> 41.0959 RPM)
    sensor.onFallingEdgeISR(2460000);
    sensor.tick(2460);
    EXPECT_NEAR(sensor.getRPM(), 41.10f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 1460000u);

    // Another opening-edge glitch 150 ms into Revolution 2 (at t = 2,610,000 µs)
    sensor.onFallingEdgeISR(2610000);
    sensor.processHardwareState(2615000, false);
    sensor.tick(2615);
    // RPM remains ~41.10 RPM and never spikes to 200.0 RPM
    EXPECT_NEAR(sensor.getRPM(), 41.10f, 0.01f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 1460000u);
}

TEST(CadenceSensorTest, HardwareSustainedClosureBlocksOpeningGlitchBeyond250ms) {
    CadenceSensor sensor(5);

    // Revolution 1: falling edge at t = 1,000,000 µs (1.0 s)
    sensor.onFallingEdgeISR(1000000);
    sensor.tick(1000);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);

    // Very slow pedalling: reed switch stays closed (LOW) for 350 ms (> 250 ms MIN_HW_INTERVAL_US)
    sensor.processHardwareState(1200000, true);
    sensor.processHardwareState(1340000, true);

    // Opening-edge RC glitch at t = 1,350,000 µs (dt = 350 ms >= 250 ms, but _waitingForOpen is still true)
    sensor.onFallingEdgeISR(1350000);
    sensor.processHardwareState(1355000, false); // Line opens HIGH
    sensor.tick(1355);
    // Must be rejected by _waitingForOpen!
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 0u);

    // Line stays HIGH for >= 50 ms -> re-arms at t = 1,405,000 µs
    sensor.processHardwareState(1405000, false);

    // Genuine second revolution at t = 3,000,000 µs (dt = 2,000,000 µs -> 30.0 RPM)
    sensor.onFallingEdgeISR(3000000);
    sensor.tick(3000);
    EXPECT_EQ(sensor.getLastPulseInterval(), 2000000u);
    EXPECT_NEAR(sensor.getRPM(), 30.0f, 0.01f);
}

TEST(CadenceSensorTest, HardwareClosureBouncePreservesLeadingEdgeTimestamp) {
    CadenceSensor sensor(5);

    // First revolution with contact bounces at +1 ms and +2 ms
    sensor.onFallingEdgeISR(1000000);
    sensor.onFallingEdgeISR(1001000);
    sensor.onFallingEdgeISR(1002000);
    sensor.tick(1002);

    // Release and re-arm (50 ms HIGH)
    sensor.processHardwareState(1100000, false);
    sensor.processHardwareState(1150000, false);

    // Second revolution at t = 2,000,000 µs with contact bounces at +1 ms and +2 ms
    sensor.onFallingEdgeISR(2000000);
    sensor.onFallingEdgeISR(2001000);
    sensor.onFallingEdgeISR(2002000);
    sensor.tick(2002);

    // Exact leading-edge interval: 2,000,000 - 1,000,000 = 1,000,000 µs (60.0 RPM)
    EXPECT_EQ(sensor.getLastPulseInterval(), 1000000u);
    EXPECT_NEAR(sensor.getRPM(), 60.0f, 0.01f);
}

TEST(CadenceSensorTest, HardwareCalibrationRig30msPulseCompatibility) {
    CadenceSensor sensor(5);

    // CalibrationRig drives a 30 ms LOW pulse every 750,000 µs (80.0 RPM)
    // Pulse 1 at t = 1,000,000 µs (commits immediately on falling edge)
    sensor.onFallingEdgeISR(1000000);
    sensor.tick(1000);
    sensor.processHardwareState(1030000, false); // +30 ms: rig releases pin HIGH
    sensor.processHardwareState(1080000, false); // +80 ms (50 ms HIGH): re-armed

    // Pulse 2 at t = 1,750,000 µs (+750 ms)
    sensor.onFallingEdgeISR(1750000);
    sensor.tick(1750);

    EXPECT_EQ(sensor.getLastPulseInterval(), 750000u);
    EXPECT_NEAR(sensor.getRPM(), 80.0f, 0.01f);
}

TEST(CadenceSensorTest, HardwareMinInterval250msLockout) {
    CadenceSensor sensor(5);

    // Pulse 1 at t = 1,000,000 µs
    sensor.onFallingEdgeISR(1000000);
    sensor.tick(1000);

    // Short 10 ms LOW pulse, then 50 ms HIGH -> re-armed at t = 1,060,000 µs
    sensor.processHardwareState(1010000, false);
    sensor.processHardwareState(1060000, false);

    // Falling edge at t = 1,150,000 µs (dt = 150 ms < 250 ms MIN_HW_INTERVAL_US) -> rejected!
    sensor.onFallingEdgeISR(1150000);
    sensor.tick(1150);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_EQ(sensor.getLastPulseInterval(), 0u);

    // Valid second pulse at t = 1,500,000 µs (dt = 500 ms -> 120.0 RPM)
    sensor.onFallingEdgeISR(1500000);
    sensor.tick(1500);
    EXPECT_EQ(sensor.getLastPulseInterval(), 500000u);
    EXPECT_NEAR(sensor.getRPM(), 120.0f, 0.01f);
}

TEST(CadenceSensorTest, MicrosWraparoundAfter71MinutesPreservesRPMAndTimeout) {
    CadenceSensor sensor(5);

    // Simulate uptime = 9,237,153 ms (~2.56 hours, past two 71.58-minute uint32_t micros() wraparounds),
    // where uint32_t pulseMicros has wrapped around to 647,218,000 µs (~647.2 s).
    sensor.onFallingEdgeISR(647218000u);
    sensor.tick(9237153u);
    sensor.processHardwareState(647248000u, false); // +30 ms HIGH
    sensor.processHardwareState(647298000u, false); // +80 ms (50 ms HIGH): re-armed

    // Second pulse 750,000 µs later (80.0 RPM) at currentMillis = 9,237,903 ms
    sensor.onFallingEdgeISR(647968000u);
    sensor.tick(9237903u);
    EXPECT_EQ(sensor.getLastPulseInterval(), 750000u);
    EXPECT_NEAR(sensor.getRPM(), 80.0f, 0.01f);
    EXPECT_TRUE(sensor.isActive());

    // Subsequent main-loop ticks 500 ms and 2900 ms later must NOT false-timeout
    sensor.tick(9238403u);
    EXPECT_NEAR(sensor.getRPM(), 80.0f, 0.01f);
    sensor.tick(9240803u);
    EXPECT_NEAR(sensor.getRPM(), 80.0f, 0.01f);

    // Genuine 3000 ms timeout at currentMillis = 9,240,903 ms
    sensor.tick(9240903u);
    EXPECT_FLOAT_EQ(sensor.getRPM(), 0.0f);
    EXPECT_FALSE(sensor.isActive());
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
