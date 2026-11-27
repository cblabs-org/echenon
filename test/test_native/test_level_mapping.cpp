/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_level_mapping.cpp
 * Description: Native unit tests for LevelMapping Echelon-to-internal resistance level conversions.
 */

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
    // C07 320-point sweep: WORKING_RANGE_MIN = 171 -> 1.0, WORKING_RANGE_MAX = 1333 -> 32.0
    EXPECT_EQ(WORKING_RANGE_MIN, 171u);
    EXPECT_EQ(WORKING_RANGE_MAX, 1333u);

    float levelMin = wiperToLevel(WORKING_RANGE_MIN);
    EXPECT_FLOAT_EQ(levelMin, 1.0f);

    float levelMax = wiperToLevel(WORKING_RANGE_MAX);
    EXPECT_FLOAT_EQ(levelMax, 32.0f);
}

TEST(LevelMappingTest, WiperToLevelClamping) {
    // At or below WORKING_RANGE_MIN (171) -> clamps to 1.0
    EXPECT_FLOAT_EQ(wiperToLevel(171), 1.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(120), 1.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(0), 1.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(50), 1.0f);

    // At or above WORKING_RANGE_MAX (1333) -> clamps to 32.0
    EXPECT_FLOAT_EQ(wiperToLevel(1333), 32.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(1400), 32.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(4095), 32.0f);
}

TEST(LevelMappingTest, B03FitMidRangeAccuracy) {
    // Check level 16 midpoint: wiper = B03_INTERCEPT + B03_SLOPE * 16
    float exactWiper16 = B03_INTERCEPT + B03_SLOPE * 16.0f;
    EXPECT_NEAR(wiperToLevel(exactWiper16), 16.0f, 1e-4f);

    uint16_t midWiper = static_cast<uint16_t>(std::round(exactWiper16));
    float levelMid = wiperToLevel(midWiper);
    EXPECT_NEAR(levelMid, 16.0f, 0.05f);
}

TEST(LevelMappingTest, WiperToNormalizedAndCustomBounds) {
    // Default C07 bounds [171, 1333]
    EXPECT_FLOAT_EQ(wiperToNormalized(171.0f), 0.0f);
    EXPECT_FLOAT_EQ(wiperToNormalized(1333.0f), 1.0f);
    EXPECT_FLOAT_EQ(wiperToNormalized(752.0f), 0.5f);
    EXPECT_FLOAT_EQ(wiperToLevel(752.0f), 16.5f);

    // Custom calibrated bounds [200, 1400]
    EXPECT_FLOAT_EQ(wiperToNormalized(200.0f, 200.0f, 1400.0f), 0.0f);
    EXPECT_FLOAT_EQ(wiperToNormalized(800.0f, 200.0f, 1400.0f), 0.5f);
    EXPECT_FLOAT_EQ(wiperToNormalized(1400.0f, 200.0f, 1400.0f), 1.0f);
    EXPECT_FLOAT_EQ(wiperToLevel(800.0f, 200.0f, 1400.0f), 16.5f);
    EXPECT_EQ(wiperToEchelon(800.0f, 200.0f, 1400.0f), 17u);
    EXPECT_EQ(wiperToInternal(200.0f, 200.0f, 1400.0f), 0u);
    EXPECT_EQ(wiperToInternal(1400.0f, 200.0f, 1400.0f), 255u);

    // Invalid / non-finite bounds return 0.0 normalized -> level 1.0
    EXPECT_FLOAT_EQ(wiperToNormalized(NAN), 0.0f);
    EXPECT_FLOAT_EQ(wiperToNormalized(500.0f, 1000.0f, 500.0f), 0.0f);
    EXPECT_FLOAT_EQ(wiperToNormalized(500.0f, 500.0f, 500.0f), 0.0f);
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


// =============================================================================
// WO-05.1: Normalized Cam Position Helpers
// =============================================================================

TEST(LevelMappingTest, EchelonContinuousToNormalized_Boundaries) {
    // Level 1.0 → 0.0
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(1.0f), 0.0f);
    // Level 32.0 → 1.0
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(32.0f), 1.0f);
    // Level 16.5 → 0.5
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(16.5f), 0.5f);
    // Below range clamps to 0.0
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(-5.0f), 0.0f);
    // Above range clamps to 1.0
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(50.0f), 1.0f);
    // NaN returns 0.0
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(NAN), 0.0f);
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(INFINITY), 0.0f);
    EXPECT_FLOAT_EQ(echelonContinuousToNormalized(-INFINITY), 0.0f);
}

TEST(LevelMappingTest, NormalizedToEchelonContinuous_Boundaries) {
    // p=0.0 → 1.0
    EXPECT_FLOAT_EQ(normalizedToEchelonContinuous(0.0f), 1.0f);
    // p=1.0 → 32.0
    EXPECT_FLOAT_EQ(normalizedToEchelonContinuous(1.0f), 32.0f);
    // p=0.5 → 16.5
    EXPECT_FLOAT_EQ(normalizedToEchelonContinuous(0.5f), 16.5f);
    // Negative clamps to 0.0 → level 1.0
    EXPECT_FLOAT_EQ(normalizedToEchelonContinuous(-1.0f), 1.0f);
    // Above 1.0 clamps → level 32.0
    EXPECT_FLOAT_EQ(normalizedToEchelonContinuous(2.0f), 32.0f);
}

TEST(LevelMappingTest, NormalizedToEchelonContinuous_RoundTrip) {
    for (float level = 1.0f; level <= 32.0f; level += 0.5f) {
        float p = echelonContinuousToNormalized(level);
        float recovered = normalizedToEchelonContinuous(p);
        EXPECT_NEAR(recovered, level, 1e-4f) << "Round-trip failed for level " << level;
    }
}

// =============================================================================
// WO-05.1: FTMS Internal Scale (0..254, 255 = UNCALIBRATED)
// =============================================================================

TEST(LevelMappingTest, NormalizedToInternal_Boundaries) {
    // p=0.0 → 0
    EXPECT_EQ(normalizedToInternal(0.0f), 0u);
    // p=1.0 → 254
    EXPECT_EQ(normalizedToInternal(1.0f), 254u);
    // p=0.5 → 127
    EXPECT_EQ(normalizedToInternal(0.5f), 127u);
    // Negative clamps → 0
    EXPECT_EQ(normalizedToInternal(-0.5f), 0u);
    // Above 1.0 clamps → 254
    EXPECT_EQ(normalizedToInternal(1.5f), 254u);
}

TEST(LevelMappingTest, InternalToNormalized_Boundaries) {
    // 0 → 0.0
    EXPECT_FLOAT_EQ(internalToNormalized(0), 0.0f);
    // 254 → 1.0
    EXPECT_FLOAT_EQ(internalToNormalized(254), 1.0f);
    // 127 → 0.5 (127/254)
    EXPECT_NEAR(internalToNormalized(127), 0.5f, 0.002f);
    // 255 (UNCALIBRATED) → 0.0
    EXPECT_FLOAT_EQ(internalToNormalized(255), 0.0f);
}

TEST(LevelMappingTest, InternalNormalized_RoundTrip) {
    for (int i = 0; i <= 254; ++i) {
        float p = internalToNormalized(static_cast<uint8_t>(i));
        uint8_t recovered = normalizedToInternal(p);
        EXPECT_EQ(recovered, static_cast<uint8_t>(i))
            << "Round-trip failed for internal=" << i;
    }
}

// =============================================================================
// WO-05.1: Peloton Power-Equivalence Mapping
// =============================================================================

TEST(LevelMappingTest, InternalToPeloton_UncalibratedReturnsZero) {
    EXPECT_EQ(internalToPeloton(255), 0u);
}

TEST(LevelMappingTest, InternalToPeloton_BoundaryValues) {
    // From the Peloton power-equivalence formula:
    // R_pel = clamp(round(pow(max(P_ref/106.66667 - 0.1, 0) / 0.000185, 1/2.7)), 0, 100)
    // internal for L1 = echelonToInternal(1) = 0
    EXPECT_EQ(internalToPeloton(0), 13u);    // L1: P=31.63W → R=13

    // L16: internal = echelonToInternal(16) ≈ 123
    uint8_t i16 = echelonToInternal(16);
    EXPECT_EQ(internalToPeloton(i16), 21u);  // L16: P=80.56W → R=21

    // L20: internal = echelonToInternal(20) ≈ 156
    uint8_t i20 = echelonToInternal(20);
    EXPECT_EQ(internalToPeloton(i20), 25u);  // L20: P=132.61W → R=25

    // L24: internal = echelonToInternal(24) ≈ 189
    uint8_t i24 = echelonToInternal(24);
    EXPECT_EQ(internalToPeloton(i24), 31u);  // L24: P=220.82W → R=31

    // L28: internal = echelonToInternal(28) ≈ 222
    uint8_t i28 = echelonToInternal(28);
    EXPECT_EQ(internalToPeloton(i28), 37u);  // L28: P=347.50W → R=37

    // L32: echelonToInternal(32) = 255 → UNCALIBRATED → 0
    // Use internal=254 (p_tilde=1.0) for L32-equivalent:
    EXPECT_EQ(internalToPeloton(254), 42u);  // p_tilde=1.0 → P=487W → R=42
}

TEST(LevelMappingTest, InternalToPeloton_Monotonicity) {
    uint8_t prev = 0;
    for (int i = 0; i <= 254; ++i) {
        uint8_t pel = internalToPeloton(static_cast<uint8_t>(i));
        EXPECT_GE(pel, prev)
            << "Peloton must be monotonically non-decreasing at internal=" << i;
        prev = pel;
    }
}
