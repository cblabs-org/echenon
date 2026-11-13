/*
 * Echenon (c) 2026 CB Labs
 * SPDX-License-Identifier: Apache-2.0
 * Module: src/echenon/DiagnosticsModule.cpp
 * Description: PSRAM-backed bounded event log implementation for production diagnostics.
 */

#include "echenon/DiagnosticsModule.h"

#include <esp_heap_caps.h>

#include <cstring>

namespace echenon {

// =============================================================================
// Lifecycle
// =============================================================================

DiagnosticsModule::~DiagnosticsModule() {
    end();
}

bool DiagnosticsModule::begin() {
    if (_buffer) {
        return true;  // Already allocated
    }

    const size_t totalBytes = EVENT_LOG_CAPACITY * sizeof(DiagnosticEvent);
    _buffer = static_cast<DiagnosticEvent*>(
        heap_caps_calloc(EVENT_LOG_CAPACITY, sizeof(DiagnosticEvent), MALLOC_CAP_SPIRAM));

    if (!_buffer) {
        // Fallback to regular heap if PSRAM unavailable
        _buffer = static_cast<DiagnosticEvent*>(
            heap_caps_calloc(EVENT_LOG_CAPACITY, sizeof(DiagnosticEvent), MALLOC_CAP_8BIT));
    }

    if (!_buffer) {
        return false;
    }

    _head  = 0;
    _count = 0;
    return true;
}

void DiagnosticsModule::end() {
    if (_buffer) {
        heap_caps_free(_buffer);
        _buffer = nullptr;
    }
    _head  = 0;
    _count = 0;
}

// =============================================================================
// Core Event Recording
// =============================================================================

void DiagnosticsModule::recordEvent(DiagEventType type, uint32_t epoch_s,
                                     uint8_t p0, uint8_t p1, uint8_t p2) {
    if (!_buffer) {
        return;
    }

    DiagnosticEvent& evt = _buffer[_head];
    evt.epoch_s    = epoch_s;
    evt.type       = type;
    evt.payload[0] = p0;
    evt.payload[1] = p1;
    evt.payload[2] = p2;

    _head = (_head + 1) % EVENT_LOG_CAPACITY;
    if (_count < EVENT_LOG_CAPACITY) {
        _count++;
    }
}

// =============================================================================
// Convenience Event Recorders
// =============================================================================

void DiagnosticsModule::recordBoot(uint32_t epoch_s, uint32_t uptime_s) {
    // 24-bit LE uptime, capped at 16,777,215 s
    uint32_t clamped = (uptime_s > 0x00FFFFFFu) ? 0x00FFFFFFu : uptime_s;
    recordEvent(DiagEventType::BOOT, epoch_s,
                static_cast<uint8_t>(clamped & 0xFF),
                static_cast<uint8_t>((clamped >> 8) & 0xFF),
                static_cast<uint8_t>((clamped >> 16) & 0xFF));
}

void DiagnosticsModule::recordSessionStart(uint32_t epoch_s, uint32_t session_id) {
    // Low 16 bits of session_id in payload[0..1], payload[2] = 0
    recordEvent(DiagEventType::SESSION_START, epoch_s,
                static_cast<uint8_t>(session_id & 0xFF),
                static_cast<uint8_t>((session_id >> 8) & 0xFF),
                0);
}

void DiagnosticsModule::recordSessionEnd(uint32_t epoch_s, uint16_t duration_s,
                                          uint16_t record_count) {
    // duration_s as 16-bit LE in payload[0..1], record_count / 64 in payload[2]
    uint8_t coarse_records = static_cast<uint8_t>(
        (record_count / 64u) > 255u ? 255u : (record_count / 64u));
    recordEvent(DiagEventType::SESSION_END, epoch_s,
                static_cast<uint8_t>(duration_s & 0xFF),
                static_cast<uint8_t>((duration_s >> 8) & 0xFF),
                coarse_records);
}

void DiagnosticsModule::recordRunawayAlert(uint32_t epoch_s, uint32_t duration_ms) {
    // duration_ms / 100, clamped to 16-bit, in payload[0..1] LE; payload[2] = 0
    uint32_t scaled = duration_ms / 100u;
    if (scaled > 0xFFFFu) {
        scaled = 0xFFFFu;
    }
    recordEvent(DiagEventType::RUNAWAY_ALERT, epoch_s,
                static_cast<uint8_t>(scaled & 0xFF),
                static_cast<uint8_t>((scaled >> 8) & 0xFF),
                0);
}

void DiagnosticsModule::recordCalibrationDone(uint32_t epoch_s, uint8_t calibration_type) {
    // payload[0] = calibration_type, payload[1..2] = 0
    recordEvent(DiagEventType::CALIBRATION_DONE, epoch_s,
                calibration_type, 0, 0);
}

void DiagnosticsModule::recordFirmwareChange(uint32_t epoch_s,
                                              uint8_t major, uint8_t minor, uint8_t patch) {
    recordEvent(DiagEventType::FIRMWARE_CHANGE, epoch_s, major, minor, patch);
}

// =============================================================================
// Accessors
// =============================================================================

size_t DiagnosticsModule::getEventCount() const {
    return _count;
}

const DiagnosticEvent* DiagnosticsModule::getEvent(size_t index) const {
    if (!_buffer || index >= _count) {
        return nullptr;
    }

    // index 0 = oldest event
    // When buffer is full, oldest is at _head (since _head just advanced past it)
    // When buffer is not full, oldest is at position 0
    size_t actual;
    if (_count < EVENT_LOG_CAPACITY) {
        actual = index;
    } else {
        actual = (_head + index) % EVENT_LOG_CAPACITY;
    }
    return &_buffer[actual];
}

void DiagnosticsModule::clearEvents() {
    _head  = 0;
    _count = 0;
    if (_buffer) {
        std::memset(_buffer, 0, EVENT_LOG_CAPACITY * sizeof(DiagnosticEvent));
    }
}

// =============================================================================
// Query Support
// =============================================================================

size_t DiagnosticsModule::queryEvents(const EventQuery& query,
                                       DiagnosticEvent* out, size_t out_capacity) const {
    if (!_buffer || !out || out_capacity == 0 || _count == 0) {
        return 0;
    }

    size_t copied = 0;
    const size_t maxResults = (query.limit < out_capacity) ? query.limit : out_capacity;

    for (size_t i = 0; i < _count && copied < maxResults; ++i) {
        const DiagnosticEvent* evt = getEvent(i);
        if (!evt) {
            continue;
        }
        if (evt->epoch_s >= query.from_epoch && evt->epoch_s <= query.to_epoch) {
            out[copied++] = *evt;
        }
    }

    return copied;
}

// =============================================================================
// ITickable (stub — wiring deferred to WO-14.4)
// =============================================================================

void DiagnosticsModule::tick(uint32_t currentMillis) {
    (void)currentMillis;
}

} // namespace echenon
