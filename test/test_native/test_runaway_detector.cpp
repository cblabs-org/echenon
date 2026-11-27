/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_runaway_detector.cpp
 * Description: Native unit tests for RunawayDetector out-of-range detection and timeout triggering.
 */

#include <gtest/gtest.h>
#include "echenon/RunawayDetector.h"
#include "echenon/RunawayDetector.cpp"

using echenon::RunawayDetector;

// =============================================================================
// RunawayDetector Unit Tests
// =============================================================================

// 1. Initial state: not runaway, not triggered
TEST(RunawayDetectorTest, InitialState) {
    RunawayDetector rd;
    EXPECT_FALSE(rd.isRunaway());
    EXPECT_FALSE(rd.wasTriggered());
    EXPECT_EQ(rd.getRunawayDurationMs(), 0u);
    EXPECT_EQ(rd.getTimeoutSeconds(), RunawayDetector::DEFAULT_TIMEOUT_S);
}

// 2. In-range stays safe: tick with wiper=500 for 20 seconds → no runaway
TEST(RunawayDetectorTest, InRangeStaysSafe) {
    RunawayDetector rd;
    // Tick every 100 ms for 20 seconds (200 ticks)
    for (uint32_t t = 0; t <= 20000; t += 100) {
        bool edge = rd.tick(t, 500);
        EXPECT_FALSE(edge) << "Should not trigger at t=" << t;
    }
    EXPECT_FALSE(rd.isRunaway());
    EXPECT_FALSE(rd.wasTriggered());
    EXPECT_EQ(rd.getRunawayDurationMs(), 0u);
}

// 3. Out-of-range for < timeout: tick with wiper=100 for 5 seconds → no runaway
TEST(RunawayDetectorTest, OutOfRangeBelowTimeout) {
    RunawayDetector rd;
    for (uint32_t t = 0; t <= 5000; t += 100) {
        bool edge = rd.tick(t, 100);
        EXPECT_FALSE(edge) << "Should not trigger at t=" << t;
    }
    EXPECT_FALSE(rd.isRunaway());
    EXPECT_FALSE(rd.wasTriggered());
    // Duration should be > 0 since we're out of range
    EXPECT_GT(rd.getRunawayDurationMs(), 0u);
}

// 4. Out-of-range for >= timeout: tick with wiper=100 for 10+ seconds → runaway detected
TEST(RunawayDetectorTest, OutOfRangeExceedsTimeout) {
    RunawayDetector rd;
    bool detected = false;
    uint32_t detectionTime = 0;
    for (uint32_t t = 0; t <= 15000; t += 100) {
        bool edge = rd.tick(t, 100);
        if (edge && !detected) {
            detected = true;
            detectionTime = t;
        }
    }
    EXPECT_TRUE(detected);
    // Should detect at exactly 10000 ms (the first tick at or after timeout)
    EXPECT_EQ(detectionTime, 10000u);
    EXPECT_TRUE(rd.isRunaway());
    EXPECT_TRUE(rd.wasTriggered());
}

// 5. Return to range cancels: out of range for 5 s, then in range → timer resets
TEST(RunawayDetectorTest, ReturnToRangeCancelsTimer) {
    RunawayDetector rd;
    // Out of range for 5 seconds
    for (uint32_t t = 0; t <= 5000; t += 100) {
        rd.tick(t, 100);
    }
    EXPECT_FALSE(rd.isRunaway());

    // Return to range
    rd.tick(5100, 500);
    EXPECT_FALSE(rd.isRunaway());
    EXPECT_EQ(rd.getRunawayDurationMs(), 0u);

    // Go out of range again — needs another full 10 s
    for (uint32_t t = 5200; t <= 14000; t += 100) {
        bool edge = rd.tick(t, 100);
        EXPECT_FALSE(edge) << "Should not trigger at t=" << t << " (only 8.8s out of range)";
    }
    EXPECT_FALSE(rd.isRunaway());

    // Should trigger at t=15200 (10s after going out at t=5200)
    bool edge = rd.tick(15200, 100);
    EXPECT_TRUE(edge);
    EXPECT_TRUE(rd.isRunaway());
}

// 6. Edge detection: tick() returns true only on the first detection tick
TEST(RunawayDetectorTest, EdgeDetectionOnlyOnce) {
    RunawayDetector rd;
    int edgeCount = 0;
    for (uint32_t t = 0; t <= 20000; t += 100) {
        if (rd.tick(t, 100)) {
            edgeCount++;
        }
    }
    // Should detect exactly once (at the transition)
    EXPECT_EQ(edgeCount, 1);
    EXPECT_TRUE(rd.isRunaway());
}

// 7. wasTriggered latches: stays true even after wiper returns to range
TEST(RunawayDetectorTest, WasTriggeredLatches) {
    RunawayDetector rd;
    // Trigger runaway
    for (uint32_t t = 0; t <= 10000; t += 100) {
        rd.tick(t, 100);
    }
    EXPECT_TRUE(rd.wasTriggered());

    // Return to range
    rd.tick(10100, 500);
    EXPECT_FALSE(rd.isRunaway());   // Runaway cleared
    EXPECT_TRUE(rd.wasTriggered()); // But wasTriggered stays latched

    // Only explicit reset() clears it
    rd.reset();
    EXPECT_FALSE(rd.wasTriggered());
    EXPECT_FALSE(rd.isRunaway());
}

// 8. Boundary values: 165 in range, 164 out of range; 1327 in range, 1328 out
TEST(RunawayDetectorTest, BoundaryValues) {
    // Lower boundary: 165 is in range
    {
        RunawayDetector rd;
        for (uint32_t t = 0; t <= 15000; t += 100) {
            rd.tick(t, 165);
        }
        EXPECT_FALSE(rd.isRunaway()) << "Wiper=165 should be in range";
    }

    // Lower boundary: 164 is out of range
    {
        RunawayDetector rd;
        for (uint32_t t = 0; t <= 15000; t += 100) {
            rd.tick(t, 164);
        }
        EXPECT_TRUE(rd.isRunaway()) << "Wiper=164 should be out of range";
    }

    // Upper boundary: 1327 is in range
    {
        RunawayDetector rd;
        for (uint32_t t = 0; t <= 15000; t += 100) {
            rd.tick(t, 1327);
        }
        EXPECT_FALSE(rd.isRunaway()) << "Wiper=1327 should be in range";
    }

    // Upper boundary: 1328 is out of range
    {
        RunawayDetector rd;
        for (uint32_t t = 0; t <= 15000; t += 100) {
            rd.tick(t, 1328);
        }
        EXPECT_TRUE(rd.isRunaway()) << "Wiper=1328 should be out of range";
    }
}

// 9. Custom timeout: set timeout to 5 s, verify detection at 5 s instead of 10
TEST(RunawayDetectorTest, CustomTimeout) {
    RunawayDetector rd;
    rd.setTimeoutSeconds(5);
    EXPECT_EQ(rd.getTimeoutSeconds(), 5u);

    bool detected = false;
    uint32_t detectionTime = 0;
    for (uint32_t t = 0; t <= 8000; t += 100) {
        bool edge = rd.tick(t, 100);
        if (edge && !detected) {
            detected = true;
            detectionTime = t;
        }
    }
    EXPECT_TRUE(detected);
    EXPECT_EQ(detectionTime, 5000u);
    EXPECT_TRUE(rd.isRunaway());
}

// 10. Wiper at 0 (fully out of range) triggers after timeout
TEST(RunawayDetectorTest, WiperAtZero) {
    RunawayDetector rd;
    for (uint32_t t = 0; t <= 10000; t += 100) {
        rd.tick(t, 0);
    }
    EXPECT_TRUE(rd.isRunaway());
    EXPECT_TRUE(rd.wasTriggered());
}

// 11. Reset clears all state including duration tracking
TEST(RunawayDetectorTest, ResetClearsAllState) {
    RunawayDetector rd;
    // Trigger runaway
    for (uint32_t t = 0; t <= 12000; t += 100) {
        rd.tick(t, 100);
    }
    EXPECT_TRUE(rd.isRunaway());
    EXPECT_TRUE(rd.wasTriggered());
    EXPECT_GT(rd.getRunawayDurationMs(), 0u);

    rd.reset();

    EXPECT_FALSE(rd.isRunaway());
    EXPECT_FALSE(rd.wasTriggered());
    EXPECT_EQ(rd.getRunawayDurationMs(), 0u);
}
