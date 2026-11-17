/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/RecalibrationUI.cpp
 * Description: Implementation of rider power-point storage, least-squares Physics P3r scale fitting, and table updates.
 */

#include "echenon/RecalibrationUI.h"
#include "echenon/ResistanceSensor.h"
#include "echenon/CadenceSensor.h"
#include "echenon/LevelMapping.h"
#include <cmath>
#include <algorithm>

#if defined(ARDUINO)
#include <Arduino.h>
#elif defined(UNIT_TEST)
#include "Arduino.h"
#endif

namespace echenon {

RecalibrationUI::RecalibrationUI(ResistanceSensor* rs, CadenceSensor* cs)
    : _resistanceSensor(rs),
      _cadenceSensor(cs) {
    _points.reserve(MAX_POINTS);
}

int RecalibrationUI::getRpmBin(float rpm) {
    if (rpm < 0.0f) {
        rpm = 0.0f;
    }
    int bin = static_cast<int>(std::round(rpm / POWER_TABLE_RPM_STEP));
    if (bin >= POWER_TABLE_RPM_BINS) {
        bin = POWER_TABLE_RPM_BINS - 1;
    }
    return bin;
}

bool RecalibrationUI::addPoint(uint8_t echelonLevel, float reportedPower) {
    uint16_t wiper = _resistanceSensor ? _resistanceSensor->getFilteredWiper() : 0;
    uint8_t internalLevel = LevelMapping::wiperToInternal(wiper);
    float cadence = _cadenceSensor ? _cadenceSensor->getRPM() : 0.0f;
    uint32_t ts = 0;
#if defined(ARDUINO) || defined(UNIT_TEST)
    ts = millis();
#endif
    return addPoint(echelonLevel, reportedPower, internalLevel, cadence, wiper, ts);
}

bool RecalibrationUI::addPoint(uint8_t echelonLevel, float reportedPower,
                              uint8_t internalLevel, float cadenceRPM,
                              uint16_t wiperCount, uint32_t timestamp) {
    if (echelonLevel < 1 || echelonLevel > POWER_TABLE_LEVELS) {
        return false;
    }
    if (reportedPower < 0.0f) {
        return false;
    }

    int newBin = getRpmBin(cadenceRPM);

    // Check if point already exists at the same (level, RPM bin)
    for (auto& pt : _points) {
        if (pt.echelonLevel == echelonLevel && getRpmBin(pt.cadenceRPM) == newBin) {
            // Replace existing point with new value (simple replacement rule for v1)
            pt.reportedPower = reportedPower;
            pt.internalLevel = internalLevel;
            pt.cadenceRPM = cadenceRPM;
            pt.wiperCount = wiperCount;
            pt.timestamp = timestamp;
            return true;
        }
    }

    if (_points.size() >= MAX_POINTS) {
        return false;
    }

    RecalPoint pt;
    pt.echelonLevel = echelonLevel;
    pt.reportedPower = reportedPower;
    pt.internalLevel = internalLevel;
    pt.cadenceRPM = cadenceRPM;
    pt.wiperCount = wiperCount;
    pt.timestamp = timestamp;
    _points.push_back(pt);
    return true;
}

bool RecalibrationUI::applyToPhysicsParams(PowerPhysicsParams& params) const {
    float sumBY = 0.0f;
    float sumBB = 0.0f;
    bool hasValidPoint = false;

    for (const auto& pt : _points) {
        if (!std::isfinite(pt.cadenceRPM) || !std::isfinite(pt.reportedPower) ||
            pt.cadenceRPM <= 0.0f || pt.reportedPower <= 0.0f) {
            continue;
        }

        float pTilde = 0.0f;
        if (pt.wiperCount > 0 && std::isfinite(params.pot_min) &&
            std::isfinite(params.pot_max) && params.pot_max > params.pot_min) {
            pTilde = std::clamp(
                (static_cast<float>(pt.wiperCount) - params.pot_min) /
                    (params.pot_max - params.pot_min),
                0.0f, 1.0f);
        } else {
            pTilde = std::clamp(
                (static_cast<float>(pt.echelonLevel) - 1.0f) / 31.0f,
                0.0f, 1.0f);
        }

        const float u = 1.0f - std::cos(params.cam_sweep_rad * pTilde);
        const float r = pt.cadenceRPM / params.cad_crit;
        const float B = std::exp(params.beta * u) *
                        ((pt.cadenceRPM * pt.cadenceRPM) / (1.0f + r * r));
        if (!std::isfinite(B) || B <= 0.0f) {
            continue;
        }

        const float Y = std::max(pt.reportedPower - params.c_fric * pt.cadenceRPM, 0.0f);
        sumBY += B * Y;
        sumBB += B * B;
        hasValidPoint = true;
    }

    if (!hasValidPoint || sumBB <= 0.0f) {
        params.a_scale = PowerEngine::A;
        return false;
    }

    const float fittedA = sumBY / sumBB;
    params.a_scale = std::clamp(fittedA, 0.25f * PowerEngine::A, 4.0f * PowerEngine::A);
    return true;
}

void RecalibrationUI::applyToPowerTable(PowerTable& table) const {
    for (const auto& pt : _points) {
        if (pt.echelonLevel >= 1 && pt.echelonLevel <= POWER_TABLE_LEVELS) {
            int levelIdx = pt.echelonLevel - 1;
            int bin = getRpmBin(pt.cadenceRPM);
            if (bin >= 0 && bin < POWER_TABLE_RPM_BINS) {
                table.watts[levelIdx][bin] = static_cast<int16_t>(std::round(pt.reportedPower));
            }
        }
    }
    if (!_points.empty()) {
        table.tier = PowerTableTier::RECALIBRATED;
    }
}

} // namespace echenon
