#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HREvents.h
 * Description: EventBus heart-rate topic, sample payload, and contact-state definitions (SPEC §8.2, §8.4).
 */

#include <cstdint>

namespace echenon {

/// HR contact state machine (SPEC §8.2, §8.4)
enum class HRContactState : uint8_t {
    UNKNOWN = 0,     ///< No data yet
    ACTIVE,          ///< Valid HR reading
    NO_CONTACT,      ///< BPM = 0 or contact flags say off-skin
    DISCONNECTED     ///< BLE connection lost
};

/// EventBus payload for topic "echenon.hr"
struct HrSample {
    uint8_t        bpm;            ///< 0 = no contact / stale
    HRContactState contactState;   ///< Current strap contact/connection state
    uint32_t       receivedAtMs;   ///< millis() when received
};

/// EventBus topic name
inline constexpr const char* HR_TOPIC = "echenon.hr";

} // namespace echenon
