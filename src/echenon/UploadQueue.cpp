/// @file UploadQueue.cpp
/// @brief Split-stage background upload state machine and LittleFS job persistence.
///        Multi-sink architecture with sequential execution and shared .tcx.gz lifecycle.

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
    : _rideStore(rideStore)
    , _eventBus(eventBus)
    , _fs(rideStore ? &rideStore->fileSystem() : nullptr) {
    if (uploader) {
        registerSink(uploader);
    }
}

UploadQueue::UploadQueue(RideStore& rideStore, IUploader* uploader, EventBus* eventBus)
    : _rideStore(&rideStore)
    , _eventBus(eventBus)
    , _fs(&rideStore.fileSystem()) {
    if (uploader) {
        registerSink(uploader);
    }
}

// =============================================================================
// Multi-Sink Registry
// =============================================================================

bool UploadQueue::registerSink(IUploader* sink) {
    if (!sink || _sinkCount >= MAX_SINKS) {
        return false;
    }
    // Prevent duplicate registration by checking serviceId
    for (size_t i = 0; i < _sinkCount; ++i) {
        if (_sinks[i] && std::strcmp(_sinks[i]->serviceId(), sink->serviceId()) == 0) {
            _sinks[i] = sink; // Replace existing registration
            return true;
        }
    }
    _sinks[_sinkCount++] = sink;
    return true;
}

IUploader* UploadQueue::getSink(size_t index) const {
    return (index < _sinkCount) ? _sinks[index] : nullptr;
}

IUploader* UploadQueue::findSinkById(const char* service_id) const {
    if (!service_id) return nullptr;
    for (size_t i = 0; i < _sinkCount; ++i) {
        if (_sinks[i] && std::strcmp(_sinks[i]->serviceId(), service_id) == 0) {
            return _sinks[i];
        }
    }
    return nullptr;
}

void UploadQueue::setUploader(IUploader* uploader) {
    if (uploader) {
        registerSink(uploader);
    }
}

IUploader* UploadQueue::sinkForJob(const UploadJob& job) const {
    if (!job.service_id.empty()) {
        return findSinkById(job.service_id.c_str());
    }
    // Fallback for legacy jobs without service_id: use first sink
    return (_sinkCount > 0) ? _sinks[0] : nullptr;
}

// =============================================================================
// Lifecycle Helpers
// =============================================================================

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
// Multi-Sink Job Helpers
// =============================================================================

const UploadJob* UploadQueue::findJob(uint32_t session_id, const std::string& service_id) const {
    for (const auto& j : _jobs) {
        if (j.session_id == session_id && j.service_id == service_id) {
            return &j;
        }
    }
    return nullptr;
}

UploadJob* UploadQueue::findJobMutable(uint32_t session_id, const std::string& service_id) {
    for (auto& j : _jobs) {
        if (j.session_id == session_id && j.service_id == service_id) {
            return &j;
        }
    }
    return nullptr;
}

bool UploadQueue::allSinkJobsTerminal(uint32_t session_id) const {
    bool found = false;
    for (const auto& j : _jobs) {
        if (j.session_id == session_id) {
            found = true;
            if (j.stage != UploadStage::STAGE_COMPLETE &&
                j.stage != UploadStage::STAGE_FAILED) {
                return false;
            }
        }
    }
    return found; // true if all terminal (or false if no jobs found)
}

// =============================================================================
// Queue Management (Enqueue / Discard / Accessors)
// =============================================================================

bool UploadQueue::enqueueSession(uint32_t session_id, uint32_t now_ms) {
    if (session_id == 0) {
        return false;
    }

    // If no sinks are registered, fall back to legacy behavior:
    // enqueue a single job with "strava" service_id for backward compatibility.
    const bool hasSinks = (_sinkCount > 0);

    bool any_enqueued = false;

    if (hasSinks) {
        // Multi-sink fan-out: create a job for each configured and enabled sink
        for (size_t i = 0; i < _sinkCount; ++i) {
            IUploader* sink = _sinks[i];
            if (!sink || !sink->isConfigured() || !sink->isEnabled()) {
                continue;
            }

            const std::string sid = sink->serviceId();

            // Check if (session_id, service_id) composite key already exists
            UploadJob* existing = findJobMutable(session_id, sid);
            if (existing) {
                if (existing->stage == UploadStage::STAGE_COMPLETE ||
                    existing->stage == UploadStage::STAGE_FAILED) {
                    existing->stage              = UploadStage::STAGE_GENERATE_TCX_GZ;
                    existing->attempt_count      = 0;
                    existing->next_attempt_ms    = now_ms;
                    existing->remote_upload_id.clear();
                    existing->remote_activity_id.clear();
                    existing->last_error.clear();
                }
                any_enqueued = true;
                continue;
            }

            // Enforce MAX_QUEUE_ENTRIES: evict oldest completed or failed job first
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
                        // Queue is full of active jobs; skip this sink
                        continue;
                    }
                }
            }

            UploadJob job{};
            job.session_id      = session_id;
            job.service_id      = sid;
            job.stage           = UploadStage::STAGE_GENERATE_TCX_GZ;
            job.attempt_count   = 0;
            job.next_attempt_ms = now_ms;
            _jobs.push_back(job);
            any_enqueued = true;
        }
    } else {
        // Legacy no-sink mode: single job with "strava" default service_id
        const std::string default_sid = "strava";

        // Check if session_id already present (legacy: match by session_id only)
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

        // Enforce MAX_QUEUE_ENTRIES
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
                    return false; // Queue full of active jobs
                }
            }
        }

        UploadJob job{};
        job.session_id      = session_id;
        job.service_id      = default_sid;
        job.stage           = UploadStage::STAGE_GENERATE_TCX_GZ;
        job.attempt_count   = 0;
        job.next_attempt_ms = now_ms;
        _jobs.push_back(job);
        any_enqueued = true;
    }

    if (any_enqueued) {
        saveQueue();
    }
    return any_enqueued;
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

    if (!_autoUploadEnabled) {
        return UpdateStatus::Skipped;
    }

    // Multi-sink: need at least one registered sink to proceed
    if (_sinkCount == 0) {
        return UpdateStatus::Skipped;
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

    // Look up the sink for this job
    IUploader* sink = sinkForJob(*readyJob);
    if (!sink) {
        // No matching sink registered for this job's service_id — skip
        return UpdateStatus::Skipped;
    }

    // Check sink preconditions
    if (!sink->isConfigured()) {
        return UpdateStatus::Skipped;
    }
    if (!sink->isAuthenticated()) {
        return UpdateStatus::AuthError;
    }

    switch (readyJob->stage) {
        case UploadStage::STAGE_GENERATE_TCX_GZ: {
            // Stage 1: Generate /rides/ride_<id>.tcx.gz (No network I/O, no BLE ScanInhibit)
            // Skip generation if .tcx.gz already exists (shared across sink jobs)
            const std::string tcx_path = getTcxGzPath(readyJob->session_id);
            bool file_already_exists = fileExistsEverywhere(tcx_path);

            if (!file_already_exists) {
                // Check canonical path too
                const std::string canonical_tcx = RideStore::getRideTcxGzPath(readyJob->session_id);
                file_already_exists = (canonical_tcx != tcx_path && fileExistsEverywhere(canonical_tcx));
            }

            bool generated = file_already_exists;

            if (!generated) {
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
            if (sink->needsTokenRefresh(now_epoch_s)) {
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
                ? _bleManager->inhibitScanning("upload_queue")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, REFRESH_STAGE_BUDGET_MS);
            std::string err_msg;
            const UploadStageResult res = sink->refreshToken(stage_timeout_ms, err_msg);

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
                ? _bleManager->inhibitScanning("upload_queue")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, POST_STAGE_BUDGET_MS);
            const std::string tcx_path = getTcxGzPath(readyJob->session_id);
            std::string upload_id;
            std::string err_msg;

            const UploadStageResult res = sink->postUpload(
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
                ? _bleManager->inhibitScanning("upload_queue")
                : BLEManager::ScanInhibit{};
#endif
            const uint32_t stage_timeout_ms =
                std::min<uint32_t>(remaining_budget_ms, POLL_STAGE_BUDGET_MS);
            std::string activity_id;
            std::string err_msg;

            const UploadStageResult res = sink->pollUploadStatus(
                readyJob->remote_upload_id,
                stage_timeout_ms,
                activity_id,
                err_msg);

            if (res == UploadStageResult::SUCCESS) {
                readyJob->remote_activity_id = activity_id;
                readyJob->stage              = UploadStage::STAGE_COMPLETE;
                readyJob->last_error.clear();

                // Shared .tcx.gz lifecycle: delete only when ALL sink jobs
                // for this session are terminal.
                if (allSinkJobsTerminal(readyJob->session_id)) {
                    const std::string tcx_path = getTcxGzPath(readyJob->session_id);
                    removeFileEverywhere(tcx_path);
                    const std::string canonical_tcx = RideStore::getRideTcxGzPath(readyJob->session_id);
                    if (canonical_tcx != tcx_path) {
                        removeFileEverywhere(canonical_tcx);
                    }
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

            // If this failure makes all sink jobs terminal, clean up .tcx.gz
            if (readyJob->stage == UploadStage::STAGE_FAILED &&
                allSinkJobsTerminal(readyJob->session_id)) {
                const std::string tcx_path = getTcxGzPath(readyJob->session_id);
                removeFileEverywhere(tcx_path);
                const std::string canonical_tcx = RideStore::getRideTcxGzPath(readyJob->session_id);
                if (canonical_tcx != tcx_path) {
                    removeFileEverywhere(canonical_tcx);
                }
            }

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
    std::snprintf(evt.service_id, sizeof(evt.service_id),
                  "%s", job.service_id.c_str());
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
        obj["service_id"]         = job.service_id;
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

        // service_id with backward compat: default to "strava" for legacy JSON
        const char* sid = obj["service_id"] | "strava";
        job.service_id = sid;

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
    // Supply transport context to all registered sinks
    for (size_t i = 0; i < _sinkCount; ++i) {
        if (_sinks[i]) {
            _sinks[i]->setTransportContext(ctx.client);
        }
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
// IPreconditionContributor Implementation (generic multi-sink)
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

PreconditionStatus UploadQueue::evalSinkConfigured(void* ctx) {
    const auto* self = static_cast<const UploadQueue*>(ctx);
    if (!self || self->_sinkCount == 0) {
        return {false, Cause::NotConfigured};
    }
    // Check if any registered sink is configured and authenticated
    bool any_configured = false;
    bool any_authenticated = false;
    for (size_t i = 0; i < self->_sinkCount; ++i) {
        if (self->_sinks[i] && self->_sinks[i]->isConfigured()) {
            any_configured = true;
            if (self->_sinks[i]->isAuthenticated()) {
                any_authenticated = true;
                break;
            }
        }
    }
    if (!any_configured) {
        return {false, Cause::NotConfigured};
    }
    if (!any_authenticated) {
        return {false, Cause::Rejected};
    }
    return {true, Cause::Met};
}

void UploadQueue::describeSinkConfigured(Cause cause, char* buf, size_t len, void* ctx) {
    (void)ctx;
    if (!buf || len == 0) return;
    switch (cause) {
        case Cause::NotConfigured:
            std::snprintf(buf, len, "Configure at least one upload service in Upload Services tab");
            break;
        case Cause::Rejected:
            std::snprintf(buf, len, "Connect an upload service account via OAuth or API key");
            break;
        default:
            std::snprintf(buf, len, "Upload service ready");
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
        &UploadQueue::evalSinkConfigured,
        &UploadQueue::describeSinkConfigured,
        this);

    registry.add(
        "ntp_synced",
        PreconditionRole::Operational,
        &UploadQueue::evalNtpSynced,
        &UploadQueue::describeNtpSynced,
        this);
}

} // namespace echenon
