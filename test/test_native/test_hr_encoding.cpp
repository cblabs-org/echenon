/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_hr_encoding.cpp
 * Description: Native unit tests for GATT 0x2A37 heart rate measurement packet encoding.
 */

#include <gtest/gtest.h>
#include "echenon/HREncoding.h"
#include "echenon/HREvents.h"

using namespace echenon;

// =============================================================================
// GATT 0x2A37 Heart Rate Measurement Encoding Tests
// =============================================================================

TEST(HREncodingTest, EncodeBpm72) {
    uint8_t out[2] = {0xFF, 0xFF};
    size_t written = encode0x2A37(72, 0x00, out, sizeof(out));
    EXPECT_EQ(written, 2u);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 72);
}

TEST(HREncodingTest, EncodeBpm0) {
    uint8_t out[2] = {0xFF, 0xFF};
    size_t written = encode0x2A37(0, 0x00, out, sizeof(out));
    EXPECT_EQ(written, 2u);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 0);
}

TEST(HREncodingTest, EncodeBpm255) {
    uint8_t out[2] = {0x00, 0x00};
    size_t written = encode0x2A37(255, 0x00, out, sizeof(out));
    EXPECT_EQ(written, 2u);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 255);
}

TEST(HREncodingTest, BufferTooSmall) {
    uint8_t out[1] = {0xAA};
    size_t written = encode0x2A37(72, 0x00, out, sizeof(out));
    EXPECT_EQ(written, 0u);
    EXPECT_EQ(out[0], 0xAA); // Must not corrupt buffer
}

TEST(HREncodingTest, NullBufferReturnsZero) {
    size_t written = encode0x2A37(72, 0x00, nullptr, 2);
    EXPECT_EQ(written, 0u);
}

TEST(HREncodingTest, ZeroLengthBufferReturnsZero) {
    uint8_t out[2] = {0xAA, 0xBB};
    size_t written = encode0x2A37(72, 0x00, out, 0);
    EXPECT_EQ(written, 0u);
    EXPECT_EQ(out[0], 0xAA);
    EXPECT_EQ(out[1], 0xBB);
}

TEST(HREncodingTest, ConvenienceTwoArgOverload) {
    uint8_t out[2] = {0xFF, 0xFF};
    size_t written = encode0x2A37(135, out, sizeof(out));
    EXPECT_EQ(written, 2u);
    EXPECT_EQ(out[0], 0x00);
    EXPECT_EQ(out[1], 135);
}

TEST(HREncodingTest, BodySensorLocationIndependent) {
    // 0x2A38 location value does not affect 0x2A37 wire payload
    uint8_t out1[2] = {};
    uint8_t out2[2] = {};
    size_t w1 = encode0x2A37(80, 0x01 /* Chest */, out1, sizeof(out1));
    size_t w2 = encode0x2A37(80, 0x02 /* Wrist */, out2, sizeof(out2));
    EXPECT_EQ(w1, 2u);
    EXPECT_EQ(w2, 2u);
    EXPECT_EQ(out1[0], out2[0]);
    EXPECT_EQ(out1[1], out2[1]);
}
