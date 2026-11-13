#pragma once

/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/SignalLogic.h
 * Description: Pure signal-processing primitives for ADC sample packing, burst de-interleaving, motor FSM, decimation, and CSV formatting.
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace echenon {

/// Scanned ADC channels: 0-6 (pot, cadence, motor) + 9 (encoder VCC); channels 7,8 reserved for PCNT EncoderModule.
constexpr uint8_t kScanChannels[8] = {0, 1, 2, 3, 4, 5, 6, 9};
/// Total number of scanned ADC channels.
constexpr size_t kScanChannelCount = sizeof(kScanChannels) / sizeof(kScanChannels[0]);

/// Map physical ADC channel to logical scan slot (0..7), or -1 if not scanned.
constexpr int channelToSlot(uint8_t channel) {
    switch (channel) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        case 6: return 6;
        case 9: return 7;
        default: return -1;
    }
}

/// Map logical scan slot (0..7) to physical ADC channel, or 0xFF if out of range.
constexpr uint8_t slotToChannel(size_t slot) {
    return (slot < kScanChannelCount) ? kScanChannels[slot] : 0xFF;
}

/// Signal names for the 8 scanned channels matching board.h pins.
constexpr const char* kChannelNames[8] = {
    "pot_wiper",    // ch 0 (PIN_POT_WIPER = 1)
    "pot_hi",       // ch 1 (PIN_POT_HI = 2)
    "pot_lo",       // ch 2 (PIN_POT_LO = 3)
    "cadence_ret",  // ch 3 (PIN_CADENCE_RETURN = 4)
    "cadence",      // ch 4 (PIN_CADENCE = 5)
    "motor_p",      // ch 5 (PIN_MOTOR_SENSE_P = 6)
    "motor_n",      // ch 6 (PIN_MOTOR_SENSE_N = 7)
    "enc_vcc"       // ch 9 (PIN_KNOB_ENC_VCC = 10)
};

// ── Sample Packing / Unpacking (Option c: channel:4 | data:12 into uint16_t) ──

/// Pack a 4-bit ADC channel index and 12-bit sample value into a single uint16_t.
constexpr uint16_t packSample(uint8_t channel, uint16_t data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(channel & 0x0F) << 12) | (data & 0x0FFF));
}

/// Extract the 4-bit physical ADC channel number from a packed uint16_t sample.
constexpr uint8_t unpackChannel(uint16_t sample) {
    return static_cast<uint8_t>((sample >> 12) & 0x0F);
}

/// Extract the 12-bit ADC sample value from a packed uint16_t sample.
constexpr uint16_t unpackData(uint16_t sample) {
    return static_cast<uint16_t>(sample & 0x0FFF);
}

// ── Timestamp Calculation ──

/// Compute per-sample microsecond timestamp from frame start, sample index, and aggregate sample rate.
constexpr uint64_t calculateSampleTimestampUs(uint64_t frameStartTsUs, size_t sampleIndex, uint32_t sampleFreqHz) {
    return frameStartTsUs + (static_cast<uint64_t>(sampleIndex) * 1000000ULL / sampleFreqHz);
}

// ── Burst De-interleave ──

/// One time-aligned scan frame across all 8 logical ADC channel slots.
struct DeinterleavedRow {
    uint16_t channels[8]; ///< Channel values mapped to slots 0..7 (channels {0, 1, 2, 3, 4, 5, 6, 9})
};

/// Stream-deinterleave packed ADC samples into frame-aligned rows, invoking emitRow(idx, row) for each.
template <typename F>
size_t deinterleaveBurstStream(const uint16_t* packed, size_t count, F&& emitRow) {
    if (!packed || count == 0) return 0;

    // Discard leading samples before the first channel 0 to align frame start
    size_t start = 0;
    while (start < count && unpackChannel(packed[start]) != 0) {
        start++;
    }
    if (start >= count) return 0;

    DeinterleavedRow curRow{};
    bool slotFilled[8] = {false};
    int lastSlot = -1;
    size_t rowsEmitted = 0;

    for (size_t i = start; i < count; i++) {
        uint8_t ch = unpackChannel(packed[i]);
        uint16_t val = unpackData(packed[i]);
        int slot = channelToSlot(ch);
        if (slot < 0) continue;

        if (slot <= lastSlot || slotFilled[slot]) {
            emitRow(rowsEmitted++, curRow);
            for (int s = 0; s < 8; s++) {
                slotFilled[s] = false;
            }
            lastSlot = -1;
        }

        curRow.channels[slot] = val;
        slotFilled[slot] = true;
        lastSlot = slot;
    }

    bool anyFilled = false;
    for (int s = 0; s < 8; s++) {
        if (slotFilled[s]) { anyFilled = true; break; }
    }
    if (anyFilled) {
        emitRow(rowsEmitted++, curRow);
    }
    return rowsEmitted;
}

/// De-interleave a packed burst buffer into a vector of DeinterleavedRow frames.
inline size_t deinterleaveBurst(const uint16_t* packed, size_t count, std::vector<DeinterleavedRow>& outRows) {
    outRows.clear();
    return deinterleaveBurstStream(packed, count, [&outRows](size_t /*idx*/, const DeinterleavedRow& row) {
        outRows.push_back(row);
    });
}

// ── Motor State Machine ──

/// H-bridge motor drive states inferred from motor sense P and N channels.
enum class MotorDirection : uint8_t {
    IDLE = 0,    ///< Coast / idle (P LOW, N LOW)
    FORWARD = 1, ///< Forward drive (P HIGH, N LOW)
    REVERSE = 2, ///< Reverse drive (P LOW, N HIGH)
    BRAKE = 3    ///< Active brake (both HIGH)
};

/**
 * @brief Hysteresis and time-debounced state machine decoding H-bridge motor
 *        direction from motor sense ADC channels 5 (P) and 6 (N).
 */
class MotorStateMachine {
public:
    static constexpr uint16_t kDefaultOnsetThresholdMv = 500;  ///< Default HIGH onset threshold (mV)
    static constexpr uint16_t kDefaultOffsetThresholdMv = 400; ///< Default LOW offset threshold with 100 mV hysteresis (mV)
    static constexpr uint64_t kDefaultDebounceUs = 5000;       ///< Default direction-change debounce duration (5 ms)

    /// Construct MotorStateMachine with hysteresis thresholds and debounce period.
    MotorStateMachine(uint16_t onsetMv = kDefaultOnsetThresholdMv,
                      uint16_t offsetMv = kDefaultOffsetThresholdMv,
                      uint64_t debounceUs = kDefaultDebounceUs)
        : _onsetMv(onsetMv), _offsetMv(offsetMv), _debounceUs(debounceUs) {}

    /// Process a millivolt sample for channel 5 or 6 and invoke callback on debounced direction changes.
    template <typename Callback>
    void processSample(uint8_t channel, uint16_t mv, uint64_t ts_us, Callback&& onDirectionChange) {
        if (channel != 5 && channel != 6) return;

        // 1. Update per-channel state with hysteresis
        if (channel == 5) {
            if (!_motorPState && mv >= _onsetMv) {
                _motorPState = true;
            } else if (_motorPState && mv <= _offsetMv) {
                _motorPState = false;
            }
        } else if (channel == 6) {
            if (!_motorNState && mv >= _onsetMv) {
                _motorNState = true;
            } else if (_motorNState && mv <= _offsetMv) {
                _motorNState = false;
            }
        }

        // 2. Derive instantaneous direction
        MotorDirection instantDir = MotorDirection::IDLE;
        if (_motorPState && !_motorNState) {
            instantDir = MotorDirection::FORWARD;
        } else if (!_motorPState && _motorNState) {
            instantDir = MotorDirection::REVERSE;
        } else if (_motorPState && _motorNState) {
            instantDir = MotorDirection::BRAKE;
        } else {
            instantDir = MotorDirection::IDLE;
        }

        // 3. Debounce filter
        if (instantDir != _currentDirection) {
            if (instantDir != _pendingDirection) {
                _pendingDirection = instantDir;
                _pendingSinceUs = ts_us;
            } else {
                if (ts_us >= _pendingSinceUs && (ts_us - _pendingSinceUs) >= _debounceUs) {
                    _currentDirection = _pendingDirection;
                    onDirectionChange(_currentDirection, ts_us);
                }
            }
        } else {
            _pendingDirection = _currentDirection;
            _pendingSinceUs = ts_us;
        }
    }

    /// Return the current debounced motor direction.
    MotorDirection getCurrentDirection() const { return _currentDirection; }
    /// Return the current latched HIGH/LOW state of motor sense P.
    bool getMotorPState() const { return _motorPState; }
    /// Return the current latched HIGH/LOW state of motor sense N.
    bool getMotorNState() const { return _motorNState; }

    /// Reset motor sense states and direction tracking to IDLE.
    void reset() {
        _motorPState = false;
        _motorNState = false;
        _currentDirection = MotorDirection::IDLE;
        _pendingDirection = MotorDirection::IDLE;
        _pendingSinceUs = 0;
    }

private:
    uint16_t _onsetMv;
    uint16_t _offsetMv;
    uint64_t _debounceUs;

    bool _motorPState = false;
    bool _motorNState = false;
    MotorDirection _currentDirection = MotorDirection::IDLE;
    MotorDirection _pendingDirection = MotorDirection::IDLE;
    uint64_t _pendingSinceUs = 0;
};

// ── Decimation Accumulator and Row ──

/// Per-channel min/mean/max summary statistics over one decimation window.
struct ChannelStats {
    uint16_t min_val = 0xFFFF; ///< Minimum sample observed in the window
    uint16_t max_val = 0;      ///< Maximum sample observed in the window
    uint32_t sum = 0;          ///< Cumulative sample sum for mean computation
    uint32_t count = 0;        ///< Number of samples accumulated in the window

    /// Reset min, max, sum, and count to empty initial state.
    void reset() {
        min_val = 0xFFFF;
        max_val = 0;
        sum = 0;
        count = 0;
    }

    /// Accumulate a single ADC sample into min, max, sum, and count.
    void add(uint16_t val) {
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        sum += val;
        count++;
    }

    /// Return integer mean of accumulated samples (0 if count == 0).
    uint32_t mean() const {
        return count > 0 ? (sum / count) : 0;
    }
};

/// Decimated 8-channel telemetry row emitted at the end of each decimation window.
struct DecimationRow {
    uint32_t timestamp_ms = 0; ///< Window completion timestamp in milliseconds
    ChannelStats channels[8];  ///< Statistics for the 8 scanned channels: {0, 1, 2, 3, 4, 5, 6, 9}

    /// Clear timestamp and all 8 channel accumulators.
    void reset() {
        timestamp_ms = 0;
        for (size_t s = 0; s < 8; s++) {
            channels[s].reset();
        }
    }
};

/**
 * @brief Fixed-interval accumulator producing DecimationRow summaries from raw ADC samples.
 */
class DecimationAccumulator {
public:
    /// Construct accumulator with the given window duration in milliseconds.
    explicit DecimationAccumulator(uint32_t windowMs = 50)
        : _windowMs(windowMs) {
        reset();
    }

    /// Reset active row accumulator and window start timestamp.
    void reset() {
        _currentRow.reset();
        _lastWindowMs = 0;
    }

    /// Add a sample directly to logical scan slot (0..7).
    void addSampleSlot(size_t slot, uint16_t val) {
        if (slot < 8) {
            _currentRow.channels[slot].add(val);
        }
    }

    /// Map physical ADC channel to logical slot and accumulate sample value.
    void addSample(uint8_t channel, uint16_t val) {
        int slot = channelToSlot(channel);
        if (slot >= 0) {
            addSampleSlot(static_cast<size_t>(slot), val);
        }
    }

    /// Check window elapsed time; if windowMs elapsed, emit outRow, reset, and return true.
    bool processTick(uint32_t nowMs, DecimationRow& outRow) {
        if (_lastWindowMs == 0) {
            _lastWindowMs = nowMs;
        }
        if (nowMs - _lastWindowMs >= _windowMs) {
            _currentRow.timestamp_ms = nowMs;
            outRow = _currentRow;
            _currentRow.reset();
            _lastWindowMs = nowMs;
            return true;
        }
        return false;
    }

    /// Return const reference to the in-progress decimation row.
    const DecimationRow& getCurrentRow() const { return _currentRow; }
    /// Return mutable reference to the in-progress decimation row.
    DecimationRow& getCurrentRow() { return _currentRow; }
    /// Return the timestamp (ms) when the current window started.
    uint32_t getLastWindowMs() const { return _lastWindowMs; }
    /// Set the current window start timestamp (ms).
    void setLastWindowMs(uint32_t lastWindowMs) { _lastWindowMs = lastWindowMs; }
    /// Return the configured decimation window length (ms).
    uint32_t getWindowMs() const { return _windowMs; }
    /// Update the decimation window length (ms).
    void setWindowMs(uint32_t windowMs) { _windowMs = windowMs; }

private:
    uint32_t _windowMs;
    uint32_t _lastWindowMs = 0;
    DecimationRow _currentRow;
};

// ── Edge Event Types and Ring Buffer ──

/// Discriminator for SignalLogger edge and discrete event records.
enum EdgeEventType : uint8_t {
    EDGE_EVENT_CADENCE = 0,          ///< Reed-switch cadence falling edge
    EDGE_EVENT_ENCODER_LEGACY_A = 1, ///< Legacy analog-polled encoder A edge
    EDGE_EVENT_ENCODER_LEGACY_B = 2, ///< Legacy analog-polled encoder B edge
    EDGE_EVENT_MOTOR_ON_LEGACY = 3,  ///< Legacy motor-on threshold edge
    EDGE_EVENT_MOTOR_OFF_LEGACY = 4, ///< Legacy motor-off threshold edge
    EDGE_EVENT_MOTOR_DIRECTION = 5,  ///< Debounced motor direction state change
    EDGE_EVENT_ENCODER = 6,          ///< PCNT hardware quadrature encoder delta event
};

/// Discrete edge or state-transition event stored in the SignalLogger event ring.
struct EdgeEvent {
    uint32_t timestamp_us; ///< Event timestamp in microseconds
    uint8_t type;          ///< Event discriminator (EdgeEventType)
    uint8_t channel;       ///< Physical ADC/GPIO channel associated with the event
    uint16_t value;        ///< Event-specific value (mV, direction, or signed delta cast)
};

/// Convert logical ring index (0 = oldest) to physical array slot index.
constexpr size_t ringBufferLogicalToPhysical(size_t logicalIdx, size_t head, size_t count, size_t capacity) {
    size_t start = (count < capacity) ? 0 : head;
    return (start + logicalIdx) % capacity;
}

/**
 * @brief Fixed-capacity circular ring buffer with chronological random access.
 */
template <typename T, size_t Capacity>
class RingBuffer {
public:
    static constexpr size_t kCapacity = Capacity; ///< Compile-time ring buffer capacity

    /// Append an item, overwriting the oldest entry when full.
    void push(const T& item) {
        _data[_head] = item;
        _head = (_head + 1) % Capacity;
        if (_count < Capacity) {
            _count++;
        }
    }

    /// Return the number of stored items (up to Capacity).
    size_t size() const { return _count; }
    /// Return the fixed compile-time capacity.
    constexpr size_t capacity() const { return Capacity; }
    /// Return true once the buffer has reached capacity and wrapped.
    bool isWrapped() const { return _count >= Capacity; }
    /// Return the current physical write head index.
    size_t head() const { return _head; }

    /// Return const reference to element at logical chronological index (0 = oldest).
    const T& operator[](size_t logicalIndex) const {
        size_t idx = ringBufferLogicalToPhysical(logicalIndex, _head, _count, Capacity);
        return _data[idx];
    }

    /// Return const reference to element at logical chronological index (0 = oldest).
    const T& at(size_t logicalIndex) const {
        return (*this)[logicalIndex];
    }

    /// Reset write head and item count to zero.
    void clear() {
        _head = 0;
        _count = 0;
    }

private:
    T _data[Capacity];
    size_t _head = 0;
    size_t _count = 0;
};

// ── CSV Formatting Helpers ──

/// CSV column header line for decimated ride log exports.
constexpr const char* kRideLogCsvHeader =
    "timestamp_ms,"
    "pot_wiper_min,pot_wiper_mean,pot_wiper_max,"
    "pot_hi_min,pot_hi_mean,pot_hi_max,"
    "pot_lo_min,pot_lo_mean,pot_lo_max,"
    "cadence_ret_min,cadence_ret_mean,cadence_ret_max,"
    "cadence_min,cadence_mean,cadence_max,"
    "motor_p_min,motor_p_mean,motor_p_max,"
    "motor_n_min,motor_n_mean,motor_n_max,"
    "enc_vcc_min,enc_vcc_mean,enc_vcc_max\n";

/// CSV column header line for edge event exports.
constexpr const char* kEventsCsvHeader = "timestamp_us,type,channel,value\n";

/// CSV column header line for de-interleaved high-rate burst exports.
constexpr const char* kBurstCsvColumns =
    "sample_idx,pot_wiper,pot_hi,pot_lo,cadence_ret,cadence,motor_p,motor_n,enc_vcc\n";

/// Format a DecimationRow as CSV into buf; return characters written (0 on invalid buffer).
inline int formatRideLogRow(const DecimationRow& row, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    int len = snprintf(buf, bufSize, "%u", static_cast<unsigned int>(row.timestamp_ms));
    for (size_t c = 0; c < 8; c++) {
        uint32_t mean = row.channels[c].mean();
        len += snprintf(buf + len, (len < static_cast<int>(bufSize)) ? (bufSize - len) : 0,
                        ",%u,%u,%u",
                        static_cast<unsigned int>(row.channels[c].min_val),
                        static_cast<unsigned int>(mean),
                        static_cast<unsigned int>(row.channels[c].max_val));
    }
    len += snprintf(buf + len, (len < static_cast<int>(bufSize)) ? (bufSize - len) : 0, "\n");
    return len;
}

/// Format a DecimationRow as a CSV std::string.
inline std::string formatRideLogRow(const DecimationRow& row) {
    char buf[256];
    formatRideLogRow(row, buf, sizeof(buf));
    return std::string(buf);
}

/// Format an EdgeEvent as CSV into buf; return characters written (0 on invalid buffer).
inline int formatEventRow(const EdgeEvent& ev, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    return snprintf(buf, bufSize, "%u,%u,%u,%u\n",
                    static_cast<unsigned int>(ev.timestamp_us),
                    static_cast<unsigned int>(ev.type),
                    static_cast<unsigned int>(ev.channel),
                    static_cast<unsigned int>(ev.value));
}

/// Format an EdgeEvent as a CSV std::string.
inline std::string formatEventRow(const EdgeEvent& ev) {
    char buf[64];
    formatEventRow(ev, buf, sizeof(buf));
    return std::string(buf);
}

/// Format the metadata comment and column header for a burst CSV export into buf.
inline int formatBurstCsvHeader(uint32_t perChanRate, uint64_t burstStartUs, uint32_t overflowCount, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    return snprintf(buf, bufSize,
                    "# sample_rate_per_channel_hz=%u,start_time_us=%llu,overflow_count=%u\n%s",
                    static_cast<unsigned int>(perChanRate),
                    static_cast<unsigned long long>(burstStartUs),
                    static_cast<unsigned int>(overflowCount),
                    kBurstCsvColumns);
}

/// Format the metadata comment and column header for a burst CSV export as std::string.
inline std::string formatBurstCsvHeader(uint32_t perChanRate, uint64_t burstStartUs, uint32_t overflowCount) {
    char buf[256];
    formatBurstCsvHeader(perChanRate, burstStartUs, overflowCount, buf, sizeof(buf));
    return std::string(buf);
}

/// Format a DeinterleavedRow burst sample as CSV into buf; return characters written.
inline int formatBurstRow(size_t sampleIdx, const DeinterleavedRow& row, char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return 0;
    int len = snprintf(buf, bufSize, "%u", static_cast<unsigned int>(sampleIdx));
    for (size_t c = 0; c < 8; c++) {
        len += snprintf(buf + len, (len < static_cast<int>(bufSize)) ? (bufSize - len) : 0,
                        ",%u", static_cast<unsigned int>(row.channels[c]));
    }
    len += snprintf(buf + len, (len < static_cast<int>(bufSize)) ? (bufSize - len) : 0, "\n");
    return len;
}

/// Format a DeinterleavedRow burst sample as a CSV std::string.
inline std::string formatBurstRow(size_t sampleIdx, const DeinterleavedRow& row) {
    char buf[128];
    formatBurstRow(sampleIdx, row, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace echenon

using echenon::DecimationRow;
using echenon::EdgeEventType;
using echenon::EdgeEvent;
using echenon::EDGE_EVENT_CADENCE;
using echenon::EDGE_EVENT_ENCODER_LEGACY_A;
using echenon::EDGE_EVENT_ENCODER_LEGACY_B;
using echenon::EDGE_EVENT_MOTOR_ON_LEGACY;
using echenon::EDGE_EVENT_MOTOR_OFF_LEGACY;
using echenon::EDGE_EVENT_MOTOR_DIRECTION;
using echenon::EDGE_EVENT_ENCODER;
