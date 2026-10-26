#pragma once

/// @file SessionManager.h
/// @brief Ride session lifecycle state machine (IDLE -> ACTIVE <-> PAUSED -> ENDED -> IDLE),
///        2 Hz fundamental-first telemetry sampling -> 1 Hz RideRecord emission,
///        IConfigurable settings ("echenon_session"), and EventBus state notifications.

#include "echenon/RideStore.h"
#include "interfaces/IConfigurable.hpp"
#include "interfaces/ITickable.hpp"

#include <cstdint>
#include <functional>

class EventBus;

namespace echenon {

/// EventBus topics for session state transition notifications (SPEC §9.1).
static constexpr const char* EVENT_SESSION_STATE = "echenon/session/state";
static constexpr const char* EVENT_SESSION_TOPIC = "echenon.session";

/// Ride session lifecycle state (SPEC §9.1).
enum class SessionState : uint8_t {
    IDLE   = 0,
    ACTIVE = 1,
    PAUSED = 2,
    ENDED  = 3
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
    SessionState oldState;
    SessionState newState;
    uint32_t     sessionId;
    uint32_t     timestampMs;
};

/**
 * @brief Manages ride session lifecycle, 2 Hz -> 1 Hz telemetry averaging into
 *        RideStore, pause/resume gap tracking, and NVS/LittleFS configuration.
 */
class SessionManager : public ITickable, public IConfigurable {
public:
    static constexpr uint16_t DEFAULT_AUTO_START_DELAY_SEC   = 5;
    static constexpr uint16_t MIN_AUTO_START_DELAY_SEC       = 1;
    static constexpr uint16_t MAX_AUTO_START_DELAY_SEC       = 60;

    static constexpr uint16_t DEFAULT_INACTIVITY_TIMEOUT_SEC = 180;
    static constexpr uint16_t MIN_INACTIVITY_TIMEOUT_SEC     = 30;
    static constexpr uint16_t MAX_INACTIVITY_TIMEOUT_SEC     = 3600;

    static constexpr uint32_t AUTO_PAUSE_DELAY_MS            = 10000; // 10 s zero cadence -> PAUSED
    static constexpr uint32_t SAMPLE_INTERVAL_MS             = 500;   // 2 Hz fundamental sampling
    static constexpr uint32_t HR_STALE_TIMEOUT_MS            = 5000;  // 5 s HR staleness timeout

    explicit SessionManager(RideStore& rideStore, EventBus* eventBus = nullptr);
    SessionManager(RideStore& rideStore, EventBus& eventBus);
    ~SessionManager() override;

    // --- EventBus wiring & lifecycle ---
    void setEventBus(EventBus* eventBus) { _eventBus = eventBus; }
    void begin();
    void end();

    // --- ITickable ---
    void tick(uint32_t currentMillis) override;
    const char* getName() const { return "SessionManager"; }

    // --- IConfigurable ("echenon_session", schema v1) ---
    const char* getConfigNamespace() const override { return "echenon_session"; }
    int getSchemaVersion() const override { return 1; }
    void applyConfig(const JsonObject& config) override;
    void serializeConfig(JsonObject& config) const override;
    void resetConfig() override;

    // --- Telemetry input (updated from main loop / sensors prior to tick) ---
    void updateTelemetry(float power_watts,
                         float cadence_rpm,
                         uint8_t heart_rate_bpm,
                         bool hr_connected,
                         uint8_t internal_resistance,
                         bool resistance_calibrated = true);

    void updateTelemetry(float power_watts,
                         float cadence_rpm,
                         uint8_t internal_resistance,
                         bool resistance_calibrated = true);

    // --- Manual Session Controls ---
    using SessionCompletedCallback = std::function<void(const RideSummary&)>;
    void setSessionCompletedCallback(SessionCompletedCallback cb) { _onSessionCompleted = std::move(cb); }

    bool startSession(uint32_t now_epoch_s = 0, uint32_t now_ms = 0);
    bool pauseSession(uint32_t now_ms = 0);
    bool resumeSession(uint32_t now_ms = 0);
    bool endSession(uint32_t now_ms = 0);
    bool discardSession(uint32_t now_ms = 0);
    bool discardActiveSession(uint32_t now_ms = 0) { return discardSession(now_ms); }
    bool resetToIdle(uint32_t now_ms = 0);

    // --- Wall-Clock / Deferred SNTP Synchronization (SPEC §9.3) ---
    void updateWallClock(uint32_t now_epoch_s);
    void updateWallClock(uint32_t now_epoch_s, uint32_t now_ms);

    // --- Configuration Accessors ---
    bool isAutoStartEnabled() const { return _autoStartEnabled; }
    void setAutoStartEnabled(bool enabled) { _autoStartEnabled = enabled; }

    uint16_t getAutoStartDelaySec() const { return _autoStartDelaySec; }
    void setAutoStartDelaySec(uint16_t delaySec);
    uint32_t getAutoStartDelayMs() const { return static_cast<uint32_t>(_autoStartDelaySec) * 1000u; }

    uint16_t getInactivityTimeoutSec() const { return _inactivityTimeoutSec; }
    void setInactivityTimeoutSec(uint16_t timeoutSec);
    uint32_t getInactivityTimeoutMs() const { return static_cast<uint32_t>(_inactivityTimeoutSec) * 1000u; }

    // --- State & Live Metrics Accessors ---
    SessionState getState() const { return _state; }
    uint32_t getSessionId() const { return _sessionId; }
    uint32_t getStartEpochS() const { return _rideStore.getStartEpochS(); }
    uint32_t getElapsedSec() const { return static_cast<uint32_t>(_rideStore.getRecordCount()); }
    uint16_t getNextTimestampOffsetS() const { return _nextTimestampOffsetS; }

    uint16_t getAvgPowerWatts() const;
    uint16_t getMaxPowerWatts() const { return _maxPowerW; }
    uint16_t getTotalWorkKj() const { return static_cast<uint16_t>((_powerSumW + 500u) / 1000u); }

    const RideSummary& getLastSummary() const { return _lastSummary; }
    RideStore& getRideStore() { return _rideStore; }
    const RideStore& getRideStore() const { return _rideStore; }

private:
    void publishStateChange(SessionState oldState, SessionState newState, uint32_t now_ms);
    void processSample500ms(uint32_t sampleMs);
    static void onHrEvent(const char* topic, const void* payload, void* ctx);

    RideStore&               _rideStore;
    EventBus*                _eventBus = nullptr;
    bool                     _subscribedHr = false;
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
};

} // namespace echenon
