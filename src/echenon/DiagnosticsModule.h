#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/DiagnosticsModule.h
 * Description: Production diagnostics module with a PSRAM-backed bounded 8-byte event ring buffer.
 */

#include <cstddef>
#include <cstdint>
#include <climits>
#include "interfaces/ITickable.hpp"

namespace echenon {

// =============================================================================
// DiagnosticEvent Type Discriminator
// =============================================================================

/// Discriminator for compact 8-byte diagnostic events.
enum class DiagEventType : uint8_t {
    BOOT             = 0, ///< Device boot event (payload: 24-bit LE uptime seconds)
    SESSION_START    = 1, ///< Ride session started (payload: 16-bit LE session ID)
    SESSION_END      = 2, ///< Ride session ended (payload: duration_s + coarse record count)
    RUNAWAY_ALERT    = 3, ///< Stock runaway detected (payload: duration in 100 ms units)
    CALIBRATION_DONE = 4, ///< Calibration completed (payload: 0=initial, 1=recalibration)
    FIRMWARE_CHANGE  = 5  ///< Firmware version change observed (payload: major, minor, patch)
};

// =============================================================================
// DiagnosticEvent — 8-byte packed event record
// =============================================================================

/// Compact 8-byte packed diagnostic event record stored in the ring buffer.
struct __attribute__((packed)) DiagnosticEvent {
    uint32_t      epoch_s;      ///< UTC epoch seconds
    DiagEventType type;         ///< Event type discriminator
    uint8_t       payload[3];   ///< Type-specific payload (see encoding spec)
};
static_assert(sizeof(DiagnosticEvent) == 8, "DiagnosticEvent must be 8 bytes");

// =============================================================================
// EventQuery — time-windowed + limit query descriptor
// =============================================================================

/// Time-windowed and count-limited query filter for DiagnosticsModule::queryEvents().
struct EventQuery {
    uint32_t from_epoch = 0;            ///< Inclusive lower bound (0 = no filter)
    uint32_t to_epoch   = UINT32_MAX;   ///< Inclusive upper bound
    size_t   limit      = 100;          ///< Maximum number of results to return
};

// =============================================================================
// DiagnosticsModule — PSRAM-backed bounded event log with ring buffer semantics
// =============================================================================

/**
 * @brief PSRAM-backed bounded diagnostic event log using ring-buffer semantics.
 *
 * Records structured 8-byte events (boot, session, runaway, calibration,
 * firmware change) and supports time-windowed queries.
 */
class DiagnosticsModule : public ITickable {
public:
    static constexpr size_t EVENT_LOG_CAPACITY = 100; ///< Maximum retained events in the ring buffer

    DiagnosticsModule() = default;
    /// Free allocated ring buffer storage.
    ~DiagnosticsModule();

    // Non-copyable
    DiagnosticsModule(const DiagnosticsModule&) = delete;
    DiagnosticsModule& operator=(const DiagnosticsModule&) = delete;

    // --- Lifecycle ---
    /// Allocate ring buffer (preferring PSRAM with heap fallback).
    bool begin();
    /// Free ring buffer and reset ring indices.
    void end();

    // --- Core Event Recording ---
    /// Append a raw diagnostic event with 3-byte payload to the ring buffer.
    void recordEvent(DiagEventType type, uint32_t epoch_s,
                     uint8_t p0 = 0, uint8_t p1 = 0, uint8_t p2 = 0);

    // --- Convenience Event Recorders ---
    /// Record a device boot event with uptime in seconds.
    void recordBoot(uint32_t epoch_s, uint32_t uptime_s);
    /// Record the start of a ride session.
    void recordSessionStart(uint32_t epoch_s, uint32_t session_id);
    /// Record the end of a ride session with duration and record count.
    void recordSessionEnd(uint32_t epoch_s, uint16_t duration_s, uint16_t record_count);
    /// Record a stock runaway alert with out-of-range duration in milliseconds.
    void recordRunawayAlert(uint32_t epoch_s, uint32_t duration_ms);
    /// Record completion of initial (0) or recalibration (1) workflow.
    void recordCalibrationDone(uint32_t epoch_s, uint8_t calibration_type);
    /// Record a firmware version change event.
    void recordFirmwareChange(uint32_t epoch_s, uint8_t major, uint8_t minor, uint8_t patch);

    // --- Accessors ---
    /// Return the number of events currently stored (up to EVENT_LOG_CAPACITY).
    size_t getEventCount() const;
    /// Return pointer to the event at logical index (0 = oldest), or nullptr if out of range.
    const DiagnosticEvent* getEvent(size_t index) const;
    /// Clear all stored events without freeing the buffer.
    void clearEvents();

    // --- Query Support ---
    /// Copy matching events in chronological order into out; return count written.
    size_t queryEvents(const EventQuery& query,
                       DiagnosticEvent* out, size_t out_capacity) const;

    // --- ITickable (stub for now) ---
    /// Periodic tick hook (no-op for ring-buffer logger).
    void tick(uint32_t currentMillis) override;

private:
    DiagnosticEvent* _buffer = nullptr;
    size_t _head  = 0;   // Next write position
    size_t _count = 0;   // Number of events stored
};

} // namespace echenon
