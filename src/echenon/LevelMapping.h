#pragma once

#include <cstdint>

namespace LevelMapping {

    // Constants (SPEC §5.2, §5.3)
    constexpr float B03_SLOPE = 39.1f;
    constexpr float B03_INTERCEPT = 97.0f;
    constexpr uint16_t WORKING_RANGE_MIN = 165;
    constexpr uint16_t WORKING_RANGE_MAX = 1327;

    // Wiper → continuous level (float, clamped 1.0–32.0)
    // Uses B03 default fit: wiper = 39.1 × level + 97
    // level = (wiper - 97) / 39.1, clamped to [1, 32]
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
