#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/QuadratureGenerator.h
 * Description: Two-channel Gray-code quadrature encoder waveform generator for the calibration rig.
 */

#include <cstdint>
#include <cstddef>

namespace echenon {

/**
 * @brief Generates 2-bit Gray-code quadrature waveforms on two GPIO pins
 *        to simulate console resistance knob rotation for the calibration rig.
 */
class QuadratureGenerator {
public:
    /// Quadrature stepping direction.
    enum class Direction {
        UP,   ///< Increment resistance level (clockwise)
        DOWN  ///< Decrement resistance level (counter-clockwise)
    };

    /// Construct generator bound to two GPIO pins (-1 means unconfigured / test mode).
    explicit QuadratureGenerator(int8_t pinA = -1, int8_t pinB = -1);

    /// Configure assigned GPIOs as OUTPUT and drive initial quadrature state.
    void begin();
    /// Assign GPIO pins, configure as OUTPUT, and drive initial quadrature state.
    void begin(int8_t pinA, int8_t pinB);
    /// Update the GPIO pin numbers without driving outputs.
    void setPins(int8_t pinA, int8_t pinB) { _pinA = pinA; _pinB = pinB; }

    /// Advance one quadrature state (one edge); call at ~50 ms intervals for ~200 ms per detent.
    void step(Direction dir);

    /// Return the current 2-bit Gray-code quadrature state (0-3).
    uint8_t getState() const { return _state; }

    /// Return the signed net step count since begin() or reset().
    int32_t getTotalSteps() const { return _totalSteps; }

    /// Reset quadrature state and step count to 0 and apply outputs.
    void reset();

    /// Compute the expected (pinA, pinB) logic levels for a given 0-3 quadrature state.
    static void getPinLevels(uint8_t state, bool& pinA, bool& pinB);

private:
    void applyState();  // Write current state to GPIOs

    int8_t _pinA = -1;
    int8_t _pinB = -1;
    uint8_t _state = 0;       // 0-3 Gray code state
    int32_t _totalSteps = 0;
};

} // namespace echenon
