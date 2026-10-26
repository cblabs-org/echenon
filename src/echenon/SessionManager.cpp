#include "echenon/SessionManager.h"

#ifdef UNIT_TEST
#include "modules/EventBus/EventBus.hpp"
#else
#include "EventBus/EventBus.hpp"
#endif

#include "echenon/HREvents.h"

#include <algorithm>
#include <cmath>

#if defined(ESP_PLATFORM)
#include <ctime>
#include <esp_log.h>
namespace {
constexpr const char* TAG = "SessionManager";
}
#endif

namespace echenon {

SessionManager::SessionManager(RideStore& rideStore, EventBus* eventBus)
    : _rideStore(rideStore), _eventBus(eventBus) {}

SessionManager::SessionManager(RideStore& rideStore, EventBus& eventBus)
    : _rideStore(rideStore), _eventBus(&eventBus) {}

SessionManager::~SessionManager() {
    end();
}

void SessionManager::begin() {
    if (_eventBus && !_subscribedHr) {
        _eventBus->subscribe(echenon::HR_TOPIC, SessionManager::onHrEvent, this);
        _subscribedHr = true;
    }
}

void SessionManager::end() {
    if (_eventBus && _subscribedHr) {
        _eventBus->unsubscribe(echenon::HR_TOPIC, SessionManager::onHrEvent, this);
        _subscribedHr = false;
    }
}

void SessionManager::onHrEvent(const char* topic, const void* payload, void* ctx) {
    (void)topic;
    if (!payload || !ctx) return;

    auto* self = static_cast<SessionManager*>(ctx);
    const auto* sample = static_cast<const echenon::HrSample*>(payload);

    self->_hrFromEventBus = true;
    if (sample->contactState == echenon::HRContactState::ACTIVE && sample->bpm > 0) {
        self->_heartRateBpm  = sample->bpm;
        self->_hrConnected   = true;
        self->_lastHrEventMs = sample->receivedAtMs;
    } else {
        self->_heartRateBpm = 0;
        self->_hrConnected  = false;
    }
}

// =============================================================================
// IConfigurable ("echenon_session", schema v1)
// =============================================================================

void SessionManager::setAutoStartDelaySec(uint16_t delaySec) {
    _autoStartDelaySec = std::clamp(delaySec, MIN_AUTO_START_DELAY_SEC, MAX_AUTO_START_DELAY_SEC);
}

void SessionManager::setInactivityTimeoutSec(uint16_t timeoutSec) {
    _inactivityTimeoutSec = std::clamp(timeoutSec, MIN_INACTIVITY_TIMEOUT_SEC, MAX_INACTIVITY_TIMEOUT_SEC);
}

void SessionManager::applyConfig(const JsonObject& config) {
    if (config["session_auto_start"].is<bool>()) {
        _autoStartEnabled = config["session_auto_start"].as<bool>();
    } else if (config["auto_start"].is<bool>()) {
        _autoStartEnabled = config["auto_start"].as<bool>();
    }

    if (config["session_auto_start_s"].is<int>()) {
        int val = config["session_auto_start_s"].as<int>();
        setAutoStartDelaySec(static_cast<uint16_t>(std::max(1, val)));
    } else if (config["auto_start_delay_s"].is<int>()) {
        int val = config["auto_start_delay_s"].as<int>();
        setAutoStartDelaySec(static_cast<uint16_t>(std::max(1, val)));
    }

    if (config["session_inactivity_s"].is<int>()) {
        int val = config["session_inactivity_s"].as<int>();
        setInactivityTimeoutSec(static_cast<uint16_t>(std::max(30, val)));
    } else if (config["inactivity_timeout_s"].is<int>()) {
        int val = config["inactivity_timeout_s"].as<int>();
        setInactivityTimeoutSec(static_cast<uint16_t>(std::max(30, val)));
    }
}

void SessionManager::serializeConfig(JsonObject& config) const {
    config["session_auto_start"]   = _autoStartEnabled;
    config["session_auto_start_s"] = _autoStartDelaySec;
    config["session_inactivity_s"] = _inactivityTimeoutSec;
}

void SessionManager::resetConfig() {
    _autoStartEnabled     = true;
    _autoStartDelaySec    = DEFAULT_AUTO_START_DELAY_SEC;
    _inactivityTimeoutSec = DEFAULT_INACTIVITY_TIMEOUT_SEC;
}

// =============================================================================
// Telemetry & Wall-Clock Updates
// =============================================================================

void SessionManager::updateTelemetry(float power_watts,
                                     float cadence_rpm,
                                     uint8_t heart_rate_bpm,
                                     bool hr_connected,
                                     uint8_t internal_resistance,
                                     bool resistance_calibrated) {
    _powerWatts           = std::isfinite(power_watts) ? std::max(0.0f, power_watts) : 0.0f;
    _cadenceRpm           = std::isfinite(cadence_rpm) ? std::max(0.0f, cadence_rpm) : 0.0f;
    _heartRateBpm         = heart_rate_bpm;
    _hrConnected          = hr_connected && (heart_rate_bpm > 0);
    _hrFromEventBus       = false;
    _internalResistance   = internal_resistance;
    _resistanceCalibrated = resistance_calibrated &&
                            (internal_resistance != LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED);
}

void SessionManager::updateTelemetry(float power_watts,
                                     float cadence_rpm,
                                     uint8_t internal_resistance,
                                     bool resistance_calibrated) {
    _powerWatts           = std::isfinite(power_watts) ? std::max(0.0f, power_watts) : 0.0f;
    _cadenceRpm           = std::isfinite(cadence_rpm) ? std::max(0.0f, cadence_rpm) : 0.0f;
    _internalResistance   = internal_resistance;
    _resistanceCalibrated = resistance_calibrated &&
                            (internal_resistance != LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED);
}

void SessionManager::updateWallClock(uint32_t now_epoch_s) {
    uint32_t now_ms = _hasTickMs ? _lastTickMs : _sessionStartMs;
    updateWallClock(now_epoch_s, now_ms);
}

void SessionManager::updateWallClock(uint32_t now_epoch_s, uint32_t now_ms) {
    _currentEpochS = now_epoch_s;
    if (_state != SessionState::ACTIVE && _state != SessionState::PAUSED) {
        return;
    }

    uint32_t elapsedSec = 0;
    int32_t deltaMs = static_cast<int32_t>(now_ms - _sessionStartMs);
    if (deltaMs > 0) {
        elapsedSec = static_cast<uint32_t>(deltaMs) / 1000u;
    }
    if (elapsedSec == 0 && _nextTimestampOffsetS > 0) {
        elapsedSec = _nextTimestampOffsetS;
    }

    _rideStore.updateStartEpoch(now_epoch_s, elapsedSec);
}

// =============================================================================
// Lifecycle State Machine
// =============================================================================

void SessionManager::publishStateChange(SessionState oldState, SessionState newState, uint32_t now_ms) {
    if (_eventBus) {
        SessionStateChangeEvent ev{oldState, newState, _sessionId, now_ms};
        _eventBus->publish(EVENT_SESSION_STATE, &ev);
        _eventBus->publish(EVENT_SESSION_TOPIC, &ev);
    }
}

bool SessionManager::startSession(uint32_t now_epoch_s, uint32_t now_ms) {
    if (_state != SessionState::IDLE && _state != SessionState::ENDED) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    uint32_t effectiveEpochS = (now_epoch_s != 0) ? now_epoch_s : _currentEpochS;
#if defined(ESP_PLATFORM) && !defined(UNIT_TEST)
    if (effectiveEpochS == 0) {
        std::time_t t = std::time(nullptr);
        if (t > 0) {
            effectiveEpochS = static_cast<uint32_t>(t);
        }
    }
#endif

    if (!_rideStore.beginSession(effectiveEpochS)) {
        return false;
    }

    SessionState oldState    = _state;
    _sessionId               = _rideStore.getActiveSessionId();
    _sessionStartMs          = effectiveMs;
    _activeIntervalStartMs   = effectiveMs;
    _accumulatedActiveMs     = 0;
    _lastSampleMs            = effectiveMs;
    _halfSamplePending       = false;
    _sample0PowerW           = 0;
    _sample0CadenceRpm       = 0;
    _nextTimestampOffsetS    = 0;
    _resumeBoundaryPending   = false;
    _cadenceNonZeroTracking  = false;
    _cadenceZeroTracking     = false;
    _powerSumW               = 0;
    _maxPowerW               = 0;
    _state                   = SessionState::ACTIVE;

#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "Session %u started (epoch=%u)", static_cast<unsigned>(_sessionId),
             static_cast<unsigned>(effectiveEpochS));
#endif

    publishStateChange(oldState, SessionState::ACTIVE, effectiveMs);
    return true;
}

bool SessionManager::pauseSession(uint32_t now_ms) {
    if (_state != SessionState::ACTIVE) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    int32_t activeDelta = static_cast<int32_t>(effectiveMs - _activeIntervalStartMs);
    if (activeDelta > 0) {
        _accumulatedActiveMs += static_cast<uint32_t>(activeDelta);
    }

    _pauseStartMs        = effectiveMs;
    _halfSamplePending   = false;
    _cadenceZeroTracking = false;
    _state               = SessionState::PAUSED;

#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "Session %u paused", static_cast<unsigned>(_sessionId));
#endif

    publishStateChange(SessionState::ACTIVE, SessionState::PAUSED, effectiveMs);
    return true;
}

bool SessionManager::resumeSession(uint32_t now_ms) {
    if (_state != SessionState::PAUSED) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    int32_t pauseDeltaMs = static_cast<int32_t>(effectiveMs - _pauseStartMs);
    if (pauseDeltaMs > 0) {
        uint32_t pauseSec = static_cast<uint32_t>(pauseDeltaMs) / 1000u;
        uint32_t nextOffset = static_cast<uint32_t>(_nextTimestampOffsetS) + pauseSec;
        _nextTimestampOffsetS = static_cast<uint16_t>(std::min<uint32_t>(nextOffset, 65535u));
    }

    _resumeBoundaryPending  = true;
    _activeIntervalStartMs  = effectiveMs;
    _lastSampleMs           = effectiveMs;
    _halfSamplePending      = false;
    _cadenceZeroTracking    = false;
    _state                  = SessionState::ACTIVE;

#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "Session %u resumed (next_offset=%u s)",
             static_cast<unsigned>(_sessionId),
             static_cast<unsigned>(_nextTimestampOffsetS));
#endif

    publishStateChange(SessionState::PAUSED, SessionState::ACTIVE, effectiveMs);
    return true;
}

bool SessionManager::endSession(uint32_t now_ms) {
    if (_state != SessionState::ACTIVE && _state != SessionState::PAUSED) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    SessionState oldState = _state;
    if (oldState == SessionState::ACTIVE) {
        int32_t activeDelta = static_cast<int32_t>(effectiveMs - _activeIntervalStartMs);
        if (activeDelta > 0) {
            _accumulatedActiveMs += static_cast<uint32_t>(activeDelta);
        }
    }

    _halfSamplePending      = false;
    _cadenceZeroTracking    = false;
    _cadenceNonZeroTracking = false;
    _resumeBoundaryPending  = false;

    _rideStore.trimTrailingZeroCadence();
    _rideStore.finalizeSession(_lastSummary);

    _state = SessionState::ENDED;

#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "Session %u ended (duration=%u s, avg=%u W, work=%u kJ)",
             static_cast<unsigned>(_sessionId),
             static_cast<unsigned>(_lastSummary.duration_s),
             static_cast<unsigned>(_lastSummary.avg_power_w),
             static_cast<unsigned>(_lastSummary.total_work_kj));
#endif

    publishStateChange(oldState, SessionState::ENDED, effectiveMs);
    if (_onSessionCompleted) {
        _onSessionCompleted(_lastSummary);
    }
    return true;
}

bool SessionManager::discardSession(uint32_t now_ms) {
    if (_state == SessionState::IDLE && !_rideStore.isSessionActive()) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    SessionState oldState = _state;
    uint32_t discardedId  = _sessionId;

    _rideStore.discardSession();

    _halfSamplePending      = false;
    _cadenceZeroTracking    = false;
    _cadenceNonZeroTracking = false;
    _resumeBoundaryPending  = false;
    _accumulatedActiveMs    = 0;
    _nextTimestampOffsetS   = 0;
    _powerSumW              = 0;
    _maxPowerW              = 0;
    _state                  = SessionState::IDLE;

    if (oldState != SessionState::IDLE) {
        _sessionId = discardedId;
        publishStateChange(oldState, SessionState::IDLE, effectiveMs);
    }
    _sessionId = 0;
    return true;
}

bool SessionManager::resetToIdle(uint32_t now_ms) {
    if (_state != SessionState::ENDED) {
        return false;
    }

    uint32_t effectiveMs = (now_ms != 0 || !_hasTickMs) ? now_ms : _lastTickMs;
    _lastTickMs = effectiveMs;
    _hasTickMs  = true;

    _cadenceNonZeroTracking = false;
    _state                  = SessionState::IDLE;
    publishStateChange(SessionState::ENDED, SessionState::IDLE, effectiveMs);
    _sessionId = 0;
    return true;
}

// =============================================================================
// ITickable & 2 Hz -> 1 Hz Sampling
// =============================================================================

void SessionManager::processSample500ms(uint32_t sampleMs) {
    uint8_t curCadence = (_cadenceRpm > 0.0f)
        ? static_cast<uint8_t>(std::lround(std::clamp(_cadenceRpm, 0.0f, 255.0f)))
        : 0u;
    uint16_t curPower = (curCadence > 0u && _powerWatts > 0.0f)
        ? static_cast<uint16_t>(std::lround(std::clamp(_powerWatts, 0.0f, 65535.0f)))
        : 0u;

    if (!_halfSamplePending) {
        _sample0PowerW      = curPower;
        _sample0CadenceRpm  = curCadence;
        _halfSamplePending  = true;
        return;
    }

    uint16_t avgPower = static_cast<uint16_t>(
        (static_cast<uint32_t>(_sample0PowerW) + static_cast<uint32_t>(curPower) + 1u) / 2u);
    uint8_t avgCadence = static_cast<uint8_t>(
        (static_cast<uint32_t>(_sample0CadenceRpm) + static_cast<uint32_t>(curCadence) + 1u) / 2u);
    _halfSamplePending = false;

    bool hrValid = _hrConnected && (_heartRateBpm > 0);
    if (_hrFromEventBus && hrValid) {
        if (static_cast<int32_t>(sampleMs - _lastHrEventMs) > static_cast<int32_t>(HR_STALE_TIMEOUT_MS)) {
            hrValid = false;
        }
    }

    uint8_t flags = 0;
    if (hrValid) {
        flags |= RIDE_FLAG_HR_VALID;
    }
    if (_resistanceCalibrated &&
        _internalResistance != LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED) {
        flags |= RIDE_FLAG_RESISTANCE_CALIBRATED;
    }
    if (_resumeBoundaryPending) {
        flags |= RIDE_FLAG_PAUSE_BOUNDARY;
        _resumeBoundaryPending = false;
    }

    RideRecord rec{};
    rec.timestamp_offset_s  = _nextTimestampOffsetS;
    rec.power_watts         = avgPower;
    rec.cadence_rpm         = avgCadence;
    rec.heart_rate_bpm      = hrValid ? _heartRateBpm : 0;
    rec.internal_resistance = _resistanceCalibrated
        ? _internalResistance
        : LevelMapping::INTERNAL_RESISTANCE_UNCALIBRATED;
    rec.flags               = flags;

    if (_rideStore.appendRecord(rec)) {
        if (_nextTimestampOffsetS < 65535u) {
            _nextTimestampOffsetS++;
        }
        _powerSumW += avgPower;
        if (avgPower > _maxPowerW) {
            _maxPowerW = avgPower;
        }
    }
}

void SessionManager::tick(uint32_t currentMillis) {
    _lastTickMs = currentMillis;
    _hasTickMs  = true;

#if defined(ESP_PLATFORM) && !defined(UNIT_TEST)
    if (_state == SessionState::ACTIVE || _state == SessionState::PAUSED) {
        if (_rideStore.getStartEpochS() < RideStore::VALID_EPOCH_THRESHOLD) {
            std::time_t nowEpoch = std::time(nullptr);
            if (nowEpoch >= static_cast<std::time_t>(RideStore::VALID_EPOCH_THRESHOLD)) {
                updateWallClock(static_cast<uint32_t>(nowEpoch), currentMillis);
            }
        }
    }
#endif

    switch (_state) {
        case SessionState::IDLE:
        case SessionState::ENDED: {
            if (_autoStartEnabled && _cadenceRpm > 0.0f) {
                if (!_cadenceNonZeroTracking) {
                    _cadenceNonZeroTracking = true;
                    _cadenceNonZeroStartMs  = currentMillis;
                } else if (static_cast<int32_t>(currentMillis - _cadenceNonZeroStartMs) >=
                           static_cast<int32_t>(getAutoStartDelayMs())) {
                    startSession(_currentEpochS, currentMillis);
                }
            } else {
                _cadenceNonZeroTracking = false;
            }
            break;
        }

        case SessionState::ACTIVE: {
            if (_cadenceRpm <= 0.0f) {
                if (!_cadenceZeroTracking) {
                    _cadenceZeroTracking = true;
                    _cadenceZeroStartMs  = currentMillis;
                } else if (static_cast<int32_t>(currentMillis - _cadenceZeroStartMs) >=
                           static_cast<int32_t>(getInactivityTimeoutMs())) {
                    endSession(currentMillis);
                    return;
                }
            } else {
                _cadenceZeroTracking = false;
            }

            uint8_t steps = 0;
            while (static_cast<int32_t>(currentMillis - _lastSampleMs) >=
                   static_cast<int32_t>(SAMPLE_INTERVAL_MS)) {
                _lastSampleMs += SAMPLE_INTERVAL_MS;
                processSample500ms(_lastSampleMs);
                if (++steps >= 8) {
                    _lastSampleMs = currentMillis;
                    break;
                }
            }
            break;
        }

        case SessionState::PAUSED: {
            if (static_cast<int32_t>(currentMillis - _pauseStartMs) >=
                static_cast<int32_t>(getInactivityTimeoutMs())) {
                endSession(currentMillis);
                return;
            }
            break;
        }

        default:
            break;
    }
}

uint16_t SessionManager::getAvgPowerWatts() const {
    size_t count = _rideStore.getRecordCount();
    if (count == 0) {
        return 0;
    }
    return static_cast<uint16_t>((_powerSumW + (count / 2u)) / count);
}

} // namespace echenon
