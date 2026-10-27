/// @file test_ride_persistence.cpp
/// @brief Native unit tests for RidePersistence save/load/delete, cap enforcement,
///        eviction, and begin() scan functionality.

#include <gtest/gtest.h>
#include "echenon/RideStore.h"
// NOTE: RideStore.cpp is unity-included by test_session_manager.cpp
#include "echenon/RidePersistence.h"
#include "echenon/RidePersistence.cpp"

using namespace echenon;

namespace {

/// Helper: create a valid RideFileHeader with the given session_id and epoch.
RideFileHeader makeHeader(uint32_t session_id, uint32_t epoch) {
    RideFileHeader hdr{};
    hdr.magic        = RIDE_FILE_MAGIC;
    hdr.version      = RIDE_FILE_VERSION;
    hdr.flags        = RIDE_HEADER_FLAG_FINALIZED;
    hdr.session_id   = session_id;
    hdr.start_epoch_s = epoch;
    return hdr;
}

/// Helper: create N sample RideRecords.
std::vector<RideRecord> makeRecords(uint16_t count, uint16_t power = 200) {
    std::vector<RideRecord> recs(count);
    for (uint16_t i = 0; i < count; ++i) {
        recs[i].timestamp_offset_s  = i;
        recs[i].power_watts         = power;
        recs[i].cadence_rpm         = 85;
        recs[i].heart_rate_bpm      = 145;
        recs[i].internal_resistance = 120;
        recs[i].flags               = RIDE_FLAG_HR_VALID | RIDE_FLAG_RESISTANCE_CALIBRATED;
    }
    return recs;
}

} // namespace

// =============================================================================
// 1. Save and Load Ride — round-trip verification
// =============================================================================

TEST(RidePersistenceTest, SaveAndLoadRide) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    persist.begin();

    RideFileHeader hdr = makeHeader(1u, 1710000000u);
    auto recs = makeRecords(10, 200);

    ASSERT_TRUE(persist.saveRide(1u, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    EXPECT_EQ(persist.getStoredCount(), 1u);

    // Load and verify round-trip
    RideFileHeader loadedHdr{};
    std::vector<RideRecord> loadedRecs;
    ASSERT_TRUE(persist.loadRide(1u, loadedHdr, loadedRecs));

    EXPECT_EQ(loadedHdr.magic, RIDE_FILE_MAGIC);
    EXPECT_EQ(loadedHdr.version, RIDE_FILE_VERSION);
    EXPECT_EQ(loadedHdr.session_id, 1u);
    EXPECT_EQ(loadedHdr.start_epoch_s, 1710000000u);
    ASSERT_EQ(loadedRecs.size(), 10u);

    for (uint16_t i = 0; i < 10; ++i) {
        EXPECT_EQ(loadedRecs[i].timestamp_offset_s, i);
        EXPECT_EQ(loadedRecs[i].power_watts, 200u);
        EXPECT_EQ(loadedRecs[i].cadence_rpm, 85u);
    }
}

// =============================================================================
// 2. Delete Ride — save, delete, verify not found
// =============================================================================

TEST(RidePersistenceTest, DeleteRide) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    persist.begin();

    RideFileHeader hdr = makeHeader(1u, 1710000000u);
    auto recs = makeRecords(5);
    ASSERT_TRUE(persist.saveRide(1u, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    EXPECT_EQ(persist.getStoredCount(), 1u);

    EXPECT_TRUE(persist.deleteRide(1u));
    EXPECT_EQ(persist.getStoredCount(), 0u);

    // Load should fail
    RideFileHeader loadedHdr{};
    std::vector<RideRecord> loadedRecs;
    EXPECT_FALSE(persist.loadRide(1u, loadedHdr, loadedRecs));
}

// =============================================================================
// 3. Stored Rides List — save 3 rides, verify list ordered by epoch
// =============================================================================

TEST(RidePersistenceTest, StoredRidesList) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    persist.begin();

    // Save 3 rides with different epochs (out of order)
    for (uint32_t i = 1; i <= 3; ++i) {
        RideFileHeader hdr = makeHeader(i, 1710000000u + (4 - i) * 1000u); // reverse epoch order
        auto recs = makeRecords(5);
        ASSERT_TRUE(persist.saveRide(i, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    }

    EXPECT_EQ(persist.getStoredCount(), 3u);

    // Verify sorted oldest-first by start_epoch_s
    const auto& rides = persist.getStoredRides();
    ASSERT_EQ(rides.size(), 3u);
    EXPECT_LT(rides[0].start_epoch_s, rides[1].start_epoch_s);
    EXPECT_LT(rides[1].start_epoch_s, rides[2].start_epoch_s);
}

// =============================================================================
// 4. Oldest-First Eviction — save rides exceeding cap, verify oldest evicted
// =============================================================================

TEST(RidePersistenceTest, OldestFirstEviction) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    // Set a very small cap to trigger eviction
    persist.setCapKb(RidePersistence::MIN_CAP_KB); // 64 KB
    persist.begin();

    // Each ride with 500 records = 500 * 8 bytes (RideRecord) + 16 bytes (header) = 4016 bytes
    // 64 KB = 65536 bytes → ~16 rides should fit, so save 20 rides to trigger eviction
    const uint16_t recordsPerRide = 500;
    for (uint32_t i = 1; i <= 20; ++i) {
        RideFileHeader hdr = makeHeader(i, 1710000000u + i * 1000u);
        auto recs = makeRecords(recordsPerRide);
        ASSERT_TRUE(persist.saveRide(i, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    }

    // Total size should be at or under the cap
    EXPECT_LE(persist.getTotalSizeBytes(), RidePersistence::MIN_CAP_KB * 1024u);
    // Some rides should have been evicted
    EXPECT_LT(persist.getStoredCount(), 20u);

    // The oldest surviving ride should not be session 1 (it was evicted)
    const auto& rides = persist.getStoredRides();
    ASSERT_GT(rides.size(), 0u);
    EXPECT_GT(rides[0].session_id, 1u);
}

// =============================================================================
// 5. Cap Enforcement — set small cap, verify eviction triggers
// =============================================================================

TEST(RidePersistenceTest, CapEnforcement) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    persist.begin();

    // Set cap below minimum — should clamp to MIN_CAP_KB
    persist.setCapKb(10u);
    EXPECT_EQ(persist.getCapKb(), RidePersistence::MIN_CAP_KB);

    // Set cap above maximum — should clamp to MAX_CAP_KB
    persist.setCapKb(10000u);
    EXPECT_EQ(persist.getCapKb(), RidePersistence::MAX_CAP_KB);

    // Set valid cap
    persist.setCapKb(256u);
    EXPECT_EQ(persist.getCapKb(), 256u);
}

// =============================================================================
// 6. Disabled Does Not Save — setEnabled(false), verify saveRide returns false
// =============================================================================

TEST(RidePersistenceTest, DisabledDoesNotSave) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(false);
    persist.begin();

    RideFileHeader hdr = makeHeader(1u, 1710000000u);
    auto recs = makeRecords(5);
    EXPECT_FALSE(persist.saveRide(1u, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    EXPECT_EQ(persist.getStoredCount(), 0u);
}

// =============================================================================
// 7. Begin Scans Existing Files — pre-populate filesystem, call begin(), verify index
// =============================================================================

TEST(RidePersistenceTest, BeginScansExistingFiles) {
    InMemoryRideFileSystem fs;

    // Phase 1: Save rides using one RidePersistence instance
    {
        RidePersistence writer(fs);
        writer.setEnabled(true);
        writer.begin();

        for (uint32_t i = 1; i <= 3; ++i) {
            RideFileHeader hdr = makeHeader(i, 1710000000u + i * 1000u);
            auto recs = makeRecords(8);
            ASSERT_TRUE(writer.saveRide(i, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
        }
        EXPECT_EQ(writer.getStoredCount(), 3u);
    }

    // Phase 2: Create a new RidePersistence on the same filesystem — begin() should scan
    {
        RidePersistence scanner(fs);
        scanner.setEnabled(true);
        scanner.begin();

        EXPECT_EQ(scanner.getStoredCount(), 3u);

        // Verify that loaded rides are consistent
        RideFileHeader loadedHdr{};
        std::vector<RideRecord> loadedRecs;
        ASSERT_TRUE(scanner.loadRide(2u, loadedHdr, loadedRecs));
        EXPECT_EQ(loadedHdr.session_id, 2u);
        EXPECT_EQ(loadedRecs.size(), 8u);
    }
}

// =============================================================================
// 8. Clear Removes All — save, clear, verify empty
// =============================================================================

TEST(RidePersistenceTest, ClearRemovesAll) {
    InMemoryRideFileSystem fs;
    RidePersistence persist(fs);
    persist.setEnabled(true);
    persist.begin();

    for (uint32_t i = 1; i <= 3; ++i) {
        RideFileHeader hdr = makeHeader(i, 1710000000u + i * 1000u);
        auto recs = makeRecords(5);
        ASSERT_TRUE(persist.saveRide(i, hdr, recs.data(), static_cast<uint16_t>(recs.size())));
    }
    ASSERT_EQ(persist.getStoredCount(), 3u);
    EXPECT_GT(persist.getTotalSizeBytes(), 0u);

    persist.clear();

    EXPECT_EQ(persist.getStoredCount(), 0u);
    EXPECT_EQ(persist.getTotalSizeBytes(), 0u);

    // Verify files are actually removed
    RideFileHeader loadedHdr{};
    std::vector<RideRecord> loadedRecs;
    EXPECT_FALSE(persist.loadRide(1u, loadedHdr, loadedRecs));
    EXPECT_FALSE(persist.loadRide(2u, loadedHdr, loadedRecs));
    EXPECT_FALSE(persist.loadRide(3u, loadedHdr, loadedRecs));
}
