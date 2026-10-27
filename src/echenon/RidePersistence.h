#pragma once

/// @file RidePersistence.h
/// @brief Persistent ride storage to LittleFS with configurable cap and oldest-first eviction.
///
/// Saves completed rides as `.ride.gz` files (gzip on ESP32, uncompressed under
/// UNIT_TEST) to `/rides/persist/` via the `IRideFileSystem` abstraction.
/// Standalone class — not wired into SessionManager or EchenonModule in this WO.

#include "echenon/RideStore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace echenon {

class RidePersistence {
public:
    static constexpr uint32_t DEFAULT_CAP_KB = 512;
    static constexpr uint32_t MIN_CAP_KB     = 64;
    static constexpr uint32_t MAX_CAP_KB     = 2048;
    static constexpr const char* PERSIST_DIR  = "/rides/persist";

    explicit RidePersistence(IRideFileSystem& fs);

    // --- Configuration ---
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setCapKb(uint32_t cap_kb);
    uint32_t getCapKb() const;

    // --- Persistence Operations ---
    bool saveRide(uint32_t session_id, const RideFileHeader& header,
                  const RideRecord* records, uint16_t record_count);
    bool deleteRide(uint32_t session_id);
    bool loadRide(uint32_t session_id,
                  RideFileHeader& out_header,
                  std::vector<RideRecord>& out_records) const;

    // --- Stored Ride Management ---
    struct StoredRideInfo {
        uint32_t session_id;
        uint32_t start_epoch_s;
        uint32_t file_size_bytes;
        uint16_t record_count;
    };
    const std::vector<StoredRideInfo>& getStoredRides() const;
    size_t getStoredCount() const;
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
