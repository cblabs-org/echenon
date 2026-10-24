#pragma once

#include <cstdint>
#include <cstddef>

namespace echenon {

class QuadratureGenerator {
public:
    enum class Direction { UP, DOWN };

    // Constructor takes two GPIO pins (-1 means unconfigured / test mode)
    explicit QuadratureGenerator(int8_t pinA = -1, int8_t pinB = -1);

    void begin();  // Configure GPIOs as OUTPUT
    void begin(int8_t pinA, int8_t pinB);
    void setPins(int8_t pinA, int8_t pinB) { _pinA = pinA; _pinB = pinB; }

    // Advance one quadrature state (one edge)
    // Call repeatedly at ~50 ms intervals for ~200 ms per detent
    void step(Direction dir);

    // Get current quadrature state (0-3)
    uint8_t getState() const { return _state; }

    // Get total step count since begin
    int32_t getTotalSteps() const { return _totalSteps; }

    // Reset state to 0
    void reset();

    // Test helper: get the expected pin levels for a given state
    static void getPinLevels(uint8_t state, bool& pinA, bool& pinB);

private:
    void applyState();  // Write current state to GPIOs

    int8_t _pinA = -1;
    int8_t _pinB = -1;
    uint8_t _state = 0;       // 0-3 Gray code state
    int32_t _totalSteps = 0;
};

} // namespace echenon
