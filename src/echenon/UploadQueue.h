#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/UploadQueue.h
 * Description: Split-stage multi-sink background upload queue implementing IDataSource, IConfigurable ("echenon_upload"), and IPreconditionContributor (SPEC.md §10.1–§10.2, §10.6).
 */

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
/// Multi-sink: (session_id, service_id) is the composite key.
struct UploadJob {
    uint32_t    session_id         = 0;                                  ///< Ride session identifier
    std::string service_id;                                              ///< Sink identifier (e.g. "strava", "intervals")
    UploadStage stage              = UploadStage::STAGE_GENERATE_TCX_GZ; ///< Current split-stage state machine step
    uint8_t     attempt_count      = 0;                                  ///< Consecutive retry attempts for current stage
    uint32_t    next_attempt_ms    = 0;                                  ///< Monotonic timestamp (ms) when next attempt is eligible
    std::string remote_upload_id;                                        ///< Remote asynchronous upload job ID (if applicable)
    std::string remote_activity_id;                                      ///< Finalized remote activity identifier
    std::string last_error;                                              ///< Most recent stage error description
};

/// Payload published on EVENT_UPLOAD_TOPIC ("echenon.upload").
/// Stored in a persistent ring buffer on UploadQueue to satisfy EventBus async dispatch safety.
struct UploadEvent {
    uint32_t    session_id         = 0;                        ///< Ride session identifier
    char        service_id[16]{};                              ///< Sink identifier for multi-sink dispatch
    UploadStage stage              = UploadStage::STAGE_COMPLETE; ///< Terminal or progress UploadStage
    uint8_t     attempt_count      = 0;                        ///< Attempt count at event emission
    char        remote_upload_id[32]{};                        ///< Remote upload ID string
    char        remote_activity_id[32]{};                      ///< Remote activity ID string
    char        error[64]{};                                   ///< Truncated error message (if failed)
};

/**
 * @brief Split-stage background upload state machine scheduled by DataManager.
 *
 * Executes at most ONE stage per `fetchUpdate()` / `stepQueue()` invocation so
 * every step remains strictly bounded within the 15 s `DeadlineToken` budget.
 * Acquires `BLEManager::ScanInhibit` (under `#ifdef WITH_BLE`) during network
 * stages (`STAGE_REFRESH_TOKEN`, `STAGE_POST_UPLOAD`, `STAGE_POLL_STATUS`).
 *
 * Multi-sink: dispatches upload jobs across up to MAX_SINKS registered
 * IUploader implementations. Sequential sink execution in registration order,
 * one network stage at a time. Shared .tcx.gz lifecycle: generate once per
 * session, delete when all sink jobs for that session terminate.
 */
class UploadQueue : public IDataSource,
                    public IConfigurable,
                    public IPreconditionContributor {
public:
    static constexpr size_t   MAX_SINKS                  = 4;      ///< Maximum registered IUploader sinks
    static constexpr size_t   MAX_QUEUE_ENTRIES          = 5;      ///< Maximum queued upload jobs (matches 5-ride FIFO ring)
    static constexpr uint8_t  MAX_ATTEMPTS               = 5;      ///< Maximum retry attempts before STAGE_FAILED
    static constexpr uint32_t POLL_DELAY_MS              = 3000u;  ///< Minimum delay between remote status polls (ms)
    static constexpr uint32_t REFRESH_STAGE_BUDGET_MS    = 8000u;  ///< Max HTTP timeout budget for STAGE_REFRESH_TOKEN (ms)
    static constexpr uint32_t POST_STAGE_BUDGET_MS       = 10000u; ///< Max HTTP timeout budget for STAGE_POST_UPLOAD (ms)
    static constexpr uint32_t POLL_STAGE_BUDGET_MS       = 8000u;  ///< Max HTTP timeout budget for STAGE_POLL_STATUS (ms)
    static constexpr uint32_t DEFAULT_UPDATE_INTERVAL_MS = 10000u; ///< Default DataManager polling interval (ms)

    static constexpr const char* DEFAULT_QUEUE_FILE_PATH = "/rides/upload_queue.json"; ///< Default LittleFS path for persisted queue JSON

    /// Callback returning current Unix epoch in seconds (used for token expiry checks).
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only on background DataManager fetchUpdate() cold path, never in tick(); supports native test spies.
    using NowEpochFn = std::function<uint32_t()>;
    /// Callback returning true when SNTP wall-clock synchronization is valid.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only on background DataManager fetchUpdate() cold path, never in tick(); supports native test spies.
    using NtpSyncedFn = std::function<bool()>;
    /// Callback invoked to generate a `.tcx.gz` file for session_id within check_deadline.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda <=16B capture) and invoked only during STAGE_GENERATE_TCX_GZ cold path, never in tick(); supports native test spies.
    using TcxGenerateFn = std::function<bool(
        uint32_t session_id,
        const std::string& out_path,
        const DeadlineCheckFn& check_deadline)>;

    /// Default constructor.
    UploadQueue() = default;
    /// Construct UploadQueue with optional primary uploader, RideStore, and EventBus.
    explicit UploadQueue(IUploader* uploader,
                         RideStore* rideStore = nullptr,
                         EventBus* eventBus = nullptr);
    /// Construct UploadQueue with RideStore reference and optional uploader/EventBus.
    UploadQueue(RideStore& rideStore,
                IUploader* uploader = nullptr,
                EventBus* eventBus = nullptr);
    /// Default destructor.
    ~UploadQueue() override = default;

    // --- Lifecycle & Dependency Wiring ---
    /// Load persisted upload queue jobs from filesystem.
    bool begin();

    // --- Multi-Sink Registry ---
    /// Register an IUploader sink (up to MAX_SINKS). Returns false if full.
    bool registerSink(IUploader* sink);

    /// Number of registered sinks.
    size_t getSinkCount() const { return _sinkCount; }

    /// Get sink by index (nullptr if out of range).
    IUploader* getSink(size_t index) const;

    /// Find a registered sink by serviceId (nullptr if not found).
    IUploader* findSinkById(const char* service_id) const;

    /// Legacy single-uploader API: calls registerSink() if not already registered.
    void setUploader(IUploader* uploader);

    /// Legacy single-uploader API: returns first registered sink (or nullptr).
    IUploader* getUploader() const { return _sinkCount > 0 ? _sinks[0] : nullptr; }

    /// Attach RideStore dependency used to locate finalized ride binaries.
    void setRideStore(RideStore* rideStore);
    /// Return attached RideStore pointer.
    RideStore* getRideStore() const { return _rideStore; }

    /// Attach EventBus pointer for publishing UploadEvent notifications.
    void setEventBus(EventBus* eventBus) { _eventBus = eventBus; }
    /// Return attached EventBus pointer.
    EventBus* getEventBus() const { return _eventBus; }

    /// Override IRideFileSystem used for queue JSON and `.tcx.gz` file cleanup.
    void setFileSystem(IRideFileSystem* fs) { _fs = fs; }
    /// Resolve active IRideFileSystem (explicit override or RideStore backing FS).
    IRideFileSystem* getFileSystem() const;

    /// Set base directory prefix for generated `.tcx.gz` files (used in tests).
    void setBaseDir(const std::string& base_dir);
    /// Return configured base directory prefix.
    const std::string& getBaseDir() const { return _baseDir; }

    /// Set path for persisted queue JSON file.
    void setQueueFilePath(const std::string& path) { _queueFilePath = path; }
    /// Return configured path for persisted queue JSON file.
    const std::string& getQueueFilePath() const { return _queueFilePath; }

    /// Register callback supplying current Unix epoch seconds (bound once at boot).
    void setNowEpochCallback(NowEpochFn fn) { _nowEpochFn = std::move(fn); }
    /// Register callback reporting SNTP wall-clock sync state (bound once at boot).
    void setNtpSyncedCallback(NtpSyncedFn fn) { _ntpSyncedFn = std::move(fn); }
    /// Register custom `.tcx.gz` generator callback (bound once at boot).
    void setTcxGeneratorCallback(TcxGenerateFn fn) { _tcxGeneratorFn = std::move(fn); }

#ifdef WITH_BLE
    /// Attach BLEManager so network upload stages can acquire ScanInhibit guards.
    void setBLEManager(BLEManager* bleManager) { _bleManager = bleManager; }
    /// Return attached BLEManager pointer.
    BLEManager* getBLEManager() const { return _bleManager; }
#endif

    // --- Queue Operations ---

    /// Enqueue upload jobs for a session across all registered, configured, and enabled sinks.
    bool enqueueSession(uint32_t session_id, uint32_t now_ms = 0);
    /// Alias for enqueueSession().
    bool enqueue(uint32_t session_id, uint32_t now_ms = 0) {
        return enqueueSession(session_id, now_ms);
    }

    /// Remove all jobs for a session from the queue and delete its .tcx.gz / .bin artifacts.
    bool discardSession(uint32_t session_id);

    /// Execute at most ONE stage of the first ready job in the queue.
    UpdateStatus stepQueue(
        uint32_t remaining_budget_ms,
        uint32_t now_ms,
        const DeadlineCheckFn& check_deadline);

    /// Convenience overload for stepQueue() without a deadline callback.
    UpdateStatus stepQueue(uint32_t remaining_budget_ms, uint32_t now_ms) {
        return stepQueue(remaining_budget_ms, now_ms, nullptr);
    }

    // --- Persistence & JSON Serialization ---
    /// Serialize all queued upload jobs into a JSON string.
    bool serializeQueueJson(std::string& out_json) const;
    /// Restore queued upload jobs from a JSON string.
    bool deserializeQueueJson(const std::string& json_str);
    /// Persist queue state to path (defaults to _queueFilePath).
    bool saveQueue(const std::string& path = "") const;
    /// Load persisted queue state from path (defaults to _queueFilePath).
    bool loadQueue(const std::string& path = "");

    // --- Accessors & Helpers ---
    /// Return total number of jobs in the queue.
    size_t size() const { return _jobs.size(); }
    /// Alias for size().
    size_t getQueueSize() const { return _jobs.size(); }
    /// Return count of non-terminal jobs remaining in the queue.
    size_t getPendingCount() const;
    /// Return true if the queue has no jobs.
    bool empty() const { return _jobs.empty(); }
    /// Remove all jobs from the queue and persist empty state.
    void clearQueue();

    /// Return read-only reference to all queued UploadJob entries.
    const std::vector<UploadJob>& getJobs() const { return _jobs; }

    /// Find job by session_id only (returns first match — legacy single-sink compat).
    const UploadJob* findJob(uint32_t session_id) const;
    /// Mutable overload of findJob(session_id).
    UploadJob* findJobMutable(uint32_t session_id);

    /// Find job by composite key (session_id, service_id).
    const UploadJob* findJob(uint32_t session_id, const std::string& service_id) const;
    /// Mutable overload of findJob(session_id, service_id).
    UploadJob* findJobMutable(uint32_t session_id, const std::string& service_id);

    /// Returns true if all jobs for a given session are in a terminal state
    /// (STAGE_COMPLETE or STAGE_FAILED).
    bool allSinkJobsTerminal(uint32_t session_id) const;

    /// Return true if automatic post-ride upload enqueueing is enabled.
    bool isAutoUploadEnabled() const { return _autoUploadEnabled; }
    /// Enable or disable automatic post-ride upload enqueueing.
    void setAutoUploadEnabled(bool enabled) { _autoUploadEnabled = enabled; }

    /// Set DataManager fetchUpdate() polling interval in milliseconds.
    void setUpdateIntervalMs(uint32_t interval_ms) { _updateIntervalMs = interval_ms; }

    /// Compute filesystem path for a session's `.tcx.gz` file.
    std::string getTcxGzPath(uint32_t session_id) const;

    /// Wraparound-safe signed readiness check: (int32_t)(now_ms - job.next_attempt_ms) >= 0.
    static bool isJobReady(const UploadJob& job, uint32_t now_ms);

    /// Exponential backoff schedule: 1->30s, 2->60s, 3->120s, >=4->300s.
    static uint32_t computeBackoffMs(uint8_t attempt_count);

    // --- IDataSource ---
    /// Execute one bounded upload stage inside DataManager's FetchContext.
    UpdateStatus fetchUpdate(const FetchContext& ctx) override;
    /// Return DataManager polling interval in milliseconds.
    uint32_t getUpdateIntervalMs() const override { return _updateIntervalMs; }
    /// Return canonical DataSource identifier ("echenon.upload").
    const char* getSourceName() const override { return "echenon.upload"; }
    /// Return DataManager priority tier (High).
    PriorityTier getPriorityTier() const override { return PriorityTier::High; }
    /// Return true (HTTPS uploads require framework TLS client context).
    bool requiresTls() const override { return true; }

    // --- IConfigurable ("echenon_upload", schema v1) ---
    /// Return configuration namespace ("echenon_upload").
    const char* getConfigNamespace() const override { return "echenon_upload"; }
    /// Return configuration schema version (1).
    int getSchemaVersion() const override { return 1; }
    /// Apply persisted JSON configuration (auto_upload).
    void applyConfig(const JsonObject& section) override;
    /// Serialize upload queue configuration into JSON object.
    void serializeConfig(JsonObject& section) const override;
    /// Reset upload queue configuration fields to defaults.
    void resetConfig() override;

    // --- IPreconditionContributor ---
    /// Register upload sink configuration and SNTP sync preconditions with PreconditionEngine.
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

    /// Look up the IUploader* for a job's service_id from the registered sinks.
    IUploader* sinkForJob(const UploadJob& job) const;

    static PreconditionStatus evalSinkConfigured(void* ctx);
    static void describeSinkConfigured(Cause cause, char* buf, size_t len, void* ctx);
    static PreconditionStatus evalNtpSynced(void* ctx);
    static void describeNtpSynced(Cause cause, char* buf, size_t len, void* ctx);

    // Multi-sink registry: static array, no heap allocation (GRAVE-05 safe).
    IUploader*       _sinks[MAX_SINKS]{};
    size_t           _sinkCount  = 0;

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

    // GRAVE-05 deviation: DI callbacks set once at boot, never reallocated.
    // Framework's own DataManager uses the same pattern for IDataSource injection.
    NowEpochFn    _nowEpochFn;
    NtpSyncedFn   _ntpSyncedFn;
    TcxGenerateFn _tcxGeneratorFn;

    static constexpr size_t EVENT_RING_SIZE = 4;
    UploadEvent _eventRing[EVENT_RING_SIZE]{};
    uint8_t     _eventRingHead = 0;
};

} // namespace echenon
