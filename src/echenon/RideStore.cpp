/// @file RideStore.cpp
/// @brief PSRAM-backed ride sample storage and LittleFS crash recovery implementation.

#include "echenon/RideStore.h"

#include <esp_heap_caps.h>
#if defined(ESP_PLATFORM)
#include <LittleFS.h>
#include <esp_log.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {
#if defined(ESP_PLATFORM)
constexpr const char* TAG = "RideStore";

class LittleFSRideFileSystem : public echenon::IRideFileSystem {
public:
    bool exists(const char* path) const override {
        return path && LittleFS.exists(path);
    }

    bool remove(const char* path) override {
        if (!path || !LittleFS.exists(path)) return false;
        return LittleFS.remove(path);
    }

    bool mkdir(const char* path) override {
        if (!path) return false;
        if (LittleFS.exists(path)) return true;
        return LittleFS.mkdir(path);
    }

    bool writeFile(const char* path, const uint8_t* data, size_t len) override {
        if (!path) return false;
        File f = LittleFS.open(path, "w");
        if (!f) return false;
        size_t written = (data && len > 0) ? f.write(data, len) : 0;
        f.close();
        return written == len;
    }

    bool appendFile(const char* path, const uint8_t* data, size_t len) override {
        if (!path) return false;
        File f = LittleFS.open(path, "a");
        if (!f) return false;
        size_t written = (data && len > 0) ? f.write(data, len) : 0;
        f.close();
        return written == len;
    }

    bool readFile(const char* path, std::vector<uint8_t>& out) const override {
        out.clear();
        if (!path || !LittleFS.exists(path)) return false;
        File f = LittleFS.open(path, "r");
        if (!f) return false;
        size_t sz = f.size();
        out.resize(sz);
        size_t got = (sz > 0) ? f.read(out.data(), sz) : 0;
        f.close();
        if (got != sz) {
            out.resize(got);
        }
        return true;
    }

    size_t fileSize(const char* path) const override {
        if (!path || !LittleFS.exists(path)) return 0;
        File f = LittleFS.open(path, "r");
        if (!f) return 0;
        size_t sz = f.size();
        f.close();
        return sz;
    }
};

LittleFSRideFileSystem& defaultLittleFs() {
    static LittleFSRideFileSystem s_fs;
    return s_fs;
}
#endif
} // anonymous namespace

namespace echenon {

// =============================================================================
// InMemoryRideFileSystem
// =============================================================================

bool InMemoryRideFileSystem::exists(const char* path) const {
    if (!path) return false;
    return _files.find(path) != _files.end();
}

bool InMemoryRideFileSystem::remove(const char* path) {
    if (!path) return false;
    return _files.erase(path) > 0;
}

bool InMemoryRideFileSystem::mkdir(const char* path) {
    (void)path;
    return true;
}

bool InMemoryRideFileSystem::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!path) return false;
    auto& vec = _files[path];
    if (data && len > 0) {
        vec.assign(data, data + len);
    } else {
        vec.clear();
    }
    return true;
}

bool InMemoryRideFileSystem::appendFile(const char* path, const uint8_t* data, size_t len) {
    if (!path) return false;
    auto& vec = _files[path];
    if (data && len > 0) {
        vec.insert(vec.end(), data, data + len);
    }
    return true;
}

bool InMemoryRideFileSystem::readFile(const char* path, std::vector<uint8_t>& out) const {
    out.clear();
    if (!path) return false;
    auto it = _files.find(path);
    if (it == _files.end()) return false;
    out = it->second;
    return true;
}

size_t InMemoryRideFileSystem::fileSize(const char* path) const {
    if (!path) return 0;
    auto it = _files.find(path);
    if (it == _files.end()) return 0;
    return it->second.size();
}

// =============================================================================
// RideStore
// =============================================================================

RideStore::RideStore(IRideFileSystem* customFs) {
    if (customFs) {
        _fs = customFs;
    } else {
#if defined(ESP_PLATFORM)
        _fs = &defaultLittleFs();
#else
        _fs = &_defaultMemFs;
#endif
    }
}

RideStore::~RideStore() {
    end();
}

void RideStore::setFileSystem(IRideFileSystem* fs) {
    if (fs) {
        _fs = fs;
    } else {
#if defined(ESP_PLATFORM)
        _fs = &defaultLittleFs();
#else
        _fs = &_defaultMemFs;
#endif
    }
}

bool RideStore::begin() {
    if (!_buffer) {
        const size_t totalBytes = MAX_RECORDS * sizeof(RideRecord);
        _buffer = static_cast<RideRecord*>(heap_caps_malloc(totalBytes, MALLOC_CAP_SPIRAM));
        if (!_buffer) {
            _buffer = static_cast<RideRecord*>(std::malloc(totalBytes));
        }
        if (!_buffer) {
#if defined(ESP_PLATFORM)
            ESP_LOGE(TAG, "Failed to allocate %u bytes for RideStore buffer",
                     static_cast<unsigned>(totalBytes));
#endif
            return false;
        }
        std::memset(_buffer, 0, totalBytes);
    }

    if (_fs) {
        _fs->mkdir(RIDES_DIR);
        loadStoredIndex();
    }
    return true;
}

void RideStore::end() {
    if (_buffer) {
        heap_caps_free(_buffer);
        _buffer = nullptr;
    }
    _recordCount = 0;
    _lastCheckpointedCount = 0;
    _checkpointHeaderWritten = false;
    _active = false;
}

std::string RideStore::getRideFilePath(uint32_t session_id) {
    return std::string(RIDES_DIR) + "/ride_" + std::to_string(session_id) + ".bin";
}

std::string RideStore::getRideTcxGzPath(uint32_t session_id) {
    return std::string(RIDES_DIR) + "/ride_" + std::to_string(session_id) + ".tcx.gz";
}

bool RideStore::beginSession(uint32_t start_epoch_s) {
    uint32_t maxId = 0;
    for (const auto& s : _storedRides) {
        if (s.session_id > maxId) {
            maxId = s.session_id;
        }
    }
    if (_nextSessionSeq <= maxId) {
        _nextSessionSeq = maxId + 1u;
    }
    uint32_t sid = _nextSessionSeq++;
    bool ntpValid = (start_epoch_s >= VALID_EPOCH_THRESHOLD);
    return startNewSession(sid, start_epoch_s, ntpValid);
}

bool RideStore::startNewSession(uint32_t session_id, uint32_t start_epoch_s, bool ntp_valid) {
    if (!_buffer && !begin()) {
        return false;
    }

    _recordCount             = 0;
    _lastCheckpointedCount   = 0;
    _checkpointFlushCount    = 0;
    _checkpointHeaderWritten = false;
    _active                  = true;

    _summary               = RideSummary{};
    _summary.session_id    = session_id;
    _summary.start_epoch_s = start_epoch_s;
    _summary.ntp_valid     = ntp_valid;

    if (session_id >= _nextSessionSeq) {
        _nextSessionSeq = session_id + 1u;
    }

    if (_fs && _fs->exists(ACTIVE_CHECKPOINT_PATH)) {
        _fs->remove(ACTIVE_CHECKPOINT_PATH);
    }
    return true;
}

bool RideStore::appendRecord(const RideRecord& record) {
    if (!_buffer && !begin()) {
        return false;
    }
    if (_recordCount >= MAX_RECORDS) {
        return false;
    }

    _buffer[_recordCount++] = record;
    recomputeSummary();

    if (static_cast<uint16_t>(_recordCount - _lastCheckpointedCount) >= CHECKPOINT_INTERVAL_RECORDS) {
        flushCheckpoint();
    }
    return true;
}

bool RideStore::flushCheckpoint() {
    if (!_fs || !_buffer || _recordCount <= _lastCheckpointedCount) {
        return false;
    }

    if (!_checkpointHeaderWritten) {
        RideFileHeader hdr{};
        hdr.magic         = RIDE_FILE_MAGIC;
        hdr.version       = RIDE_FILE_VERSION;
        hdr.flags         = _summary.ntp_valid ? RIDE_HEADER_FLAG_NTP_VALID : 0u;
        hdr.session_id    = _summary.session_id;
        hdr.start_epoch_s = _summary.start_epoch_s;

        std::vector<uint8_t> chunk(sizeof(RideFileHeader) + _recordCount * sizeof(RideRecord));
        std::memcpy(chunk.data(), &hdr, sizeof(RideFileHeader));
        std::memcpy(chunk.data() + sizeof(RideFileHeader),
                    _buffer,
                    _recordCount * sizeof(RideRecord));

        if (!_fs->writeFile(ACTIVE_CHECKPOINT_PATH, chunk.data(), chunk.size())) {
            return false;
        }
        _checkpointHeaderWritten = true;
    } else {
        const uint16_t deltaCount = _recordCount - _lastCheckpointedCount;
        const uint8_t* deltaPtr = reinterpret_cast<const uint8_t*>(&_buffer[_lastCheckpointedCount]);
        const size_t deltaBytes = static_cast<size_t>(deltaCount) * sizeof(RideRecord);
        if (!_fs->appendFile(ACTIVE_CHECKPOINT_PATH, deltaPtr, deltaBytes)) {
            return false;
        }
    }

    _lastCheckpointedCount = _recordCount;
    _checkpointFlushCount++;
    return true;
}

void RideStore::updateStartEpoch(uint32_t start_epoch_s, bool ntp_valid) {
    _summary.start_epoch_s = start_epoch_s;
    _summary.ntp_valid     = ntp_valid;

    // If checkpoint file already has a header on disk, rewrite with updated start_epoch_s
    if (_checkpointHeaderWritten && _fs && _buffer && _lastCheckpointedCount > 0) {
        RideFileHeader hdr{};
        hdr.magic         = RIDE_FILE_MAGIC;
        hdr.version       = RIDE_FILE_VERSION;
        hdr.flags         = _summary.ntp_valid ? RIDE_HEADER_FLAG_NTP_VALID : 0u;
        hdr.session_id    = _summary.session_id;
        hdr.start_epoch_s = _summary.start_epoch_s;

        std::vector<uint8_t> chunk(sizeof(RideFileHeader) + _lastCheckpointedCount * sizeof(RideRecord));
        std::memcpy(chunk.data(), &hdr, sizeof(RideFileHeader));
        std::memcpy(chunk.data() + sizeof(RideFileHeader),
                    _buffer,
                    _lastCheckpointedCount * sizeof(RideRecord));
        _fs->writeFile(ACTIVE_CHECKPOINT_PATH, chunk.data(), chunk.size());
    }
}

void RideStore::updateStartEpoch(uint32_t now_epoch_s, uint32_t elapsed_s) {
    if (_summary.start_epoch_s < VALID_EPOCH_THRESHOLD && now_epoch_s >= VALID_EPOCH_THRESHOLD) {
        uint32_t backComputed = (now_epoch_s > elapsed_s) ? (now_epoch_s - elapsed_s) : now_epoch_s;
        updateStartEpoch(backComputed, true);
    }
}

uint16_t RideStore::trimTrailingZeroCadence(uint16_t trim_seconds) {
    if (!_buffer || _recordCount == 0 || trim_seconds == 0) {
        return 0;
    }

    uint16_t trimmed = 0;
    while (_recordCount > 0 && trimmed < trim_seconds &&
            _buffer[_recordCount - 1].cadence_rpm == 0) {
        _recordCount--;
        trimmed++;
    }

    if (_lastCheckpointedCount > _recordCount) {
        _lastCheckpointedCount = _recordCount;
    }
    recomputeSummary();
    return trimmed;
}

void RideStore::recomputeSummary() {
    _summary.record_count      = _recordCount;
    _summary.active_duration_s = _recordCount;

    if (!_buffer || _recordCount == 0) {
        _summary.duration_s      = 0;
        _summary.avg_power_w     = 0;
        _summary.max_power_w     = 0;
        _summary.avg_cadence_rpm = 0;
        _summary.avg_hr_bpm      = 0;
        _summary.max_hr_bpm      = 0;
        _summary.total_work_kj   = 0;
        return;
    }

    uint32_t sumPower   = 0;
    uint16_t maxPower   = 0;
    uint32_t sumCadence = 0;
    uint32_t sumHr      = 0;
    uint8_t  maxHr      = 0;
    uint16_t validHrCnt = 0;

    for (uint16_t i = 0; i < _recordCount; ++i) {
        const RideRecord& r = _buffer[i];
        sumPower += r.power_watts;
        if (r.power_watts > maxPower) {
            maxPower = r.power_watts;
        }
        sumCadence += r.cadence_rpm;

        if ((r.flags & RIDE_FLAG_HR_VALID) && r.heart_rate_bpm > 0) {
            sumHr += r.heart_rate_bpm;
            if (r.heart_rate_bpm > maxHr) {
                maxHr = r.heart_rate_bpm;
            }
            validHrCnt++;
        }
    }

    _summary.duration_s      = _recordCount;
    _summary.avg_power_w     = static_cast<uint16_t>((sumPower + (_recordCount / 2u)) / _recordCount);
    _summary.max_power_w     = maxPower;
    _summary.avg_cadence_rpm = static_cast<uint8_t>((sumCadence + (_recordCount / 2u)) / _recordCount);
    _summary.avg_hr_bpm      = validHrCnt > 0
        ? static_cast<uint8_t>((sumHr + (validHrCnt / 2u)) / validHrCnt)
        : 0;
    _summary.max_hr_bpm      = maxHr;
    // 1 Hz records: sumPower (W * 1 s) == total work in Joules
    _summary.total_work_kj   = static_cast<uint16_t>((sumPower + 500u) / 1000u);
}

bool RideStore::writeFullRideFile(const char* path, bool finalized) {
    if (!_fs || !path) return false;

    RideFileHeader hdr{};
    hdr.magic         = RIDE_FILE_MAGIC;
    hdr.version       = RIDE_FILE_VERSION;
    hdr.flags         = (_summary.ntp_valid ? RIDE_HEADER_FLAG_NTP_VALID : 0u) |
                        (finalized ? RIDE_HEADER_FLAG_FINALIZED : 0u);
    hdr.session_id    = _summary.session_id;
    hdr.start_epoch_s = _summary.start_epoch_s;

    const size_t recordsBytes = static_cast<size_t>(_recordCount) * sizeof(RideRecord);
    std::vector<uint8_t> payload(sizeof(RideFileHeader) + recordsBytes);
    std::memcpy(payload.data(), &hdr, sizeof(RideFileHeader));
    if (_buffer && recordsBytes > 0) {
        std::memcpy(payload.data() + sizeof(RideFileHeader), _buffer, recordsBytes);
    }

    return _fs->writeFile(path, payload.data(), payload.size());
}

bool RideStore::finalizeSession() {
    if (!_buffer) {
        return false;
    }

    recomputeSummary();

    if (_summary.session_id == 0) {
        _summary.session_id = (_summary.start_epoch_s > 0)
            ? _summary.start_epoch_s
            : static_cast<uint32_t>(_storedRides.size() + 1u);
    }

    if (_fs) {
        _fs->mkdir(RIDES_DIR);
        const std::string ridePath = getRideFilePath(_summary.session_id);
        if (!writeFullRideFile(ridePath.c_str(), true)) {
            return false;
        }
        if (_fs->exists(ACTIVE_CHECKPOINT_PATH)) {
            _fs->remove(ACTIVE_CHECKPOINT_PATH);
        }
    }

    // Update or append to _storedRides
    auto existingIt = std::find_if(_storedRides.begin(), _storedRides.end(),
        [this](const RideSummary& s) { return s.session_id == _summary.session_id; });
    if (existingIt != _storedRides.end()) {
        *existingIt = _summary;
    } else {
        _storedRides.push_back(_summary);
    }

    // Enforce MAX_STORED_RIDES (5) FIFO eviction
    while (_storedRides.size() > MAX_STORED_RIDES) {
        const uint32_t evictedId = _storedRides.front().session_id;
        _storedRides.erase(_storedRides.begin());
        if (_fs && evictedId != _summary.session_id) {
            const std::string evictedBin = getRideFilePath(evictedId);
            const std::string evictedTcx = getRideTcxGzPath(evictedId);
            if (_fs->exists(evictedBin.c_str())) _fs->remove(evictedBin.c_str());
            if (_fs->exists(evictedTcx.c_str())) _fs->remove(evictedTcx.c_str());
        }
    }

    saveStoredIndex();
    _active                  = false;
    _lastCheckpointedCount   = 0;
    _checkpointHeaderWritten = false;
    return true;
}

bool RideStore::finalizeSession(RideSummary& out_summary) {
    if (!finalizeSession()) {
        return false;
    }
    out_summary = _summary;
    return true;
}

void RideStore::clearActiveSession() {
    _recordCount             = 0;
    _lastCheckpointedCount   = 0;
    _checkpointFlushCount    = 0;
    _checkpointHeaderWritten = false;
    _active                  = false;
    _summary                 = RideSummary{};

    if (_fs && _fs->exists(ACTIVE_CHECKPOINT_PATH)) {
        _fs->remove(ACTIVE_CHECKPOINT_PATH);
    }
}

void RideStore::discardSession() {
    clearActiveSession();
}

bool RideStore::recoverInterruptedSession(uint16_t min_records, RideSummary* out_summary) {
    if (!_fs || !_fs->exists(ACTIVE_CHECKPOINT_PATH)) {
        return false;
    }
    if (!_buffer && !begin()) {
        return false;
    }

    std::vector<uint8_t> raw;
    if (!_fs->readFile(ACTIVE_CHECKPOINT_PATH, raw) || raw.empty()) {
        _fs->remove(ACTIVE_CHECKPOINT_PATH);
        return false;
    }

    uint32_t recoveredId    = 0;
    uint32_t recoveredEpoch = 0;
    bool     recoveredNtp   = false;
    const uint8_t* recordsPtr = raw.data();
    size_t recordsBytes       = raw.size();

    if (raw.size() >= sizeof(RideFileHeader)) {
        RideFileHeader hdr{};
        std::memcpy(&hdr, raw.data(), sizeof(RideFileHeader));
        if (hdr.magic == RIDE_FILE_MAGIC && hdr.version == RIDE_FILE_VERSION) {
            recoveredId    = hdr.session_id;
            recoveredEpoch = hdr.start_epoch_s;
            recoveredNtp   = (hdr.flags & RIDE_HEADER_FLAG_NTP_VALID) != 0;
            recordsPtr     = raw.data() + sizeof(RideFileHeader);
            recordsBytes   = raw.size() - sizeof(RideFileHeader);
        }
    }

    const size_t totalRecordsInFile = recordsBytes / sizeof(RideRecord);
    if (totalRecordsInFile < min_records) {
        _fs->remove(ACTIVE_CHECKPOINT_PATH);
        return false;
    }

    const uint16_t countToLoad = static_cast<uint16_t>(
        std::min<size_t>(totalRecordsInFile, MAX_RECORDS));

    if (recoveredId == 0) {
        recoveredId = (recoveredEpoch > 0)
            ? recoveredEpoch
            : static_cast<uint32_t>(_storedRides.size() + 1u);
    }

    _summary               = RideSummary{};
    _summary.session_id    = recoveredId;
    _summary.start_epoch_s = recoveredEpoch;
    _summary.ntp_valid     = recoveredNtp;

    std::memcpy(_buffer, recordsPtr, static_cast<size_t>(countToLoad) * sizeof(RideRecord));
    _recordCount             = countToLoad;
    _lastCheckpointedCount   = countToLoad;
    _checkpointHeaderWritten = false;
    _active                  = true;

    recomputeSummary();
    if (!finalizeSession()) {
        return false;
    }
    if (out_summary) {
        *out_summary = _summary;
    }
    return true;
}

bool RideStore::discardSession(uint32_t session_id) {
    bool removed = false;

    if (_active && _summary.session_id == session_id) {
        clearActiveSession();
        removed = true;
    }

    auto it = std::find_if(_storedRides.begin(), _storedRides.end(),
        [session_id](const RideSummary& s) { return s.session_id == session_id; });
    if (it != _storedRides.end()) {
        _storedRides.erase(it);
        removed = true;
        saveStoredIndex();
    }

    if (_fs) {
        const std::string binPath = getRideFilePath(session_id);
        const std::string tcxPath = getRideTcxGzPath(session_id);
        if (_fs->exists(binPath.c_str())) {
            _fs->remove(binPath.c_str());
            removed = true;
        }
        if (_fs->exists(tcxPath.c_str())) {
            _fs->remove(tcxPath.c_str());
            removed = true;
        }
    }

    return removed;
}

const RideSummary* RideStore::findStoredRide(uint32_t session_id) const {
    for (const auto& s : _storedRides) {
        if (s.session_id == session_id) {
            return &s;
        }
    }
    return nullptr;
}

bool RideStore::loadStoredRide(uint32_t session_id,
                               RideSummary& out_summary,
                               std::vector<RideRecord>& out_records) const {
    out_records.clear();
    if (!_fs) return false;

    const std::string path = getRideFilePath(session_id);
    std::vector<uint8_t> raw;
    if (!_fs->readFile(path.c_str(), raw) || raw.size() < sizeof(RideFileHeader)) {
        return false;
    }

    RideFileHeader hdr{};
    std::memcpy(&hdr, raw.data(), sizeof(RideFileHeader));
    if (hdr.magic != RIDE_FILE_MAGIC || hdr.version != RIDE_FILE_VERSION) {
        return false;
    }

    const size_t recordsBytes = raw.size() - sizeof(RideFileHeader);
    const size_t count = recordsBytes / sizeof(RideRecord);
    out_records.resize(count);
    if (count > 0) {
        std::memcpy(out_records.data(), raw.data() + sizeof(RideFileHeader), count * sizeof(RideRecord));
    }

    const RideSummary* stored = findStoredRide(session_id);
    if (stored) {
        out_summary = *stored;
    } else {
        out_summary               = RideSummary{};
        out_summary.session_id    = hdr.session_id;
        out_summary.start_epoch_s = hdr.start_epoch_s;
        out_summary.ntp_valid     = (hdr.flags & RIDE_HEADER_FLAG_NTP_VALID) != 0;
        out_summary.record_count  = static_cast<uint16_t>(count);
        out_summary.active_duration_s = static_cast<uint16_t>(count);
        if (count > 0) {
            uint32_t sumPower = 0;
            uint16_t maxPower = 0;
            uint32_t sumCad   = 0;
            uint32_t sumHr    = 0;
            uint8_t  maxHr    = 0;
            uint16_t hrCnt    = 0;
            for (const auto& r : out_records) {
                sumPower += r.power_watts;
                if (r.power_watts > maxPower) maxPower = r.power_watts;
                sumCad += r.cadence_rpm;
                if ((r.flags & RIDE_FLAG_HR_VALID) && r.heart_rate_bpm > 0) {
                    sumHr += r.heart_rate_bpm;
                    if (r.heart_rate_bpm > maxHr) maxHr = r.heart_rate_bpm;
                    hrCnt++;
                }
            }
            const uint16_t lastPlusOne = static_cast<uint16_t>(out_records.back().timestamp_offset_s + 1u);
            out_summary.duration_s      = std::max<uint16_t>(static_cast<uint16_t>(count), lastPlusOne);
            out_summary.avg_power_w     = static_cast<uint16_t>((sumPower + count / 2u) / count);
            out_summary.max_power_w     = maxPower;
            out_summary.avg_cadence_rpm = static_cast<uint8_t>((sumCad + count / 2u) / count);
            out_summary.avg_hr_bpm      = hrCnt > 0 ? static_cast<uint8_t>((sumHr + hrCnt / 2u) / hrCnt) : 0;
            out_summary.max_hr_bpm      = maxHr;
            out_summary.total_work_kj   = static_cast<uint16_t>((sumPower + 500u) / 1000u);
        }
    }
    return true;
}

void RideStore::saveStoredIndex() {
    if (!_fs) return;
    if (_storedRides.empty()) {
        if (_fs->exists(RIDES_INDEX_PATH)) {
            _fs->remove(RIDES_INDEX_PATH);
        }
        return;
    }
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(_storedRides.data());
    const size_t bytes = _storedRides.size() * sizeof(RideSummary);
    _fs->writeFile(RIDES_INDEX_PATH, ptr, bytes);
}

void RideStore::loadStoredIndex() {
    if (!_fs || !_fs->exists(RIDES_INDEX_PATH)) return;
    std::vector<uint8_t> raw;
    if (!_fs->readFile(RIDES_INDEX_PATH, raw) || raw.empty()) return;
    if (raw.size() % sizeof(RideSummary) != 0) return;

    const size_t count = raw.size() / sizeof(RideSummary);
    std::vector<RideSummary> loaded(count);
    std::memcpy(loaded.data(), raw.data(), raw.size());

    _storedRides.clear();
    for (const auto& s : loaded) {
        const std::string binPath = getRideFilePath(s.session_id);
        if (_fs->exists(binPath.c_str())) {
            _storedRides.push_back(s);
        }
    }
    while (_storedRides.size() > MAX_STORED_RIDES) {
        _storedRides.erase(_storedRides.begin());
    }
}

} // namespace echenon
