#include <gtest/gtest.h>
#include "echenon/QuadratureGenerator.h"
#include "echenon/QuadratureGenerator.cpp"

using namespace echenon;

// =============================================================================
// QuadratureGenerator Unit Tests (WO-02.5)
// =============================================================================

TEST(QuadratureGeneratorTest, InitialState) {
    QuadratureGenerator gen;
    EXPECT_EQ(gen.getState(), 0);
    EXPECT_EQ(gen.getTotalSteps(), 0);
}

TEST(QuadratureGeneratorTest, ForwardSequence) {
    QuadratureGenerator gen;
    // Step UP 4 times: 0 -> 1 -> 2 -> 3 -> 0
    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 1);
    EXPECT_EQ(gen.getTotalSteps(), 1);

    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 2);
    EXPECT_EQ(gen.getTotalSteps(), 2);

    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 3);
    EXPECT_EQ(gen.getTotalSteps(), 3);

    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 0);
    EXPECT_EQ(gen.getTotalSteps(), 4);
}

TEST(QuadratureGeneratorTest, ReverseSequence) {
    QuadratureGenerator gen;
    // Step DOWN 4 times: 0 -> 3 -> 2 -> 1 -> 0
    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 3);
    EXPECT_EQ(gen.getTotalSteps(), -1);

    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 2);
    EXPECT_EQ(gen.getTotalSteps(), -2);

    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 1);
    EXPECT_EQ(gen.getTotalSteps(), -3);

    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 0);
    EXPECT_EQ(gen.getTotalSteps(), -4);
}

TEST(QuadratureGeneratorTest, DirectionReversal) {
    QuadratureGenerator gen;
    // Step UP twice
    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 1);
    EXPECT_EQ(gen.getTotalSteps(), 1);

    gen.step(QuadratureGenerator::Direction::UP);
    EXPECT_EQ(gen.getState(), 2);
    EXPECT_EQ(gen.getTotalSteps(), 2);

    // Step DOWN twice -> returns to start
    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 1);
    EXPECT_EQ(gen.getTotalSteps(), 1);

    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getState(), 0);
    EXPECT_EQ(gen.getTotalSteps(), 0);
}

TEST(QuadratureGeneratorTest, LevelCount) {
    QuadratureGenerator gen;
    // 16 steps UP = 1 full level (4 detents * 4 states)
    for (int i = 0; i < 16; ++i) {
        gen.step(QuadratureGenerator::Direction::UP);
    }
    EXPECT_EQ(gen.getState(), 0);
    EXPECT_EQ(gen.getTotalSteps(), 16);
}

TEST(QuadratureGeneratorTest, PinLevelsGrayCode) {
    bool pinA = false;
    bool pinB = false;

    // State 0: A=LOW, B=LOW
    QuadratureGenerator::getPinLevels(0, pinA, pinB);
    EXPECT_FALSE(pinA);
    EXPECT_FALSE(pinB);

    // State 1: A=HIGH, B=LOW
    QuadratureGenerator::getPinLevels(1, pinA, pinB);
    EXPECT_TRUE(pinA);
    EXPECT_FALSE(pinB);

    // State 2: A=HIGH, B=HIGH
    QuadratureGenerator::getPinLevels(2, pinA, pinB);
    EXPECT_TRUE(pinA);
    EXPECT_TRUE(pinB);

    // State 3: A=LOW, B=HIGH
    QuadratureGenerator::getPinLevels(3, pinA, pinB);
    EXPECT_FALSE(pinA);
    EXPECT_TRUE(pinB);

    // Modulo test (e.g. state 4 -> state 0, state 7 -> state 3)
    QuadratureGenerator::getPinLevels(4, pinA, pinB);
    EXPECT_FALSE(pinA);
    EXPECT_FALSE(pinB);

    QuadratureGenerator::getPinLevels(7, pinA, pinB);
    EXPECT_FALSE(pinA);
    EXPECT_TRUE(pinB);
}

TEST(QuadratureGeneratorTest, TotalStepTrackingAndReset) {
    QuadratureGenerator gen;
    EXPECT_EQ(gen.getTotalSteps(), 0);

    // Step UP 5 times
    for (int i = 0; i < 5; ++i) {
        gen.step(QuadratureGenerator::Direction::UP);
    }
    EXPECT_EQ(gen.getTotalSteps(), 5);
    EXPECT_EQ(gen.getState(), 1);

    // Step DOWN 2 times
    gen.step(QuadratureGenerator::Direction::DOWN);
    gen.step(QuadratureGenerator::Direction::DOWN);
    EXPECT_EQ(gen.getTotalSteps(), 3);
    EXPECT_EQ(gen.getState(), 3);

    // Reset restores state 0 and total steps 0
    gen.reset();
    EXPECT_EQ(gen.getTotalSteps(), 0);
    EXPECT_EQ(gen.getState(), 0);
}
