#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/ResistanceSensor.h
 * Description: Potentiometer wiper ADC reader with exponential moving average (EMA) filtering.
 */

#include <cstdint>
#include <cstddef>

#if !defined(UNIT_TEST) && defined(BOARD_HEADER)
  #include BOARD_HEADER
#endif

#ifdef PIN_POT_WIPER
constexpr int8_t DEFAULT_POT_WIPER_PIN = PIN_POT_WIPER; ///< Default GPIO pin for potentiometer wiper ADC (from board.h)
#else
constexpr int8_t DEFAULT_POT_WIPER_PIN = 1;             ///< Fallback GPIO pin for potentiometer wiper ADC
#endif

namespace echenon {

/**
 * @brief Samples the resistance potentiometer wiper ADC and applies an EMA filter.
 */
class ResistanceSensor {
public:
    static constexpr float DEFAULT_ALPHA = 0.15f; ///< Exponential moving average smoothing factor (0 < alpha <= 1)

    /// Construct a ResistanceSensor bound to the given ADC-capable GPIO pin.
    explicit ResistanceSensor(int8_t pin = DEFAULT_POT_WIPER_PIN);
    ~ResistanceSensor() = default;

    /// Configure the wiper GPIO pin as INPUT.
    void begin();
    /// Sample the potentiometer ADC pin and update the EMA filter.
    void tick(uint32_t currentMillis);

    /// Return the rounded EMA-filtered 12-bit wiper ADC reading (0 before first sample).
    uint16_t getFilteredWiper() const;
    /// Return the unrounded EMA-filtered wiper reading as float (0.0f before first sample).
    float getFilteredWiperFloat() const { return _initialized ? _filteredWiper : 0.0f; }
    /// Return the most recent raw unfiltered ADC reading.
    uint16_t getRawWiper() const { return _rawWiper; }
    /// Return true if the filtered wiper lies within [WORKING_RANGE_MIN, WORKING_RANGE_MAX].
    bool isInWorkingRange() const;
    /// Return the configured GPIO pin number.
    int8_t getPin() const { return _pin; }

    /// Inject a synthetic 12-bit ADC sample for unit testing or calibration simulation.
    void injectSample(uint16_t adcValue);
    /// Reset filter state to uninitialized zero.
    void reset();
    /// Reset and pre-seed the EMA filter to initialFiltered.
    void reset(float initialFiltered);

private:
    void applySample(uint16_t sample);

    int8_t _pin;
    uint16_t _rawWiper = 0;
    float _filteredWiper = 0.0f;
    bool _initialized = false;
};

} // namespace echenon
