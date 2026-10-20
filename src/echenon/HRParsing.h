#pragma once

#include <cstdint>
#include <cstddef>
#include "echenon/HREvents.h"

namespace echenon {

struct ParsedHrMeasurement {
    uint16_t       bpm = 0;
    HRContactState contactState = HRContactState::UNKNOWN;
    bool           hasEnergy = false;
    uint16_t       energyExpended = 0;
    bool           hasRr = false;
    float          lastRrMs = 0.0f;
    uint8_t        fieldsConsumed = 0;  // bytes parsed
};

/**
 * @brief Parse a 0x2A37 HR Measurement notification payload.
 *
 * Implements the Bluetooth SIG Heart Rate Service 0x2A37 specification:
 * - Bit 0: Heart rate value format (0 = uint8, 1 = uint16 little-endian)
 * - Bit 1: Sensor contact detected
 * - Bit 2: Sensor contact supported
 * - Bit 3: Energy expended present (uint16 little-endian kJ)
 * - Bit 4: RR-interval present (uint16 little-endian 1/1024 s)
 *
 * Contact state determination (SPEC §8.2):
 * - If BPM == 0: NO_CONTACT
 * - If contact supported and not detected: NO_CONTACT
 * - Otherwise: ACTIVE
 *
 * @param data Pointer to raw notification bytes
 * @param length Length of payload
 * @param out Output struct populated on success
 * @return true if valid, false if truncated/invalid
 */
inline bool parse0x2A37(const uint8_t* data, size_t length, ParsedHrMeasurement& out) {
    if (!data || length < 2) {
        return false;
    }

    uint8_t flags = data[0];
    bool hrIs16Bit        = (flags & 0x01) != 0;
    bool contactDetected  = (flags & 0x02) != 0;
    bool contactSupported = (flags & 0x04) != 0;
    bool energyPresent    = (flags & 0x08) != 0;
    bool rrPresent        = (flags & 0x10) != 0;

    uint16_t bpm;
    size_t idx;
    if (hrIs16Bit) {
        if (length < 3) {
            return false;
        }
        bpm = static_cast<uint16_t>(data[1] | (static_cast<uint16_t>(data[2]) << 8));
        idx = 3;
    } else {
        bpm = data[1];
        idx = 2;
    }

    out.bpm = bpm;

    // Contact state determination (SPEC §8.2)
    if (bpm == 0 || (contactSupported && !contactDetected)) {
        out.contactState = HRContactState::NO_CONTACT;
    } else {
        out.contactState = HRContactState::ACTIVE;
    }

    // Energy Expended (uint16 kJ)
    if (energyPresent) {
        if (idx + 2 > length) {
            return false;
        }
        out.hasEnergy = true;
        out.energyExpended = static_cast<uint16_t>(data[idx] | (static_cast<uint16_t>(data[idx + 1]) << 8));
        idx += 2;
    } else {
        out.hasEnergy = false;
        out.energyExpended = 0;
    }

    // RR-intervals (uint16 1/1024 s each)
    if (rrPresent) {
        if (idx + 2 > length) {
            return false;
        }
        out.hasRr = true;
        while (idx + 2 <= length) {
            uint16_t rrRaw = static_cast<uint16_t>(data[idx] | (static_cast<uint16_t>(data[idx + 1]) << 8));
            out.lastRrMs = static_cast<float>(rrRaw) / 1024.0f * 1000.0f;
            idx += 2;
        }
    } else {
        out.hasRr = false;
        out.lastRrMs = 0.0f;
    }

    out.fieldsConsumed = static_cast<uint8_t>(idx);
    return true;
}

} // namespace echenon
