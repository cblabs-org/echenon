#include "echenon/LevelMapping.h"
#include <cmath>
#include <algorithm>

namespace LevelMapping {

float wiperToLevel(uint16_t wiper) {
    float lvl = (static_cast<float>(wiper) - B03_INTERCEPT) / B03_SLOPE;
    if (lvl < 1.0f) return 1.0f;
    if (lvl > 32.0f) return 32.0f;
    return lvl;
}

uint8_t levelToInternal(float level) {
    if (level <= 1.0f) return 0;
    if (level >= 32.0f) return 255;
    float val = std::round((level - 1.0f) * 255.0f / 31.0f);
    if (val < 0.0f) return 0;
    if (val > 255.0f) return 255;
    return static_cast<uint8_t>(val);
}

uint8_t internalToEchelon(uint8_t i) {
    float val = std::round(static_cast<float>(i) * 31.0f / 255.0f) + 1.0f;
    if (val < 1.0f) return 1;
    if (val > 32.0f) return 32;
    return static_cast<uint8_t>(val);
}

uint8_t echelonToInternal(uint8_t echelonLevel) {
    if (echelonLevel <= 1) return 0;
    if (echelonLevel >= 32) return 255;
    float val = std::round(static_cast<float>(echelonLevel - 1) * 255.0f / 31.0f);
    if (val < 0.0f) return 0;
    if (val > 255.0f) return 255;
    return static_cast<uint8_t>(val);
}

uint8_t wiperToEchelon(uint16_t wiper) {
    return internalToEchelon(wiperToInternal(wiper));
}

uint8_t wiperToInternal(uint16_t wiper) {
    return levelToInternal(wiperToLevel(wiper));
}

} // namespace LevelMapping
