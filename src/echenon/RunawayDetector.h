#pragma once

#include <cstdint>

namespace echenon {

/**
 * @brief Detects stock-runaway conditions when the display-path wiper stays
 *        outside the working range for longer than a configurable timeout.
 *
 * Behaviour:
 * - When the filtered wiper is outside [RANGE_MIN, RANGE_MAX]:
 *   - If not already tracking: record the start time, begin tracking
 *   - If already tracking and timeout exceeded: trigger runaway
 * - When the wiper returns to range: clear the timer and runaway state
 *   (_wasTriggered remains latched until explicit reset())
 * - tick() returns true only on the first detection tick (edge detection)
 */
class RunawayDetector {
public:
    /// Default timeout: 10 seconds (owner decision 2026-10-20)
    static constexpr uint16_t DEFAULT_TIMEOUT_S = 10;

    /// Working range constants (from LevelMapping §5.2)
    static constexpr uint16_t RANGE_MIN = 165;
    static constexpr uint16_t RANGE_MAX = 1327;

    RunawayDetector() = default;

    // --- Configuration ---
    void setTimeoutSeconds(uint16_t seconds);
    uint16_t getTimeoutSeconds() const;

    // --- Tick ---
    /// Called every tick with the current filtered wiper count and millis.
    /// Returns true if a runaway was just detected (transition edge).
    bool tick(uint32_t currentMillis, uint16_t filteredWiper);

    // --- State Queries ---
    bool isRunaway() const;              ///< Currently in runaway state
    bool wasTriggered() const;           ///< Latched: has been triggered since last reset()
    uint32_t getRunawayDurationMs() const; ///< How long in out-of-range state (0 if in range)

    // --- Reset ---
    /// Clears all state including the latched wasTriggered flag.
    void reset();

private:
    uint16_t _timeoutS = DEFAULT_TIMEOUT_S;
    uint32_t _outOfRangeStartMs = 0;
    uint32_t _lastTickMs = 0;
    bool _outOfRange = false;
    bool _runawayDetected = false;
    bool _wasTriggered = false;
};

} // namespace echenon
