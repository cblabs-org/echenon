#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/IUploader.h
 * Description: Pure capability interface for cloud activity uploaders (SPEC.md §10.2).
 */

#include <cstdint>
#include <string>

namespace echenon {

/// Result of executing a single upload stage on an IUploader implementation.
enum class UploadStageResult : uint8_t {
    SUCCESS         = 0, ///< Stage completed; advance to next stage
    RETRYABLE_ERROR = 1, ///< Transient network/5xx/429 error; backoff and retry
    PERMANENT_ERROR = 2, ///< Unrecoverable error (401 revoked, invalid file, etc.)
    IN_PROGRESS     = 3  ///< Remote processing still in progress; poll again after delay
};

/// Convert UploadStageResult to a human-readable static string.
inline const char* uploadStageResultToString(UploadStageResult result) {
    switch (result) {
        case UploadStageResult::SUCCESS:         return "SUCCESS";
        case UploadStageResult::RETRYABLE_ERROR: return "RETRYABLE_ERROR";
        case UploadStageResult::PERMANENT_ERROR: return "PERMANENT_ERROR";
        case UploadStageResult::IN_PROGRESS:     return "IN_PROGRESS";
        default:                                 return "UNKNOWN";
    }
}

/**
 * @brief Capability interface for cloud activity uploaders (e.g. Strava v3 API).
 *
 * `UploadQueue` owns the scheduling, persistence, retry/backoff state machine,
 * `DeadlineToken` budget enforcement, and `BLEManager::ScanInhibit` guard,
 * invoking at most one stage method per `fetchUpdate()` cycle.
 */
class IUploader {
public:
    virtual ~IUploader() = default;

    /// Canonical lowercase identifier for this service (e.g. "strava").
    virtual const char* serviceId() const = 0;

    /// Human-readable display name (defaults to serviceId()).
    virtual const char* name() const { return serviceId(); }

    /// Returns true if API application credentials (e.g. client_id & client_secret) are configured.
    virtual bool isConfigured() const = 0;

    /// Returns true if user OAuth tokens (access_token & refresh_token) are present.
    virtual bool isAuthenticated() const = 0;

    /// Returns true if the current access token is expired (or near expiry) at `now_epoch_s`.
    virtual bool needsTokenRefresh(uint32_t now_epoch_s) const = 0;

    /// Alias for needsTokenRefresh() matching SPEC.md §10.2 naming.
    virtual bool needsRefresh(uint32_t now_epoch_s) const {
        return needsTokenRefresh(now_epoch_s);
    }

    /// Execute the OAuth token refresh stage within `timeout_ms`.
    virtual UploadStageResult refreshToken(uint32_t timeout_ms, std::string& err_msg) = 0;

    /// Upload the compressed `.tcx.gz` file for `session_id` within `timeout_ms`,
    /// populating `out_upload_id` on acceptance.
    virtual UploadStageResult postUpload(
        uint32_t session_id,
        const std::string& tcx_gz_path,
        uint32_t timeout_ms,
        std::string& out_upload_id,
        std::string& err_msg) = 0;

    /// Poll processing status for `upload_id` within `timeout_ms`,
    /// populating `out_activity_id` when complete.
    virtual UploadStageResult pollUploadStatus(
        const std::string& upload_id,
        uint32_t timeout_ms,
        std::string& out_activity_id,
        std::string& err_msg) = 0;

    /// Returns true if this sink is enabled for auto-upload (default: true).
    /// Disabled sinks are skipped during enqueueSession() fan-out.
    virtual bool isEnabled() const { return true; }

    /// Optional hook allowing UploadQueue::fetchUpdate() to supply the framework's
    /// pre-configured transport client (e.g. WiFiClientSecure* from FetchContext).
    virtual void setTransportContext(void* client_ctx) {
        (void)client_ctx;
    }
};

} // namespace echenon
