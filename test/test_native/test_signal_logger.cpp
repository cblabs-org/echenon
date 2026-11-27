/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: test/test_native/test_signal_logger.cpp
 * Description: Native unit tests for SignalLogger decimation, ring buffer ordering, and burst CSV formatting.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include "echenon/SignalLogic.h"
#include "echenon/SignalLogger.h"

using namespace echenon;

// =============================================================================
// B. Decimation Tests
// =============================================================================

TEST(SignalLoggerDecimationTest, FeedingNSamplesProducesCorrectMinMaxMean) {
    DecimationAccumulator accum(50); // 50 ms window

    // Initial state: empty row
    const auto& cur = accum.getCurrentRow();
    EXPECT_EQ(cur.channels[0].count, 0u);
    EXPECT_EQ(cur.channels[0].min_val, 0xFFFF);
    EXPECT_EQ(cur.channels[0].max_val, 0u);
    EXPECT_EQ(cur.channels[0].mean(), 0u);

    // Feed 5 raw samples for slot 0 (ADC channel 0 = pot_wiper):
    // Values: 1200, 1500, 900, 2100, 1800
    // min = 900, max = 2100, sum = 7500, count = 5, mean = 1500
    std::vector<uint16_t> slot0_samples = {1200, 1500, 900, 2100, 1800};
    for (uint16_t s : slot0_samples) {
        accum.addSample(0, s); // channel 0 maps to slot 0
    }

    EXPECT_EQ(cur.channels[0].count, 5u);
    EXPECT_EQ(cur.channels[0].min_val, 900);
    EXPECT_EQ(cur.channels[0].max_val, 2100);
    EXPECT_EQ(cur.channels[0].sum, 7500u);
    EXPECT_EQ(cur.channels[0].mean(), 1500u);

    // Feed samples for slot 7 (ADC channel 9 = enc_vcc)
    // Values: 3300, 3310, 3290
    // min = 3290, max = 3310, sum = 9900, count = 3, mean = 3300
    accum.addSample(9, 3300);
    accum.addSample(9, 3310);
    accum.addSample(9, 3290);

    EXPECT_EQ(cur.channels[7].count, 3u);
    EXPECT_EQ(cur.channels[7].min_val, 3290);
    EXPECT_EQ(cur.channels[7].max_val, 3310);
    EXPECT_EQ(cur.channels[7].sum, 9900u);
    EXPECT_EQ(cur.channels[7].mean(), 3300u);

    // Un-sampled channel slot (e.g. slot 1) remains in default state
    EXPECT_EQ(cur.channels[1].count, 0u);
    EXPECT_EQ(cur.channels[1].min_val, 0xFFFF);
    EXPECT_EQ(cur.channels[1].max_val, 0u);
    EXPECT_EQ(cur.channels[1].mean(), 0u);
}

TEST(SignalLoggerDecimationTest, WindowBoundaryProducesRowAndResetsAccumulators) {
    DecimationAccumulator accum(50); // 50 ms window
    DecimationRow emittedRow{};

    // First tick at t = 1000 ms initializes last window timestamp
    EXPECT_FALSE(accum.processTick(1000, emittedRow));

    // Add some samples within the window
    accum.addSample(0, 100);
    accum.addSample(0, 200);

    // Tick at t = 1030 ms (only 30 ms elapsed < 50 ms window)
    EXPECT_FALSE(accum.processTick(1030, emittedRow));
    EXPECT_EQ(accum.getCurrentRow().channels[0].count, 2u);

    // Add one more sample
    accum.addSample(0, 300);

    // Tick at t = 1050 ms (exactly 50 ms elapsed): window boundary hit!
    EXPECT_TRUE(accum.processTick(1050, emittedRow));

    // Verify emitted row has correct timestamp and stats
    EXPECT_EQ(emittedRow.timestamp_ms, 1050u);
    EXPECT_EQ(emittedRow.channels[0].count, 3u);
    EXPECT_EQ(emittedRow.channels[0].min_val, 100);
    EXPECT_EQ(emittedRow.channels[0].max_val, 300);
    EXPECT_EQ(emittedRow.channels[0].sum, 600u);
    EXPECT_EQ(emittedRow.channels[0].mean(), 200u);

    // Verify accumulator was cleanly reset for the next window
    const auto& cur = accum.getCurrentRow();
    EXPECT_EQ(cur.channels[0].count, 0u);
    EXPECT_EQ(cur.channels[0].min_val, 0xFFFF);
    EXPECT_EQ(cur.channels[0].max_val, 0u);
    EXPECT_EQ(cur.channels[0].sum, 0u);
    EXPECT_EQ(cur.channels[0].mean(), 0u);

    // Immediate next tick at 1055 ms should not trigger
    EXPECT_FALSE(accum.processTick(1055, emittedRow));
}

TEST(SignalLoggerDecimationTest, EmptyWindowProducesZeroOrSentinel) {
    DecimationAccumulator accum(50);
    DecimationRow emittedRow{};

    accum.processTick(2000, emittedRow);

    // No samples added; elapse the 50 ms window
    EXPECT_TRUE(accum.processTick(2050, emittedRow));

    EXPECT_EQ(emittedRow.timestamp_ms, 2050u);
    for (size_t s = 0; s < 8; s++) {
        EXPECT_EQ(emittedRow.channels[s].count, 0u);
        EXPECT_EQ(emittedRow.channels[s].sum, 0u);
        EXPECT_EQ(emittedRow.channels[s].min_val, 0xFFFF); // sentinel value
        EXPECT_EQ(emittedRow.channels[s].max_val, 0u);
        EXPECT_EQ(emittedRow.channels[s].mean(), 0u);      // mean is 0 when count == 0
    }
}

// =============================================================================
// C. Event Ordering Tests
// =============================================================================

TEST(SignalLoggerEventOrderingTest, EventsStoredInTimestampOrder) {
    RingBuffer<EdgeEvent, 16> ring;

    // Push 4 events with increasing timestamps
    ring.push({100000u, EDGE_EVENT_CADENCE, 4, 1});
    ring.push({105000u, EDGE_EVENT_MOTOR_DIRECTION, 5, 1});
    ring.push({112000u, EDGE_EVENT_CADENCE, 4, 0});
    ring.push({120000u, EDGE_EVENT_ENCODER, 8, 1});

    ASSERT_EQ(ring.size(), 4u);
    EXPECT_FALSE(ring.isWrapped());

    // Verify chronological order
    uint32_t prev_ts = 0;
    for (size_t i = 0; i < ring.size(); i++) {
        const auto& ev = ring[i];
        EXPECT_GT(ev.timestamp_us, prev_ts);
        prev_ts = ev.timestamp_us;
    }

    EXPECT_EQ(ring[0].timestamp_us, 100000u);
    EXPECT_EQ(ring[0].type, EDGE_EVENT_CADENCE);
    EXPECT_EQ(ring[1].timestamp_us, 105000u);
    EXPECT_EQ(ring[1].type, EDGE_EVENT_MOTOR_DIRECTION);
    EXPECT_EQ(ring[2].timestamp_us, 112000u);
    EXPECT_EQ(ring[2].type, EDGE_EVENT_CADENCE);
    EXPECT_EQ(ring[3].timestamp_us, 120000u);
    EXPECT_EQ(ring[3].type, EDGE_EVENT_ENCODER);
}

TEST(SignalLoggerEventOrderingTest, RingBufferWrapAroundPreservesChronologicalOrder) {
    // Ring buffer with small capacity = 5
    RingBuffer<EdgeEvent, 5> ring;

    // Push 8 events with timestamps 100, 200, 300, 400, 500, 600, 700, 800
    for (uint32_t ts = 100; ts <= 800; ts += 100) {
        ring.push({ts, EDGE_EVENT_CADENCE, 4, static_cast<uint16_t>(ts / 100)});
    }

    // Size must be capped at capacity = 5
    ASSERT_EQ(ring.size(), 5u);
    EXPECT_TRUE(ring.isWrapped());

    // The oldest 3 events (100, 200, 300) must have been overwritten.
    // Remaining events must be exactly: 400, 500, 600, 700, 800 in strict order!
    std::vector<uint32_t> expected_timestamps = {400, 500, 600, 700, 800};
    for (size_t i = 0; i < ring.size(); i++) {
        EXPECT_EQ(ring[i].timestamp_us, expected_timestamps[i])
            << "Mismatch at index " << i;
        EXPECT_EQ(ring[i].value, expected_timestamps[i] / 100);
    }
}

// =============================================================================
// D. CSV Format Tests
// =============================================================================

TEST(SignalLoggerCsvFormatTest, RideLogHeaderHasExpectedColumns) {
    std::string header(kRideLogCsvHeader);

    // Must end with newline
    ASSERT_FALSE(header.empty());
    EXPECT_EQ(header.back(), '\n');

    // Remove trailing newline for tokenizing
    std::string trimmed = header.substr(0, header.size() - 1);
    std::stringstream ss(trimmed);
    std::string col;
    std::vector<std::string> columns;
    while (std::getline(ss, col, ',')) {
        columns.push_back(col);
    }

    // Expected: 1 timestamp + 8 channels * 3 stats (min, mean, max) = 25 columns
    ASSERT_EQ(columns.size(), 25u);
    EXPECT_EQ(columns[0], "timestamp_ms");

    const char* expected_prefixes[8] = {
        "pot_wiper", "pot_hi", "pot_lo", "cadence_ret",
        "cadence", "motor_p", "motor_n", "enc_vcc"
    };

    for (size_t ch = 0; ch < 8; ch++) {
        size_t base = 1 + ch * 3;
        EXPECT_EQ(columns[base], std::string(expected_prefixes[ch]) + "_min");
        EXPECT_EQ(columns[base + 1], std::string(expected_prefixes[ch]) + "_mean");
        EXPECT_EQ(columns[base + 2], std::string(expected_prefixes[ch]) + "_max");
    }
}

TEST(SignalLoggerCsvFormatTest, RideLogRowProducesExpectedCsvString) {
    DecimationRow row{};
    row.timestamp_ms = 12345;

    // Fill each channel with distinct known values
    // Slot 0: min 100, max 300, sum 600, count 3 -> mean 200
    row.channels[0].min_val = 100;
    row.channels[0].max_val = 300;
    row.channels[0].sum = 600;
    row.channels[0].count = 3;

    // Slot 1: min 500, max 700, sum 1800, count 3 -> mean 600
    row.channels[1].min_val = 500;
    row.channels[1].max_val = 700;
    row.channels[1].sum = 1800;
    row.channels[1].count = 3;

    // Slots 2..7 with deterministic values
    for (size_t s = 2; s < 8; s++) {
        uint16_t base = static_cast<uint16_t>(s * 1000);
        row.channels[s].min_val = base + 10;
        row.channels[s].max_val = base + 30;
        row.channels[s].sum = (base + 20) * 2;
        row.channels[s].count = 2;
    }

    std::string csv = formatRideLogRow(row);

    std::string expected =
        "12345,"
        "100,200,300,"
        "500,600,700,"
        "2010,2020,2030,"
        "3010,3020,3030,"
        "4010,4020,4030,"
        "5010,5020,5030,"
        "6010,6020,6030,"
        "7010,7020,7030\n";

    EXPECT_EQ(csv, expected);
}

TEST(SignalLoggerCsvFormatTest, RideLogEmptyRowProducesExpectedCsvString) {
    DecimationRow row{};
    row.reset();
    row.timestamp_ms = 50;

    std::string csv = formatRideLogRow(row);

    // Empty channels have min=65535, mean=0, max=0
    std::string expected =
        "50,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0,"
        "65535,0,0\n";

    EXPECT_EQ(csv, expected);
}

TEST(SignalLoggerCsvFormatTest, BurstCsvFormat) {
    // 1. Burst Header format test
    uint32_t perChanRate = 10416;
    uint64_t burstStartUs = 5000000ULL;
    uint32_t overflowCount = 0;

    std::string header = formatBurstCsvHeader(perChanRate, burstStartUs, overflowCount);
    std::string expectedHeader =
        "# sample_rate_per_channel_hz=10416,start_time_us=5000000,overflow_count=0\n"
        "sample_idx,pot_wiper,pot_hi,pot_lo,cadence_ret,cadence,motor_p,motor_n,enc_vcc\n";

    EXPECT_EQ(header, expectedHeader);

    // 2. Burst Row format test (sample_idx, ch0..ch9 values)
    DeinterleavedRow row{};
    row.channels[0] = 1200; // pot_wiper (ch 0)
    row.channels[1] = 3300; // pot_hi (ch 1)
    row.channels[2] = 0;    // pot_lo (ch 2)
    row.channels[3] = 10;   // cadence_ret (ch 3)
    row.channels[4] = 3280; // cadence (ch 4)
    row.channels[5] = 450;  // motor_p (ch 5)
    row.channels[6] = 50;   // motor_n (ch 6)
    row.channels[7] = 3305; // enc_vcc (ch 9)

    std::string rowCsv = formatBurstRow(42, row);
    std::string expectedRow = "42,1200,3300,0,10,3280,450,50,3305\n";

    EXPECT_EQ(rowCsv, expectedRow);
}

TEST(SignalLoggerCsvFormatTest, BurstCsvFormatHeaderAbove32BitTimestamp) {
    // Regression guard: burstStartUs > 2^32 (71.5 min uptime) must format cleanly
    // without %llu (which corrupts va_list under 32-bit newlib-nano).
    const uint32_t perChanRate = 10416;
    const uint64_t burstStartUs = 5000000000ULL;
    const uint32_t overflowCount = 7;

    const std::string header = formatBurstCsvHeader(perChanRate, burstStartUs, overflowCount);
    const std::string expectedHeader =
        std::string("# sample_rate_per_channel_hz=10416,start_time_us=5000000000,overflow_count=7\n") +
        kBurstCsvColumns;

    EXPECT_EQ(header, expectedHeader);
}

TEST(SignalLoggerCsvFormatTest, EventsCsvFormat) {
    EXPECT_STREQ(kEventsCsvHeader, "timestamp_us,type,channel,value\n");

    EdgeEvent ev{9876543u, EDGE_EVENT_MOTOR_DIRECTION, 5, 2};
    std::string evCsv = formatEventRow(ev);
    EXPECT_EQ(evCsv, "9876543,5,5,2\n");
}
