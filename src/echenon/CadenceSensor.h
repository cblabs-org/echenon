#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/CadenceSensor.h
 * Description: Reed-switch cadence sensor driver with ISR leading-edge capture, hardware lockout, and open-release hysteresis.
 */

#include <cstdint>
#include <cstddef>

#if !defined(UNIT_TEST) && defined(BOARD_HEADER)
  #include BOARD_HEADER
#endif

#ifdef PIN_CADENCE
constexpr int8_t DEFAULT_CADENCE_PIN = PIN_CADENCE; ///< Default GPIO pin for reed-switch cadence input (from board.h)
#else
constexpr int8_t DEFAULT_CADENCE_PIN = 5;           ///< Fallback GPIO pin for reed-switch cadence input
#endif

namespace echenon {

/**
 * @brief Measures crank cadence (RPM) from reed-switch falling-edge pulses
 *        with software debounce, hardware lockout, and open-release hysteresis.
 */
class CadenceSensor {
public:
    static constexpr uint32_t DEBOUNCE_LOCKOUT_US = 25000;   ///< Software contact-bounce lockout (25 ms)
    static constexpr uint32_t REARM_HIGH_US       = 50000;   ///< Continuous HIGH open-release hysteresis (50 ms)
    static constexpr uint32_t MIN_HW_INTERVAL_US  = 250000;  ///< Minimum hardware edge lockout (250 ms, 240 RPM ceiling)
    static constexpr uint32_t TIMEOUT_US          = 3000000; ///< Maximum inter-pulse period in microseconds (3 s)
    static constexpr uint32_t TIMEOUT_MS          = 3000;    ///< Inactivity timeout in milliseconds before zeroing RPM (3 s)
    static constexpr float    MAX_RPM             = 200.0f;  ///< Upper sanity clamp for computed cadence (RPM)

    /// Construct a CadenceSensor bound to the given GPIO pin.
    explicit CadenceSensor(int8_t pin = DEFAULT_CADENCE_PIN);
    /// Detach interrupt and release hardware glitch-filter resources.
    ~CadenceSensor();

    /// Configure GPIO pin, optional hardware glitch filter, and falling-edge ISR.
    void begin();
    /// Update open-release hysteresis and compute RPM or apply inactivity timeout.
    void tick(uint32_t currentMillis);

    /// Return the current computed crank cadence in RPM.
    float getRPM() const { return _rpm; }
    /// Return the most recent inter-pulse interval in microseconds.
    uint32_t getLastPulseInterval() const { return _lastIntervalUs; }
    /// Return true while cadence is non-zero.
    bool isActive() const { return _rpm > 0.0f; }
    /// Return the configured GPIO pin number.
    int8_t getPin() const { return _pin; }

    /// Hardware immediate leading-edge ISR handler with 250 ms lockout and open-release latch.
    void onFallingEdgeISR(uint32_t nowMicros);
    /// Advance open-release hysteresis state using sampled GPIO polarity.
    void processHardwareState(uint32_t currentMicros, bool pinIsLow);

    /// Inject a synthetic pulse timestamp (in microseconds) for unit tests.
    void injectPulse(uint32_t pulseMicros);
    /// Reset all pulse timing, hysteresis flags, and RPM state.
    void reset();

#if !defined(UNIT_TEST) && defined(ARDUINO)
    /// Static Arduino ISR trampoline forwarding to the active CadenceSensor instance.
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
    volatile bool _waitingForOpen = false;
    bool _highTimingActive = false;
    uint32_t _highStableStartMicros = 0;
    float _rpm = 0.0f;
};

} // namespace echenon
