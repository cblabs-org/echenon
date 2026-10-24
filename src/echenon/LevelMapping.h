#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace LevelMapping {

    // Constants — Phase 3 bench-verified fit (C03, R²=0.9998)
    // Supersedes B03 fit (39.1, 97.0) per docs/measurements/phase3/C03
    constexpr float B03_SLOPE = 37.26f;
    constexpr float B03_INTERCEPT = 120.0f;
    constexpr uint16_t WORKING_RANGE_MIN = 157;   // pot @ level 1
    constexpr uint16_t WORKING_RANGE_MAX = 1313;  // pot @ level 32

    // Wiper → continuous level (float, clamped 1.0–32.0)
    // Uses Phase 3 fit: wiper = 37.26 × level + 120
    // level = (wiper - 120) / 37.26, clamped to [1, 32]
    float wiperToLevel(uint16_t wiper);

    // Continuous level → canonical internal scale i ∈ 0…255
    // i = round((level - 1) × 255 / 31)
    uint8_t levelToInternal(float level);

    // Internal scale → Echelon display level L ∈ 1…32
    // L = round(i × 31 / 255) + 1
    uint8_t internalToEchelon(uint8_t i);

    // Echelon level → internal scale
    // i = round((L - 1) × 255 / 31)
    uint8_t echelonToInternal(uint8_t echelonLevel);

    // Convenience: wiper → Echelon level
    uint8_t wiperToEchelon(uint16_t wiper);

    // Convenience: wiper → internal scale
    uint8_t wiperToInternal(uint16_t wiper);

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
