#pragma once

#include <cstdint>

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

} // namespace LevelMapping

namespace echenon {
    namespace LevelMapping = ::LevelMapping;
}
