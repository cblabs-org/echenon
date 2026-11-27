/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_signal_logic.cpp
 * Description: Native unit tests for SignalLogic sample packing/unpacking and threshold detection.
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include "echenon/SignalLogic.h"

using namespace echenon;

// =============================================================================
// A. Sample Packing and Unpacking Tests
// =============================================================================

TEST(SignalLogicTest, PackUnpackBasic) {
    for (uint8_t ch = 0; ch < 16; ch++) {
        for (uint16_t data : {0, 1, 100, 1023, 2048, 4095}) {
            uint16_t packed = packSample(ch, data);
            EXPECT_EQ(unpackChannel(packed), ch);
            EXPECT_EQ(unpackData(packed), data);
        }
    }
}

TEST(SignalLogicTest, PackMaskingLimits) {
    // Channel is 4 bits (0..15), data is 12 bits (0..4095)
    uint16_t packed = packSample(0x1F, 0xFFFF);
    EXPECT_EQ(unpackChannel(packed), 0x0F);
    EXPECT_EQ(unpackData(packed), 0x0FFF);
}

// =============================================================================
// B. Channel Slot Mapping Tests
// =============================================================================

TEST(SignalLogicTest, ChannelToSlotMapping) {
    // The 8 scanned ADC channels are: {0, 1, 2, 3, 4, 5, 6, 9}
    EXPECT_EQ(channelToSlot(0), 0); // pot_wiper
    EXPECT_EQ(channelToSlot(1), 1); // pot_hi
    EXPECT_EQ(channelToSlot(2), 2); // pot_lo
    EXPECT_EQ(channelToSlot(3), 3); // cadence_ret
    EXPECT_EQ(channelToSlot(4), 4); // cadence
    EXPECT_EQ(channelToSlot(5), 5); // motor_p
    EXPECT_EQ(channelToSlot(6), 6); // motor_n
    EXPECT_EQ(channelToSlot(9), 7); // enc_vcc

    // Released / un-scanned channels
    EXPECT_EQ(channelToSlot(7), -1); // Encoder A (PCNT)
    EXPECT_EQ(channelToSlot(8), -1); // Encoder B (PCNT)
    EXPECT_EQ(channelToSlot(10), -1);
}

TEST(SignalLogicTest, SlotToChannelMapping) {
    EXPECT_EQ(slotToChannel(0), 0);
    EXPECT_EQ(slotToChannel(1), 1);
    EXPECT_EQ(slotToChannel(2), 2);
    EXPECT_EQ(slotToChannel(3), 3);
    EXPECT_EQ(slotToChannel(4), 4);
    EXPECT_EQ(slotToChannel(5), 5);
    EXPECT_EQ(slotToChannel(6), 6);
    EXPECT_EQ(slotToChannel(7), 9);
    EXPECT_EQ(slotToChannel(8), 0xFF);
}

// =============================================================================
// C. Burst De-interleave Tests
// =============================================================================

TEST(SignalLogicTest, DeinterleaveIdealSequence) {
    std::vector<uint16_t> stream;
    // Produce 3 scan frames in DMA order: {0, 1, 2, 3, 4, 5, 6, 9}
    const uint8_t chs[8] = {0, 1, 2, 3, 4, 5, 6, 9};
    for (int frame = 0; frame < 3; frame++) {
        for (int s = 0; s < 8; s++) {
            uint16_t val = static_cast<uint16_t>((frame + 1) * 100 + s);
            stream.push_back(packSample(chs[s], val));
        }
    }

    std::vector<DeinterleavedRow> rows;
    size_t count = deinterleaveBurst(stream.data(), stream.size(), rows);

    ASSERT_EQ(count, 3u);
    ASSERT_EQ(rows.size(), 3u);

    for (int frame = 0; frame < 3; frame++) {
        for (int s = 0; s < 8; s++) {
            uint16_t expected = static_cast<uint16_t>((frame + 1) * 100 + s);
            EXPECT_EQ(rows[frame].channels[s], expected)
                << "Mismatch at frame " << frame << ", slot " << s;
        }
    }
}

TEST(SignalLogicTest, DeinterleaveLeadingPartialFrame) {
    std::vector<uint16_t> stream;
    // Partial leading frame: start at channel 4, 5, 6, 9
    stream.push_back(packSample(4, 999));
    stream.push_back(packSample(5, 999));
    stream.push_back(packSample(6, 999));
    stream.push_back(packSample(9, 999));

    // Then 2 full cycles starting with channel 0
    const uint8_t chs[8] = {0, 1, 2, 3, 4, 5, 6, 9};
    for (int frame = 0; frame < 2; frame++) {
        for (int s = 0; s < 8; s++) {
            uint16_t val = static_cast<uint16_t>((frame + 1) * 10 + s);
            stream.push_back(packSample(chs[s], val));
        }
    }

    std::vector<DeinterleavedRow> rows;
    size_t count = deinterleaveBurst(stream.data(), stream.size(), rows);

    ASSERT_EQ(count, 2u);
    for (int frame = 0; frame < 2; frame++) {
        for (int s = 0; s < 8; s++) {
            uint16_t expected = static_cast<uint16_t>((frame + 1) * 10 + s);
            EXPECT_EQ(rows[frame].channels[s], expected);
        }
    }
}

TEST(SignalLogicTest, DeinterleaveMissingSampleNoColumnShift) {
    std::vector<uint16_t> stream;
    // Frame 0: channel 2 is missing: {0, 1, 3, 4, 5, 6, 9}
    stream.push_back(packSample(0, 100));
    stream.push_back(packSample(1, 101));
    // channel 2 dropped!
    stream.push_back(packSample(3, 103));
    stream.push_back(packSample(4, 104));
    stream.push_back(packSample(5, 105));
    stream.push_back(packSample(6, 106));
    stream.push_back(packSample(9, 109));

    // Frame 1: complete frame {0, 1, 2, 3, 4, 5, 6, 9}
    stream.push_back(packSample(0, 200));
    stream.push_back(packSample(1, 201));
    stream.push_back(packSample(2, 202));
    stream.push_back(packSample(3, 203));
    stream.push_back(packSample(4, 204));
    stream.push_back(packSample(5, 205));
    stream.push_back(packSample(6, 206));
    stream.push_back(packSample(9, 209));

    std::vector<DeinterleavedRow> rows;
    size_t count = deinterleaveBurst(stream.data(), stream.size(), rows);

    ASSERT_EQ(count, 2u);
    // In frame 0, slot 2 was missing; slots 3..7 MUST NOT be shifted into slot 2!
    EXPECT_EQ(rows[0].channels[0], 100);
    EXPECT_EQ(rows[0].channels[1], 101);
    EXPECT_EQ(rows[0].channels[3], 103);
    EXPECT_EQ(rows[0].channels[4], 104);
    EXPECT_EQ(rows[0].channels[5], 105);
    EXPECT_EQ(rows[0].channels[6], 106);
    EXPECT_EQ(rows[0].channels[7], 109);

    // Frame 1 is completely intact
    EXPECT_EQ(rows[1].channels[0], 200);
    EXPECT_EQ(rows[1].channels[1], 201);
    EXPECT_EQ(rows[1].channels[2], 202);
    EXPECT_EQ(rows[1].channels[3], 203);
    EXPECT_EQ(rows[1].channels[4], 204);
    EXPECT_EQ(rows[1].channels[5], 205);
    EXPECT_EQ(rows[1].channels[6], 206);
    EXPECT_EQ(rows[1].channels[7], 209);
}

// =============================================================================
// D. Motor State Machine Tests
// =============================================================================

TEST(SignalLogicTest, MotorInitialStateIsIdle) {
    MotorStateMachine fsm;
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE);
    EXPECT_FALSE(fsm.getMotorPState());
    EXPECT_FALSE(fsm.getMotorNState());
}

TEST(SignalLogicTest, MotorForwardTransitionWithDebounce) {
    MotorStateMachine fsm(500, 400, 5000); // 500mV onset, 400mV offset, 5000us debounce

    struct EventRecord {
        MotorDirection dir;
        uint64_t ts;
    };
    std::vector<EventRecord> events;
    auto recordEvent = [&](MotorDirection dir, uint64_t ts) {
        events.push_back({dir, ts});
    };

    // t=0: Motor P goes HIGH (600 mV)
    fsm.processSample(5, 600, 0, recordEvent);
    EXPECT_TRUE(fsm.getMotorPState());
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE); // Still debouncing!
    EXPECT_EQ(events.size(), 0u);

    // t=2000: Still holding
    fsm.processSample(5, 600, 2000, recordEvent);
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE);
    EXPECT_EQ(events.size(), 0u);

    // t=4999: Just before 5 ms debounce
    fsm.processSample(5, 600, 4999, recordEvent);
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE);
    EXPECT_EQ(events.size(), 0u);

    // t=5000: Debounce window reached!
    fsm.processSample(5, 600, 5000, recordEvent);
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::FORWARD);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].dir, MotorDirection::FORWARD);
    EXPECT_EQ(events[0].ts, 5000u);

    // t=6000: Subsequent samples do NOT fire duplicate events
    fsm.processSample(5, 600, 6000, recordEvent);
    EXPECT_EQ(events.size(), 1u);
}

TEST(SignalLogicTest, MotorReverseTransition) {
    MotorStateMachine fsm(500, 400, 5000);
    std::vector<MotorDirection> events;

    // Motor N goes HIGH (600 mV)
    fsm.processSample(6, 600, 0, [&](MotorDirection dir, uint64_t) { events.push_back(dir); });
    fsm.processSample(6, 600, 5000, [&](MotorDirection dir, uint64_t) { events.push_back(dir); });

    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::REVERSE);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], MotorDirection::REVERSE);
}

TEST(SignalLogicTest, MotorBrakeTransition) {
    MotorStateMachine fsm(500, 400, 5000);
    std::vector<MotorDirection> events;

    // Both Motor P and Motor N go HIGH
    fsm.processSample(5, 600, 0, [&](MotorDirection dir, uint64_t) { events.push_back(dir); });
    fsm.processSample(6, 600, 0, [&](MotorDirection dir, uint64_t) { events.push_back(dir); });
    fsm.processSample(5, 600, 5000, [&](MotorDirection dir, uint64_t) { events.push_back(dir); });

    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::BRAKE);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], MotorDirection::BRAKE);
}

TEST(SignalLogicTest, MotorChatterRejection) {
    MotorStateMachine fsm(500, 400, 5000);
    int eventCount = 0;
    auto onEvent = [&](MotorDirection, uint64_t) { eventCount++; };

    // Glitch HIGH for 2 ms, then LOW
    fsm.processSample(5, 600, 0, onEvent);
    fsm.processSample(5, 600, 2000, onEvent);
    fsm.processSample(5, 300, 2100, onEvent); // Glitch ends
    fsm.processSample(5, 300, 6000, onEvent);

    // No event should have been emitted because the pulse did not hold for 5 ms
    EXPECT_EQ(eventCount, 0);
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE);
}

TEST(SignalLogicTest, MotorHysteresis) {
    MotorStateMachine fsm(500, 400, 5000);
    int eventCount = 0;
    auto onEvent = [&](MotorDirection, uint64_t) { eventCount++; };

    // Cross onset threshold (500 mV) -> 600 mV
    fsm.processSample(5, 600, 0, onEvent);
    fsm.processSample(5, 600, 5000, onEvent);
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::FORWARD);
    EXPECT_EQ(eventCount, 1);

    // Drop to 450 mV (between offset 400 mV and onset 500 mV)
    fsm.processSample(5, 450, 10000, onEvent);
    fsm.processSample(5, 450, 16000, onEvent);
    // Hysteresis holds P state HIGH!
    EXPECT_TRUE(fsm.getMotorPState());
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::FORWARD);
    EXPECT_EQ(eventCount, 1); // No new event

    // Drop below offset threshold (400 mV) -> 350 mV
    fsm.processSample(5, 350, 17000, onEvent);
    fsm.processSample(5, 350, 22000, onEvent);
    EXPECT_FALSE(fsm.getMotorPState());
    EXPECT_EQ(fsm.getCurrentDirection(), MotorDirection::IDLE);
    EXPECT_EQ(eventCount, 2);
}

// =============================================================================
// E. Timestamp Maths Tests
// =============================================================================

TEST(SignalLogicTest, CalculateSampleTimestampUs) {
    uint32_t freq = 83333;

    // Sample index 0 is exactly frame start
    EXPECT_EQ(calculateSampleTimestampUs(1000000ULL, 0, freq), 1000000ULL);

    // Sample index matching frequency is exactly +1,000,000 us (1 second)
    EXPECT_EQ(calculateSampleTimestampUs(1000000ULL, 83333, freq), 2000000ULL);

    // Intermediate samples
    uint64_t ts0 = calculateSampleTimestampUs(500000ULL, 0, freq);
    uint64_t ts1 = calculateSampleTimestampUs(500000ULL, 1, freq);
    uint64_t ts2 = calculateSampleTimestampUs(500000ULL, 2, freq);
    EXPECT_GT(ts1, ts0);
    EXPECT_GT(ts2, ts1);

    // 1000 samples at 83,333 Hz ≈ 12,000 us (12 ms)
    EXPECT_EQ(calculateSampleTimestampUs(0, 1000, freq), 12000ULL);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
