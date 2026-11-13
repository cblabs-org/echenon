#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/PowerTable.h
 * Description: 32x31 power lookup table data structure with CSV parsing and NVS binary serialisation.
 * Reference: Lookup table concepts reference MIT-licensed prior art
 *            (ptx2/gymnasticon, cagnulein/qdomyos-zwift, lefty01/ESP32_TTGO_FTMS; see SPEC.md §2.3).
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>

namespace echenon {

// Table dimensions (SPEC §6.2)
constexpr int POWER_TABLE_LEVELS = 32;       ///< Number of Echelon resistance levels (1–32)
constexpr int POWER_TABLE_RPM_BINS = 31;     ///< Number of cadence bins: 0, 5, 10, ..., 150 RPM
constexpr float POWER_TABLE_RPM_STEP = 5.0f; ///< Cadence bin spacing in RPM
constexpr float POWER_TABLE_RPM_MAX = 150.0f; ///< Maximum cadence bin value in RPM

// Serialised binary format constants
constexpr uint32_t POWER_TABLE_MAGIC = 0x504F5754; ///< Binary header magic "POWT" (little-endian)
constexpr uint8_t POWER_TABLE_VERSION = 1;         ///< Binary serialisation format version
constexpr size_t SERIALISED_TABLE_SIZE = 4 + 1 + 1 + 2 + 32 + (POWER_TABLE_LEVELS * POWER_TABLE_RPM_BINS * sizeof(int16_t)); ///< Exact serialised blob size (2024 bytes)

// Table tier IDs (SPEC §6.3)
#ifdef DEFAULT
#undef DEFAULT
#endif
/// Active power table provenance tier (SPEC §6.3).
enum class PowerTableTier : uint8_t {
    DEFAULT = 1,       ///< Compiled-in default
    SELECTED = 2,      ///< Selected built-in or custom CSV upload
    RECALIBRATED = 3   ///< Updated by recalibration UI
};

/**
 * @brief Power lookup table data structure (SPEC §6.2, §6.3, §6.4).
 *
 * 32 Echelon levels × 31 cadence bins (0 to 150 RPM in 5 RPM increments).
 * Header-only struct supporting CSV parsing, binary serialisation for NVS,
 * and tier tracking.
 */
struct PowerTable {
    int16_t watts[POWER_TABLE_LEVELS][POWER_TABLE_RPM_BINS] = {};  ///< Power grid in watts (32 levels × 31 RPM bins)
    PowerTableTier tier = PowerTableTier::DEFAULT;                 ///< Provenance tier of this table
    char id[32] = "";   ///< Null-terminated table identifier string (e.g. "ex3-default")

    /// Reset all watts to 0, tier to DEFAULT, and clear id.
    void clear() {
        for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
            for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
                watts[l][r] = 0;
            }
        }
        tier = PowerTableTier::DEFAULT;
        id[0] = '\0';
    }

    /// Parse from CSV blob (level,rpm_0,rpm_5,...,rpm_150 format); return true on success.
    bool parseCSV(const char* csvData, size_t length) {
        if (!csvData || length == 0) {
            return false;
        }

        int16_t tempWatts[POWER_TABLE_LEVELS][POWER_TABLE_RPM_BINS] = {};
        uint32_t levelMask = 0;
        int validRows = 0;

        size_t i = 0;
        while (i < length) {
            // Find start and end of line
            size_t lineStart = i;
            while (i < length && csvData[i] != '\n' && csvData[i] != '\r') {
                i++;
            }
            size_t lineEnd = i;
            while (i < length && (csvData[i] == '\n' || csvData[i] == '\r')) {
                i++;
            }

            // Trim leading whitespace
            while (lineStart < lineEnd && (csvData[lineStart] == ' ' || csvData[lineStart] == '\t')) {
                lineStart++;
            }
            // Trim trailing whitespace
            while (lineEnd > lineStart && (csvData[lineEnd - 1] == ' ' || csvData[lineEnd - 1] == '\t')) {
                lineEnd--;
            }

            if (lineStart >= lineEnd) {
                continue; // Skip empty line
            }

            char firstChar = csvData[lineStart];
            if (firstChar == '#' || (firstChar >= 'A' && firstChar <= 'Z') || (firstChar >= 'a' && firstChar <= 'z')) {
                // Header or comment line
                continue;
            }

            // Parse: level, rpm_0, rpm_5, ..., rpm_150
            const char* ptr = &csvData[lineStart];
            const char* endPtr = &csvData[lineEnd];

            char* next = nullptr;
            long levelVal = std::strtol(ptr, &next, 10);
            if (ptr == next || levelVal < 1 || levelVal > POWER_TABLE_LEVELS) {
                return false;
            }
            ptr = next;

            int rowIdx = static_cast<int>(levelVal - 1);

            for (int bin = 0; bin < POWER_TABLE_RPM_BINS; ++bin) {
                while (ptr < endPtr && (*ptr == ' ' || *ptr == '\t')) ptr++;
                if (ptr >= endPtr || *ptr != ',') {
                    return false;
                }
                ptr++; // skip ','
                while (ptr < endPtr && (*ptr == ' ' || *ptr == '\t')) ptr++;
                if (ptr >= endPtr) {
                    return false;
                }
                long wattVal = std::strtol(ptr, &next, 10);
                if (ptr == next) {
                    return false;
                }
                ptr = next;
                if (wattVal < -32768 || wattVal > 32767) {
                    return false;
                }
                tempWatts[rowIdx][bin] = static_cast<int16_t>(wattVal);
            }

            while (ptr < endPtr && (*ptr == ' ' || *ptr == '\t')) ptr++;
            if (ptr < endPtr) {
                return false; // Trailing garbage on the line
            }

            if ((levelMask & (1UL << rowIdx)) != 0) {
                return false; // Duplicate level entry
            }

            levelMask |= (1UL << rowIdx);
            validRows++;
        }

        if (validRows == POWER_TABLE_LEVELS && levelMask == 0xFFFFFFFFUL) {
            for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
                for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
                    watts[l][r] = tempWatts[l][r];
                }
            }
            tier = PowerTableTier::SELECTED;
            return true;
        }

        return false;
    }

    /// Serialise table into a flat byte buffer for NVS storage; return bytes written or 0 on error.
    size_t serialise(uint8_t* buf, size_t bufLen) const {
        if (!buf || bufLen < SERIALISED_TABLE_SIZE) {
            return 0;
        }

        size_t offset = 0;
        // Magic (4 bytes, little-endian)
        buf[offset++] = static_cast<uint8_t>(POWER_TABLE_MAGIC & 0xFF);
        buf[offset++] = static_cast<uint8_t>((POWER_TABLE_MAGIC >> 8) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((POWER_TABLE_MAGIC >> 16) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((POWER_TABLE_MAGIC >> 24) & 0xFF);

        // Version (1 byte)
        buf[offset++] = POWER_TABLE_VERSION;

        // Tier (1 byte)
        buf[offset++] = static_cast<uint8_t>(tier);

        // Reserved (2 bytes)
        buf[offset++] = 0;
        buf[offset++] = 0;

        // ID (32 bytes, null-padded)
        std::memset(&buf[offset], 0, 32);
        std::strncpy(reinterpret_cast<char*>(&buf[offset]), id, 31);
        offset += 32;

        // Watts (32 levels × 31 bins × 2 bytes, little-endian)
        for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
            for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
                int16_t w = watts[l][r];
                buf[offset++] = static_cast<uint8_t>(w & 0xFF);
                buf[offset++] = static_cast<uint8_t>((w >> 8) & 0xFF);
            }
        }

        return offset;
    }

    /// Deserialise table from a flat byte buffer; return true if magic, version, and size match.
    bool deserialise(const uint8_t* buf, size_t bufLen) {
        if (!buf || bufLen < SERIALISED_TABLE_SIZE) {
            return false;
        }

        size_t offset = 0;
        uint32_t magic = static_cast<uint32_t>(buf[0]) |
                         (static_cast<uint32_t>(buf[1]) << 8) |
                         (static_cast<uint32_t>(buf[2]) << 16) |
                         (static_cast<uint32_t>(buf[3]) << 24);
        offset += 4;

        if (magic != POWER_TABLE_MAGIC) {
            return false;
        }

        uint8_t version = buf[offset++];
        if (version != POWER_TABLE_VERSION) {
            return false;
        }

        tier = static_cast<PowerTableTier>(buf[offset++]);
        offset += 2; // skip reserved 2 bytes

        std::memcpy(id, &buf[offset], 32);
        id[31] = '\0';
        offset += 32;

        for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
            for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
                uint16_t low = buf[offset++];
                uint16_t high = buf[offset++];
                int16_t w = static_cast<int16_t>(low | (high << 8));
                watts[l][r] = w;
            }
        }

        return true;
    }
};

} // namespace echenon
