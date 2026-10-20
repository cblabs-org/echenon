#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "echenon/PowerTable.h"

namespace echenon {

class ResistanceSensor;
class CadenceSensor;

/**
 * @brief Individual power-point correction storage struct (SPEC §6.3 Tier 3).
 */
struct RecalPoint {
    uint8_t echelonLevel;     // entered by rider (1..32)
    float reportedPower;       // entered by rider (watts)
    uint8_t internalLevel;     // live i from LevelMapping
    float cadenceRPM;          // live cadence
    uint16_t wiperCount;       // live wiper
    uint32_t timestamp;        // millis()
};

/**
 * @brief Manages individual power-point corrections (SPEC §6.3 Tier 3).
 *
 * Stores up to 100 recalibration points. When a new point is added at the same
 * (level, RPM bin), the new point replaces the existing point (simple replacement rule).
 */
class RecalibrationUI {
public:
    static constexpr size_t MAX_POINTS = 100;
    static constexpr uint32_t AVERAGING_WINDOW_MS = 5000;

    explicit RecalibrationUI(ResistanceSensor* rs = nullptr, CadenceSensor* cs = nullptr);
    ~RecalibrationUI() = default;

    void setSensors(ResistanceSensor* rs, CadenceSensor* cs) {
        _resistanceSensor = rs;
        _cadenceSensor = cs;
    }

    // Add a point using live sensor readings
    bool addPoint(uint8_t echelonLevel, float reportedPower);

    // Add a point with explicit values (for tests or pre-calculated data)
    bool addPoint(uint8_t echelonLevel, float reportedPower, uint8_t internalLevel,
                  float cadenceRPM, uint16_t wiperCount, uint32_t timestamp);

    const std::vector<RecalPoint>& getPoints() const { return _points; }
    size_t getPointCount() const { return _points.size(); }
    size_t getMaxPoints() const { return MAX_POINTS; }

    void reset() { _points.clear(); }

    // Helper: apply points to a PowerTable (updates nearest cell for each point)
    void applyToPowerTable(PowerTable& table) const;

    // Helper to calculate RPM bin from cadence
    static int getRpmBin(float rpm);

private:
    ResistanceSensor* _resistanceSensor = nullptr;
    CadenceSensor* _cadenceSensor = nullptr;
    std::vector<RecalPoint> _points;
};

} // namespace echenon
