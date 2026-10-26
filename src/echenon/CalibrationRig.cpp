#ifdef WITH_CALIBRATION_RIG

#include "echenon/CalibrationRig.h"
#include "echenon/LevelMapping.h"
#include <cmath>
#include <cstdlib>

namespace echenon {

CalibrationRig::CalibrationRig()
    : _encoder(-1, -1) {
}

void CalibrationRig::begin(int8_t encA, int8_t encB, int8_t cadencePin) {
    _cadencePin = cadencePin;
    _encPinA = encA;
    _encPinB = encB;
    _encoder.begin(encA, encB);
#ifndef UNIT_TEST
    if (_cadencePin >= 0) {
        pinMode(_cadencePin, OUTPUT);
        digitalWrite(_cadencePin, LOW);   // NPN off = reed open (idle)
    }
#endif
    _cadencePinHigh = false;
    _state = State::IDLE;
    _currentLevel = 1;
    _targetLevel = 1;
    _stepsRemaining = 0;
    _cadenceRPM = 0;
    _cadencePeriodMs = 0;
    _lastCadenceToggle = 0;
    _lastStepMs = 0;
    _settling = false;
    _settleTimedOut = false;
    _settlePhaseStartMs = 0;
    _serialBuffer = "";
}

uint16_t CalibrationRig::expectedPot(uint8_t level) const {
    float exp = LevelMapping::B03_SLOPE * static_cast<float>(level) + LevelMapping::B03_INTERCEPT;
    if (exp < 0.0f) exp = 0.0f;
    return static_cast<uint16_t>(roundf(exp));
}

void CalibrationRig::handleSet(uint8_t targetLevel, uint16_t cadenceRPM) {
    if (targetLevel < 1) targetLevel = 1;
    if (targetLevel > 32) targetLevel = 32;

    _targetLevel = targetLevel;
    _cadenceRPM = cadenceRPM;
    if (_cadenceRPM > 0) {
        _cadencePeriodMs = 60000 / _cadenceRPM;
    } else {
        _cadencePeriodMs = 0;
    }
    _cadencePinHigh = false;
#ifndef UNIT_TEST
    _lastCadenceToggle = millis();  // prevent burst on first tick
    if (_cadencePin >= 0) {
        digitalWrite(_cadencePin, LOW);  // NPN off = reed open (idle)
    }
#endif

    if (_targetLevel > _currentLevel) {
        _stepsRemaining = (_targetLevel - _currentLevel) * STEPS_PER_LEVEL;
        _stepDirection = QuadratureGenerator::Direction::UP;    // UP = increase (bench-verified: pot rises with UP steps)
        _state = State::SEEKING;
        _settling = false;
        _settleTimedOut = false;
        _settlePhaseStartMs = 0;
    } else if (_targetLevel < _currentLevel) {
        _stepsRemaining = (_currentLevel - _targetLevel) * STEPS_PER_LEVEL;
        _stepDirection = QuadratureGenerator::Direction::DOWN;  // DOWN = decrease (bench-verified: pot falls with DOWN steps)
        _state = State::SEEKING;
        _settling = false;
        _settleTimedOut = false;
        _settlePhaseStartMs = 0;
    } else {
        _stepsRemaining = 0;
        _state = State::SEEKING;
        _settling = false;
        _settleTimedOut = false;
        _settlePhaseStartMs = 0;
    }
}

void CalibrationRig::handleHome() {
    _targetLevel = 1;
    // 31 levels × STEPS_PER_LEVEL = guaranteed full sweep to level 1
    _stepsRemaining = 31 * STEPS_PER_LEVEL;
    _stepDirection = QuadratureGenerator::Direction::DOWN;  // DOWN = decrease to level 1 (bench-verified)
    _state = State::SEEKING;
    _settling = false;
    _settleTimedOut = false;
    _settlePhaseStartMs = 0;
}

void CalibrationRig::handleStop() {
    _cadenceRPM = 0;
    _cadencePeriodMs = 0;
    _cadencePinHigh = false;
#ifndef UNIT_TEST
    if (_cadencePin >= 0) {
        digitalWrite(_cadencePin, LOW);   // NPN off = reed open (idle)
    }
#endif
    _stepsRemaining = 0;
    _settling = false;
    _settleTimedOut = false;
    _settlePhaseStartMs = 0;
    _state = State::IDLE;
}

String CalibrationRig::handleStatus() {
    const char* stateStr = "IDLE";
    switch (_state) {
        case State::IDLE:    stateStr = "IDLE"; break;
        case State::SEEKING: stateStr = "SEEKING"; break;
        case State::SETTLED: stateStr = "SETTLED"; break;
        case State::ERROR:   stateStr = "ERROR"; break;
    }
    uint8_t derivedLevel = LevelMapping::wiperToEchelon(_lastPotAdc);
    char buf[128];
    snprintf(buf, sizeof(buf), "STATUS state=%s level=%u target=%u derived=%u pot=%u cadence=%u",
             stateStr, _currentLevel, _targetLevel, derivedLevel, _lastPotAdc, _cadenceRPM);
    return String(buf);
}

void CalibrationRig::handleCommand(const String& cmd) {
    String line = cmd;
    line.trim();
    if (line.isEmpty()) return;

    if (line.startsWith("SET") || line.startsWith("set")) {
        int levelIdx = line.indexOf("level=");
        if (levelIdx < 0) levelIdx = line.indexOf("LEVEL=");
        int cadenceIdx = line.indexOf("cadence=");
        if (cadenceIdx < 0) cadenceIdx = line.indexOf("CADENCE=");

        if (levelIdx >= 0 && cadenceIdx >= 0) {
            int levelVal = line.substring(levelIdx + 6).toInt();
            int cadenceVal = line.substring(cadenceIdx + 8).toInt();

            if (levelVal >= 1 && levelVal <= 32 && cadenceVal >= 0 && cadenceVal <= 200) {
                handleSet(static_cast<uint8_t>(levelVal), static_cast<uint16_t>(cadenceVal));
#ifndef UNIT_TEST
                Serial.printf("SEEKING level=%u pot=%u\n", _targetLevel, _lastPotAdc);
#endif
            } else {
#ifndef UNIT_TEST
                Serial.println("ERROR invalid parameters");
#endif
            }
        } else {
#ifndef UNIT_TEST
            Serial.println("ERROR invalid parameters");
#endif
        }
    } else if (line.equalsIgnoreCase("HOME")) {
        handleHome();
#ifndef UNIT_TEST
        Serial.printf("SEEKING level=1 pot=%u\n", _lastPotAdc);
#endif
    } else if (line.equalsIgnoreCase("STATUS")) {
        String status = handleStatus();
#ifndef UNIT_TEST
        Serial.println(status);
#endif
    } else if (line.equalsIgnoreCase("STOP")) {
        handleStop();
#ifndef UNIT_TEST
        Serial.println("OK STOP");
#endif
    } else if (line.equalsIgnoreCase("DIAG")) {
#ifndef UNIT_TEST
        // Read back output pin states via digitalRead
        int encA_read = (_encPinA >= 0) ? digitalRead(_encPinA) : -1;
        int encB_read = (_encPinB >= 0) ? digitalRead(_encPinB) : -1;
        int cad_read  = (_cadencePin >= 0) ? digitalRead(_cadencePin) : -1;

        // Read passive ADC sensors: pot wiper (GPIO 1), cadence (GPIO 5), enc_vcc (GPIO 10)
        int pot_raw  = analogRead(1);   // ADC1_CH0 — pot wiper
        int cad_adc  = analogRead(5);   // ADC1_CH4 — cadence wire
        int enc_vcc  = analogRead(10);  // ADC1_CH9 — encoder VCC
        int encA_adc = analogRead(8);   // ADC1_CH7 — encoder A sense
        int encB_adc = analogRead(9);   // ADC1_CH8 — encoder B sense

        Serial.printf("DIAG enc_pinA=%d(%s) enc_pinB=%d(%s) cad_pin=%d(%s) "
                       "qstate=%u steps=%ld "
                       "pot_raw=%d cad_adc=%d enc_vcc=%d encA_adc=%d encB_adc=%d "
                       "pot_filt=%u\n",
                       _encPinA, (encA_read == 1 ? "HIGH" : encA_read == 0 ? "LOW" : "NC"),
                       _encPinB, (encB_read == 1 ? "HIGH" : encB_read == 0 ? "LOW" : "NC"),
                       _cadencePin, (cad_read == 1 ? "HIGH" : cad_read == 0 ? "LOW" : "NC"),
                       _encoder.getState(), (long)_encoder.getTotalSteps(),
                       pot_raw, cad_adc, enc_vcc, encA_adc, encB_adc,
                       _lastPotAdc);
#endif
    } else if (line.startsWith("STEP") || line.startsWith("step")) {
#ifndef UNIT_TEST
        // Manual stepping: STEP UP [count] [delay_us]
        // count: number of edges (default 1, max 1024)
        // delay_us: microseconds between edges (default 0, max 100000)
        // e.g. "STEP UP 192 1000" = 192 edges with 1ms between each
        String rest = line.substring(4);
        rest.trim();
        QuadratureGenerator::Direction dir = QuadratureGenerator::Direction::DOWN;
        int count = 1;
        int delay_us = 0;

        // Parse direction
        String afterDir;
        if (rest.startsWith("UP") || rest.startsWith("up")) {
            dir = QuadratureGenerator::Direction::UP;
            afterDir = rest.substring(2);
        } else if (rest.startsWith("DOWN") || rest.startsWith("down")) {
            dir = QuadratureGenerator::Direction::DOWN;
            afterDir = rest.substring(4);
        }
        afterDir.trim();

        // Parse count and optional delay_us
        if (afterDir.length() > 0) {
            int spaceIdx = afterDir.indexOf(' ');
            if (spaceIdx > 0) {
                // Two numbers: count delay_us
                int parsed = afterDir.substring(0, spaceIdx).toInt();
                if (parsed > 0 && parsed <= 1024) count = parsed;
                String delayStr = afterDir.substring(spaceIdx + 1);
                delayStr.trim();
                int parsedDelay = delayStr.toInt();
                if (parsedDelay >= 0 && parsedDelay <= 100000) delay_us = parsedDelay;
            } else {
                // Single number: count
                int parsed = afterDir.toInt();
                if (parsed > 0 && parsed <= 1024) count = parsed;
            }
        }

        uint32_t stepStart = micros();
        for (int i = 0; i < count; i++) {
            _encoder.step(dir);
            if (delay_us > 0 && i < count - 1) {
                delayMicroseconds(delay_us);
            }
        }
        uint32_t stepDuration = micros() - stepStart;

        int encA_read = (_encPinA >= 0) ? digitalRead(_encPinA) : -1;
        int encB_read = (_encPinB >= 0) ? digitalRead(_encPinB) : -1;
        Serial.printf("STEP dir=%s count=%d delay_us=%d duration_us=%u qstate=%u steps=%ld pinA=%d(%s) pinB=%d(%s)\n",
                       (dir == QuadratureGenerator::Direction::UP ? "UP" : "DOWN"),
                       count, delay_us, stepDuration,
                       _encoder.getState(), (long)_encoder.getTotalSteps(),
                       _encPinA, (encA_read == 1 ? "HIGH" : encA_read == 0 ? "LOW" : "NC"),
                       _encPinB, (encB_read == 1 ? "HIGH" : encB_read == 0 ? "LOW" : "NC"));
#endif
    } else {
#ifndef UNIT_TEST
        Serial.println("ERROR unknown command");
#endif
    }
}

void CalibrationRig::processSerialInput() {
#ifndef UNIT_TEST
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            handleCommand(_serialBuffer);
            _serialBuffer = "";
        } else {
            if (_serialBuffer.length() < 128) {
                _serialBuffer += c;
            }
        }
    }
#endif
}

void CalibrationRig::tick(uint32_t currentMillis, uint16_t potAdc) {
    _lastPotAdc = potAdc;

    // 1. Process serial commands
    processSerialInput();

    // 2. Cadence simulation (NPN: LOW=idle/open, HIGH=pulse/closed)
    if (_cadenceRPM > 0 && _cadencePeriodMs > CADENCE_PULSE_MS) {
        uint32_t elapsed = currentMillis - _lastCadenceToggle;
        if (!_cadencePinHigh) {
            // Idle (LOW / reed open) — wait for next pulse
            if (elapsed >= (_cadencePeriodMs - CADENCE_PULSE_MS)) {
                _cadencePinHigh = true;
                _lastCadenceToggle = currentMillis;
#ifndef UNIT_TEST
                if (_cadencePin >= 0) {
                    digitalWrite(_cadencePin, HIGH);  // NPN on = reed closed
                }
#endif
            }
        } else {
            // Pulse (HIGH / reed closed) — hold for CADENCE_PULSE_MS then release
            if (elapsed >= CADENCE_PULSE_MS) {
                _cadencePinHigh = false;
                _lastCadenceToggle = currentMillis;
#ifndef UNIT_TEST
                if (_cadencePin >= 0) {
                    digitalWrite(_cadencePin, LOW);   // NPN off = reed open
                }
#endif
            }
        }
    } else {
        if (_cadencePinHigh) {
            _cadencePinHigh = false;
#ifndef UNIT_TEST
            if (_cadencePin >= 0) {
                digitalWrite(_cadencePin, LOW);       // NPN off = reed open
            }
#endif
        }
    }

    // 3. Encoder stepping & Pot Settle monitoring
    if (_state == State::SEEKING) {
        if (_stepsRemaining > 0) {
            if (currentMillis - _lastStepMs >= STEP_INTERVAL_MS) {
                _lastStepMs = currentMillis;
                _encoder.step(_stepDirection);
                _stepsRemaining--;
                if (_stepsRemaining == 0) {
                    _currentLevel = _targetLevel;
                    if (_currentLevel == 1) {
                        _encoder.reset();
                    }
                    _settling = false;
                }
            }
        } else {
            // Encoder stepping complete: monitor pot settle
            if (!_settling && _settlePhaseStartMs == 0) {
                _settlePhaseStartMs = currentMillis;  // mark when settle phase began
            }

            // Check for settle timeout (30s)
            if (_settlePhaseStartMs > 0 && (currentMillis - _settlePhaseStartMs >= SETTLE_TIMEOUT_MS)) {
                _state = State::SETTLED;
                _settling = false;
                _settleTimedOut = true;
#ifndef UNIT_TEST
                Serial.printf("SETTLED level=%u pot=%u cadence=%u TIMEOUT\n", _currentLevel, potAdc, _cadenceRPM);
#endif
                return;  // early return — don't check pot
            }

            uint16_t exp = expectedPot(_targetLevel);
            int32_t diff = std::abs(static_cast<int32_t>(potAdc) - static_cast<int32_t>(exp));
            if (diff <= SETTLE_TOLERANCE) {
                if (!_settling) {
                    _settling = true;
                    _settleStartMs = currentMillis;
                } else if (currentMillis - _settleStartMs >= SETTLE_DURATION_MS) {
                    _state = State::SETTLED;
                    _settling = false;
#ifndef UNIT_TEST
                    Serial.printf("SETTLED level=%u pot=%u cadence=%u\n", _currentLevel, potAdc, _cadenceRPM);
#endif
                }
            } else {
                _settling = false;
            }
        }
    }
}

} // namespace echenon

#endif // WITH_CALIBRATION_RIG
