#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/LevelMapping.h
 * Description: Resistance potentiometer wiper, Echelon 1-32 level, FTMS internal 0-255, and Peloton scale conversions.
 * Reference: Resistance scale mapping concepts reference MIT-licensed prior art
 *            (ptx2/gymnasticon, cagnulein/qdomyos-zwift, lefty01/ESP32_TTGO_FTMS; see SPEC.md §2.3).
 */

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace LevelMapping {

    // Constants — C07 320-point sweep model (docs/power-physics-model.md §1)
    constexpr uint16_t WORKING_RANGE_MIN = 171;   ///< Default potentiometer ADC count at Echelon level 1
    constexpr uint16_t WORKING_RANGE_MAX = 1333;  ///< Default potentiometer ADC count at Echelon level 32
    constexpr float B03_SLOPE = (1333.0f - 171.0f) / 31.0f; ///< B03 linear ADC counts per Echelon level (37.483871f)
    constexpr float B03_INTERCEPT = 171.0f - B03_SLOPE;     ///< B03 linear intercept at level 0 (133.516129f)

    /// Wiper (float) → normalized cam position p̃ ∈ [0.0, 1.0]
    inline float wiperToNormalized(
        float wiper,
        float potMin = static_cast<float>(WORKING_RANGE_MIN),
        float potMax = static_cast<float>(WORKING_RANGE_MAX)) {
        if (!std::isfinite(wiper) || !std::isfinite(potMin) ||
            !std::isfinite(potMax) || potMax <= potMin) {
            return 0.0f;
        }
        return std::clamp((wiper - potMin) / (potMax - potMin), 0.0f, 1.0f);
    }

    /// Wiper → continuous Echelon level (float, clamped 1.0–32.0).
    float wiperToLevel(
        float wiper,
        float potMin = static_cast<float>(WORKING_RANGE_MIN),
        float potMax = static_cast<float>(WORKING_RANGE_MAX));

    /// Continuous level → canonical internal scale i ∈ 0…255: round((level - 1) × 255 / 31).
    uint8_t levelToInternal(float level);

    /// Internal scale → Echelon display level L ∈ 1…32: round(i × 31 / 255) + 1.
    uint8_t internalToEchelon(uint8_t i);

    /// Echelon level → internal scale: round((L - 1) × 255 / 31).
    uint8_t echelonToInternal(uint8_t echelonLevel);

    /// Convenience: wiper → discrete Echelon level (1..32).
    uint8_t wiperToEchelon(
        float wiper,
        float potMin = static_cast<float>(WORKING_RANGE_MIN),
        float potMax = static_cast<float>(WORKING_RANGE_MAX));

    /// Convenience: wiper → FTMS internal scale (0..255).
    uint8_t wiperToInternal(
        float wiper,
        float potMin = static_cast<float>(WORKING_RANGE_MIN),
        float potMax = static_cast<float>(WORKING_RANGE_MAX));

    // =========================================================================
    // WO-05.1: Unified BikeMetrics — Normalized cam/wiper position helpers
    // =========================================================================

    /// FTMS internal resistance sentinel: device not yet calibrated
    constexpr uint8_t INTERNAL_RESISTANCE_UNCALIBRATED = 255;

    /// Echelon continuous level → normalized cam position p̃ ∈ [0.0, 1.0]
    /// Returns 0.0f for non-finite inputs.
    inline float echelonContinuousToNormalized(float continuous_level) {
        if (!std::isfinite(continuous_level)) return 0.0f;
        return std::clamp((continuous_level - 1.0f) / 31.0f, 0.0f, 1.0f);
    }

    /// Normalized → Echelon continuous level ℓ ∈ [1.0, 32.0]
    inline float normalizedToEchelonContinuous(float p_tilde) {
        return 1.0f + 31.0f * std::clamp(p_tilde, 0.0f, 1.0f);
    }

    /// Normalized → FTMS internal scale i ∈ {0..254} (255 = UNCALIBRATED)
    inline uint8_t normalizedToInternal(float p_tilde) {
        return static_cast<uint8_t>(
            std::round(254.0f * std::clamp(p_tilde, 0.0f, 1.0f)));
    }

    /// FTMS internal scale → normalized cam position.
    /// Returns 0.0f if internal == INTERNAL_RESISTANCE_UNCALIBRATED (255).
    inline float internalToNormalized(uint8_t internal) {
        if (internal == INTERNAL_RESISTANCE_UNCALIBRATED) return 0.0f;
        return std::clamp(static_cast<float>(internal) / 254.0f, 0.0f, 1.0f);
    }

    /// Echelon internal scale (0..255) → Peloton 0–100 power-equivalence level.
    /// Returns 0 if internal == 255 (UNCALIBRATED sentinel).
    /// Uses the Peloton power-equivalence formula at 80 RPM reference cadence.
    uint8_t internalToPeloton(uint8_t internal);

} // namespace LevelMapping

namespace echenon {
    namespace LevelMapping = ::LevelMapping;
}
