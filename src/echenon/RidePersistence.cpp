/// @file RidePersistence.cpp
/// @brief Persistent ride storage implementation with gzip compression (ESP32)
///        and raw binary fallback (UNIT_TEST / native builds).

#include "echenon/RidePersistence.h"
#include "echenon/TCXGenerator.h"

#include <algorithm>
#include <cstring>

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#define RIDE_PERSIST_HAS_COMPRESSION 1
#else
#include <cstdio>
// Stub ESP_LOG macros for native test builds (provided by test stubs)
#if __has_include(<esp_log.h>)
#include <esp_log.h>
#else
#define ESP_LOGE(tag, fmt, ...) printf("E (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("D (%s): " fmt "\n", tag, ##__VA_ARGS__)
#endif
#define RIDE_PERSIST_HAS_COMPRESSION 0
#endif

namespace {
constexpr const char* TAG = "RidePersistence";
} // anonymous namespace

namespace echenon {

// =============================================================================
// Construction & Configuration
// =============================================================================

RidePersistence::RidePersistence(IRideFileSystem& fs)
    : _fs(fs) {}

void RidePersistence::setEnabled(bool enabled) {
    _enabled = enabled;
}

bool RidePersistence::isEnabled() const {
    return _enabled;
}

void RidePersistence::setCapKb(uint32_t cap_kb) {
    if (cap_kb < MIN_CAP_KB) cap_kb = MIN_CAP_KB;
    if (cap_kb > MAX_CAP_KB) cap_kb = MAX_CAP_KB;
    _capKb = cap_kb;
}

uint32_t RidePersistence::getCapKb() const {
    return _capKb;
}

// =============================================================================
// Lifecycle
// =============================================================================

void RidePersistence::begin() {
    _fs.mkdir(PERSIST_DIR);
    scanStoredRides();
}

void RidePersistence::clear() {
    for (const auto& info : _storedRides) {
        const std::string path = ridePath(info.session_id);
        if (_fs.exists(path.c_str())) {
            _fs.remove(path.c_str());
        }
    }
    _storedRides.clear();
}

// =============================================================================
// Stored Ride Accessors
// =============================================================================

const std::vector<RidePersistence::StoredRideInfo>& RidePersistence::getStoredRides() const {
    return _storedRides;
}

size_t RidePersistence::getStoredCount() const {
    return _storedRides.size();
}

uint32_t RidePersistence::getTotalSizeBytes() const {
    uint32_t total = 0;
    for (const auto& info : _storedRides) {
        total += info.file_size_bytes;
    }
    return total;
}

// =============================================================================
// Save / Load / Delete Operations
// =============================================================================

bool RidePersistence::saveRide(uint32_t session_id, const RideFileHeader& header,
                               const RideRecord* records, uint16_t record_count) {
    if (!_enabled) return false;
    if (!records && record_count > 0) return false;

    // Serialize header + records into a contiguous buffer
    const size_t recordsBytes = static_cast<size_t>(record_count) * sizeof(RideRecord);
    const size_t rawSize = sizeof(RideFileHeader) + recordsBytes;
    std::vector<uint8_t> raw(rawSize);
    std::memcpy(raw.data(), &header, sizeof(RideFileHeader));
    if (records && recordsBytes > 0) {
        std::memcpy(raw.data() + sizeof(RideFileHeader), records, recordsBytes);
    }

    // Estimate compressed size (worst case: raw + overhead)
    const uint32_t estimatedSize = static_cast<uint32_t>(rawSize);

    // Evict oldest rides until we have enough space
    const uint32_t capBytes = _capKb * 1024u;
    while (getTotalSizeBytes() + estimatedSize > capBytes && !_storedRides.empty()) {
        evictOldest();
    }

    _fs.mkdir(PERSIST_DIR);
    const std::string path = ridePath(session_id);

    if (!compressAndWrite(path.c_str(), raw.data(), raw.size())) {
        ESP_LOGE(TAG, "Failed to write ride %u to %s", session_id, path.c_str());
        return false;
    }

    // Record metadata
    const uint32_t actualSize = static_cast<uint32_t>(_fs.fileSize(path.c_str()));

    // Remove any existing entry for this session_id
    auto it = std::find_if(_storedRides.begin(), _storedRides.end(),
        [session_id](const StoredRideInfo& s) { return s.session_id == session_id; });
    if (it != _storedRides.end()) {
        _storedRides.erase(it);
    }

    StoredRideInfo info{};
    info.session_id      = session_id;
    info.start_epoch_s   = header.start_epoch_s;
    info.file_size_bytes = actualSize;
    info.record_count    = record_count;
    _storedRides.push_back(info);

    // Sort by start_epoch_s (oldest first) for consistent eviction order
    std::sort(_storedRides.begin(), _storedRides.end(),
        [](const StoredRideInfo& a, const StoredRideInfo& b) {
            return a.start_epoch_s < b.start_epoch_s;
        });

    ESP_LOGI(TAG, "Saved ride %u (%u records, %u bytes on disk)",
             session_id, record_count, actualSize);
    return true;
}

bool RidePersistence::deleteRide(uint32_t session_id) {
    const std::string path = ridePath(session_id);
    bool removed = false;

    if (_fs.exists(path.c_str())) {
        _fs.remove(path.c_str());
        removed = true;
    }

    auto it = std::find_if(_storedRides.begin(), _storedRides.end(),
        [session_id](const StoredRideInfo& s) { return s.session_id == session_id; });
    if (it != _storedRides.end()) {
        _storedRides.erase(it);
        removed = true;
    }

    return removed;
}

bool RidePersistence::loadRide(uint32_t session_id,
                               RideFileHeader& out_header,
                               std::vector<RideRecord>& out_records) const {
    out_records.clear();

    const std::string path = ridePath(session_id);
    std::vector<uint8_t> decompressed;
    if (!readAndDecompress(path.c_str(), decompressed)) {
        return false;
    }

    if (decompressed.size() < sizeof(RideFileHeader)) {
        ESP_LOGE(TAG, "Ride file %s too small (%zu bytes)", path.c_str(), decompressed.size());
        return false;
    }

    std::memcpy(&out_header, decompressed.data(), sizeof(RideFileHeader));

    if (out_header.magic != RIDE_FILE_MAGIC || out_header.version != RIDE_FILE_VERSION) {
        ESP_LOGE(TAG, "Invalid magic/version in ride file %s", path.c_str());
        return false;
    }

    const size_t recordsBytes = decompressed.size() - sizeof(RideFileHeader);
    const size_t count = recordsBytes / sizeof(RideRecord);
    out_records.resize(count);
    if (count > 0) {
        std::memcpy(out_records.data(),
                    decompressed.data() + sizeof(RideFileHeader),
                    count * sizeof(RideRecord));
    }

    return true;
}

// =============================================================================
// Compression / Decompression
// =============================================================================

#if RIDE_PERSIST_HAS_COMPRESSION

bool RidePersistence::compressAndWrite(const char* path, const uint8_t* data, size_t len) {
    // Compress into RFC 1952 gzip stream via PSRAM-allocated StreamingGzipEncoder.
    // Memory footprint is ~5.2 KB in PSRAM (0 DRAM pressure), preventing ESP32-S3 heap exhaustion.
    std::vector<uint8_t> compressed;
    if (!TCXGenerator::compressGzip(data, len, compressed)) {
        ESP_LOGE(TAG, "compressGzip failed for %s", path);
        return false;
    }
    return _fs.writeFile(path, compressed.data(), compressed.size());
}

bool RidePersistence::readAndDecompress(const char* path,
                                        std::vector<uint8_t>& out) const {
    out.clear();
    std::vector<uint8_t> fileData;
    if (!_fs.readFile(path, fileData) || fileData.empty()) {
        return false;
    }

    // Decompress via TCXGenerator::decompressGzip if RFC 1952 gzip header (0x1F, 0x8B) is present
    if (fileData.size() >= 18 && fileData[0] == 0x1fu && fileData[1] == 0x8bu) {
        if (!TCXGenerator::decompressGzip(fileData.data(), fileData.size(), out)) {
            ESP_LOGE(TAG, "decompressGzip failed for %s", path);
            out.clear();
            return false;
        }
        return true;
    }

    // Fallback for uncompressed raw binary files (e.g. legacy or test files)
    out = std::move(fileData);
    return true;
}

#else
// UNIT_TEST / native builds: store uncompressed (no miniz available).
// Deviation: files are NOT gzip-compressed in test builds. The file extension
// remains .ride.gz for API consistency but contains raw binary data.

bool RidePersistence::compressAndWrite(const char* path, const uint8_t* data, size_t len) {
    return _fs.writeFile(path, data, len);
}

bool RidePersistence::readAndDecompress(const char* path,
                                        std::vector<uint8_t>& out) const {
    out.clear();
    return _fs.readFile(path, out) && !out.empty();
}

#endif // RIDE_PERSIST_HAS_COMPRESSION

// =============================================================================
// Eviction & Scanning
// =============================================================================

void RidePersistence::evictOldest() {
    if (_storedRides.empty()) return;

    // _storedRides is sorted oldest-first by start_epoch_s
    const StoredRideInfo& oldest = _storedRides.front();
    const std::string path = ridePath(oldest.session_id);

    ESP_LOGW(TAG, "Evicting oldest ride %u (epoch %u, %u bytes) to free space",
             oldest.session_id, oldest.start_epoch_s, oldest.file_size_bytes);

    if (_fs.exists(path.c_str())) {
        _fs.remove(path.c_str());
    }
    _storedRides.erase(_storedRides.begin());
}

void RidePersistence::scanStoredRides() {
    _storedRides.clear();

    std::vector<std::string> entries;
    if (!_fs.listDir(PERSIST_DIR, entries)) {
        return;  // Directory doesn't exist yet — nothing to scan
    }

    const std::string dirPrefix = std::string(PERSIST_DIR) + "/";

    for (const auto& filename : entries) {
        // Only process .ride.gz files
        const std::string suffix = ".ride.gz";
        if (filename.size() <= suffix.size() ||
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }

        const std::string fullPath = dirPrefix + filename;
        const size_t fileSize = _fs.fileSize(fullPath.c_str());
        if (fileSize == 0) continue;

        // Try to decompress and read the header
        std::vector<uint8_t> decompressed;
        if (!readAndDecompress(fullPath.c_str(), decompressed)) {
            ESP_LOGW(TAG, "Cannot read ride file %s, skipping", fullPath.c_str());
            continue;
        }

        if (decompressed.size() < sizeof(RideFileHeader)) {
            ESP_LOGW(TAG, "Ride file %s too small (%zu bytes), skipping",
                     fullPath.c_str(), decompressed.size());
            continue;
        }

        RideFileHeader hdr{};
        std::memcpy(&hdr, decompressed.data(), sizeof(RideFileHeader));

        if (hdr.magic != RIDE_FILE_MAGIC || hdr.version != RIDE_FILE_VERSION) {
            ESP_LOGW(TAG, "Invalid magic/version in %s, skipping", fullPath.c_str());
            continue;
        }

        const size_t recordsBytes = decompressed.size() - sizeof(RideFileHeader);
        const uint16_t recordCount = static_cast<uint16_t>(recordsBytes / sizeof(RideRecord));

        StoredRideInfo info{};
        info.session_id      = hdr.session_id;
        info.start_epoch_s   = hdr.start_epoch_s;
        info.file_size_bytes = static_cast<uint32_t>(fileSize);
        info.record_count    = recordCount;
        _storedRides.push_back(info);
    }

    // Sort oldest-first by start_epoch_s for consistent eviction order
    std::sort(_storedRides.begin(), _storedRides.end(),
        [](const StoredRideInfo& a, const StoredRideInfo& b) {
            return a.start_epoch_s < b.start_epoch_s;
        });

    ESP_LOGI(TAG, "Scanned %zu persisted rides (%u bytes total)",
             _storedRides.size(), getTotalSizeBytes());
}

// =============================================================================
// Path Helpers
// =============================================================================

std::string RidePersistence::ridePath(uint32_t session_id) {
    return std::string(PERSIST_DIR) + "/ride_" + std::to_string(session_id) + ".ride.gz";
}

} // namespace echenon
