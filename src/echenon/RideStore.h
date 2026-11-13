#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/RideStore.h
 * Description: PSRAM-backed 1 Hz ride sample storage with 30 s LittleFS crash recovery checkpoints and 5-ride history ring (SPEC.md §5.3, §9.2–§9.3).
 */

#include "echenon/LevelMapping.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace echenon {

// =============================================================================
// RideRecord Flags & Packed 8-Byte Structure (SPEC.md §5.3 & §9.3)
// =============================================================================

static constexpr uint8_t RIDE_FLAG_HR_VALID              = (1u << 0); ///< Flag bit: heart_rate_bpm is valid
static constexpr uint8_t RIDE_FLAG_RESISTANCE_CALIBRATED = (1u << 1); ///< Flag bit: resistance reading is calibrated
static constexpr uint8_t RIDE_FLAG_PAUSE_BOUNDARY        = (1u << 2); ///< Flag bit: sample immediately follows a pause/resume gap

/**
 * @brief Packed 8-byte 1 Hz telemetry sample record stored in PSRAM and `.bin` ride files.
 */
struct __attribute__((packed)) RideRecord {
    union {
        uint16_t timestamp_offset_s;  ///< Seconds since session start (0..65535)
        uint16_t elapsed_sec;         ///< Alias for SPEC.md §9.3
    };
    uint16_t power_watts;             ///< 1 Hz average power from 2 Hz samples (W)
    uint8_t  cadence_rpm;             ///< 1 Hz average cadence from 2 Hz samples (RPM, 0..255)
    uint8_t  heart_rate_bpm;          ///< Instantaneous HR (BPM, 0 if invalid)
    uint8_t  internal_resistance;     ///< Canonical 0..254 = round(254 * pTilde), 255 = UNCALIBRATED
    uint8_t  flags;                   ///< bit 0: hr_valid, bit 1: resistance_calibrated, bit 2: pause_boundary

    /// Normalized cam position p_tilde in [0.0, 1.0] (0.0f when UNCALIBRATED = 255).
    float normalizedResistance() const {
        return internal_resistance <= 254
            ? static_cast<float>(internal_resistance) / 254.0f
            : 0.0f;
    }

    /// Reconstruct Echelon display level 1..32 (returns 0 if UNCALIBRATED = 255).
    uint8_t echelonLevel() const {
        if (internal_resistance == LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED) {
            return 0;
        }
        return LevelMapping::internalToEchelon(internal_resistance);
    }

    /// Reconstruct Peloton 0..100 power-equivalence level (returns 0 if UNCALIBRATED = 255).
    uint8_t pelotonLevel() const {
        return LevelMapping::internalToPeloton(internal_resistance);
    }

    /// Return true if RIDE_FLAG_HR_VALID is set.
    bool isHrValid() const { return (flags & RIDE_FLAG_HR_VALID) != 0; }
    /// Return true if RIDE_FLAG_RESISTANCE_CALIBRATED is set.
    bool isResistanceCalibrated() const { return (flags & RIDE_FLAG_RESISTANCE_CALIBRATED) != 0; }
    /// Return true if RIDE_FLAG_PAUSE_BOUNDARY is set.
    bool isPauseBoundary() const { return (flags & RIDE_FLAG_PAUSE_BOUNDARY) != 0; }
};
static_assert(sizeof(RideRecord) == 8, "RideRecord must be 8 bytes");

// =============================================================================
// Session Header & Summary (RideSummary)
// =============================================================================

/**
 * @brief Aggregate summary statistics for an active or completed ride session.
 */
struct RideSummary {
    uint32_t session_id        = 0;     ///< Unique sequential or epoch-derived session ID
    uint32_t start_epoch_s     = 0;     ///< Session start Unix timestamp (seconds)
    uint16_t duration_s        = 0;     ///< Total elapsed wall-clock duration (seconds)
    uint16_t active_duration_s = 0;     ///< Active pedaling record count (seconds)
    uint16_t record_count      = 0;     ///< Number of 1 Hz RideRecords captured
    uint16_t avg_power_w       = 0;     ///< Mean power across records (W)
    uint16_t max_power_w       = 0;     ///< Peak 1 Hz power (W)
    uint8_t  avg_cadence_rpm   = 0;     ///< Mean cadence across records (RPM)
    uint8_t  avg_hr_bpm        = 0;     ///< Mean valid heart rate (BPM; 0 if no valid HR)
    uint8_t  max_hr_bpm        = 0;     ///< Peak valid heart rate (BPM; 0 if no valid HR)
    uint16_t total_work_kj     = 0;     ///< Total mechanical work (kJ, rounded)
    bool     ntp_valid         = false; ///< True if start_epoch_s was SNTP-synchronized
};

// =============================================================================
// Binary Checkpoint / Stored Ride File Header (16 bytes)
// =============================================================================

static constexpr uint32_t RIDE_FILE_MAGIC   = 0x45434852u;          ///< Binary file magic "ECHR"
static constexpr uint16_t RIDE_FILE_VERSION = 1u;                   ///< Binary ride format schema version
static constexpr uint16_t RIDE_HEADER_FLAG_NTP_VALID = (1u << 0);   ///< Header flag: SNTP wall-clock valid
static constexpr uint16_t RIDE_HEADER_FLAG_FINALIZED = (1u << 1);   ///< Header flag: session finalized cleanly

/**
 * @brief Packed 16-byte header prepended to checkpoint and finalized `.bin` ride files.
 */
struct __attribute__((packed)) RideFileHeader {
    uint32_t magic;         ///< Must equal RIDE_FILE_MAGIC ("ECHR")
    uint16_t version;       ///< Format version (RIDE_FILE_VERSION)
    uint16_t flags;         ///< Bitmask of RIDE_HEADER_FLAG_*
    uint32_t session_id;    ///< Session identifier
    uint32_t start_epoch_s; ///< Session start Unix epoch (seconds)
};
static_assert(sizeof(RideFileHeader) == 16, "RideFileHeader must be 16 bytes");

// =============================================================================
// Injectable Filesystem Abstraction (LittleFS on ESP32, In-Memory in Native Tests)
// =============================================================================

/**
 * @brief Filesystem abstraction decoupler (LittleFS on ESP32, in-memory map on native tests).
 */
class IRideFileSystem {
public:
    virtual ~IRideFileSystem() = default;
    /// Return true if file or directory exists at path.
    virtual bool exists(const char* path) const = 0;
    /// Delete file at path; return true on success.
    virtual bool remove(const char* path) = 0;
    /// Create directory at path (idempotent).
    virtual bool mkdir(const char* path) = 0;
    /// Overwrite file at path with data bytes.
    virtual bool writeFile(const char* path, const uint8_t* data, size_t len) = 0;
    /// Append data bytes to file at path.
    virtual bool appendFile(const char* path, const uint8_t* data, size_t len) = 0;
    /// Read entire file at path into out vector.
    virtual bool readFile(const char* path, std::vector<uint8_t>& out) const = 0;
    /// Return file size in bytes (0 if absent).
    virtual size_t fileSize(const char* path) const = 0;
    /// List filenames (not full paths) in a directory. Returns false if dir doesn't exist.
    virtual bool listDir(const char* dirPath, std::vector<std::string>& out) const = 0;

    /// Convenience std::string overload for exists().
    bool exists(const std::string& path) const { return exists(path.c_str()); }
    /// Convenience std::string overload for remove().
    bool remove(const std::string& path) { return remove(path.c_str()); }
};

/**
 * @brief In-memory IRideFileSystem implementation for native unit tests.
 */
class InMemoryRideFileSystem : public IRideFileSystem {
public:
    using IRideFileSystem::exists;
    using IRideFileSystem::remove;
    /// Return true if path exists in the in-memory map.
    bool exists(const char* path) const override;
    /// Remove path from the in-memory map.
    bool remove(const char* path) override;
    /// No-op directory creation (always returns true).
    bool mkdir(const char* path) override;
    /// Store data bytes at path in memory.
    bool writeFile(const char* path, const uint8_t* data, size_t len) override;
    /// Append data bytes to path in memory.
    bool appendFile(const char* path, const uint8_t* data, size_t len) override;
    /// Read bytes at path into out.
    bool readFile(const char* path, std::vector<uint8_t>& out) const override;
    /// Return stored byte length at path.
    size_t fileSize(const char* path) const override;
    /// List direct child filenames under dirPath.
    bool listDir(const char* dirPath, std::vector<std::string>& out) const override;

    /// Clear all stored in-memory files.
    void clear() { _files.clear(); }
    /// Return read-only map of all stored in-memory files.
    const std::map<std::string, std::vector<uint8_t>>& files() const { return _files; }

private:
    std::map<std::string, std::vector<uint8_t>> _files;
};

// =============================================================================
// RideStore
// =============================================================================

/**
 * @brief PSRAM-backed 1 Hz ride recorder with 30 s crash recovery checkpoints
 *        and a 5-ride FIFO history ring on LittleFS.
 */
class RideStore {
public:
    static constexpr size_t   MAX_RECORDS                  = 14400; ///< 4 hours @ 1 Hz (115,200 B)
    static constexpr uint16_t CHECKPOINT_INTERVAL_RECORDS  = 30;    ///< 30 s checkpoint delta
    static constexpr size_t   MAX_STORED_RIDES             = 5;     ///< 5-ride FIFO history cap
    static constexpr uint16_t DEFAULT_RECOVERY_MIN_RECORDS = 60;    ///< 60 s crash recovery floor
    static constexpr uint32_t VALID_EPOCH_THRESHOLD        = 1700000000u; ///< Minimum Unix epoch considered SNTP-valid

    static constexpr const char* ACTIVE_CHECKPOINT_PATH = "/ride_active.bin";  ///< Crash-recovery checkpoint path
    static constexpr const char* ACTIVE_FILE_PATH       = "/ride_active.bin";  ///< Alias for ACTIVE_CHECKPOINT_PATH
    static constexpr const char* RIDES_DIR              = "/rides";            ///< Directory for finalized ride files
    static constexpr const char* RIDES_INDEX_PATH       = "/rides/index.bin";  ///< Persisted 5-ride RideSummary index

    /// Construct RideStore using customFs (or LittleFS/InMemory default when nullptr).
    explicit RideStore(IRideFileSystem* customFs = nullptr);
    /// Release PSRAM buffer on destruction.
    ~RideStore();

    RideStore(const RideStore&) = delete;
    RideStore& operator=(const RideStore&) = delete;

    // --- Lifecycle ---
    /// Allocate PSRAM sample buffer and load stored rides index from filesystem.
    bool begin();
    /// Free PSRAM sample buffer and reset active session state.
    void end();
    /// Return true if the PSRAM sample buffer is allocated.
    bool isAllocated() const { return _buffer != nullptr; }

    /// Override the backing IRideFileSystem (falls back to platform default if nullptr).
    void setFileSystem(IRideFileSystem* fs);
    /// Return mutable reference to active IRideFileSystem.
    IRideFileSystem& fileSystem() { return *_fs; }
    /// Return const reference to active IRideFileSystem.
    const IRideFileSystem& fileSystem() const { return *_fs; }
    /// Return reference to internal default InMemoryRideFileSystem.
    InMemoryRideFileSystem& memoryFileSystem() { return _defaultMemFs; }

    // --- Active Session Operations ---
    /// Start a new active session with auto-assigned session_id.
    bool beginSession(uint32_t start_epoch_s = 0);
    /// Start a new active session with explicit session_id, start_epoch_s, and ntp_valid flag.
    bool startNewSession(uint32_t session_id, uint32_t start_epoch_s, bool ntp_valid);
    /// Append one 1 Hz RideRecord and trigger checkpoint flush every 30 records.
    bool appendRecord(const RideRecord& record);
    /// Write or append uncheckpointed records to ACTIVE_CHECKPOINT_PATH.
    bool flushCheckpoint();
    /// Update start_epoch_s and rewrite checkpoint header if already written.
    void updateStartEpoch(uint32_t start_epoch_s, bool ntp_valid = true);
    /// Back-compute start_epoch_s from current SNTP epoch and elapsed_s once valid.
    void updateStartEpoch(uint32_t now_epoch_s, uint32_t elapsed_s);
    /// Trim trailing zero-cadence records (up to trim_seconds) and recompute summary.
    uint16_t trimTrailingZeroCadence(uint16_t trim_seconds = 0xFFFFu);
    /// Alias for trimTrailingZeroCadence().
    uint16_t trimTrailingZeros(uint16_t trim_seconds = 0xFFFFu) {
        return trimTrailingZeroCadence(trim_seconds);
    }
    /// Finalize active session to `/rides/ride_<id>.bin`, evict FIFO > 5, and save index.
    bool finalizeSession();
    /// Finalize active session and output the computed RideSummary.
    bool finalizeSession(RideSummary& out_summary);
    /// Clear active session state and remove any checkpoint file without finalizing.
    void clearActiveSession();
    /// Alias for clearActiveSession().
    void discardSession();

    // --- Crash Recovery & Stored Ride Management ---
    /// Recover an interrupted session from ACTIVE_CHECKPOINT_PATH if >= min_records.
    bool recoverInterruptedSession(uint16_t min_records = DEFAULT_RECOVERY_MIN_RECORDS,
                                   RideSummary* out_summary = nullptr);
    /// Discard active or stored session matching session_id and delete its files.
    bool discardSession(uint32_t session_id);
    /// Load a stored ride's summary and 1 Hz records from filesystem.
    bool loadStoredRide(uint32_t session_id,
                        RideSummary& out_summary,
                        std::vector<RideRecord>& out_records) const;
    /// Find stored RideSummary by session_id (nullptr if not found).
    const RideSummary* findStoredRide(uint32_t session_id) const;

    // --- Accessors ---
    /// Return true while a ride session is active.
    bool isSessionActive() const { return _active; }
    /// Return active session_id (0 if none).
    uint32_t getActiveSessionId() const { return _summary.session_id; }
    /// Return active session start Unix epoch (seconds).
    uint32_t getStartEpochS() const { return _summary.start_epoch_s; }
    /// Return raw pointer to PSRAM 1 Hz RideRecord buffer.
    const RideRecord* getRecords() const { return _buffer; }
    /// Return number of 1 Hz records in the active session.
    uint16_t getRecordCount() const { return _recordCount; }
    /// Return number of records persisted in the active checkpoint file.
    uint16_t getCheckpointedCount() const { return _lastCheckpointedCount; }
    /// Return total number of checkpoint flushes performed in the active session.
    uint32_t getCheckpointFlushCount() const { return _checkpointFlushCount; }
    /// Alias for getCheckpointFlushCount().
    uint32_t getCheckpointCount() const { return _checkpointFlushCount; }
    /// Return summary for the active session.
    const RideSummary& getActiveSummary() const { return _summary; }
    /// Return vector of completed rides in the 5-ride history ring.
    const std::vector<RideSummary>& getStoredRides() const { return _storedRides; }
    /// Alias for getStoredRides().
    const std::vector<RideSummary>& getCompletedRides() const { return _storedRides; }

    /// Format canonical `.bin` path for session_id.
    static std::string getRideFilePath(uint32_t session_id);
    /// Alias for getRideFilePath().
    static std::string formatRidePath(uint32_t session_id) { return getRideFilePath(session_id); }
    /// Format canonical `.tcx.gz` path for session_id.
    static std::string getRideTcxGzPath(uint32_t session_id);

private:
    uint32_t _nextSessionSeq = 1;
    void recomputeSummary();
    bool writeFullRideFile(const char* path, bool finalized);
    void saveStoredIndex();
    void loadStoredIndex();

    IRideFileSystem*       _fs = nullptr;
    InMemoryRideFileSystem _defaultMemFs;

    RideRecord* _buffer                = nullptr;
    uint16_t    _recordCount           = 0;
    uint16_t    _lastCheckpointedCount = 0;
    uint32_t    _checkpointFlushCount  = 0;
    bool        _checkpointHeaderWritten = false;
    bool        _active                = false;

    RideSummary              _summary;
    std::vector<RideSummary> _storedRides;
};

} // namespace echenon
