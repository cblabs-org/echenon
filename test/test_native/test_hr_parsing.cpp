/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_hr_parsing.cpp
 * Description: Native unit tests for GATT 0x2A37 heart rate measurement packet parsing.
 */

#include <gtest/gtest.h>
#include "echenon/HRParsing.h"
#include "echenon/HREvents.h"

using namespace echenon;

// =============================================================================
// GATT 0x2A37 Heart Rate Measurement Parsing Tests (SPEC §8.2, WO4)
// =============================================================================

TEST(HRParsingTest, Uint8BpmNoFlags) {
    // Beat armband format: 8-bit BPM, contact not supported
    const uint8_t data[] = {0x00, 72};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 72);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_FALSE(out.hasEnergy);
    EXPECT_FALSE(out.hasRr);
    EXPECT_EQ(out.fieldsConsumed, 2);
}

TEST(HRParsingTest, Uint16Bpm) {
    // 16-bit BPM format (0x0120 = 288 BPM)
    const uint8_t data[] = {0x01, 0x20, 0x01};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 288);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_FALSE(out.hasEnergy);
    EXPECT_FALSE(out.hasRr);
    EXPECT_EQ(out.fieldsConsumed, 3);
}

TEST(HRParsingTest, ContactSupportedAndDetected) {
    // Flags 0x06: contact supported (bit 2) + detected (bit 1)
    const uint8_t data[] = {0x06, 80};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 80);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_EQ(out.fieldsConsumed, 2);
}

TEST(HRParsingTest, ContactSupportedAndNotDetected) {
    // Flags 0x04: contact supported (bit 2) + NOT detected (bit 1 clear)
    const uint8_t data[] = {0x04, 80};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 80);
    EXPECT_EQ(out.contactState, HRContactState::NO_CONTACT);
    EXPECT_EQ(out.fieldsConsumed, 2);
}

TEST(HRParsingTest, BpmZeroWithNoContactFlags) {
    // BPM = 0 with flags 0x00 -> NO_CONTACT
    const uint8_t data[] = {0x00, 0};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 0);
    EXPECT_EQ(out.contactState, HRContactState::NO_CONTACT);
    EXPECT_EQ(out.fieldsConsumed, 2);
}

TEST(HRParsingTest, EnergyExpendedPresent) {
    // Flags 0x08: Energy expended present (0x0000 = 0 kJ)
    const uint8_t data[] = {0x08, 72, 0x00, 0x00};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 72);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_TRUE(out.hasEnergy);
    EXPECT_EQ(out.energyExpended, 0);
    EXPECT_FALSE(out.hasRr);
    EXPECT_EQ(out.fieldsConsumed, 4);

    // Non-zero energy: 0x012C = 300 kJ
    const uint8_t data2[] = {0x08, 72, 0x2C, 0x01};
    EXPECT_TRUE(parse0x2A37(data2, sizeof(data2), out));
    EXPECT_TRUE(out.hasEnergy);
    EXPECT_EQ(out.energyExpended, 300);
}

TEST(HRParsingTest, RrIntervalPresent) {
    // Flags 0x10: RR-interval present (0x0400 = 1024 -> 1000 ms)
    const uint8_t data[] = {0x10, 72, 0x00, 0x04};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 72);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_FALSE(out.hasEnergy);
    EXPECT_TRUE(out.hasRr);
    EXPECT_NEAR(out.lastRrMs, 1000.0f, 0.01f);
    EXPECT_EQ(out.fieldsConsumed, 4);
}

TEST(HRParsingTest, AllFlagsSet) {
    // 16-bit (0x01) + contact detected (0x06) + energy (0x08) + RR (0x10) = 0x1F
    // BPM = 288 (0x0120), Energy = 100 kJ (0x0064), RR = 1024 (0x0400 -> 1000 ms)
    const uint8_t data[] = {0x1F, 0x20, 0x01, 0x64, 0x00, 0x00, 0x04};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 288);
    EXPECT_EQ(out.contactState, HRContactState::ACTIVE);
    EXPECT_TRUE(out.hasEnergy);
    EXPECT_EQ(out.energyExpended, 100);
    EXPECT_TRUE(out.hasRr);
    EXPECT_NEAR(out.lastRrMs, 1000.0f, 0.01f);
    EXPECT_EQ(out.fieldsConsumed, 7);
}

TEST(HRParsingTest, TruncatedPayload) {
    ParsedHrMeasurement out;
    // Null pointer
    EXPECT_FALSE(parse0x2A37(nullptr, 2, out));

    // Zero length
    const uint8_t empty[] = {0x00};
    EXPECT_FALSE(parse0x2A37(empty, 0, out));

    // 1 byte (flags only, missing BPM)
    EXPECT_FALSE(parse0x2A37(empty, 1, out));
}

TEST(HRParsingTest, Uint16BpmTruncated) {
    // 16-bit flag set, but only 2 bytes provided (needs 3)
    const uint8_t data[] = {0x01, 72};
    ParsedHrMeasurement out;
    EXPECT_FALSE(parse0x2A37(data, 2, out));
}

TEST(HRParsingTest, EnergyTruncated) {
    // Energy flag set, but only 3 bytes provided (needs 4)
    const uint8_t data[] = {0x08, 72, 0x00};
    ParsedHrMeasurement out;
    EXPECT_FALSE(parse0x2A37(data, sizeof(data), out));
}

TEST(HRParsingTest, RrIntervalTruncated) {
    // RR flag set, but only 3 bytes provided (needs 4)
    const uint8_t data[] = {0x10, 72, 0x00};
    ParsedHrMeasurement out;
    EXPECT_FALSE(parse0x2A37(data, sizeof(data), out));
}

TEST(HRParsingTest, MultipleRrIntervals) {
    // 2 RR intervals: 512 (500 ms) and 1024 (1000 ms)
    // lastRrMs should hold the final interval (1000 ms)
    const uint8_t data[] = {0x10, 72, 0x00, 0x02, 0x00, 0x04};
    ParsedHrMeasurement out;
    EXPECT_TRUE(parse0x2A37(data, sizeof(data), out));
    EXPECT_EQ(out.bpm, 72);
    EXPECT_TRUE(out.hasRr);
    EXPECT_NEAR(out.lastRrMs, 1000.0f, 0.01f);
    EXPECT_EQ(out.fieldsConsumed, 6);
}
