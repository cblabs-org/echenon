#pragma once

/// @file DiagnosticsRoutes.h
/// @brief Diagnostics and run-data query REST API route-registration helper.
///
/// Standalone class that registers `/api/echenon/events`, `/api/echenon/runs`,
/// `/api/echenon/runs/<id>.csv.gz`, and `DELETE /api/echenon/runs/<id>` on a
/// PsychicHttpServer. Dependencies are injected by reference; this class does
/// not own DiagnosticsModule or RidePersistence.
///
/// Route registration methods are guarded with `#ifndef UNIT_TEST` since they
/// depend on PsychicHttpServer (ESP32 only). Helper functions like
/// `diagEventTypeToString()` and payload decoding are unconditionally compiled
/// for native testing.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "echenon/DiagnosticsModule.h"

namespace echenon {

// Forward declarations
class RidePersistence;

// =============================================================================
// Event Type String Conversion (unconditional — native-testable)
// =============================================================================

/// Returns canonical uppercase string for the given DiagEventType.
/// Returns "UNKNOWN" for unrecognised values.
const char* diagEventTypeToString(DiagEventType type);

// =============================================================================
// Payload Decoding Helpers (unconditional — native-testable)
// =============================================================================

/// Decode BOOT payload: 3-byte little-endian uptime_s (max 16,777,215).
inline uint32_t decodeBootPayload(const uint8_t payload[3]) {
    return static_cast<uint32_t>(payload[0])
         | (static_cast<uint32_t>(payload[1]) << 8)
         | (static_cast<uint32_t>(payload[2]) << 16);
}

/// Decode SESSION_START payload: low 16-bit LE session_id (payload[2] unused).
inline uint32_t decodeSessionStartPayload(const uint8_t payload[3]) {
    return static_cast<uint32_t>(payload[0])
         | (static_cast<uint32_t>(payload[1]) << 8);
}

/// Decode SESSION_END payload: duration_s (uint16 LE in [0..1]),
///   record_count_approx = payload[2] * 64 (coarse granularity).
inline void decodeSessionEndPayload(const uint8_t payload[3],
                                    uint16_t& duration_s,
                                    uint16_t& record_count_approx) {
    duration_s = static_cast<uint16_t>(payload[0])
               | (static_cast<uint16_t>(payload[1]) << 8);
    record_count_approx = static_cast<uint16_t>(payload[2]) * 64u;
}

/// Decode RUNAWAY_ALERT payload: (duration_ms / 100) stored as 16-bit LE in [0..1].
/// Returns the reconstructed duration in milliseconds (resolution: 100 ms).
inline uint32_t decodeRunawayPayload(const uint8_t payload[3]) {
    uint32_t scaled = static_cast<uint32_t>(payload[0])
                    | (static_cast<uint32_t>(payload[1]) << 8);
    return scaled * 100u;
}

/// Decode CALIBRATION_DONE payload: calibration_type in payload[0].
///   0 = "initial", 1 = "recalibration".
inline uint8_t decodeCalibrationPayload(const uint8_t payload[3]) {
    return payload[0];
}

/// Decode FIRMWARE_CHANGE payload: major.minor.patch.
inline void decodeFirmwarePayload(const uint8_t payload[3],
                                  uint8_t& major,
                                  uint8_t& minor,
                                  uint8_t& patch) {
    major = payload[0];
    minor = payload[1];
    patch = payload[2];
}

/// Format the type-specific payload JSON fragment into `buf`.
/// Returns number of characters written (excluding null terminator), or 0 on error.
int formatPayloadJson(DiagEventType type, const uint8_t payload[3],
                      char* buf, size_t buf_size);

} // namespace echenon

// =============================================================================
// Forward declarations in global namespace (PsychicHttp, AuthManager)
// =============================================================================

#ifndef UNIT_TEST
class PsychicHttpServer;
class AuthManager;
#endif

// =============================================================================
// DiagnosticsRoutes Class
// =============================================================================

namespace echenon {

class DiagnosticsRoutes {
public:
    DiagnosticsRoutes(DiagnosticsModule& diag, RidePersistence& persistence);

    /// Register all diagnostic routes on the given server.
    /// authManager is used for authentication checks.
#ifndef UNIT_TEST
    void registerRoutes(PsychicHttpServer& server, AuthManager& auth);
#endif

private:
    DiagnosticsModule& _diag;
    RidePersistence&   _persistence;
};

} // namespace echenon
