/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HRSynthetic.cpp
 * Description: Implementation of the diagnostic synthetic 1 Hz heart rate generator.
 */

#include "echenon/HRSynthetic.h"

#ifdef WITH_HR_SYNTHETIC

#include "EventBus/EventBus.hpp"
#include <cmath>

namespace echenon {

void HRSynthetic::tick(uint32_t currentMillis) {
    if (!_bus) {
        return;
    }

    if (currentMillis - _lastPublishMs < 1000 && _lastPublishMs != 0) {
        return;
    }
    _lastPublishMs = currentMillis;

    // 60–120 BPM sine wave with 10 s period
    constexpr float PERIOD_MS = 10000.0f;
    constexpr float TWO_PI = 6.28318530718f;
    float phase = (currentMillis % static_cast<uint32_t>(PERIOD_MS)) / PERIOD_MS;
    float bpmVal = 90.0f + 30.0f * std::sin(TWO_PI * phase);
    uint8_t bpm = static_cast<uint8_t>(std::round(bpmVal));

    HrSample& sample = _sampleRing[_ringHead];
    _ringHead = (_ringHead + 1) % 4;

    sample.bpm = bpm;
    sample.contactState = HRContactState::ACTIVE;
    sample.receivedAtMs = currentMillis;

    _bus->publish(HR_TOPIC, &sample);
}

} // namespace echenon

#endif // WITH_HR_SYNTHETIC
