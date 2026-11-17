#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HRSynthetic.h
 * Description: Synthetic 1 Hz sine-wave heart rate generator (60-120 BPM) for diagnostic builds without a BLE strap.
 */

#ifdef WITH_HR_SYNTHETIC

#include "interfaces/ITickable.hpp"
#include "echenon/HREvents.h"
#include <cstddef>

class EventBus;

namespace echenon {

/**
 * @brief Diagnostic synthetic heart rate publisher (60-120 BPM sine wave, 10 s period).
 *
 * Publishes HrSample events to EventBus at 1 Hz when WITH_HR_SYNTHETIC is enabled.
 */
class HRSynthetic : public ITickable {
public:
    /// Default constructor.
    HRSynthetic() = default;
    /// Default destructor.
    ~HRSynthetic() override = default;

    /// Attach the framework EventBus used to publish synthetic HrSample events.
    void setEventBus(EventBus* bus) { _bus = bus; }

    /// Publish a synthetic 60-120 BPM HrSample on HR_TOPIC at 1 Hz.
    void tick(uint32_t currentMillis) override;

private:
    EventBus* _bus = nullptr;
    uint32_t  _lastPublishMs = 0;
    HrSample  _sampleRing[4] = {};
    size_t    _ringHead = 0;
};

} // namespace echenon

#endif // WITH_HR_SYNTHETIC
