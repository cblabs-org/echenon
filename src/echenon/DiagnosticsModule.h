#pragma once

/// @file DiagnosticsModule.h
/// @brief Production diagnostics module with a PSRAM-backed bounded event log.
///        Records structured diagnostic events (boot, session, runaway, calibration,
///        firmware change) in a compact 8-byte ring buffer format.

#include <cstddef>
#include <cstdint>
#include <climits>

class ITickable;  // Forward declaration; actual interface in AppFoundation.h

namespace echenon {

// =============================================================================
// DiagnosticEvent Type Discriminator
// =============================================================================

enum class DiagEventType : uint8_t {
    BOOT             = 0,
    SESSION_START    = 1,
    SESSION_END      = 2,
    RUNAWAY_ALERT    = 3,
    CALIBRATION_DONE = 4,
    FIRMWARE_CHANGE  = 5
};

// =============================================================================
// DiagnosticEvent — 8-byte packed event record
// =============================================================================

struct __attribute__((packed)) DiagnosticEvent {
    uint32_t      epoch_s;      // UTC epoch seconds
    DiagEventType type;         // Event type discriminator
    uint8_t       payload[3];   // Type-specific payload (see encoding spec)
};
static_assert(sizeof(DiagnosticEvent) == 8, "DiagnosticEvent must be 8 bytes");

// =============================================================================
// EventQuery — time-windowed + limit query descriptor
// =============================================================================

struct EventQuery {
    uint32_t from_epoch = 0;            // inclusive lower bound (0 = no filter)
    uint32_t to_epoch   = UINT32_MAX;   // inclusive upper bound
    size_t   limit      = 100;          // max results
};

// =============================================================================
// DiagnosticsModule — PSRAM-backed bounded event log with ring buffer semantics
// =============================================================================

class DiagnosticsModule : public ITickable {
public:
    static constexpr size_t EVENT_LOG_CAPACITY = 100;

    DiagnosticsModule() = default;
    ~DiagnosticsModule();

    // Non-copyable
    DiagnosticsModule(const DiagnosticsModule&) = delete;
    DiagnosticsModule& operator=(const DiagnosticsModule&) = delete;

    // --- Lifecycle ---
    bool begin();     // Allocates ring buffer
    void end();       // Frees ring buffer

    // --- Core Event Recording ---
    void recordEvent(DiagEventType type, uint32_t epoch_s,
                     uint8_t p0 = 0, uint8_t p1 = 0, uint8_t p2 = 0);

    // --- Convenience Event Recorders ---
    void recordBoot(uint32_t epoch_s, uint32_t uptime_s);
    void recordSessionStart(uint32_t epoch_s, uint32_t session_id);
    void recordSessionEnd(uint32_t epoch_s, uint16_t duration_s, uint16_t record_count);
    void recordRunawayAlert(uint32_t epoch_s, uint32_t duration_ms);
    void recordCalibrationDone(uint32_t epoch_s, uint8_t calibration_type);
    void recordFirmwareChange(uint32_t epoch_s, uint8_t major, uint8_t minor, uint8_t patch);

    // --- Accessors ---
    size_t getEventCount() const;                        // Number of events stored (≤ capacity)
    const DiagnosticEvent* getEvent(size_t index) const; // 0 = oldest
    void clearEvents();

    // --- Query Support ---
    size_t queryEvents(const EventQuery& query,
                       DiagnosticEvent* out, size_t out_capacity) const;

    // --- ITickable (stub for now) ---
    void tick(uint32_t currentMillis) override;

private:
    DiagnosticEvent* _buffer = nullptr;
    size_t _head  = 0;   // Next write position
    size_t _count = 0;   // Number of events stored
};

} // namespace echenon
