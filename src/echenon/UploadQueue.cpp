/// @file UploadQueue.cpp
/// @brief Split-stage background upload queue state machine and LittleFS job persistence.

#include "echenon/UploadQueue.h"

#include <ArduinoJson.h>

#if defined(ESP_PLATFORM)
#include <LittleFS.h>
#include <esp_log.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#if !defined(ESP_PLATFORM)
#include <sys/stat.h>
#endif

namespace echenon {

namespace {

#if !defined(ESP_PLATFORM)
void ensurePosixParentDirs(const std::string& file_path) {
    size_t pos = 0;
    while ((pos = file_path.find('/', pos + 1)) != std::string::npos) {
        const std::string dir = file_path.substr(0, pos);
        if (!dir.empty() && dir != "." && dir != "/") {
            ::mkdir(dir.c_str(), 0755);
        }
    }
}

bool writePosixFile(const std::string& path, const uint8_t* data, size_t len) {
    if (path.empty()) return false;
    // Only write to relative or ./tmp paths on host POSIX to avoid touching root "/"
    if (path[0] == '/' && path.rfind("/tmp/", 0) != 0) {
        return false;
    }
    ensurePosixParentDirs(path);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t written = (data && len > 0) ? std::fwrite(data, 1, len, f) : 0;
    std::fclose(f);
    return written == len;
}

bool readPosixFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    if (path.empty()) return false;
    if (path[0] == '/' && path.rfind("/tmp/", 0) != 0) {
        return false;
    }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    if (sz > 0) {
        const size_t got = std::fread(out.data(), 1, static_cast<size_t>(sz), f);
        out.resize(got);
    }
    std::fclose(f);
    return true;
}

bool posixFileExists(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/' && path.rfind("/tmp/", 0) != 0) {
        return false;
    }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}
#endif

} // namespace

// =============================================================================
// Enum Helpers
// =============================================================================

const char* uploadStageToString(UploadStage stage) {
    switch (stage) {
        case UploadStage::STAGE_GENERATE_TCX_GZ: return "STAGE_GENERATE_TCX_GZ";
        case UploadStage::STAGE_REFRESH_TOKEN:   return "STAGE_REFRESH_TOKEN";
        case UploadStage::STAGE_POST_UPLOAD:     return "STAGE_POST_UPLOAD";
        case UploadStage::STAGE_POLL_STATUS:     return "STAGE_POLL_STATUS";
        case UploadStage::STAGE_COMPLETE:        return "STAGE_COMPLETE";
        case UploadStage::STAGE_FAILED:          return "STAGE_FAILED";
        default:                                 return "STAGE_GENERATE_TCX_GZ";
    }
}

bool uploadStageFromString(const char* str, UploadStage& out_stage) {
    if (!str) return false;
    if (std::strcmp(str, "STAGE_GENERATE_TCX_GZ") == 0 || std::strcmp(str, "GENERATE_TCX_GZ") == 0) {
        out_stage = UploadStage::STAGE_GENERATE_TCX_GZ;
        return true;
    }
    if (std::strcmp(str, "STAGE_REFRESH_TOKEN") == 0 || std::strcmp(str, "REFRESH_TOKEN") == 0) {
        out_stage = UploadStage::STAGE_REFRESH_TOKEN;
        return true;
    }
    if (std::strcmp(str, "STAGE_POST_UPLOAD") == 0 || std::strcmp(str, "POST_UPLOAD") == 0) {
        out_stage = UploadStage::STAGE_POST_UPLOAD;
        return true;
    }
    if (std::strcmp(str, "STAGE_POLL_STATUS") == 0 || std::strcmp(str, "POLL_STATUS") == 0) {
        out_stage = UploadStage::STAGE_POLL_STATUS;
        return true;
    }
    if (std::strcmp(str, "STAGE_COMPLETE") == 0 || std::strcmp(str, "COMPLETE") == 0) {
        out_stage = UploadStage::STAGE_COMPLETE;
        return true;
    }
    if (std::strcmp(str, "STAGE_FAILED") == 0 || std::strcmp(str, "FAILED") == 0) {
        out_stage = UploadStage::STAGE_FAILED;
        return true;
    }
    return false;
}

// =============================================================================
// Constructors & Lifecycle
// =============================================================================

UploadQueue::UploadQueue(IUploader* uploader, RideStore* rideStore, EventBus* eventBus)
    : _uploader(uploader)
    , _rideStore(rideStore)
    , _eventBus(eventBus)
    , _fs(rideStore ? &rideStore->fileSystem() : nullptr) {}

UploadQueue::UploadQueue(RideStore& rideStore, IUploader* uploader, EventBus* eventBus)
    : _uploader(uploader)
    , _rideStore(&rideStore)
    , _eventBus(eventBus)
    , _fs(&rideStore.fileSystem()) {}

void UploadQueue::setRideStore(RideStore* rideStore) {
    _rideStore = rideStore;
    if (_rideStore && !_fs) {
        _fs = &_rideStore->fileSystem();
    }
}

IRideFileSystem* UploadQueue::getFileSystem() const {
    if (_fs) return _fs;
    if (_rideStore) return &_rideStore->fileSystem();
    return nullptr;
}

void UploadQueue::setBaseDir(const std::string& base_dir) {
    _baseDir = base_dir;
    while (!_baseDir.empty() && _baseDir.back() == '/') {
        _baseDir.pop_back();
    }
    if (_baseDir.empty()) {
        _queueFilePath = DEFAULT_QUEUE_FILE_PATH;
    } else {
        _queueFilePath = _baseDir + "/upload_queue.json";
    }
}

bool UploadQueue::begin() {
    IRideFileSystem* fs = getFileSystem();
    if (fs) {
        fs->mkdir(RideStore::RIDES_DIR);
    }
    loadQueue();
    return true;
}

std::string UploadQueue::getTcxGzPath(uint32_t session_id) const {
    if (!_baseDir.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s/ride_%u.tcx.gz",
                      _baseDir.c_str(), static_cast<unsigned>(session_id));
        return std::string(buf);
    }
    return RideStore::getRideTcxGzPath(session_id);
}

// =============================================================================
// Readiness & Exponential Backoff Helpers
// =============================================================================

bool UploadQueue::isJobReady(const UploadJob& job, uint32_t now_ms) {
    if (job.stage == UploadStage::STAGE_COMPLETE ||
        job.stage == UploadStage::STAGE_FAILED) {
        return false;
    }
    // Wraparound-safe signed 32-bit comparison (PR-05 WO-05.4 invariant)
    return static_cast<int32_t>(now_ms - job.next_attempt_ms) >= 0;
}

uint32_t UploadQueue::computeBackoffMs(uint8_t attempt_count) {
    switch (attempt_count) {
        case 0:
        case 1:  return 30000u;  // 30 s
        case 2:  return 60000u;  // 60 s
        case 3:  return 120000u; // 120 s
        default: return 300000u; // 300 s
    }
}

// =============================================================================
// Queue Management (Enqueue / Discard / Accessors)
// =============================================================================

bool UploadQueue::enqueueSession(uint32_t session_id, uint32_t now_ms) {
    if (session_id == 0) {
        return false;
    }

    // Check if session_id is already present in the queue
    for (auto& existing : _jobs) {
        if (existing.session_id == session_id) {
            if (existing.stage == UploadStage::STAGE_COMPLETE ||
                existing.stage == UploadStage::STAGE_FAILED) {
                existing.stage              = UploadStage::STAGE_GENERATE_TCX_GZ;
                existing.attempt_count      = 0;
                existing.next_attempt_ms    = now_ms;
                existing.remote_upload_id.clear();
                existing.remote_activity_id.clear();
                existing.last_error.clear();
                saveQueue();
            }
            return true;
        }
    }

    // Enforce MAX_QUEUE_ENTRIES (5): evict oldest completed or failed job first
    if (_jobs.size() >= MAX_QUEUE_ENTRIES) {
        auto completeIt = std::find_if(_jobs.begin(), _jobs.end(),
            [](const UploadJob& j) { return j.stage == UploadStage::STAGE_COMPLETE; });
        if (completeIt != _jobs.end()) {
            _jobs.erase(completeIt);
        } else {
            auto failedIt = std::find_if(_jobs.begin(), _jobs.end(),
                [](const UploadJob& j) { return j.stage == UploadStage::STAGE_FAILED; });
            if (failedIt != _jobs.end()) {
                _jobs.erase(failedIt);
            } else {
                // Queue is full of 5 active jobs
                return false;
            }
        }
    }

    UploadJob job{};
    job.session_id      = session_id;
    job.stage           = UploadStage::STAGE_GENERATE_TCX_GZ;
    job.attempt_count   = 0;
    job.next_attempt_ms = now_ms;
    _jobs.push_back(job);

    saveQueue();
    return true;
}

bool UploadQueue::discardSession(uint32_t session_id) {
    bool removed = false;

    auto it = std::remove_if(_jobs.begin(), _jobs.end(),
        [session_id](const UploadJob& j) { return j.session_id == session_id; });
    if (it != _jobs.end()) {
        _jobs.erase(it, _jobs.end());
        removed = true;
        saveQueue();
    }

    // Remove .tcx.gz and .bin artifacts
    const std::string tcxPath = getTcxGzPath(session_id);
    if (removeFileEverywhere(tcxPath)) {
        removed = true;
    }
    const std::string canonicalTcxPath = RideStore::getRideTcxGzPath(session_id);
    if (canonicalTcxPath != tcxPath && removeFileEverywhere(canonicalTcxPath)) {
        removed = true;
    }
    const std::string canonicalBinPath = RideStore::getRideFilePath(session_id);
    if (removeFileEverywhere(canonicalBinPath)) {
        removed = true;
    }
    if (!_baseDir.empty()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s/ride_%u.bin",
                      _baseDir.c_str(), static_cast<unsigned>(session_id));
        if (removeFileEverywhere(std::string(buf))) {
            removed = true;
        }
    }

    if (_rideStore) {
        if (_rideStore->discardSession(session_id)) {
            removed = true;
        }
    }

    return removed;
}

size_t UploadQueue::getPendingCount() const {
    size_t count = 0;
    for (const auto& j : _jobs) {
        if (j.stage != UploadStage::STAGE_COMPLETE &&
            j.stage != UploadStage::STAGE_FAILED) {
            ++count;
        }
    }
    return count;
}

void UploadQueue::clearQueue() {
    _jobs.clear();
    saveQueue();
}

const UploadJob* UploadQueue::findJob(uint32_t session_id) const {
    for (const auto& j : _jobs) {
        if (j.session_id == session_id) {
            return &j;
        }
    }
    return nullptr;
}

UploadJob* UploadQueue::findJobMutable(uint32_t session_id) {
    for (auto& j : _jobs) {
        if (j.session_id == session_id) {
            return &j;
        }
    }
    return nullptr;
}

// =============================================================================
// Split-Stage Execution Engine (Strictly At Most ONE Stage per stepQueue Call)
// =============================================================================

UpdateStatus UploadQueue::stepQueue(
    uint32_t remaining_budget_ms,
    uint32_t now_ms,
    const DeadlineCheckFn& check_deadline) {
    if (!_autoUploadEnabled || !_uploader || !_uploader->isConfigured()) {
        return UpdateStatus::Skipped;
    }
    if (!_uploader->isAuthenticated()) {
        return UpdateStatus::AuthError;
    }
    if (remaining_budget_ms == 0 || (check_deadline && !check_deadline())) {
        return UpdateStatus::Skipped;
    }

    // Find the first ready active job in FIFO order
    UploadJob* readyJob = nullptr;
    for (auto& job : _jobs) {
        if (isJobReady(job, now_ms)) {
            readyJob = &job;
            break;
        }
    }

    if (!readyJob) {
        return UpdateStatus::Skipped;
    }

    switch (readyJob->stage) {
        case UploadStage::STAGE_GENERATE_TCX_GZ: {
            // Stage 1: Generate /rides/ride_<id>.tcx.gz (No network I/O, no BLE ScanInhibit)
            const std::string tcx_path = getTcxGzPath(readyJob->session_id);
            bool generated = false;

            if (_tcxGeneratorFn) {
                generated = _tcxGeneratorFn(readyJob->session_id, tcx_path, check_deadline);
            } else if (_rideStore) {
                const std::string store_tcx_path = RideStore::getRideTcxGzPath(readyJob->session_id);
                generated = TCXGenerator::generateTcxGzToFile(
                    *_rideStore, readyJob->session_id, store_tcx_path, check_deadline);
                if (generated && tcx_path != store_tcx_path) {
                    std::vector<uint8_t> gz_bytes;
                    if (_rideStore->fileSystem().readFile(store_tcx_path.c_str(), gz_bytes)) {
                        _rideStore->fileSystem().writeFile(
                            tcx_path.c_str(),
                            gz_bytes.empty() ? nullptr : gz_bytes.data(),
                            gz_bytes.size());
#if !defined(ESP_PLATFORM)
                        writePosixFile(
                            tcx_path,
                            gz_bytes.empty() ? nullptr : gz_bytes.data(),
                            gz_bytes.size());
#endif
                    }
                } else if (generated) {
#if !defined(ESP_PLATFORM)
                    std::vector<uint8_t> gz_bytes;
                    if (_rideStore->fileSystem().readFile(store_tcx_path.c_str(), gz_bytes)) {
                        writePosixFile(
                            tcx_path,
                            gz_bytes.empty() ? nullptr : gz_bytes.data(),
                            gz_bytes.size());
                    }
#endif
                }
            } else {
                // Standalone mode without RideStore: allow progression if file exists or mock mode
                generated = true;
            }

            if (!generated) {
                handleStageFailure(
                    *readyJob,
                    UploadStageResult::RETRYABLE_ERROR,
                    "Failed to generate .tcx.gz",
                    now_ms);
                saveQueue();
                return UpdateStatus::FetchError;
            }

            const uint32_t now_epoch_s = currentEpochS();
            if (_uploader->needsTokenRefresh(now_epoch_s)) {
                readyJob->stage = UploadStage::STAGE_REFRESH_TOKEN;
            } else {
                readyJob->stage = UploadStage::STAGE_POST_UPLOAD;
            }
            readyJob->attempt_count   = 0;
            readyJob->next_attempt_ms = now_ms;
            readyJob->last_error.clear();
            saveQueue();
            return UpdateStatus::Success;
        }

        case UploadStage::STAGE_REFRESH_TOKEN: {
            // Stage 2: Refresh OAuth token (Network/TLS -> acquire BLE ScanInhibit)
#ifdef WITH_BLE
            auto scan_inhibit = _bleManager
                ? _bleManager->inhibitScanning("strava_upload")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, REFRESH_STAGE_BUDGET_MS);
            std::string err_msg;
            const UploadStageResult res = _uploader->refreshToken(stage_timeout_ms, err_msg);

            if (res == UploadStageResult::SUCCESS) {
                readyJob->stage           = UploadStage::STAGE_POST_UPLOAD;
                readyJob->attempt_count   = 0;
                readyJob->next_attempt_ms = now_ms;
                readyJob->last_error.clear();
                saveQueue();
                return UpdateStatus::Success;
            }
            if (res == UploadStageResult::IN_PROGRESS) {
                readyJob->next_attempt_ms = now_ms + POLL_DELAY_MS;
                saveQueue();
                return UpdateStatus::Success;
            }

            handleStageFailure(*readyJob, res, err_msg, now_ms);
            saveQueue();
            return (res == UploadStageResult::PERMANENT_ERROR)
                ? UpdateStatus::AuthError
                : mapFailureToUpdateStatus(res, err_msg);
        }

        case UploadStage::STAGE_POST_UPLOAD: {
            // Stage 3: POST multipart .tcx.gz (Network/TLS -> acquire BLE ScanInhibit)
#ifdef WITH_BLE
            auto scan_inhibit = _bleManager
                ? _bleManager->inhibitScanning("strava_upload")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, POST_STAGE_BUDGET_MS);
            const std::string tcx_path = getTcxGzPath(readyJob->session_id);
            std::string upload_id;
            std::string err_msg;

            const UploadStageResult res = _uploader->postUpload(
                readyJob->session_id,
                tcx_path,
                stage_timeout_ms,
                upload_id,
                err_msg);

            if (res == UploadStageResult::SUCCESS) {
                readyJob->remote_upload_id = upload_id;
                readyJob->stage            = UploadStage::STAGE_POLL_STATUS;
                readyJob->attempt_count    = 0;
                readyJob->next_attempt_ms  = now_ms + POLL_DELAY_MS;
                readyJob->last_error.clear();
                saveQueue();
                return UpdateStatus::Success;
            }
            if (res == UploadStageResult::IN_PROGRESS) {
                readyJob->next_attempt_ms = now_ms + POLL_DELAY_MS;
                saveQueue();
                return UpdateStatus::Success;
            }

            handleStageFailure(*readyJob, res, err_msg, now_ms);
            saveQueue();
            return mapFailureToUpdateStatus(res, err_msg);
        }

        case UploadStage::STAGE_POLL_STATUS: {
            // Stage 4: Poll remote processing status (Network/TLS -> acquire BLE ScanInhibit)
#ifdef WITH_BLE
            auto scan_inhibit = _bleManager
                ? _bleManager->inhibitScanning("strava_upload")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, POLL_STAGE_BUDGET_MS);
            std::string activity_id;
            std::string err_msg;

            const UploadStageResult res = _uploader->pollUploadStatus(
                readyJob->remote_upload_id,
                stage_timeout_ms,
                activity_id,
                err_msg);

            if (res == UploadStageResult::SUCCESS) {
                readyJob->remote_activity_id = activity_id;
                readyJob->stage              = UploadStage::STAGE_COMPLETE;
                readyJob->last_error.clear();

                // Delete temporary .tcx.gz artifact upon completion
                const std::string tcx_path = getTcxGzPath(readyJob->session_id);
                removeFileEverywhere(tcx_path);
                const std::string canonical_tcx = RideStore::getRideTcxGzPath(readyJob->session_id);
                if (canonical_tcx != tcx_path) {
                    removeFileEverywhere(canonical_tcx);
                }

                saveQueue();
                publishUploadEvent(*readyJob);
                return UpdateStatus::Success;
            }
            if (res == UploadStageResult::IN_PROGRESS) {
                readyJob->stage           = UploadStage::STAGE_POLL_STATUS;
                readyJob->next_attempt_ms = now_ms + POLL_DELAY_MS;
                saveQueue();
                return UpdateStatus::Success;
            }

            handleStageFailure(*readyJob, res, err_msg, now_ms);
            saveQueue();
            return mapFailureToUpdateStatus(res, err_msg);
        }

        case UploadStage::STAGE_COMPLETE:
        case UploadStage::STAGE_FAILED:
        default:
            return UpdateStatus::Skipped;
    }
}

void UploadQueue::handleStageFailure(
    UploadJob& job,
    UploadStageResult result,
    const std::string& err_msg,
    uint32_t now_ms) {
    job.attempt_count = static_cast<uint8_t>(job.attempt_count + 1u);
    job.last_error    = err_msg;

    if (result == UploadStageResult::PERMANENT_ERROR ||
        job.attempt_count >= MAX_ATTEMPTS) {
        job.stage = UploadStage::STAGE_FAILED;
        publishUploadEvent(job);
    } else {
        job.next_attempt_ms = now_ms + computeBackoffMs(job.attempt_count);
    }
}

UpdateStatus UploadQueue::mapFailureToUpdateStatus(
    UploadStageResult result,
    const std::string& err_msg) {
    if (err_msg.find("429") != std::string::npos ||
        err_msg.find("rate limit") != std::string::npos ||
        err_msg.find("Rate Limit") != std::string::npos) {
        return UpdateStatus::RateLimited;
    }
    if (err_msg.find("401") != std::string::npos ||
        err_msg.find("auth") != std::string::npos ||
        err_msg.find("Auth") != std::string::npos) {
        return UpdateStatus::AuthError;
    }
    if (result == UploadStageResult::PERMANENT_ERROR) {
        return UpdateStatus::ParseError;
    }
    return UpdateStatus::FetchError;
}

void UploadQueue::publishUploadEvent(const UploadJob& job) {
    if (!_eventBus) return;

    UploadEvent& evt = _eventRing[_eventRingHead];
    _eventRingHead = static_cast<uint8_t>((_eventRingHead + 1u) % EVENT_RING_SIZE);

    std::memset(&evt, 0, sizeof(UploadEvent));
    evt.session_id    = job.session_id;
    evt.stage         = job.stage;
    evt.attempt_count = job.attempt_count;
    std::snprintf(evt.remote_upload_id, sizeof(evt.remote_upload_id),
                  "%s", job.remote_upload_id.c_str());
    std::snprintf(evt.remote_activity_id, sizeof(evt.remote_activity_id),
                  "%s", job.remote_activity_id.c_str());
    std::snprintf(evt.error, sizeof(evt.error),
                  "%s", job.last_error.c_str());

    _eventBus->publish(EVENT_UPLOAD_TOPIC, &evt);
}

// =============================================================================
// JSON Persistence (serializeQueueJson / deserializeQueueJson / saveQueue / loadQueue)
// =============================================================================

bool UploadQueue::serializeQueueJson(std::string& out_json) const {
    out_json.clear();
    JsonDocument doc;
    doc["version"] = 1;
    doc["auto_upload_enabled"] = _autoUploadEnabled;
    JsonArray jobsArr = doc["jobs"].to<JsonArray>();

    for (const auto& job : _jobs) {
        JsonObject obj = jobsArr.add<JsonObject>();
        obj["session_id"]         = job.session_id;
        obj["stage"]              = uploadStageToString(job.stage);
        obj["attempt_count"]      = job.attempt_count;
        obj["next_attempt_ms"]    = job.next_attempt_ms;
        obj["remote_upload_id"]   = job.remote_upload_id;
        obj["remote_activity_id"] = job.remote_activity_id;
        obj["last_error"]         = job.last_error;
    }

    serializeJson(doc, out_json);
    return !out_json.empty();
}

bool UploadQueue::deserializeQueueJson(const std::string& json_str) {
    if (json_str.empty()) {
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        return false;
    }

    if (doc["auto_upload_enabled"].is<bool>()) {
        _autoUploadEnabled = doc["auto_upload_enabled"].as<bool>();
    }

    if (!doc["jobs"].is<JsonArray>()) {
        return false;
    }

    _jobs.clear();
    JsonArray jobsArr = doc["jobs"].as<JsonArray>();
    for (JsonObject obj : jobsArr) {
        if (_jobs.size() >= MAX_QUEUE_ENTRIES) {
            break;
        }
        UploadJob job{};
        job.session_id = obj["session_id"] | 0u;
        if (job.session_id == 0) {
            continue;
        }

        const char* stageStr = obj["stage"] | "STAGE_GENERATE_TCX_GZ";
        UploadStage parsedStage = UploadStage::STAGE_GENERATE_TCX_GZ;
        if (uploadStageFromString(stageStr, parsedStage)) {
            job.stage = parsedStage;
        } else if (obj["stage"].is<int>()) {
            const int rawStage = obj["stage"].as<int>();
            if (rawStage >= 0 && rawStage <= static_cast<int>(UploadStage::STAGE_FAILED)) {
                job.stage = static_cast<UploadStage>(rawStage);
            }
        }

        job.attempt_count      = static_cast<uint8_t>(obj["attempt_count"] | 0u);
        job.next_attempt_ms    = obj["next_attempt_ms"] | 0u;
        job.remote_upload_id   = obj["remote_upload_id"] | "";
        job.remote_activity_id = obj["remote_activity_id"] | "";
        job.last_error         = obj["last_error"] | "";

        _jobs.push_back(job);
    }

    return true;
}

bool UploadQueue::saveQueue(const std::string& path) const {
    const std::string target_path = path.empty() ? _queueFilePath : path;
    if (target_path.empty()) {
        return false;
    }

    std::string json_str;
    if (!serializeQueueJson(json_str)) {
        return false;
    }

    bool saved = false;
    IRideFileSystem* fs = getFileSystem();
    if (fs) {
        fs->mkdir(RideStore::RIDES_DIR);
        if (fs->writeFile(
                target_path.c_str(),
                reinterpret_cast<const uint8_t*>(json_str.data()),
                json_str.size())) {
            saved = true;
        }
    }

#if !defined(ESP_PLATFORM)
    if (writePosixFile(
            target_path,
            reinterpret_cast<const uint8_t*>(json_str.data()),
            json_str.size())) {
        saved = true;
    }
#endif

    return saved;
}

bool UploadQueue::loadQueue(const std::string& path) {
    const std::string target_path = path.empty() ? _queueFilePath : path;
    if (target_path.empty()) {
        return false;
    }

    std::vector<uint8_t> raw;
    bool loaded = false;

    IRideFileSystem* fs = getFileSystem();
    if (fs && fs->exists(target_path.c_str())) {
        loaded = fs->readFile(target_path.c_str(), raw);
    }

#if !defined(ESP_PLATFORM)
    if (!loaded && posixFileExists(target_path)) {
        loaded = readPosixFile(target_path, raw);
    }
#endif

    if (!loaded || raw.empty()) {
        return false;
    }

    const std::string json_str(reinterpret_cast<const char*>(raw.data()), raw.size());
    return deserializeQueueJson(json_str);
}

bool UploadQueue::removeFileEverywhere(const std::string& path) {
    if (path.empty()) return false;
    bool removed = false;

    IRideFileSystem* fs = getFileSystem();
    if (fs && fs->exists(path.c_str())) {
        if (fs->remove(path.c_str())) {
            removed = true;
        }
    }

#if defined(ESP_PLATFORM)
    if (LittleFS.exists(path.c_str())) {
        if (LittleFS.remove(path.c_str())) {
            removed = true;
        }
    }
#else
    if (posixFileExists(path)) {
        if (std::remove(path.c_str()) == 0) {
            removed = true;
        }
    }
#endif

    return removed;
}

bool UploadQueue::fileExistsEverywhere(const std::string& path) const {
    if (path.empty()) return false;
    IRideFileSystem* fs = getFileSystem();
    if (fs && fs->exists(path.c_str())) {
        return true;
    }
#if defined(ESP_PLATFORM)
    if (LittleFS.exists(path.c_str())) {
        return true;
    }
#else
    if (posixFileExists(path)) {
        return true;
    }
#endif
    return false;
}

// =============================================================================
// IDataSource Implementation
// =============================================================================

UpdateStatus UploadQueue::fetchUpdate(const FetchContext& ctx) {
#ifdef ESP_PLATFORM
    if (_uploader) {
        _uploader->setTransportContext(ctx.client);
    }
#endif
    return stepQueue(
        ctx.deadline.remainingMs(),
        millis(),
        [&ctx]() -> bool { return !ctx.deadline.isExpired(); });
}

// =============================================================================
// IConfigurable Implementation ("echenon_upload", schema v1)
// =============================================================================

void UploadQueue::applyConfig(const JsonObject& section) {
    if (section["auto_upload_enabled"].is<bool>()) {
        _autoUploadEnabled = section["auto_upload_enabled"].as<bool>();
    } else if (section["auto_upload"].is<bool>()) {
        _autoUploadEnabled = section["auto_upload"].as<bool>();
    }
}

void UploadQueue::serializeConfig(JsonObject& section) const {
    section["auto_upload_enabled"] = _autoUploadEnabled;
}

void UploadQueue::resetConfig() {
    _autoUploadEnabled = true;
}

// =============================================================================
// IPreconditionContributor Implementation ("strava_configured", "ntp_synced")
// =============================================================================

uint32_t UploadQueue::currentEpochS() const {
    if (_nowEpochFn) {
        return _nowEpochFn();
    }
    const std::time_t now_t = std::time(nullptr);
    return (now_t > 0) ? static_cast<uint32_t>(now_t) : 0u;
}

bool UploadQueue::isNtpSynced() const {
    if (_ntpSyncedFn) {
        return _ntpSyncedFn();
    }
    if (_nowEpochFn) {
        return _nowEpochFn() >= RideStore::VALID_EPOCH_THRESHOLD;
    }
    const uint32_t epoch = currentEpochS();
    return epoch >= RideStore::VALID_EPOCH_THRESHOLD;
}

PreconditionStatus UploadQueue::evalStravaConfigured(void* ctx) {
    const auto* self = static_cast<const UploadQueue*>(ctx);
    if (!self || !self->_uploader || !self->_uploader->isConfigured()) {
        return {false, Cause::NotConfigured};
    }
    if (!self->_uploader->isAuthenticated()) {
        return {false, Cause::Rejected};
    }
    return {true, Cause::Met};
}

void UploadQueue::describeStravaConfigured(Cause cause, char* buf, size_t len, void* ctx) {
    (void)ctx;
    if (!buf || len == 0) return;
    switch (cause) {
        case Cause::NotConfigured:
            std::snprintf(buf, len, "Configure Strava Client ID & Secret in Upload Services tab");
            break;
        case Cause::Rejected:
            std::snprintf(buf, len, "Connect Strava account via OAuth in Upload Services tab");
            break;
        default:
            std::snprintf(buf, len, "Strava uploader ready");
            break;
    }
}

PreconditionStatus UploadQueue::evalNtpSynced(void* ctx) {
    const auto* self = static_cast<const UploadQueue*>(ctx);
    if (!self || !self->isNtpSynced()) {
        return {false, Cause::Connecting};
    }
    return {true, Cause::Met};
}

void UploadQueue::describeNtpSynced(Cause cause, char* buf, size_t len, void* ctx) {
    (void)ctx;
    if (!buf || len == 0) return;
    if (cause == Cause::Met) {
        std::snprintf(buf, len, "Wall-clock synchronized via NTP");
    } else {
        std::snprintf(buf, len, "Waiting for NTP wall-clock synchronization");
    }
}

void UploadQueue::registerPreconditions(IPreconditionRegistry& registry) {
    registry.add(
        "strava_configured",
        PreconditionRole::Operational,
        &UploadQueue::evalStravaConfigured,
        &UploadQueue::describeStravaConfigured,
        this);

    registry.add(
        "ntp_synced",
        PreconditionRole::Operational,
        &UploadQueue::evalNtpSynced,
        &UploadQueue::describeNtpSynced,
        this);
}

} // namespace echenon
