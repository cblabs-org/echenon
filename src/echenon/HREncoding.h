#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/HREncoding.h
 * Description: Bluetooth SIG Heart Rate Measurement (0x2A37) payload encoder for BLE relay notifications.
 */

#include <cstddef>
#include <cstdint>

namespace echenon {

/**
 * @brief Encode a Bluetooth SIG Heart Rate Measurement payload (GATT 0x2A37).
 *
 * Encodes uint8 BPM with flags byte 0x00 (uint8 format, contact not supported,
 * no energy expended, no RR-interval).
 *
 * Note: Body Sensor Location (0x2A38) is exposed on a separate characteristic,
 * not inside the 0x2A37 payload per Heart Rate Service specification.
 *
 * @param bpm Heart rate in beats per minute
 * @param bodySensorLocation Body sensor location (unused in 0x2A37, exposed on 0x2A38)
 * @param out Output buffer
 * @param outLen Size of output buffer
 * @return Number of bytes written (2 on success, 0 if buffer too small or null)
 */
inline size_t encode0x2A37(uint8_t bpm, uint8_t bodySensorLocation, uint8_t* out, size_t outLen) {
    (void)bodySensorLocation;
    if (!out || outLen < 2) {
        return 0;
    }
    out[0] = 0x00;
    out[1] = bpm;
    return 2;
}

/**
 * @brief Convenience overload omitting bodySensorLocation.
 */
inline size_t encode0x2A37(uint8_t bpm, uint8_t* out, size_t outLen) {
    return encode0x2A37(bpm, 0x00, out, outLen);
}

} // namespace echenon
