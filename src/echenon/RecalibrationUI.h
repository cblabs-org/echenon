#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/RecalibrationUI.h
 * Description: Rider-reported power recalibration store for Physics P3r scale fitting and power table overrides.
 */

#include <cstdint>
#include <cstddef>
#include <vector>
#include "echenon/PowerTable.h"
#include "echenon/PowerEngine.h"

namespace echenon {

class ResistanceSensor;
class CadenceSensor;

/**
 * @brief Individual power-point correction storage struct (SPEC §6.3 Tier 3).
 */
struct RecalPoint {
    uint8_t echelonLevel;     ///< Rider-entered console resistance level (1..32)
    float reportedPower;       ///< Rider-entered reference power (watts)
    uint8_t internalLevel;     ///< Live internal resistance i from LevelMapping (0..255)
    float cadenceRPM;          ///< Live crank cadence at capture time (RPM)
    uint16_t wiperCount;       ///< Live filtered potentiometer wiper reading (ADC counts)
    uint32_t timestamp;        ///< Capture timestamp in milliseconds (millis())
};

/**
 * @brief Manages individual power-point corrections (SPEC §6.3 Tier 3).
 *
 * Stores up to 100 recalibration points. When a new point is added at the same
 * (level, RPM bin), the new point replaces the existing point (simple replacement rule).
 */
class RecalibrationUI {
public:
    static constexpr size_t MAX_POINTS = 100;             ///< Maximum stored recalibration points
    static constexpr uint32_t AVERAGING_WINDOW_MS = 5000; ///< Recommended sensor averaging window (ms)

    /// Construct RecalibrationUI optionally bound to live ResistanceSensor and CadenceSensor instances.
    explicit RecalibrationUI(ResistanceSensor* rs = nullptr, CadenceSensor* cs = nullptr);
    ~RecalibrationUI() = default;

    /// Attach or replace the ResistanceSensor and CadenceSensor pointers.
    void setSensors(ResistanceSensor* rs, CadenceSensor* cs) {
        _resistanceSensor = rs;
        _cadenceSensor = cs;
    }

    /// Add a recalibration point using live sensor readings at echelonLevel (1-32).
    bool addPoint(uint8_t echelonLevel, float reportedPower);

    /// Add a recalibration point with explicit sensor values (for tests or pre-calculated data).
    bool addPoint(uint8_t echelonLevel, float reportedPower, uint8_t internalLevel,
                  float cadenceRPM, uint16_t wiperCount, uint32_t timestamp);

    /// Return read-only reference to the stored recalibration points.
    const std::vector<RecalPoint>& getPoints() const { return _points; }
    /// Return the number of currently stored recalibration points.
    size_t getPointCount() const { return _points.size(); }
    /// Return the maximum capacity (MAX_POINTS).
    size_t getMaxPoints() const { return MAX_POINTS; }

    /// Clear all stored recalibration points.
    void reset() { _points.clear(); }

    /// Fit least-squares eddy-current scale factor (params.a_scale) on Physics P3r from stored points.
    bool applyToPhysicsParams(PowerPhysicsParams& params) const;

    /// Apply stored points to a PowerTable, updating the nearest (level, RPM bin) cell for each point.
    void applyToPowerTable(PowerTable& table) const;

    /// Compute the 0-indexed PowerTable RPM bin corresponding to cadence rpm.
    static int getRpmBin(float rpm);

private:
    ResistanceSensor* _resistanceSensor = nullptr;
    CadenceSensor* _cadenceSensor = nullptr;
    std::vector<RecalPoint> _points;
};

} // namespace echenon
