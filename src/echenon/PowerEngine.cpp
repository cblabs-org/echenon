#include "echenon/PowerEngine.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace echenon {

PowerEngine::PowerEngine() {
    loadDefault();
}

void PowerEngine::setTable(const PowerTable& table) {
    _table = table;
}

const PowerTable& PowerEngine::getTable() const {
    return _table;
}

void PowerEngine::loadDefault() {
    _table.tier = PowerTableTier::DEFAULT;
    std::strncpy(_table.id, "ex3-default", sizeof(_table.id) - 1);
    _table.id[sizeof(_table.id) - 1] = '\0';

    // Physics heuristic: P ≈ k × level × RPM² (SPEC §6.3 Tier 1 placeholder)
    // k = 0.0012 gives ~123 W at level 16 / 80 RPM (within ~100–200 W range)
    constexpr float k = 0.0012f;
    for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
        float level = static_cast<float>(l + 1); // 1 to 32
        for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
            float rpm = static_cast<float>(r) * POWER_TABLE_RPM_STEP; // 0, 5, ..., 150
            float p = k * level * rpm * rpm;
            _table.watts[l][r] = static_cast<int16_t>(std::round(p));
        }
    }
}

float PowerEngine::computePower(float level, float cadenceRPM) const {
    // Zero cadence → always 0 W (SPEC §6.2)
    if (cadenceRPM <= 0.0f) {
        return 0.0f;
    }

    // Clamp level to [1.0, 32.0]
    if (level < 1.0f) level = 1.0f;
    if (level > static_cast<float>(POWER_TABLE_LEVELS)) {
        level = static_cast<float>(POWER_TABLE_LEVELS);
    }

    // Clamp cadence to [0.0, POWER_TABLE_RPM_MAX]
    if (cadenceRPM > POWER_TABLE_RPM_MAX) {
        cadenceRPM = POWER_TABLE_RPM_MAX;
    }

    // Level axis: 1.0..32.0 -> fractional index 0.0..31.0
    float levelIdx = level - 1.0f;
    int i0 = static_cast<int>(std::floor(levelIdx));
    if (i0 < 0) i0 = 0;
    if (i0 >= POWER_TABLE_LEVELS - 1) {
        i0 = POWER_TABLE_LEVELS - 1;
    }
    int i1 = (i0 < POWER_TABLE_LEVELS - 1) ? i0 + 1 : i0;
    float tLevel = (i0 == i1) ? 0.0f : (levelIdx - static_cast<float>(i0));

    // RPM axis: 0.0..150.0 -> fractional index 0.0..30.0 (step = 5.0)
    float rpmIdx = cadenceRPM / POWER_TABLE_RPM_STEP;
    int j0 = static_cast<int>(std::floor(rpmIdx));
    if (j0 < 0) j0 = 0;
    if (j0 >= POWER_TABLE_RPM_BINS - 1) {
        j0 = POWER_TABLE_RPM_BINS - 1;
    }
    int j1 = (j0 < POWER_TABLE_RPM_BINS - 1) ? j0 + 1 : j0;
    float tRpm = (j0 == j1) ? 0.0f : (rpmIdx - static_cast<float>(j0));

    // Bilinear interpolation between the four surrounding grid points
    float w00 = static_cast<float>(_table.watts[i0][j0]);
    float w01 = static_cast<float>(_table.watts[i0][j1]);
    float w10 = static_cast<float>(_table.watts[i1][j0]);
    float w11 = static_cast<float>(_table.watts[i1][j1]);

    float w0 = w00 * (1.0f - tRpm) + w01 * tRpm;
    float w1 = w10 * (1.0f - tRpm) + w11 * tRpm;
    float p = w0 * (1.0f - tLevel) + w1 * tLevel;

    // Power clamped to max(0, P)
    if (p < 0.0f) {
        p = 0.0f;
    }

    return p;
}

void PowerEngine::tick(float level, float cadenceRPM) {
    _lastPower = computePower(level, cadenceRPM);
}

} // namespace echenon
