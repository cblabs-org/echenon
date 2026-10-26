#pragma once

/// @file UploadQueue.h
/// @brief Split-stage background upload queue implementing IDataSource,
///        IConfigurable ("echenon_upload"), and IPreconditionContributor
///        (SPEC.md §10.1–§10.2, §10.6).

#include "echenon/IUploader.h"
#include "echenon/RideStore.h"
#include "echenon/TCXGenerator.h"

#include "interfaces/IConfigurable.hpp"
#include "interfaces/IDataSource.hpp"
#include "interfaces/IPreconditionContributor.hpp"
#include "interfaces/IPreconditionRegistry.hpp"
#include "modules/EventBus/EventBus.hpp"

#ifdef WITH_BLE
#include "modules/BLEManager/BLEManager.hpp"
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace echenon {

/// EventBus topic published when an upload job completes or fails (SPEC.md §10.2).
static constexpr const char* EVENT_UPLOAD_TOPIC = "echenon.upload";

/// Stages of the split-stage upload state machine (SPEC.md §10.2).
enum class UploadStage : uint8_t {
    STAGE_GENERATE_TCX_GZ = 0, ///< Stream /rides/ride_<id>.bin -> /rides/ride_<id>.tcx.gz
    STAGE_REFRESH_TOKEN   = 1, ///< Refresh OAuth access token if expired
    STAGE_POST_UPLOAD     = 2, ///< POST multipart .tcx.gz and capture remote_upload_id
    STAGE_POLL_STATUS     = 3, ///< Poll remote upload status until activity_id is ready
    STAGE_COMPLETE        = 4, ///< Upload completed; .tcx.gz removed
    STAGE_FAILED          = 5  ///< Upload failed permanently or exhausted MAX_ATTEMPTS
};

/// Convert UploadStage to canonical string representation.
const char* uploadStageToString(UploadStage stage);

/// Parse UploadStage from string representation (returns false if unrecognized).
bool uploadStageFromString(const char* str, UploadStage& out_stage);

/// Persistent state for a single queued session upload (SPEC.md §10.2).
struct UploadJob {
    uint32_t    session_id         = 0;
    UploadStage stage              = UploadStage::STAGE_GENERATE_TCX_GZ;
    uint8_t     attempt_count      = 0;
    uint32_t    next_attempt_ms    = 0;
    std::string remote_upload_id;
    std::string remote_activity_id;
    std::string last_error;
};

/// Payload published on EVENT_UPLOAD_TOPIC ("echenon.upload").
/// Stored in a persistent ring buffer on UploadQueue to satisfy EventBus async dispatch safety.
struct UploadEvent {
    uint32_t    session_id         = 0;
    UploadStage stage              = UploadStage::STAGE_COMPLETE;
    uint8_t     attempt_count      = 0;
    char        remote_upload_id[32]{};
    char        remote_activity_id[32]{};
    char        error[64]{};
};

/**
 * @brief Split-stage background upload state machine scheduled by DataManager.
 *
 * Executes at most ONE stage per `fetchUpdate()` / `stepQueue()` invocation so
 * every step remains strictly bounded within the 15 s `DeadlineToken` budget.
 * Acquires `BLEManager::ScanInhibit` (under `#ifdef WITH_BLE`) during network
 * stages (`STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`).
 */
class UploadQueue : public IDataSource,
                    public IConfigurable,
                    public IPreconditionContributor {
public:
    static constexpr size_t   MAX_QUEUE_ENTRIES          = 5;
    static constexpr uint8_t  MAX_ATTEMPTS               = 5;
    static constexpr uint32_t POLL_DELAY_MS              = 3000u;
    static constexpr uint32_t REFRESH_STAGE_BUDGET_MS    = 8000u;
    static constexpr uint32_t POST_STAGE_BUDGET_MS       = 10000u;
    static constexpr uint32_t POLL_STAGE_BUDGET_MS       = 8000u;
    static constexpr uint32_t DEFAULT_UPDATE_INTERVAL_MS = 10000u;

    static constexpr const char* DEFAULT_QUEUE_FILE_PATH = "/rides/upload_queue.json";

    using NowEpochFn = std::function<uint32_t()>;
    using NtpSyncedFn = std::function<bool()>;
    using TcxGenerateFn = std::function<bool(
        uint32_t session_id,
        const std::string& out_path,
        const DeadlineCheckFn& check_deadline)>;

    UploadQueue() = default;
    explicit UploadQueue(IUploader* uploader,
                         RideStore* rideStore = nullptr,
                         EventBus* eventBus = nullptr);
    UploadQueue(RideStore& rideStore,
                IUploader* uploader = nullptr,
                EventBus* eventBus = nullptr);
    ~UploadQueue() override = default;

    // --- Lifecycle & Dependency Wiring ---
    bool begin();
    void setUploader(IUploader* uploader) { _uploader = uploader; }
    IUploader* getUploader() const { return _uploader; }

    void setRideStore(RideStore* rideStore);
    RideStore* getRideStore() const { return _rideStore; }

    void setEventBus(EventBus* eventBus) { _eventBus = eventBus; }
    EventBus* getEventBus() const { return _eventBus; }

    void setFileSystem(IRideFileSystem* fs) { _fs = fs; }
    IRideFileSystem* getFileSystem() const;

    void setBaseDir(const std::string& base_dir);
    const std::string& getBaseDir() const { return _baseDir; }

    void setQueueFilePath(const std::string& path) { _queueFilePath = path; }
    const std::string& getQueueFilePath() const { return _queueFilePath; }

    void setNowEpochCallback(NowEpochFn fn) { _nowEpochFn = std::move(fn); }
    void setNtpSyncedCallback(NtpSyncedFn fn) { _ntpSyncedFn = std::move(fn); }
    void setTcxGeneratorCallback(TcxGenerateFn fn) { _tcxGeneratorFn = std::move(fn); }

#ifdef WITH_BLE
    void setBLEManager(BLEManager* bleManager) { _bleManager = bleManager; }
    BLEManager* getBLEManager() const { return _bleManager; }
#endif

    // --- Queue Operations ---
    bool enqueueSession(uint32_t session_id, uint32_t now_ms = 0);
    bool enqueue(uint32_t session_id, uint32_t now_ms = 0) {
        return enqueueSession(session_id, now_ms);
    }

    /// Remove a session from the queue and delete its .tcx.gz / .bin artifacts.
    bool discardSession(uint32_t session_id);

    /// Execute at most ONE stage of the first ready job in the queue.
    UpdateStatus stepQueue(
        uint32_t remaining_budget_ms,
        uint32_t now_ms,
        const DeadlineCheckFn& check_deadline);

    UpdateStatus stepQueue(uint32_t remaining_budget_ms, uint32_t now_ms) {
        return stepQueue(remaining_budget_ms, now_ms, nullptr);
    }

    // --- Persistence & JSON Serialization ---
    bool serializeQueueJson(std::string& out_json) const;
    bool deserializeQueueJson(const std::string& json_str);
    bool saveQueue(const std::string& path = "") const;
    bool loadQueue(const std::string& path = "");

    // --- Accessors & Helpers ---
    size_t size() const { return _jobs.size(); }
    size_t getQueueSize() const { return _jobs.size(); }
    size_t getPendingCount() const;
    bool empty() const { return _jobs.empty(); }
    void clearQueue();

    const std::vector<UploadJob>& getJobs() const { return _jobs; }
    const UploadJob* findJob(uint32_t session_id) const;
    UploadJob* findJobMutable(uint32_t session_id);

    bool isAutoUploadEnabled() const { return _autoUploadEnabled; }
    void setAutoUploadEnabled(bool enabled) { _autoUploadEnabled = enabled; }

    void setUpdateIntervalMs(uint32_t interval_ms) { _updateIntervalMs = interval_ms; }

    std::string getTcxGzPath(uint32_t session_id) const;

    /// Wraparound-safe signed readiness check: (int32_t)(now_ms - job.next_attempt_ms) >= 0.
    static bool isJobReady(const UploadJob& job, uint32_t now_ms);

    /// Exponential backoff schedule: 1->30s, 2->60s, 3->120s, >=4->300s.
    static uint32_t computeBackoffMs(uint8_t attempt_count);

    // --- IDataSource ---
    UpdateStatus fetchUpdate(const FetchContext& ctx) override;
    uint32_t getUpdateIntervalMs() const override { return _updateIntervalMs; }
    const char* getSourceName() const override { return "echenon.upload"; }
    PriorityTier getPriorityTier() const override { return PriorityTier::High; }
    bool requiresTls() const override { return true; }

    // --- IConfigurable ("echenon_upload", schema v1) ---
    const char* getConfigNamespace() const override { return "echenon_upload"; }
    int getSchemaVersion() const override { return 1; }
    void applyConfig(const JsonObject& section) override;
    void serializeConfig(JsonObject& section) const override;
    void resetConfig() override;

    // --- IPreconditionContributor ---
    void registerPreconditions(IPreconditionRegistry& registry) override;

private:
    void handleStageFailure(
        UploadJob& job,
        UploadStageResult result,
        const std::string& err_msg,
        uint32_t now_ms);

    static UpdateStatus mapFailureToUpdateStatus(
        UploadStageResult result,
        const std::string& err_msg);

    void publishUploadEvent(const UploadJob& job);
    bool removeFileEverywhere(const std::string& path);
    bool fileExistsEverywhere(const std::string& path) const;
    bool isNtpSynced() const;
    uint32_t currentEpochS() const;

    static PreconditionStatus evalStravaConfigured(void* ctx);
    static void describeStravaConfigured(Cause cause, char* buf, size_t len, void* ctx);
    static PreconditionStatus evalNtpSynced(void* ctx);
    static void describeNtpSynced(Cause cause, char* buf, size_t len, void* ctx);

    IUploader*       _uploader   = nullptr;
    RideStore*       _rideStore  = nullptr;
    EventBus*        _eventBus   = nullptr;
    IRideFileSystem* _fs         = nullptr;

#ifdef WITH_BLE
    BLEManager*      _bleManager = nullptr;
#endif

    std::vector<UploadJob> _jobs;
    std::string            _baseDir;
    std::string            _queueFilePath = DEFAULT_QUEUE_FILE_PATH;
    bool                   _autoUploadEnabled = true;
    uint32_t               _updateIntervalMs  = DEFAULT_UPDATE_INTERVAL_MS;

    NowEpochFn    _nowEpochFn;
    NtpSyncedFn   _ntpSyncedFn;
    TcxGenerateFn _tcxGeneratorFn;

    static constexpr size_t EVENT_RING_SIZE = 4;
    UploadEvent _eventRing[EVENT_RING_SIZE]{};
    uint8_t     _eventRingHead = 0;
};

} // namespace echenon
