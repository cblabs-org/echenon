#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/FTMSData.h
 * Description: FTMS v1.0 binary packet encoder (0x2ACC Feature and 0x2AD2 Indoor Bike Data) — pure C++ header, no BLE dependency.
 * Reference: BLE FTMS GATT service structure references MIT-licensed prior art
 *            (jamesjmtaylor/esp32-ftms-server, dbsqp/bluetooth-reebok-57e, lefty01/ESP32_TTGO_FTMS; see SPEC.md §2.3).
 */

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

namespace echenon {

// =========================================================================
// FTMS UUID & Feature Constants (SPEC §7.2–§7.4)
// =========================================================================

/// FTMS Service UUID (Bluetooth SIG assigned)
constexpr uint16_t FTMS_SERVICE_UUID          = 0x1826;
/// Fitness Machine Feature characteristic UUID
constexpr uint16_t FTMS_FEATURE_CHAR_UUID     = 0x2ACC;
/// Indoor Bike Data characteristic UUID
constexpr uint16_t FTMS_INDOOR_BIKE_DATA_UUID = 0x2AD2;
/// Fitness Machine Control Point characteristic UUID
constexpr uint16_t FTMS_CONTROL_POINT_UUID    = 0x2AD9;

/// Fitness Machine Features bitmask (v1: Cadence | Resistance | HR | Power)
/// Bit 1: Average Speed, Bit 7: Resistance Level, Bit 10: Heart Rate, Bit 14: Power
constexpr uint32_t FTMS_MACHINE_FEATURES_V1   = 0x00004482u;
/// Target Setting Features (v1: read-only, no targets)
constexpr uint32_t FTMS_TARGET_FEATURES_V1    = 0x00000000u;

/// Indoor Bike Data flags for v1 packet layout:
/// Bit 0 = 0 (Instantaneous Speed present), Bit 2 = 1 (Cadence),
/// Bit 5 = 1 (Resistance Level), Bit 6 = 1 (Power), Bit 9 = 1 (Heart Rate)
constexpr uint16_t FTMS_INDOOR_BIKE_FLAGS_V1  = 0x0264u;

constexpr size_t FTMS_FEATURE_PACKET_SIZE     = 8;  ///< Fixed byte length of 0x2ACC Fitness Machine Feature payload
constexpr size_t FTMS_INDOOR_BIKE_PACKET_SIZE = 11; ///< Fixed byte length of 0x2AD2 Indoor Bike Data v1 payload

// =========================================================================
// Telemetry Data Structure
// =========================================================================

/// Telemetry snapshot for encoding into FTMS Indoor Bike Data (0x2AD2).
struct IndoorBikeTelemetry {
    float   speed_kph           = 0.0f;   ///< Virtual road speed (km/h)
    float   cadence_rpm         = 0.0f;   ///< Instantaneous cadence (RPM)
    uint8_t internal_resistance = 0;      ///< Canonical internal resistance (0–254; 255 = uncalibrated)
    int16_t power_watts         = 0;      ///< Instantaneous power (W)
    uint8_t heart_rate_bpm      = 0;      ///< Heart rate (BPM; 0 = no strap / stale)
};

// =========================================================================
// Packet Encoding Functions
// =========================================================================

/// Encode the 8-byte Fitness Machine Feature characteristic value.
/// @param out   Output buffer (must be at least 8 bytes)
/// @param machine_features  Machine features bitmask (default: v1)
/// @param target_features   Target setting features bitmask (default: 0)
inline void encodeFitnessMachineFeature(
    uint8_t out[8],
    uint32_t machine_features = FTMS_MACHINE_FEATURES_V1,
    uint32_t target_features  = FTMS_TARGET_FEATURES_V1) {
    // Little-endian encoding of two uint32_t values
    out[0] = static_cast<uint8_t>(machine_features & 0xFF);
    out[1] = static_cast<uint8_t>((machine_features >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((machine_features >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((machine_features >> 24) & 0xFF);
    out[4] = static_cast<uint8_t>(target_features & 0xFF);
    out[5] = static_cast<uint8_t>((target_features >> 8) & 0xFF);
    out[6] = static_cast<uint8_t>((target_features >> 16) & 0xFF);
    out[7] = static_cast<uint8_t>((target_features >> 24) & 0xFF);
}

/// Encode the 11-byte Indoor Bike Data characteristic notification value.
/// @param t     Telemetry snapshot to encode
/// @param out   Output buffer (must be at least 11 bytes)
/// @return      Always returns 11 (the encoded packet size)
inline size_t encodeIndoorBikeData(const IndoorBikeTelemetry& t, uint8_t out[11]) {
    // Bytes 0..1: Flags = 0x0264
    const uint16_t flags = FTMS_INDOOR_BIKE_FLAGS_V1;
    out[0] = static_cast<uint8_t>(flags & 0xFF);
    out[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);

    // Bytes 2..3: Instantaneous Speed (uint16, resolution 0.01 km/h)
    const float speed_clamped = std::clamp(t.speed_kph, 0.0f, 655.35f);
    const uint16_t speed_raw = static_cast<uint16_t>(
        std::round(speed_clamped * 100.0f));
    out[2] = static_cast<uint8_t>(speed_raw & 0xFF);
    out[3] = static_cast<uint8_t>((speed_raw >> 8) & 0xFF);

    // Bytes 4..5: Instantaneous Cadence (uint16, resolution 0.5 RPM)
    const float cadence_clamped = std::clamp(t.cadence_rpm, 0.0f, 32767.5f);
    const uint16_t cadence_raw = static_cast<uint16_t>(
        std::round(cadence_clamped * 2.0f));
    out[4] = static_cast<uint8_t>(cadence_raw & 0xFF);
    out[5] = static_cast<uint8_t>((cadence_raw >> 8) & 0xFF);

    // Bytes 6..7: Resistance Level (int16; 0 if uncalibrated 255)
    int16_t resistance;
    if (t.internal_resistance == 255) {
        resistance = 0;
    } else {
        resistance = static_cast<int16_t>(t.internal_resistance);
    }
    out[6] = static_cast<uint8_t>(static_cast<uint16_t>(resistance) & 0xFF);
    out[7] = static_cast<uint8_t>((static_cast<uint16_t>(resistance) >> 8) & 0xFF);

    // Bytes 8..9: Instantaneous Power (int16, clamped to [0, 4000])
    const int16_t power_clamped = static_cast<int16_t>(
        std::clamp(static_cast<int>(t.power_watts), 0, 4000));
    out[8] = static_cast<uint8_t>(static_cast<uint16_t>(power_clamped) & 0xFF);
    out[9] = static_cast<uint8_t>((static_cast<uint16_t>(power_clamped) >> 8) & 0xFF);

    // Byte 10: Heart Rate (uint8, BPM; 0 when no strap / stale)
    out[10] = t.heart_rate_bpm;

    return FTMS_INDOOR_BIKE_PACKET_SIZE;
}

} // namespace echenon
