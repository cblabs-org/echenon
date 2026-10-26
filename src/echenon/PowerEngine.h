#pragma once

#include "echenon/PowerTable.h"
#include <cmath>

namespace echenon {

/**
 * @brief Parameters for the 4-parameter first-principles electromechanical
 * eddy-current + drivetrain friction model (Physics P3r, SPEC §6.1 / §6.2).
 *
 * Derived in docs/power-physics-model.md from eccentric-cam air-gap kinematics
 * (151 deg total sweep), Kloss high-RPM magnetic skin-effect rollover, and
 * Poly-V belt + bearing Coulomb drag.
 */
struct PowerPhysicsParams {
    float a_scale       = 0.0048839f; // A: Baseline eddy-current coefficient (W / RPM^2)
    float beta          = 1.630327f;  // beta: Exponential air-gap magnetic flux gain
    float cad_crit      = 128.1495f;  // cad_c: Kloss inductive rollover cadence (RPM)
    float c_fric        = 0.114260f;  // c_fric: Drivetrain belt/bearing drag (W / RPM)
    float cam_sweep_rad = 2.635447f;  // 151 deg eccentric cam total rotation (rad)
    float pot_min       = 171.0f;     // Default Level 1 raw 12-bit ADC count
    float pot_max       = 1333.0f;    // Default Level 32 raw 12-bit ADC count
};

/**
 * @brief Power calculation engine implementing the first-principles Physics P3r
 * model (Tier 1 DEFAULT) and bilinear interpolation on the 32x31 per-level x
 * cadence table (Tier 2 SELECTED / Tier 3 RECALIBRATED) (SPEC §6.2–§6.3).
 *
 * Enforces zero power at zero cadence and clamps output to [0.0, 2000.0] W.
 */
class PowerEngine {
public:
    static constexpr float A               = 0.0048839f;
    static constexpr float BETA            = 1.630327f;
    static constexpr float CAD_CRIT        = 128.1495f;
    static constexpr float C_FRIC          = 0.114260f;
    static constexpr float CAM_SWEEP_RAD   = 2.635447f;
    static constexpr float POT_MIN_DEFAULT = 171.0f;
    static constexpr float POT_MAX_DEFAULT = 1333.0f;
    static constexpr float MAX_WATTS       = 2000.0f;

    // =========================================================================
    // WO-05.1: Peloton 0–100 power-equivalence, virtual speed & inverse laws
    // =========================================================================

    /// Reference cadence for Peloton power-equivalence matching (RPM)
    static constexpr float PELOTON_REF_CADENCE_RPM = 80.0f;

    /// Compute Peloton 0–100 resistance from normalized cam position p̃ ∈ [0,1]
    static uint8_t computePelotonResistanceFromNormalized(
        float p_tilde,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    /// Compute Peloton 0–100 resistance from continuous Echelon level ∈ [1,32]
    static uint8_t computePelotonResistance(
        float continuous_level,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    /// Solve flat-road cycling equilibrium 0.245v³ + 3.53v = P for virtual speed (km/h)
    static float computeVirtualSpeedKph(float power_watts);

    /// Closed-form analytical inverse of Physics P3r: target watts → normalized cam position
    static float targetPowerToNormalized(
        float target_watts,
        float cadence_rpm,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    /// Peloton 0–100 level → normalized cam position (via reference power inversion)
    static float pelotonToNormalized(
        float peloton_level,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    PowerEngine();

    // Set the active power table (Tier 2 SELECTED / Tier 3 RECALIBRATED override)
    void setTable(const PowerTable& table);

    // Get the active power table (for inspection, REST API, serialisation)
    const PowerTable& getTable() const;

    // Load the compiled-in default table and enable Tier 1 Physics P3r evaluation
    void loadDefault();

    // Evaluate first-principles Physics P3r model directly from normalized cam position p̃ ∈ [0.0, 1.0]
    static float computePowerFromNormalized(
        float p_tilde,
        float cadence_rpm,
        const PowerPhysicsParams& params);

    template <typename Params = PowerPhysicsParams>
    static float computePowerFromNormalized(
        float p_tilde,
        float cadence_rpm) {
        return computePowerFromNormalized(p_tilde, cadence_rpm, Params{});
    }

    // Evaluate first-principles Physics P3r model from continuous level in [1.0, 32.0]
    static float computePowerPhysics(
        float continuous_level,
        float cadence_rpm,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    // Evaluate first-principles Physics P3r model directly from raw potentiometer ADC
    static float computePowerFromPot(
        float pot_adc,
        float cadence_rpm,
        float pot_min = POT_MIN_DEFAULT,
        float pot_max = POT_MAX_DEFAULT,
        const PowerPhysicsParams& params = PowerPhysicsParams{});

    // Compute power using bilinear interpolation over the active 32x31 PowerTable
    float computePowerTable(float level, float cadenceRPM) const;

    // Primary power calculation entry point from normalized cam position p̃ ∈ [0.0, 1.0]
    float computePowerFromNormalized(float p_tilde, float cadenceRPM) const;

    // Primary power calculation entry point:
    // - Tier 1 (DEFAULT "ex3-default"): evaluates continuous Physics P3r formula
    // - Tier 2/3 (SELECTED / RECALIBRATED or custom table): bilinear interpolation on _table
    float computePower(float level, float cadenceRPM) const;

    // Physics parameter accessors
    const PowerPhysicsParams& getPhysicsParams() const { return _physicsParams; }
    void setPhysicsParams(const PowerPhysicsParams& params) { _physicsParams = params; }

    // Get the last computed power (cached from tick)
    float getLastPower() const { return _lastPower; }

    // Tick: compute power from current sensor readings
    void tick(float level, float cadenceRPM);
    void tickFromNormalized(float p_tilde, float cadenceRPM);

private:
    PowerTable _table;
    PowerPhysicsParams _physicsParams{};
    bool _usePhysicsModel = true;
    float _lastPower = 0.0f;
};

} // namespace echenon
