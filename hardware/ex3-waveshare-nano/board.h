// hardware/ex3-waveshare-nano/board.h
#pragma once
#include "board_pins.h"

// ── Identity & capabilities (framework template fields) ──
#define BOARD_NAME        "EX-3 on Waveshare ESP32-S3-Nano"
#define BOARD_ID_STRING   "ex3-waveshare-nano"
constexpr bool HAS_PSRAM = true;

// ── Bike signals (HAS_*) and pins: Phase 0/1 map, SPEC §4.4.1 ──
// Every tap: series resistor, INPUT with no internal pull-up or pull-down.
#define HAS_RESISTANCE_POT   1
constexpr int8_t PIN_POT_WIPER        = 1;   // A0  Blue,   ADC1
constexpr int8_t PIN_POT_HI           = 2;   // A1  Yellow, ADC1
constexpr int8_t PIN_POT_LO           = 3;   // A2  Green,  ADC1 (strapping pin, §4.4.1)

#define HAS_CADENCE          1
constexpr int8_t PIN_CADENCE_RETURN   = 4;   // A3  Grey,   ADC1
constexpr int8_t PIN_CADENCE          = 5;   // D2  Orange, ADC1 / interrupt

#define HAS_MOTOR_SENSE      1
constexpr int8_t PIN_MOTOR_SENSE_P    = 6;   // D3  Red,   ADC1 via ÷4 divider
constexpr int8_t PIN_MOTOR_SENSE_N    = 7;   // D4  Black, ADC1 via ÷4 divider

#define HAS_KNOB_ENCODER     1
constexpr int8_t PIN_KNOB_ENC_A       = 8;   // D5  Black,  ADC1 (J3 left pin)
constexpr int8_t PIN_KNOB_ENC_B       = 9;   // D6  Red,    ADC1 (J3 right pin)
constexpr int8_t PIN_KNOB_ENC_VCC     = 10;  // D7  Orange, ADC1 (J3 centre pin, Phase 0/1 only)

#ifdef WITH_ENCODER
#include "EncoderModule/EncoderConfig.h"
constexpr BoardEncoder BOARD_ENCODERS[] = {
    {.id = "knob", .pinA = PIN_KNOB_ENC_A, .pinB = PIN_KNOB_ENC_B,
     .stepsPerDetent = 4, .reversed = false,
     .accel = {.enabled = false}}
};
constexpr size_t BOARD_ENCODER_COUNT =
    sizeof(BOARD_ENCODERS) / sizeof(BOARD_ENCODERS[0]);
#endif

#define HAS_MOTOR_CONTROL    0               // MITM board only (Phase 5+)

#ifdef WITH_CALIBRATION_RIG
// Calibration rig output pins (owner decision 2026-10-18)
constexpr int8_t PIN_RIG_ENC_A    = 17;  // D8  → console encoder Ch A
constexpr int8_t PIN_RIG_ENC_B    = 18;  // D12 → console encoder Ch B
constexpr int8_t PIN_RIG_CADENCE  = 11;  // D9  → cadence MOSFET gate
#endif

// No display pin block and no button block: WITH_DISPLAY_U8G2 and
// WITH_BUTTONS are forbidden for this target (§12.3).
