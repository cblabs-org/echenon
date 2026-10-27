#pragma once

/// @file RideStore.h
/// @brief PSRAM-backed 1 Hz ride sample storage with 30 s LittleFS crash recovery
///        checkpoints and 5-ride completed history ring (SPEC.md §5.3, §9.2–§9.3).

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

static constexpr uint8_t RIDE_FLAG_HR_VALID              = (1u << 0);
static constexpr uint8_t RIDE_FLAG_RESISTANCE_CALIBRATED = (1u << 1);
static constexpr uint8_t RIDE_FLAG_PAUSE_BOUNDARY        = (1u << 2);

struct __attribute__((packed)) RideRecord {
    union {
        uint16_t timestamp_offset_s;  // Seconds since session start (0..65535)
        uint16_t elapsed_sec;         // Alias for SPEC.md §9.3
    };
    uint16_t power_watts;             // 1 Hz average power from 2 Hz samples (W)
    uint8_t  cadence_rpm;             // 1 Hz average cadence from 2 Hz samples (RPM, 0..255)
    uint8_t  heart_rate_bpm;          // Instantaneous HR (BPM, 0 if invalid)
    uint8_t  internal_resistance;     // Canonical 0..254 = round(254 * pTilde), 255 = UNCALIBRATED
    uint8_t  flags;                   // bit 0: hr_valid, bit 1: resistance_calibrated, bit 2: pause_boundary

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

    bool isHrValid() const { return (flags & RIDE_FLAG_HR_VALID) != 0; }
    bool isResistanceCalibrated() const { return (flags & RIDE_FLAG_RESISTANCE_CALIBRATED) != 0; }
    bool isPauseBoundary() const { return (flags & RIDE_FLAG_PAUSE_BOUNDARY) != 0; }
};
static_assert(sizeof(RideRecord) == 8, "RideRecord must be 8 bytes");

// =============================================================================
// Session Header & Summary (RideSummary)
// =============================================================================

struct RideSummary {
    uint32_t session_id        = 0;
    uint32_t start_epoch_s     = 0;
    uint16_t duration_s        = 0;
    uint16_t active_duration_s = 0;
    uint16_t record_count      = 0;
    uint16_t avg_power_w       = 0;
    uint16_t max_power_w       = 0;
    uint8_t  avg_cadence_rpm   = 0;
    uint8_t  avg_hr_bpm        = 0;
    uint8_t  max_hr_bpm        = 0;
    uint16_t total_work_kj     = 0;
    bool     ntp_valid         = false;
};

// =============================================================================
// Binary Checkpoint / Stored Ride File Header (16 bytes)
// =============================================================================

static constexpr uint32_t RIDE_FILE_MAGIC   = 0x45434852u; // "ECHR"
static constexpr uint16_t RIDE_FILE_VERSION = 1u;
static constexpr uint16_t RIDE_HEADER_FLAG_NTP_VALID = (1u << 0);
static constexpr uint16_t RIDE_HEADER_FLAG_FINALIZED = (1u << 1);

struct __attribute__((packed)) RideFileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t session_id;
    uint32_t start_epoch_s;
};
static_assert(sizeof(RideFileHeader) == 16, "RideFileHeader must be 16 bytes");

// =============================================================================
// Injectable Filesystem Abstraction (LittleFS on ESP32, In-Memory in Native Tests)
// =============================================================================

class IRideFileSystem {
public:
    virtual ~IRideFileSystem() = default;
    virtual bool exists(const char* path) const = 0;
    virtual bool remove(const char* path) = 0;
    virtual bool mkdir(const char* path) = 0;
    virtual bool writeFile(const char* path, const uint8_t* data, size_t len) = 0;
    virtual bool appendFile(const char* path, const uint8_t* data, size_t len) = 0;
    virtual bool readFile(const char* path, std::vector<uint8_t>& out) const = 0;
    virtual size_t fileSize(const char* path) const = 0;
    /// List filenames (not full paths) in a directory. Returns false if dir doesn't exist.
    virtual bool listDir(const char* dirPath, std::vector<std::string>& out) const = 0;

    bool exists(const std::string& path) const { return exists(path.c_str()); }
    bool remove(const std::string& path) { return remove(path.c_str()); }
};

class InMemoryRideFileSystem : public IRideFileSystem {
public:
    using IRideFileSystem::exists;
    using IRideFileSystem::remove;
    bool exists(const char* path) const override;
    bool remove(const char* path) override;
    bool mkdir(const char* path) override;
    bool writeFile(const char* path, const uint8_t* data, size_t len) override;
    bool appendFile(const char* path, const uint8_t* data, size_t len) override;
    bool readFile(const char* path, std::vector<uint8_t>& out) const override;
    size_t fileSize(const char* path) const override;
    bool listDir(const char* dirPath, std::vector<std::string>& out) const override;

    void clear() { _files.clear(); }
    const std::map<std::string, std::vector<uint8_t>>& files() const { return _files; }

private:
    std::map<std::string, std::vector<uint8_t>> _files;
};

// =============================================================================
// RideStore
// =============================================================================

class RideStore {
public:
    static constexpr size_t   MAX_RECORDS                  = 14400; // 4 hours @ 1 Hz (115,200 B)
    static constexpr uint16_t CHECKPOINT_INTERVAL_RECORDS  = 30;    // 30 s checkpoint delta
    static constexpr size_t   MAX_STORED_RIDES             = 5;     // 5-ride FIFO history cap
    static constexpr uint16_t DEFAULT_RECOVERY_MIN_RECORDS = 60;    // 60 s crash recovery floor
    static constexpr uint32_t VALID_EPOCH_THRESHOLD        = 1700000000u;

    static constexpr const char* ACTIVE_CHECKPOINT_PATH = "/ride_active.bin";
    static constexpr const char* ACTIVE_FILE_PATH       = "/ride_active.bin";
    static constexpr const char* RIDES_DIR              = "/rides";
    static constexpr const char* RIDES_INDEX_PATH       = "/rides/index.bin";

    explicit RideStore(IRideFileSystem* customFs = nullptr);
    ~RideStore();

    RideStore(const RideStore&) = delete;
    RideStore& operator=(const RideStore&) = delete;

    // --- Lifecycle ---
    bool begin();
    void end();
    bool isAllocated() const { return _buffer != nullptr; }

    void setFileSystem(IRideFileSystem* fs);
    IRideFileSystem& fileSystem() { return *_fs; }
    const IRideFileSystem& fileSystem() const { return *_fs; }
    InMemoryRideFileSystem& memoryFileSystem() { return _defaultMemFs; }

    // --- Active Session Operations ---
    bool beginSession(uint32_t start_epoch_s = 0);
    bool startNewSession(uint32_t session_id, uint32_t start_epoch_s, bool ntp_valid);
    bool appendRecord(const RideRecord& record);
    bool flushCheckpoint();
    void updateStartEpoch(uint32_t start_epoch_s, bool ntp_valid = true);
    void updateStartEpoch(uint32_t now_epoch_s, uint32_t elapsed_s);
    uint16_t trimTrailingZeroCadence(uint16_t trim_seconds = 0xFFFFu);
    uint16_t trimTrailingZeros(uint16_t trim_seconds = 0xFFFFu) {
        return trimTrailingZeroCadence(trim_seconds);
    }
    bool finalizeSession();
    bool finalizeSession(RideSummary& out_summary);
    void clearActiveSession();
    void discardSession();

    // --- Crash Recovery & Stored Ride Management ---
    bool recoverInterruptedSession(uint16_t min_records = DEFAULT_RECOVERY_MIN_RECORDS,
                                   RideSummary* out_summary = nullptr);
    bool discardSession(uint32_t session_id);
    bool loadStoredRide(uint32_t session_id,
                        RideSummary& out_summary,
                        std::vector<RideRecord>& out_records) const;
    const RideSummary* findStoredRide(uint32_t session_id) const;

    // --- Accessors ---
    bool isSessionActive() const { return _active; }
    uint32_t getActiveSessionId() const { return _summary.session_id; }
    uint32_t getStartEpochS() const { return _summary.start_epoch_s; }
    const RideRecord* getRecords() const { return _buffer; }
    uint16_t getRecordCount() const { return _recordCount; }
    uint16_t getCheckpointedCount() const { return _lastCheckpointedCount; }
    uint32_t getCheckpointFlushCount() const { return _checkpointFlushCount; }
    uint32_t getCheckpointCount() const { return _checkpointFlushCount; }
    const RideSummary& getActiveSummary() const { return _summary; }
    const std::vector<RideSummary>& getStoredRides() const { return _storedRides; }
    const std::vector<RideSummary>& getCompletedRides() const { return _storedRides; }

    static std::string getRideFilePath(uint32_t session_id);
    static std::string formatRidePath(uint32_t session_id) { return getRideFilePath(session_id); }
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
