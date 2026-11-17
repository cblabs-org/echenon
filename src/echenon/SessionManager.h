#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/SessionManager.h
 * Description: Ride session lifecycle state machine (IDLE -> ACTIVE <-> PAUSED -> ENDED -> IDLE), 2 Hz -> 1 Hz sampling, and IConfigurable settings.
 */

#include "echenon/RideStore.h"
#include "interfaces/IConfigurable.hpp"
#include "interfaces/ITickable.hpp"

#include <cstdint>
#include <functional>

class EventBus;

namespace echenon {

/// EventBus topic for SessionStateChangeEvent transitions (SPEC §9.1).
static constexpr const char* EVENT_SESSION_STATE = "echenon/session/state";
/// Alternate EventBus topic for SessionStateChangeEvent notifications (SPEC §9.1).
static constexpr const char* EVENT_SESSION_TOPIC = "echenon.session";

/// Ride session lifecycle state (SPEC §9.1).
enum class SessionState : uint8_t {
    IDLE   = 0, ///< Waiting for manual start or auto-start cadence threshold
    ACTIVE = 1, ///< Recording 2 Hz -> 1 Hz telemetry into RideStore
    PAUSED = 2, ///< Recording suspended while tracking pause duration
    ENDED  = 3  ///< Session finalized and ready for reset to IDLE
};

/// Convert SessionState to canonical uppercase string literal.
inline const char* sessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::IDLE:   return "IDLE";
        case SessionState::ACTIVE: return "ACTIVE";
        case SessionState::PAUSED: return "PAUSED";
        case SessionState::ENDED:  return "ENDED";
        default:                   return "IDLE";
    }
}

/// Payload published on EVENT_SESSION_STATE ("echenon/session/state").
struct SessionStateChangeEvent {
    SessionState oldState;    ///< Previous session state before transition
    SessionState newState;    ///< New session state after transition
    uint32_t     sessionId;   ///< Active or completed session identifier
    uint32_t     timestampMs; ///< Monotonic timestamp of transition (ms)
};

/**
 * @brief Manages ride session lifecycle, 2 Hz -> 1 Hz telemetry averaging into
 *        RideStore, pause/resume gap tracking, and NVS/LittleFS configuration.
 */
class SessionManager : public ITickable, public IConfigurable {
public:
    static constexpr uint16_t DEFAULT_AUTO_START_DELAY_SEC   = 5;    ///< Default auto-start pedaling delay (s)
    static constexpr uint16_t MIN_AUTO_START_DELAY_SEC       = 1;    ///< Minimum auto-start delay (s)
    static constexpr uint16_t MAX_AUTO_START_DELAY_SEC       = 60;   ///< Maximum auto-start delay (s)

    static constexpr uint16_t DEFAULT_INACTIVITY_TIMEOUT_SEC = 180;  ///< Default zero-cadence inactivity timeout (s)
    static constexpr uint16_t MIN_INACTIVITY_TIMEOUT_SEC     = 30;   ///< Minimum inactivity timeout (s)
    static constexpr uint16_t MAX_INACTIVITY_TIMEOUT_SEC     = 3600; ///< Maximum inactivity timeout (s)

    static constexpr uint32_t SAMPLE_INTERVAL_MS             = 500;  ///< 2 Hz fundamental sampling period (ms)
    static constexpr uint32_t HR_STALE_TIMEOUT_MS            = 5000; ///< 5 s HR staleness timeout (ms)

    /// Construct SessionManager with RideStore and optional EventBus pointer.
    explicit SessionManager(RideStore& rideStore, EventBus* eventBus = nullptr);
    /// Construct SessionManager with RideStore and EventBus reference.
    SessionManager(RideStore& rideStore, EventBus& eventBus);
    /// Unsubscribe from EventBus HR events on destruction.
    ~SessionManager() override;

    // --- EventBus wiring & lifecycle ---
    /// Attach EventBus pointer for HR subscription and state change publications.
    void setEventBus(EventBus* eventBus) { _eventBus = eventBus; }
    /// Subscribe to EventBus HR_TOPIC notifications.
    void begin();
    /// Unsubscribe from EventBus HR_TOPIC notifications.
    void end();

    // --- ITickable ---
    /// Advance auto-start, inactivity timeout, SNTP wall-clock sync, and 2 Hz sampling.
    void tick(uint32_t currentMillis) override;
    /// Return module name identifier.
    const char* getName() const { return "SessionManager"; }

    // --- IConfigurable ("echenon_session", schema v1) ---
    /// Return configuration namespace ("echenon_session").
    const char* getConfigNamespace() const override { return "echenon_session"; }
    /// Return configuration schema version (1).
    int getSchemaVersion() const override { return 1; }
    /// Apply persisted JSON configuration for auto-start and inactivity timeout.
    void applyConfig(const JsonObject& config) override;
    /// Serialize session configuration into JSON object.
    void serializeConfig(JsonObject& config) const override;
    /// Reset configuration fields to defaults.
    void resetConfig() override;

    // --- Telemetry input (updated from main loop / sensors prior to tick) ---
    /// Update latest power, cadence, HR, and canonical resistance inputs.
    void updateTelemetry(float power_watts,
                         float cadence_rpm,
                         uint8_t heart_rate_bpm,
                         bool hr_connected,
                         uint8_t internal_resistance,
                         bool resistance_calibrated = true);

    /// Update latest power, cadence, and resistance inputs (preserving EventBus HR).
    void updateTelemetry(float power_watts,
                         float cadence_rpm,
                         uint8_t internal_resistance,
                         bool resistance_calibrated = true);

    // --- Manual Session Controls ---
    /// Callback invoked when a ride session ends and is finalized.
    // GRAVE-05: Bound once at boot (stateless/SBO-small lambda capturing <=16B) and invoked only on session end, never in tick() hot path; supports native test spies.
    using SessionCompletedCallback = std::function<void(const RideSummary&)>;
    /// Register callback invoked on session completion.
    void setSessionCompletedCallback(SessionCompletedCallback cb) { _onSessionCompleted = std::move(cb); }

    /// Transition IDLE/ENDED -> ACTIVE and start a new RideStore session.
    bool startSession(uint32_t now_epoch_s = 0, uint32_t now_ms = 0);
    /// Transition ACTIVE -> PAUSED and record pause start timestamp.
    bool pauseSession(uint32_t now_ms = 0);
    /// Transition PAUSED -> ACTIVE and advance timestamp offset by pause gap.
    bool resumeSession(uint32_t now_ms = 0);
    /// Trim trailing zeros, finalize RideStore session, transition -> ENDED, and fire callback.
    bool endSession(uint32_t now_ms = 0);
    /// Discard active session and transition -> IDLE without saving.
    bool discardSession(uint32_t now_ms = 0);
    /// Alias for discardSession().
    bool discardActiveSession(uint32_t now_ms = 0) { return discardSession(now_ms); }
    /// Transition ENDED -> IDLE after session summary acknowledgement.
    bool resetToIdle(uint32_t now_ms = 0);

    // --- Wall-Clock / Deferred SNTP Synchronization (SPEC §9.3) ---
    /// Back-fill session start_epoch_s using latest tick timestamp once SNTP locks.
    void updateWallClock(uint32_t now_epoch_s);
    /// Back-fill session start_epoch_s using explicit monotonic timestamp now_ms.
    void updateWallClock(uint32_t now_epoch_s, uint32_t now_ms);

    // --- Configuration Accessors ---
    /// Return true if cadence-triggered auto-start is enabled.
    bool isAutoStartEnabled() const { return _autoStartEnabled; }
    /// Enable or disable cadence-triggered auto-start.
    void setAutoStartEnabled(bool enabled) { _autoStartEnabled = enabled; }

    /// Return auto-start pedaling delay in seconds.
    uint16_t getAutoStartDelaySec() const { return _autoStartDelaySec; }
    /// Set auto-start pedaling delay in seconds (clamped to [MIN, MAX]).
    void setAutoStartDelaySec(uint16_t delaySec);
    /// Return auto-start pedaling delay in milliseconds.
    uint32_t getAutoStartDelayMs() const { return static_cast<uint32_t>(_autoStartDelaySec) * 1000u; }

    /// Return zero-cadence inactivity timeout in seconds.
    uint16_t getInactivityTimeoutSec() const { return _inactivityTimeoutSec; }
    /// Set zero-cadence inactivity timeout in seconds (clamped to [MIN, MAX]).
    void setInactivityTimeoutSec(uint16_t timeoutSec);
    /// Return zero-cadence inactivity timeout in milliseconds.
    uint32_t getInactivityTimeoutMs() const { return static_cast<uint32_t>(_inactivityTimeoutSec) * 1000u; }

    // --- State & Live Metrics Accessors ---
    /// Return current SessionState.
    SessionState getState() const { return _state; }
    /// Return current session_id (0 when IDLE).
    uint32_t getSessionId() const { return _sessionId; }
    /// Return active session start Unix epoch (seconds).
    uint32_t getStartEpochS() const { return _rideStore.getStartEpochS(); }
    /// Return active recorded sample duration in seconds.
    uint32_t getElapsedSec() const { return static_cast<uint32_t>(_rideStore.getRecordCount()); }
    /// Return next 1 Hz record timestamp offset in seconds.
    uint16_t getNextTimestampOffsetS() const { return _nextTimestampOffsetS; }

    /// Return session-average power in watts.
    uint16_t getAvgPowerWatts() const;
    /// Return session peak 1 Hz power in watts.
    uint16_t getMaxPowerWatts() const { return _maxPowerW; }
    /// Return cumulative mechanical work in kilojoules.
    uint16_t getTotalWorkKj() const { return static_cast<uint16_t>((_powerSumW + 500u) / 1000u); }
    /// Return active heart rate in BPM (0 if disconnected).
    uint8_t  getHeartRateBpm() const { return (_hrConnected && _heartRateBpm > 0) ? _heartRateBpm : 0; }

    /// Return RideSummary from the most recently ended session.
    const RideSummary& getLastSummary() const { return _lastSummary; }
    /// Return mutable reference to backing RideStore.
    RideStore& getRideStore() { return _rideStore; }
    /// Return const reference to backing RideStore.
    const RideStore& getRideStore() const { return _rideStore; }

private:
    void publishStateChange(SessionState oldState, SessionState newState, uint32_t now_ms);
    void processSample500ms(uint32_t sampleMs);
    static void onHrEvent(const char* topic, const void* payload, void* ctx);

    RideStore&               _rideStore;
    EventBus*                _eventBus = nullptr;
    bool                     _subscribedHr = false;
    // GRAVE-05 deviation: DI callback set once at boot, never reallocated.
    // Framework's own DataManager uses the same pattern for IDataSource injection.
    SessionCompletedCallback _onSessionCompleted;

    // Configuration ("echenon_session")
    bool     _autoStartEnabled     = true;
    uint16_t _autoStartDelaySec    = DEFAULT_AUTO_START_DELAY_SEC;
    uint16_t _inactivityTimeoutSec = DEFAULT_INACTIVITY_TIMEOUT_SEC;

    // Session state
    SessionState _state            = SessionState::IDLE;
    uint32_t     _sessionId        = 0;
    uint32_t     _currentEpochS    = 0;
    uint32_t     _sessionStartMs   = 0;
    uint32_t     _lastTickMs       = 0;
    bool         _hasTickMs        = false;

    // Auto-start & auto-pause timers
    bool     _cadenceNonZeroTracking = false;
    uint32_t _cadenceNonZeroStartMs  = 0;
    bool     _cadenceZeroTracking    = false;
    uint32_t _cadenceZeroStartMs     = 0;
    uint32_t _pauseStartMs           = 0;
    uint32_t _activeIntervalStartMs  = 0;
    uint32_t _accumulatedActiveMs    = 0;

    // 2 Hz -> 1 Hz telemetry averaging state
    uint32_t _lastSampleMs          = 0;
    bool     _halfSamplePending     = false;
    uint16_t _sample0PowerW         = 0;
    uint8_t  _sample0CadenceRpm     = 0;
    uint16_t _nextTimestampOffsetS  = 0;
    bool     _resumeBoundaryPending = false;

    // Latest telemetry inputs
    float    _powerWatts           = 0.0f;
    float    _cadenceRpm           = 0.0f;
    uint8_t  _heartRateBpm         = 0;
    bool     _hrConnected          = false;
    uint32_t _lastHrEventMs        = 0;
    bool     _hrFromEventBus       = false;
    uint8_t  _internalResistance   = 0;
    bool     _resistanceCalibrated = true;

    // Live session metrics & finalized summary
    uint64_t    _powerSumW         = 0;
    uint16_t    _maxPowerW         = 0;
    RideSummary _lastSummary{};

    // Persistent EventBus payload — avoids stack-local lifetime issues (D3 safety)
    SessionStateChangeEvent _lastEvent{};
};

} // namespace echenon
