#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/RidePersistence.h
 * Description: Persistent LittleFS ride storage (.ride.gz) with configurable cap and oldest-first eviction.
 */

#include "echenon/RideStore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace echenon {

/**
 * @brief Persistent ride archive manager storing compressed `.ride.gz` files
 *        in `/rides/persist/` with configurable storage cap and oldest-first eviction.
 */
class RidePersistence {
public:
    static constexpr uint32_t DEFAULT_CAP_KB = 512;             ///< Default persistent storage cap (KB)
    static constexpr uint32_t MIN_CAP_KB     = 64;              ///< Minimum configurable storage cap (KB)
    static constexpr uint32_t MAX_CAP_KB     = 2048;            ///< Maximum configurable storage cap (KB)
    static constexpr const char* PERSIST_DIR = "/rides/persist"; ///< LittleFS directory for persisted ride files

    /// Construct RidePersistence backed by the given IRideFileSystem.
    explicit RidePersistence(IRideFileSystem& fs);

    // --- Configuration ---
    /// Enable or disable persistent ride archiving.
    void setEnabled(bool enabled);
    /// Return true if persistent ride archiving is enabled.
    bool isEnabled() const;
    /// Set persistent storage cap in KB (clamped to [MIN_CAP_KB, MAX_CAP_KB]).
    void setCapKb(uint32_t cap_kb);
    /// Return configured persistent storage cap in KB.
    uint32_t getCapKb() const;

    // --- Persistence Operations ---
    /// Compress and save a ride file, evicting oldest entries if cap would be exceeded.
    bool saveRide(uint32_t session_id, const RideFileHeader& header,
                  const RideRecord* records, uint16_t record_count);
    /// Delete a persisted ride file by session_id.
    bool deleteRide(uint32_t session_id);
    /// Load and decompress a persisted ride header and its 1 Hz records.
    bool loadRide(uint32_t session_id,
                  RideFileHeader& out_header,
                  std::vector<RideRecord>& out_records) const;

    // --- Stored Ride Management ---
    /// Metadata summary for a persisted `.ride.gz` archive on disk.
    struct StoredRideInfo {
        uint32_t session_id;      ///< Unique session identifier
        uint32_t start_epoch_s;   ///< Session start Unix epoch (seconds)
        uint32_t file_size_bytes; ///< Compressed size on filesystem (bytes)
        uint16_t record_count;    ///< Number of 1 Hz RideRecords in archive
    };
    /// Return metadata for all persisted rides sorted oldest-first by start_epoch_s.
    const std::vector<StoredRideInfo>& getStoredRides() const;
    /// Return number of persisted rides currently stored.
    size_t getStoredCount() const;
    /// Return total disk consumption of all persisted rides in bytes.
    uint32_t getTotalSizeBytes() const;

    // --- Lifecycle ---
    void begin();   ///< Scans PERSIST_DIR for existing .ride.gz files
    void clear();   ///< Deletes all stored rides

private:
    bool compressAndWrite(const char* path, const uint8_t* data, size_t len);
    bool readAndDecompress(const char* path,
                           std::vector<uint8_t>& out) const;
    void evictOldest();
    void scanStoredRides();
    static std::string ridePath(uint32_t session_id);

    IRideFileSystem& _fs;
    bool             _enabled = false;
    uint32_t         _capKb   = DEFAULT_CAP_KB;
    std::vector<StoredRideInfo> _storedRides;
};

} // namespace echenon
