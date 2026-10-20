#pragma once

#include "echenon/PowerTable.h"

namespace echenon {

/**
 * @brief Power calculation engine implementing bilinear interpolation on the
 * empirical per-level × cadence table (SPEC §6.2).
 *
 * Interpolates on continuous level ℓ ∈ [1.0, 32.0] and cadence ∈ [0.0, 150.0] RPM.
 * Enforces zero power at zero cadence and non-negative clamping: max(0, P).
 */
class PowerEngine {
public:
    PowerEngine();

    // Set the active power table
    void setTable(const PowerTable& table);

    // Get the active power table (for inspection, serialisation)
    const PowerTable& getTable() const;

    // Load the compiled-in default table
    void loadDefault();

    // Compute power using bilinear interpolation
    // level: continuous level ℓ (1.0–32.0) from LevelMapping
    // cadenceRPM: current cadence in RPM
    // Returns watts, clamped to max(0, P)
    float computePower(float level, float cadenceRPM) const;

    // Get the last computed power (cached from tick)
    float getLastPower() const { return _lastPower; }

    // Tick: compute power from current sensor readings
    void tick(float level, float cadenceRPM);

private:
    PowerTable _table;
    float _lastPower = 0.0f;
};

} // namespace echenon
