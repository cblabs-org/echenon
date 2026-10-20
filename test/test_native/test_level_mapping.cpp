#include <gtest/gtest.h>
#include "echenon/LevelMapping.h"
#include "echenon/LevelMapping.cpp"
#include <cmath>

using namespace LevelMapping;

// =============================================================================
// LevelMapping Unit Tests
// =============================================================================

TEST(LevelMappingTest, EndpointsRoundTrip) {
    // Echelon 1 <-> Internal 0
    EXPECT_EQ(echelonToInternal(1), 0u);
    EXPECT_EQ(internalToEchelon(0), 1u);

    // Echelon 32 <-> Internal 255
    EXPECT_EQ(echelonToInternal(32), 255u);
    EXPECT_EQ(internalToEchelon(255), 32u);
}

TEST(LevelMappingTest, All32LevelsRoundTrip) {
    for (uint8_t L = 1; L <= 32; ++L) {
        uint8_t internal = echelonToInternal(L);
        uint8_t recovered = internalToEchelon(internal);
        EXPECT_EQ(recovered, L) << "Failed round-trip for Echelon level " << static_cast<int>(L);
    }
}

TEST(LevelMappingTest, WorkingRangeEndpointsWiperToLevel) {
    // Wiper 165 (min working range) -> (165 - 97) / 39.1 = 68 / 39.1 = 1.7391...
    float levelMin = wiperToLevel(WORKING_RANGE_MIN);
    EXPECT_NEAR(levelMin, 1.7391f, 0.01f);

    // Wiper 1327 (max working range) -> (1327 - 97) / 39.1 = 1230 / 39.1 = 31.4578...
    float levelMax = wiperToLevel(WORKING_RANGE_MAX);
    EXPECT_NEAR(levelMax, 31.4578f, 0.01f);
}

TEST(LevelMappingTest, WiperToLevelClamping) {
    // Exactly at intercept 97: (97 - 97) / 39.1 = 0.0 -> clamps to 1.0
    EXPECT_FLOAT_EQ(wiperToLevel(97), 1.0f);

    // Below 97 (e.g. 0): negative -> clamps to 1.0
    EXPECT_FLOAT_EQ(wiperToLevel(0), 1.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(50), 1.0f);

    // Above range (e.g. 1400): (1400 - 97) / 39.1 = 33.32 -> clamps to 32.0
    EXPECT_FLOAT_EQ(wiperToLevel(1400), 32.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(4095), 32.0f);
}

TEST(LevelMappingTest, B03FitMidRangeAccuracy) {
    // Check level 16 midpoint: wiper = 97 + 39.1 * 16 = 722.6 -> ~723 counts
    uint16_t midWiper = static_cast<uint16_t>(std::round(B03_INTERCEPT + B03_SLOPE * 16.0f));
    float levelMid = wiperToLevel(midWiper);
    EXPECT_NEAR(levelMid, 16.0f, 0.05f);
}

TEST(LevelMappingTest, WiperToEchelonValidRange) {
    uint8_t echMin = wiperToEchelon(WORKING_RANGE_MIN);
    EXPECT_GE(echMin, 1u);
    EXPECT_LE(echMin, 32u);

    uint8_t echMax = wiperToEchelon(WORKING_RANGE_MAX);
    EXPECT_GE(echMax, 1u);
    EXPECT_LE(echMax, 32u);
    EXPECT_EQ(echMax, 32u); // Wiper 1327 maps to top Echelon level 32
}

TEST(LevelMappingTest, LevelToInternalClamping) {
    EXPECT_EQ(levelToInternal(0.0f), 0u);
    EXPECT_EQ(levelToInternal(1.0f), 0u);
    EXPECT_EQ(levelToInternal(32.0f), 255u);
    EXPECT_EQ(levelToInternal(50.0f), 255u);
}

TEST(LevelMappingTest, EchelonToInternalClamping) {
    EXPECT_EQ(echelonToInternal(0), 0u);
    EXPECT_EQ(echelonToInternal(1), 0u);
    EXPECT_EQ(echelonToInternal(32), 255u);
    EXPECT_EQ(echelonToInternal(100), 255u);
}

TEST(LevelMappingTest, InternalToEchelonClamping) {
    EXPECT_EQ(internalToEchelon(0), 1u);
    EXPECT_EQ(internalToEchelon(255), 32u);
}

TEST(LevelMappingTest, Monotonicity) {
    float prevLevel = 0.0f;
    uint8_t prevInternal = 0;
    uint8_t prevEchelon = 0;

    for (uint16_t w = 0; w <= 1500; w += 20) {
        float lvl = wiperToLevel(w);
        uint8_t internal = wiperToInternal(w);
        uint8_t ech = wiperToEchelon(w);

        EXPECT_GE(lvl, prevLevel);
        EXPECT_GE(internal, prevInternal);
        EXPECT_GE(ech, prevEchelon);

        prevLevel = lvl;
        prevInternal = internal;
        prevEchelon = ech;
    }
}
