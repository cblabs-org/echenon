#pragma once

#include <cstdint>
#include <cstddef>

#if !defined(UNIT_TEST) && defined(BOARD_HEADER)
  #include BOARD_HEADER
#endif

#ifdef PIN_POT_WIPER
constexpr int8_t DEFAULT_POT_WIPER_PIN = PIN_POT_WIPER;
#else
constexpr int8_t DEFAULT_POT_WIPER_PIN = 1;
#endif

namespace echenon {

class ResistanceSensor {
public:
    static constexpr float DEFAULT_ALPHA = 0.15f;

    explicit ResistanceSensor(int8_t pin = DEFAULT_POT_WIPER_PIN);
    ~ResistanceSensor() = default;

    void begin();
    void tick(uint32_t currentMillis);

    uint16_t getFilteredWiper() const;
    uint16_t getRawWiper() const { return _rawWiper; }
    bool isInWorkingRange() const;
    int8_t getPin() const { return _pin; }

    // Test / calibration injection
    void injectSample(uint16_t adcValue);
    void reset();
    void reset(float initialFiltered);

private:
    void applySample(uint16_t sample);

    int8_t _pin;
    uint16_t _rawWiper = 0;
    float _filteredWiper = 0.0f;
    bool _initialized = false;
};

} // namespace echenon
