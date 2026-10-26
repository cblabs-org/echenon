#include "echenon/LevelMapping.h"
#include "echenon/PowerEngine.h"
#include <cmath>
#include <algorithm>

namespace LevelMapping {

float wiperToLevel(float wiper, float potMin, float potMax) {
    return normalizedToEchelonContinuous(wiperToNormalized(wiper, potMin, potMax));
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

uint8_t wiperToEchelon(float wiper, float potMin, float potMax) {
    return static_cast<uint8_t>(
        std::clamp(std::round(wiperToLevel(wiper, potMin, potMax)), 1.0f, 32.0f));
}

uint8_t wiperToInternal(float wiper, float potMin, float potMax) {
    return levelToInternal(wiperToLevel(wiper, potMin, potMax));
}

uint8_t internalToPeloton(uint8_t internal) {
    if (internal == INTERNAL_RESISTANCE_UNCALIBRATED) return 0;
    float p_tilde = static_cast<float>(internal) / 254.0f;
    return echenon::PowerEngine::computePelotonResistanceFromNormalized(p_tilde);
}

} // namespace LevelMapping
