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

float PowerEngine::computePowerFromNormalized(
    float p_tilde,
    float cadence_rpm,
    const PowerPhysicsParams& params) {
    if (!std::isfinite(cadence_rpm) || !std::isfinite(p_tilde) ||
        cadence_rpm <= 0.0f) {
        return 0.0f;
    }

    const float p_norm = std::clamp(p_tilde, 0.0f, 1.0f);
    return evaluateKlossPower(p_norm, cadence_rpm, params);
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

float PowerEngine::computePowerFromNormalized(float p_tilde, float cadenceRPM) const {
    if (_usePhysicsModel) {
        return computePowerFromNormalized(p_tilde, cadenceRPM, _physicsParams);
    }
    if (!std::isfinite(p_tilde)) {
        return 0.0f;
    }
    return computePowerTable(1.0f + 31.0f * std::clamp(p_tilde, 0.0f, 1.0f), cadenceRPM);
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

void PowerEngine::tickFromNormalized(float p_tilde, float cadenceRPM) {
    _lastPower = computePowerFromNormalized(p_tilde, cadenceRPM);
}

// =========================================================================
// WO-05.1: Peloton 0–100, Virtual Speed, Inverse Control Laws
// =========================================================================

uint8_t PowerEngine::computePelotonResistanceFromNormalized(
    float p_tilde,
    const PowerPhysicsParams& params) {
    p_tilde = std::clamp(p_tilde, 0.0f, 1.0f);
    // Evaluate Physics P3r at reference cadence 80 RPM
    const float continuous_level = 1.0f + 31.0f * p_tilde;
    const float P_ref = computePowerPhysics(continuous_level, PELOTON_REF_CADENCE_RPM, params);

    // Peloton power equation inversion: P_pel(R, 80) = (0.000185 * R^2.7 + 0.1) * 106.66667
    const float term = std::max(P_ref / 106.66667f - 0.1f, 0.0f);
    const float R_raw = std::pow(term / 0.000185f, 1.0f / 2.7f);
    int R_pel = static_cast<int>(std::round(R_raw));
    return static_cast<uint8_t>(std::clamp(R_pel, 0, 100));
}

uint8_t PowerEngine::computePelotonResistance(
    float continuous_level,
    const PowerPhysicsParams& params) {
    if (!std::isfinite(continuous_level) || continuous_level < 1.0f) return 0;
    const float p_tilde = std::clamp((continuous_level - 1.0f) / 31.0f, 0.0f, 1.0f);
    return computePelotonResistanceFromNormalized(p_tilde, params);
}

float PowerEngine::computeVirtualSpeedKph(float power_watts) {
    if (!std::isfinite(power_watts) || power_watts <= 0.0f) return 0.0f;

    // Newton-Raphson to solve 0.245*v^3 + 3.53*v = P (v in m/s)
    float v = std::cbrt(power_watts / 0.245f); // initial guess
    for (int i = 0; i < 6; ++i) {
        const float f = 0.245f * v * v * v + 3.53f * v - power_watts;
        const float fp = 3.0f * 0.245f * v * v + 3.53f;
        if (fp == 0.0f) break;
        v -= f / fp;
    }

    const float speed_kph = 3.6f * v;
    return speed_kph >= 0.0f ? speed_kph : 0.0f;
}

float PowerEngine::targetPowerToNormalized(
    float target_watts,
    float cadence_rpm,
    const PowerPhysicsParams& params) {
    if (!std::isfinite(target_watts) || !std::isfinite(cadence_rpm) ||
        target_watts <= 0.0f || cadence_rpm <= 0.0f) {
        return 0.0f;
    }

    // Exact closed-form analytical inverse of Physics P3r (§8.4)
    // Step 1: Subtract friction to isolate eddy-current power
    const float P_eddy = std::max(target_watts - params.c_fric * cadence_rpm, 0.0f);

    // Step 2: Solve for the exponential air-gap term E = exp(beta * u)
    const float r = cadence_rpm / params.cad_crit;
    const float E = std::clamp(
        P_eddy * (1.0f + r * r) / (params.a_scale * cadence_rpm * cadence_rpm),
        1.0f,
        std::exp(1.87462f * params.beta));

    // Step 3: Recover u from E
    const float u_target = std::log(E) / params.beta;

    // Step 4: Invert the cam kinematics u = 1 - cos(cam_sweep * p)
    const float p_target = std::clamp(
        std::acos(1.0f - u_target) / params.cam_sweep_rad,
        0.0f, 1.0f);

    return p_target;
}

float PowerEngine::pelotonToNormalized(
    float peloton_level,
    const PowerPhysicsParams& params) {
    // Peloton power equation: P_ref = (0.000185 * R^2.7 + 0.1) * 106.66667
    const float R_clamped = std::clamp(peloton_level, 0.0f, 100.0f);
    const float P_ref = (0.000185f * std::pow(R_clamped, 2.7f) + 0.1f) * 106.66667f;
    return targetPowerToNormalized(P_ref, PELOTON_REF_CADENCE_RPM, params);
}

} // namespace echenon
