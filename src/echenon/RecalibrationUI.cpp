#include "echenon/RecalibrationUI.h"
#include "echenon/ResistanceSensor.h"
#include "echenon/CadenceSensor.h"
#include "echenon/LevelMapping.h"
#include <cmath>

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
