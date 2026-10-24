#include "echenon/PowerEngine.h"
#include "echenon/PowerTableDefault.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace echenon {

namespace {

inline float evaluateKlossPower(
    float p_norm,
    float cadence_rpm,
    const PowerPhysicsParams& params) {
    const float u = 1.0f - std::cos(params.cam_sweep_rad * p_norm);
    const float r = cadence_rpm / params.cad_crit;
    const float p_eddy =
        params.a_scale * std::exp(params.beta * u) *
        ((cadence_rpm * cadence_rpm) / (1.0f + r * r));
    const float p = p_eddy + params.c_fric * cadence_rpm;
    return std::clamp(p, 0.0f, PowerEngine::MAX_WATTS);
}

} // namespace

PowerEngine::PowerEngine() {
    loadDefault();
}

void PowerEngine::setTable(const PowerTable& table) {
    _table = table;
    _usePhysicsModel =
        (_table.tier == PowerTableTier::DEFAULT) &&
        (std::strcmp(_table.id, "ex3-default") == 0);
}

const PowerTable& PowerEngine::getTable() const {
    return _table;
}

void PowerEngine::loadDefault() {
    _table.tier = PowerTableTier::DEFAULT;
    std::strncpy(_table.id, "ex3-default", sizeof(_table.id) - 1);
    _table.id[sizeof(_table.id) - 1] = '\0';
    _usePhysicsModel = true;

    // Empirical table from 320-point calibration sweep (2026-10-24).
    // Populates the 32 levels x 31 RPM bins fallback/reference grid for REST API
    // inspection and table serialisation, while Tier 1 runtime evaluation uses
    // the continuous 4-parameter first-principles formula (Physics P3r).
    for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
        for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
            _table.watts[l][r] = defaults::EX3_DEFAULT_POWER_TABLE[l][r];
        }
    }
}

float PowerEngine::computePowerPhysics(
    float continuous_level,
    float cadence_rpm,
    const PowerPhysicsParams& params) {
    if (!std::isfinite(cadence_rpm) || !std::isfinite(continuous_level) ||
        cadence_rpm <= 0.0f) {
        return 0.0f;
    }

    const float p_norm = std::clamp((continuous_level - 1.0f) / 31.0f, 0.0f, 1.0f);
    return evaluateKlossPower(p_norm, cadence_rpm, params);
}

float PowerEngine::computePowerFromPot(
    float pot_adc,
    float cadence_rpm,
    float pot_min,
    float pot_max,
    const PowerPhysicsParams& params) {
    if (!std::isfinite(cadence_rpm) || !std::isfinite(pot_adc) ||
        !std::isfinite(pot_min) || !std::isfinite(pot_max) ||
        cadence_rpm <= 0.0f || pot_max <= pot_min) {
        return 0.0f;
    }

    const float p_norm = std::clamp((pot_adc - pot_min) / (pot_max - pot_min), 0.0f, 1.0f);
    return evaluateKlossPower(p_norm, cadence_rpm, params);
}

float PowerEngine::computePowerTable(float level, float cadenceRPM) const {
    // Zero cadence -> always 0 W (SPEC §6.2)
    if (!std::isfinite(cadenceRPM) || !std::isfinite(level) || cadenceRPM <= 0.0f) {
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

    return std::clamp(p, 0.0f, MAX_WATTS);
}

float PowerEngine::computePower(float level, float cadenceRPM) const {
    if (_usePhysicsModel) {
        return computePowerPhysics(level, cadenceRPM, _physicsParams);
    }
    return computePowerTable(level, cadenceRPM);
}

void PowerEngine::tick(float level, float cadenceRPM) {
    _lastPower = computePower(level, cadenceRPM);
}

} // namespace echenon
