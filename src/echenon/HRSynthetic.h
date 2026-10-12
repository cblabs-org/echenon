#pragma once

#ifdef WITH_HR_SYNTHETIC

#include "interfaces/ITickable.hpp"
#include "echenon/HREvents.h"
#include <cstddef>

class EventBus;

namespace echenon {

class HRSynthetic : public ITickable {
public:
    HRSynthetic() = default;
    ~HRSynthetic() override = default;

    void setEventBus(EventBus* bus) { _bus = bus; }

    void tick(uint32_t currentMillis) override;

private:
    EventBus* _bus = nullptr;
    uint32_t  _lastPublishMs = 0;
    HrSample  _sampleRing[4] = {};
    size_t    _ringHead = 0;
};

} // namespace echenon

#endif // WITH_HR_SYNTHETIC
