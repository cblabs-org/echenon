/// @file test_tcx_generator.cpp
/// @brief Native unit tests for TCX/TCX.gz generation, gzip round-trip, and virtual speed.

#include <gtest/gtest.h>

#include "echenon/PowerEngine.h"
#include "echenon/RideStore.h"
#include "echenon/SessionManager.h"
#include "echenon/TCXGenerator.h"
#include "echenon/TCXGenerator.cpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace echenon;

namespace {

RideRecord makeRecord(
    uint16_t offset_s,
    uint16_t watts,
    uint8_t cadence_rpm,
    uint8_t hr_bpm = 0,
    bool hr_valid = false,
    uint8_t internal_res = 100) {
    RideRecord r{};
    r.timestamp_offset_s  = offset_s;
    r.power_watts         = watts;
    r.cadence_rpm         = cadence_rpm;
    r.heart_rate_bpm      = hr_bpm;
    r.internal_resistance = internal_res;
    r.flags               = RIDE_FLAG_RESISTANCE_CALIBRATED |
                            (hr_valid ? RIDE_FLAG_HR_VALID : 0u);
    return r;
}

size_t countSubstring(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

// =============================================================================
// 1. ISO-8601 UTC Timestamp Formatting & IEEE 802.3 CRC32 Tests
// =============================================================================

TEST(TCXGeneratorTest, Iso8601UtcFormattingAndCrc32) {
    EXPECT_EQ(TCXGenerator::formatIso8601Utc(0u), "1970-01-01T00:00:00Z");
    // 2024-02-29T12:34:56Z (leap year day)
    EXPECT_EQ(TCXGenerator::formatIso8601Utc(1709210096u), "2024-02-29T12:34:56Z");
    // 2026-09-17T21:00:00Z (SPEC.md §9.4 example timestamp)
    EXPECT_EQ(TCXGenerator::formatIso8601Utc(1789678800u), "2026-09-17T21:00:00Z");

    // Standard IEEE 802.3 CRC32 check value for ASCII "123456789" is 0xCBF43926
    const char* check_str = "123456789";
    const uint32_t crc = TCXGenerator::computeCrc32(
        reinterpret_cast<const uint8_t*>(check_str), 9);
    EXPECT_EQ(crc, 0xCBF43926u);
}

// =============================================================================
// 2. Synthetic Ride XML Structure, Virtual Speed, Coasting, Pause Gap & HR
// =============================================================================

TEST(TCXGeneratorTest, SyntheticRideXmlSchemaVirtualSpeedCoastingPauseAndHr) {
    // Start at 2026-09-17T21:00:00Z (1789678800)
    constexpr uint32_t kStartEpoch = 1789678800u;

    // Build synthetic ride:
    // t=0: Active pedalling 200 W, 85 RPM, HR=142 valid
    // t=1: Active pedalling 265 W, 90 RPM, HR=145 valid
    // t=2: Active coasting    0 W,  0 RPM, HR=140 valid (Xert MPA coasting continuity)
    // t=3: Active coasting   50 W (stale), 0 RPM, HR=0 invalid -> must force 0 W & 0.00 m/s & omit HR
    // [60-second manual pause gap: t=4..63 no samples recorded]
    // t=64: Resumed pedalling 180 W, 80 RPM, HR=150 invalid (flag=0) -> omit HR tag
    // t=65: Resumed pedalling 220 W, 88 RPM, HR=152 valid
    std::vector<RideRecord> records = {
        makeRecord(0,  200, 85, 142, true),
        makeRecord(1,  265, 90, 145, true),
        makeRecord(2,    0,  0, 140, true),
        makeRecord(3,   50,  0,   0, false),
        makeRecord(64, 180, 80, 150, false),
        makeRecord(65, 220, 88, 152, true),
    };

    RideSummary summary{};
    summary.session_id        = 7;
    summary.start_epoch_s     = kStartEpoch;
    summary.duration_s        = 66;
    summary.active_duration_s = 6;
    summary.record_count      = static_cast<uint16_t>(records.size());
    summary.avg_power_w       = 144;
    summary.max_power_w       = 265;
    summary.avg_cadence_rpm   = 57;
    summary.avg_hr_bpm        = 145;
    summary.max_hr_bpm        = 152;
    summary.total_work_kj     = 1;
    summary.ntp_valid         = true;

    std::string xml;
    const bool ok = TCXGenerator::generateTcx(
        summary,
        records,
        [&xml](const uint8_t* data, size_t len) -> bool {
            xml.append(reinterpret_cast<const char*>(data), len);
            return true;
        });
    ASSERT_TRUE(ok);
    ASSERT_FALSE(xml.empty());

    // 1. Verify root schema & Creator/Author metadata
    EXPECT_NE(xml.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), std::string::npos);
    EXPECT_NE(xml.find("<TrainingCenterDatabase xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\""),
              std::string::npos);
    EXPECT_NE(xml.find("xmlns:ns3=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\""),
              std::string::npos);
    EXPECT_NE(xml.find("<Activity Sport=\"Biking\">"), std::string::npos);
    EXPECT_NE(xml.find("<Id>2026-09-17T21:00:00Z</Id>"), std::string::npos);
    EXPECT_NE(xml.find("<Lap StartTime=\"2026-09-17T21:00:00Z\">"), std::string::npos);
    EXPECT_NE(xml.find("<TotalTimeSeconds>66.00</TotalTimeSeconds>"), std::string::npos);
    EXPECT_NE(xml.find("<Calories>1</Calories>"), std::string::npos);
    EXPECT_NE(xml.find("<AverageHeartRateBpm><Value>145</Value></AverageHeartRateBpm>"), std::string::npos);
    EXPECT_NE(xml.find("<MaximumHeartRateBpm><Value>152</Value></MaximumHeartRateBpm>"), std::string::npos);
    EXPECT_NE(xml.find("<Intensity>Active</Intensity>"), std::string::npos);
    EXPECT_NE(xml.find("<TriggerMethod>Manual</TriggerMethod>"), std::string::npos);
    EXPECT_NE(xml.find("<ns3:AvgWatts>144</ns3:AvgWatts>"), std::string::npos);
    EXPECT_NE(xml.find("<ns3:MaxWatts>265</ns3:MaxWatts>"), std::string::npos);
    EXPECT_NE(xml.find("<Name>Echenon EX-3</Name>"), std::string::npos);
    EXPECT_NE(xml.find("<Name>Echenon Firmware</Name>"), std::string::npos);
    EXPECT_NE(xml.find("</TrainingCenterDatabase>"), std::string::npos);

    // 2. Verify exact number of emitted <Trackpoint> elements (6 records -> 6 trackpoints)
    EXPECT_EQ(countSubstring(xml, "<Trackpoint>"), 6u);
    EXPECT_EQ(countSubstring(xml, "</Trackpoint>"), 6u);

    // 3. Verify virtual speed v_mps = PowerEngine::computeVirtualSpeedKph(W) / 3.6f
    //    and monotonic cumulative distance integration
    const float v0 = PowerEngine::computeVirtualSpeedKph(200.0f) / 3.6f;
    const float v1 = PowerEngine::computeVirtualSpeedKph(265.0f) / 3.6f;
    const float v2 = 0.0f; // Coasting
    const float v3 = 0.0f; // Coasting with stale power_watts
    const float v4 = PowerEngine::computeVirtualSpeedKph(180.0f) / 3.6f;
    const float v5 = PowerEngine::computeVirtualSpeedKph(220.0f) / 3.6f;

    const float d0 = v0;
    const float d1 = d0 + v1;
    const float d2 = d1 + v2;
    const float d3 = d2 + v3;
    const float d4 = d3 + v4;
    const float d5 = d4 + v5;

    char expected_tag[128];
    std::snprintf(expected_tag, sizeof(expected_tag), "<ns3:Speed>%.2f</ns3:Speed>", static_cast<double>(v0));
    EXPECT_NE(xml.find(expected_tag), std::string::npos);

    std::snprintf(expected_tag, sizeof(expected_tag), "<ns3:Speed>%.2f</ns3:Speed>", static_cast<double>(v1));
    EXPECT_NE(xml.find(expected_tag), std::string::npos);

    std::snprintf(expected_tag, sizeof(expected_tag), "<DistanceMeters>%.2f</DistanceMeters>", static_cast<double>(d0));
    EXPECT_NE(xml.find(expected_tag), std::string::npos);

    std::snprintf(expected_tag, sizeof(expected_tag), "<DistanceMeters>%.2f</DistanceMeters>", static_cast<double>(d1));
    EXPECT_NE(xml.find(expected_tag), std::string::npos);

    std::snprintf(expected_tag, sizeof(expected_tag), "<DistanceMeters>%.2f</DistanceMeters>", static_cast<double>(d5));
    // Appears in both <Lap> header and final <Trackpoint>
    EXPECT_GE(countSubstring(xml, expected_tag), 2u);

    // 4. Verify coasting trackpoints (t=2 and t=3) are emitted with 0 RPM, 0.00 m/s, 0 W
    EXPECT_NE(xml.find("<Time>2026-09-17T21:00:02Z</Time>"), std::string::npos);
    EXPECT_NE(xml.find("<Time>2026-09-17T21:00:03Z</Time>"), std::string::npos);
    EXPECT_EQ(countSubstring(xml, "<Cadence>0</Cadence>"), 2u);
    EXPECT_EQ(countSubstring(xml, "<ns3:Speed>0.00</ns3:Speed>"), 2u);
    EXPECT_EQ(countSubstring(xml, "<ns3:Watts>0</ns3:Watts>"), 2u);

    // 5. Verify 60-second pause gap: no trackpoints between 21:00:04Z and 21:01:03Z
    EXPECT_EQ(xml.find("<Time>2026-09-17T21:00:04Z</Time>"), std::string::npos);
    EXPECT_EQ(xml.find("<Time>2026-09-17T21:00:30Z</Time>"), std::string::npos);
    EXPECT_EQ(xml.find("<Time>2026-09-17T21:01:03Z</Time>"), std::string::npos);
    // Resumed at t=64 (21:01:04Z) and t=65 (21:01:05Z)
    EXPECT_NE(xml.find("<Time>2026-09-17T21:01:04Z</Time>"), std::string::npos);
    EXPECT_NE(xml.find("<Time>2026-09-17T21:01:05Z</Time>"), std::string::npos);

    // 6. Verify <HeartRateBpm> is emitted ONLY on the 4 valid-HR trackpoints (not on t=3 or t=64)
    EXPECT_EQ(countSubstring(xml, "<HeartRateBpm><Value>"), 4u);
    EXPECT_EQ(xml.find("<HeartRateBpm><Value>150</Value></HeartRateBpm>"), std::string::npos);
    EXPECT_NE(xml.find("<HeartRateBpm><Value>152</Value></HeartRateBpm>"), std::string::npos);
}

// =============================================================================
// 3. Omission of Lap & Trackpoint HR When Session Has No Valid HR
// =============================================================================

TEST(TCXGeneratorTest, OmitLapAndTrackpointHeartRateWhenNoValidHrSamplesExist) {
    std::vector<RideRecord> records = {
        makeRecord(0, 150, 80, 0, false),
        makeRecord(1, 175, 84, 140, false), // HR value present but RIDE_FLAG_HR_VALID == 0
    };

    RideSummary summary{};
    summary.start_epoch_s = 1789678800u;

    std::string xml;
    ASSERT_TRUE(TCXGenerator::generateTcx(
        summary,
        records,
        [&xml](const uint8_t* data, size_t len) -> bool {
            xml.append(reinterpret_cast<const char*>(data), len);
            return true;
        }));

    EXPECT_EQ(xml.find("AverageHeartRateBpm"), std::string::npos);
    EXPECT_EQ(xml.find("MaximumHeartRateBpm"), std::string::npos);
    EXPECT_EQ(xml.find("HeartRateBpm"), std::string::npos);
}

// =============================================================================
// 4. Bounded 4 KB Scratch Buffer Streaming & Deadline Cancellation
// =============================================================================

TEST(TCXGeneratorTest, Bounded4KBScratchBufferAndDeadlineCancellation) {
    std::vector<RideRecord> records;
    records.reserve(250);
    for (uint16_t i = 0; i < 250; ++i) {
        records.push_back(makeRecord(i, 210, 88, 148, true));
    }

    RideSummary summary{};
    summary.session_id    = 42;
    summary.start_epoch_s = 1789678800u;

    size_t chunk_calls = 0;
    size_t max_chunk_size = 0;
    size_t total_bytes = 0;

    ASSERT_TRUE(TCXGenerator::generateTcx(
        summary,
        records,
        [&](const uint8_t* data, size_t len) -> bool {
            EXPECT_NE(data, nullptr);
            EXPECT_GT(len, 0u);
            EXPECT_LE(len, TCXGenerator::SCRATCH_BUFFER_SIZE);
            ++chunk_calls;
            if (len > max_chunk_size) max_chunk_size = len;
            total_bytes += len;
            return true;
        }));

    EXPECT_GT(chunk_calls, 10u);
    EXPECT_EQ(max_chunk_size, TCXGenerator::SCRATCH_BUFFER_SIZE);
    EXPECT_GT(total_bytes, 50000u);

    // Deadline expiry mid-stream aborts cleanly and returns false
    size_t deadline_checks = 0;
    const bool aborted = TCXGenerator::generateTcx(
        summary,
        records,
        [](const uint8_t*, size_t) -> bool { return true; },
        [&deadline_checks]() -> bool {
            ++deadline_checks;
            return deadline_checks <= 3;
        });
    EXPECT_FALSE(aborted);
    EXPECT_EQ(deadline_checks, 4u);
}

// =============================================================================
// 5. Gzip (.tcx.gz) RFC 1952 Header, CRC32/ISIZE Trailer & Decompression Round-Trip
// =============================================================================

TEST(TCXGeneratorTest, GzipStreamAndRideStoreFileRoundTripDecompression) {
    InMemoryRideFileSystem fs;
    RideStore store(&fs);
    ASSERT_TRUE(store.begin());
    ASSERT_TRUE(store.startNewSession(99, 1789678800u, true));

    for (uint16_t i = 0; i < 65; ++i) {
        const bool coasting = (i >= 20 && i < 25);
        ASSERT_TRUE(store.appendRecord(makeRecord(
            i,
            coasting ? 0 : static_cast<uint16_t>(180 + (i % 40)),
            coasting ? 0 : static_cast<uint8_t>(80 + (i % 15)),
            static_cast<uint8_t>(135 + (i % 20)),
            (i % 5) != 0)));
    }
    RideSummary finalized_summary{};
    ASSERT_TRUE(store.finalizeSession(finalized_summary));

    // Generate reference uncompressed .tcx from store
    std::string expected_xml;
    ASSERT_TRUE(TCXGenerator::generateTcxFromStore(
        store,
        99,
        [&expected_xml](const uint8_t* data, size_t len) -> bool {
            expected_xml.append(reinterpret_cast<const char*>(data), len);
            return true;
        }));
    ASSERT_FALSE(expected_xml.empty());

    // Stream .tcx.gz to file via RideStore helper
    ASSERT_TRUE(TCXGenerator::generateTcxGzToFile(store, 99));
    const std::string gz_path = RideStore::getRideTcxGzPath(99);
    ASSERT_TRUE(fs.exists(gz_path));

    std::vector<uint8_t> gz_bytes;
    ASSERT_TRUE(fs.readFile(gz_path.c_str(), gz_bytes));
    ASSERT_GT(gz_bytes.size(), 18u);

    // Verify RFC 1952 gzip header magic & compression method
    EXPECT_EQ(gz_bytes[0], 0x1f);
    EXPECT_EQ(gz_bytes[1], 0x8b);
    EXPECT_EQ(gz_bytes[2], 0x08);

    // Verify meaningful compression ratio on repetitive TCX XML
    EXPECT_LT(gz_bytes.size(), expected_xml.size() / 2);

    // Decompress and verify exact byte-for-byte match + CRC32 + ISIZE
    std::vector<uint8_t> decompressed;
    ASSERT_TRUE(TCXGenerator::decompressGzip(gz_bytes.data(), gz_bytes.size(), decompressed));
    const std::string actual_xml(
        reinterpret_cast<const char*>(decompressed.data()),
        decompressed.size());
    EXPECT_EQ(actual_xml, expected_xml);

    // Verify corrupted trailer CRC32 is rejected by decompressGzip
    gz_bytes[gz_bytes.size() - 8] ^= 0x55u;
    std::vector<uint8_t> corrupted_out;
    EXPECT_FALSE(TCXGenerator::decompressGzip(gz_bytes.data(), gz_bytes.size(), corrupted_out));

    // Verify deadline failure during generateTcxGzToFile removes partial output file
    const std::string partial_path = "/rides/ride_99_partial.tcx.gz";
    EXPECT_FALSE(TCXGenerator::generateTcxGzToFile(
        store,
        99,
        partial_path,
        []() -> bool { return false; }));
    EXPECT_FALSE(fs.exists(partial_path));
}

TEST(TCXGeneratorTest, CompressGzipArbitraryBinaryRoundTrip) {
    // Generate synthetic binary payload (e.g. serialized RideRecords)
    std::vector<uint8_t> payload(1024);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 37) & 0xFF);
    }

    std::vector<uint8_t> compressed;
    ASSERT_TRUE(TCXGenerator::compressGzip(payload.data(), payload.size(), compressed));
    ASSERT_GE(compressed.size(), 18u);
    EXPECT_EQ(compressed[0], 0x1f);
    EXPECT_EQ(compressed[1], 0x8b);
    EXPECT_EQ(compressed[2], 0x08);

    std::vector<uint8_t> decompressed;
    ASSERT_TRUE(TCXGenerator::decompressGzip(compressed.data(), compressed.size(), decompressed));
    EXPECT_EQ(decompressed, payload);
}
