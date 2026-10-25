#pragma once

#include <cstdint>
#include <cstddef>

#if !defined(UNIT_TEST) && defined(BOARD_HEADER)
  #include BOARD_HEADER
#endif

#ifdef PIN_CADENCE
constexpr int8_t DEFAULT_CADENCE_PIN = PIN_CADENCE;
#else
constexpr int8_t DEFAULT_CADENCE_PIN = 5;
#endif

namespace echenon {

class CadenceSensor {
public:
    static constexpr uint32_t DEBOUNCE_LOCKOUT_US = 25000;   // 25 ms
    static constexpr uint32_t CONFIRM_LOW_US      = 5000;    // 5 ms post-edge LOW confirmation
    static constexpr uint32_t REARM_HIGH_US       = 50000;   // 50 ms continuous HIGH open-release hysteresis
    static constexpr uint32_t MIN_HW_INTERVAL_US  = 250000;  // 250 ms (240 RPM) hardware lockout
    static constexpr uint32_t TIMEOUT_US          = 3000000; // 3 s (3,000,000 µs)
    static constexpr uint32_t TIMEOUT_MS          = 3000;    // 3 s (3,000 ms)
    static constexpr float    MAX_RPM             = 200.0f;

    explicit CadenceSensor(int8_t pin = DEFAULT_CADENCE_PIN);
    ~CadenceSensor();

    void begin();
    void tick(uint32_t currentMillis);

    float getRPM() const { return _rpm; }
    uint32_t getLastPulseInterval() const { return _lastIntervalUs; }
    bool isActive() const { return _rpm > 0.0f; }
    int8_t getPin() const { return _pin; }

    // Hardware two-state closed-hold confirmation + open-release hysteresis state machine
    void onFallingEdgeISR(uint32_t nowMicros);
    void processHardwareState(uint32_t currentMicros, bool pinIsLow);

    // Test / event injection
    void injectPulse(uint32_t pulseMicros);
    void reset();

#if !defined(UNIT_TEST) && defined(ARDUINO)
    static void handleISR();
#endif

private:
    void handlePulse(uint32_t pulseMicros);

    int8_t _pin;
    void* _glitchFilter = nullptr;
    volatile uint32_t _lastPulseMicros = 0;
    volatile uint32_t _lastPulseMs = 0;
    volatile uint32_t _lastIntervalUs = 0;
    volatile bool _newPulse = false;
    volatile bool _hasFirstPulse = false;
    volatile uint32_t _pendingPulseMicros = 0;
    volatile bool _hasPendingPulse = false;
    volatile bool _waitingForOpen = false;
    bool _highTimingActive = false;
    uint32_t _highStableStartMicros = 0;
    float _rpm = 0.0f;
};

} // namespace echenon
