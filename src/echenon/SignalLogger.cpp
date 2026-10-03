#include "SignalLogger.h"
#include <PsychicHttp.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <cstring>

static const char* TAG = "SignalLogger";
static SignalLogger* s_instance = nullptr;
static portMUX_TYPE eventLogSpinlock = portMUX_INITIALIZER_UNLOCKED;

void SignalLogger::applyConfig(const JsonObject& config) {
    if (config["decimation_window_ms"].is<uint32_t>()) {
        _decimationWindowMs = config["decimation_window_ms"].as<uint32_t>();
    }
    if (config["burst_duration_ms"].is<uint32_t>()) {
        _burstDurationMs = config["burst_duration_ms"].as<uint32_t>();
    }
}

void SignalLogger::serializeConfig(JsonObject& config) const {
    config["decimation_window_ms"] = _decimationWindowMs;
    config["burst_duration_ms"] = _burstDurationMs;
}

void SignalLogger::resetConfig() {
    _decimationWindowMs = kDecimationWindowMs;
    _burstDurationMs = kBurstDurationDefaultMs;
}


void SignalLogger::begin() {
    if (_initialized) return;

    ESP_LOGI(TAG, "Allocating PSRAM buffers...");
    if (!_rideLogBuffer) _rideLogBuffer = (DecimationRow*)heap_caps_malloc(kRideLogMaxRows * sizeof(DecimationRow), MALLOC_CAP_SPIRAM);
    if (!_eventLogBuffer) _eventLogBuffer = (EdgeEvent*)heap_caps_malloc(kEventLogMaxEvents * sizeof(EdgeEvent), MALLOC_CAP_SPIRAM);
    
    // burst capacity: 10 channels * 5000 Hz * 30s = 1.5M samples
    _burstBufferCapacity = kNumChannels * 5000 * (kBurstDurationMaxMs / 1000); 
    // wait, actually ADC continuous is 83333 Hz total, 8333 Hz per channel.
    // So 8333 * 30 = 250,000 samples per channel.
    // Total samples = 2,500,000.
    // 2.5M * 2 bytes = 5MB. That's a bit too big with the ride log (4.3MB). Total 9.3MB > 8MB PSRAM.
    // Let's cap burst to 3MB (1,500,000 samples)
    _burstBufferCapacity = 1500000;
    if (!_burstBuffer) _burstBuffer = (uint16_t*)heap_caps_malloc(_burstBufferCapacity * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (!_rideLogBuffer || !_eventLogBuffer || !_burstBuffer) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers!");
        return;
    }
    ESP_LOGI(TAG, "PSRAM allocated successfully");

    // Initialize ADC continuous
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4000,
        .conv_frame_size = 1000,
    };
    if (!_adcHandle) {
        if (adc_continuous_new_handle(&adc_config, &_adcHandle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create ADC handle");
            return;
        }
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = kAdcSampleFreq,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    
    adc_digi_pattern_config_t adc_pattern[10] = {};
    for (int i = 0; i < 10; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = i; // assumes channels 0-9
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = ADC_BITWIDTH_12;
    }
    dig_cfg.pattern_num = 10;
    dig_cfg.adc_pattern = adc_pattern;

    static bool _adc_configured = false;
    if (!_adc_configured) {
        if (adc_continuous_config(_adcHandle, &dig_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config ADC");
            return;
        }
        _adc_configured = true;
    }

    // ADC Cali
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (!_adcCaliHandle) {
        adc_cali_create_scheme_curve_fitting(&cali_config, &_adcCaliHandle);
    }

    if (adc_continuous_start(_adcHandle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ADC");
        return;
    }
    _capturing = true;
    _initialized = true;

    s_instance = this;

    // NOTE: Do NOT call gpio_config() on ADC pins (GPIO 5, 8).
    // gpio_config(GPIO_MODE_INPUT) disconnects the pin from the ADC analog
    // peripheral and reconnects it to the digital GPIO matrix, breaking
    // ADC continuous readings. Cadence and encoder edges are detected via
    // ADC software thresholds in handleSoftwareThresholds() instead.

    // Reset row accumulators
    memset(&_currentRow, 0, sizeof(_currentRow));
    for (int i = 0; i < 10; i++) {
        _currentRow.channels[i].min_val = 0xFFFF;
    }

    startBurst("power-on", kBurstDurationDefaultMs);
}

void SignalLogger::stop() {
    if (_adcHandle) {
        adc_continuous_stop(_adcHandle);
        adc_continuous_deinit(_adcHandle);
        _adcHandle = nullptr;
    }
    _capturing = false;
}

void SignalLogger::tick(uint32_t now) {
    if (!_capturing) {
        static int fail_count = 0;
        if (fail_count++ % 1000 == 0) {
            
        }
        return;
    }
    if (_lastWindowMs == 0) _lastWindowMs = now;

    uint8_t buf[1024];
    uint32_t out_length = 0;
    while (adc_continuous_read(_adcHandle, buf, sizeof(buf), &out_length, 0) == ESP_OK) {
        processAdcFrame(buf, out_length);
    }

    if (now - _lastWindowMs >= _decimationWindowMs) {
        _currentRow.timestamp_ms = now;
        _rideLogBuffer[_rideLogHead] = _currentRow;
        _rideLogHead = (_rideLogHead + 1) % kRideLogMaxRows;
        if (_rideLogCount < kRideLogMaxRows) _rideLogCount++;
        
        static uint32_t last_log = 0;
        if (_rideLogCount % 20 == 0 && _rideLogCount != last_log) {
            printf("SignalLogger: Decimation row count: %u\n", _rideLogCount); ESP_LOGI(TAG, "Decimation row count: %u", _rideLogCount);
            last_log = _rideLogCount;
        }

        // Reset accumulators
        memset(&_currentRow, 0, sizeof(_currentRow));
        for (int i = 0; i < 10; i++) {
            _currentRow.channels[i].min_val = 0xFFFF;
        }
        _lastWindowMs = now;
    }

    if (_burstActive && (now - _burstStartMs >= _burstDurationMs || _burstBufferCount >= _burstBufferCapacity)) {
        _burstActive = false;
        ESP_LOGI(TAG, "Burst capture ended. Samples: %u", _burstBufferCount);
    }
}

void SignalLogger::processAdcFrame(uint8_t* buf, uint32_t length) {
    static uint32_t s_diagSamples[12] = {};  // 0-9 valid channels, 10=invalid, 11=total
    static uint16_t s_ch4Min = 0xFFFF, s_ch4Max = 0;
    static bool s_diagDone = false;

    uint32_t ts_us = esp_timer_get_time();
    for (uint32_t i = 0; i < length; i += sizeof(adc_digi_output_data_t)) {
        adc_digi_output_data_t* p = (adc_digi_output_data_t*)&buf[i];
        uint8_t chan = p->type2.channel;
        uint16_t data = p->type2.data;

        s_diagSamples[11]++;
        if (chan >= kNumChannels) {
            s_diagSamples[10]++;
            continue;
        }
        s_diagSamples[chan]++;
        if (chan == 4) {
            if (data < s_ch4Min) s_ch4Min = data;
            if (data > s_ch4Max) s_ch4Max = data;
        }

        // Accumulate min/mean/max
        if (data < _currentRow.channels[chan].min_val) _currentRow.channels[chan].min_val = data;
        if (data > _currentRow.channels[chan].max_val) _currentRow.channels[chan].max_val = data;
        _currentRow.channels[chan].sum += data;
        _currentRow.channels[chan].count++;

        if (_burstActive && _burstBufferCount < _burstBufferCapacity) {
            _burstBuffer[_burstBufferCount++] = data;
        }

        // Software thresholds (calc mV roughly if needed, or use raw if cali is slow, but requirement says calibrated mV)
        int mv = data;
        if (_adcCaliHandle) {
            adc_cali_raw_to_voltage(_adcCaliHandle, data, &mv);
        }

        handleSoftwareThresholds(chan, mv, ts_us);
    }

    // One-shot diagnostic after 10 seconds
    if (!s_diagDone && s_diagSamples[11] > 500000) {
        s_diagDone = true;
        ESP_LOGW(TAG, "=== ADC DIAGNOSTIC (after %lu total samples) ===", s_diagSamples[11]);
        for (int c = 0; c < 10; c++) {
            ESP_LOGW(TAG, "  CH%d: %lu samples", c, s_diagSamples[c]);
        }
        ESP_LOGW(TAG, "  Invalid channel: %lu samples", s_diagSamples[10]);
        ESP_LOGW(TAG, "  CH4 raw range: min=%u max=%u", s_ch4Min, s_ch4Max);
        
        // Dump raw DMA struct for each channel to check unit/channel tagging
        ESP_LOGW(TAG, "  === Last raw sample per channel (unit.chan.data) ===");
    }

    // Log first sample of each channel once
    static uint16_t s_chanSeen = 0;
    if (s_chanSeen != 0x3FF) {
        for (uint32_t i = 0; i < length; i += sizeof(adc_digi_output_data_t)) {
            adc_digi_output_data_t* p = (adc_digi_output_data_t*)&buf[i];
            uint8_t ch = p->type2.channel;
            uint8_t unit = p->type2.unit;
            uint16_t d = p->type2.data;
            if (ch < 10 && !(s_chanSeen & (1 << ch))) {
                s_chanSeen |= (1 << ch);
                ESP_LOGW(TAG, "  RAW CH%u: unit=%u data=%u", ch, unit, d);
            }
        }
    }
}

void SignalLogger::handleSoftwareThresholds(uint8_t channel, uint16_t mv, uint32_t ts_us) {
    if (channel == 4) { // Cadence (ADC1_CH4 = GPIO 5): HIGH idle, LOW on magnet
        if (_cadenceHigh && mv < kCadenceThresholdMv - 200) {
            _cadenceHigh = false;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 0, channel, 0 }; // type=0: cadence falling
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        } else if (!_cadenceHigh && mv > kCadenceThresholdMv + 200) {
            _cadenceHigh = true;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 0, channel, 1 }; // type=0: cadence rising
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        }
    } else if (channel == 8) { // Ch B
        if (!_chBHigh && mv > kChBThresholdMv + 50) {
            _chBHigh = true;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 2, channel, 1 };
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        } else if (_chBHigh && mv < kChBThresholdMv - 50) {
            _chBHigh = false;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 2, channel, 0 };
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        }
    } else if (channel == 5 || channel == 6) { // Motor
        if (!_motorOn && mv > kMotorOnsetThresholdMv) {
            _motorOn = true;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 3, channel, 1 };
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        } else if (_motorOn && mv < kMotorOnsetThresholdMv - 100) {
            _motorOn = false;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, 4, channel, 0 };
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        }
    }
}

bool SignalLogger::startBurst(const char* trigger, uint32_t durationMs) {
    if (_burstActive) return false;
    _burstDurationMs = (durationMs > kBurstDurationMaxMs) ? kBurstDurationMaxMs : durationMs;
    _burstStartMs = esp_timer_get_time() / 1000;
    _burstBufferCount = 0;
    _burstActive = true;
    ESP_LOGI(TAG, "Burst started: %s (%u ms)", trigger, _burstDurationMs);
    return true;
}

bool SignalLogger::isCapturing() const { return _capturing; }
bool SignalLogger::isBurstActive() const { return _burstActive; }

void SignalLogger::registerRoutes(PsychicHttpServer& server) {
    server.on("/api/echenon/status", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        JsonDocument doc;
        doc["ride_log_rows"] = _rideLogCount;
        doc["burst_active"] = _burstActive;
        doc["uptime_ms"] = esp_timer_get_time() / 1000;
        doc["sample_rate"] = kAdcSampleFreq;
        doc["free_psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        String res;
        serializeJson(doc, res);
        return response->send(200, "application/json", res.c_str());
    });

    server.on("/api/echenon/burst/start", HTTP_POST, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        if (_burstActive) {
            return response->send(409, "application/json", "{\"error\": \"Burst already active\"}");
        }
        
        uint32_t duration = kBurstDurationDefaultMs;
        if (request->body().length() > 0) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request->body());
            if (!error && doc["duration_ms"].is<uint32_t>()) {
                duration = doc["duration_ms"];
            }
        }
        
        if (startBurst("manual", duration)) {
            return response->send(200, "application/json", "{\"status\": \"started\"}");
        }
        return response->send(500, "application/json", "{\"error\": \"Failed to start\"}");
    });

    server.on("/api/echenon/ride-log.csv", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        
        response->setContentType("text/csv");
        
        const char* header = "timestamp_ms,ch0_min,ch0_mean,ch0_max,ch1_min,ch1_mean,ch1_max,ch2_min,ch2_mean,ch2_max,ch3_min,ch3_mean,ch3_max,ch4_min,ch4_mean,ch4_max,ch5_min,ch5_mean,ch5_max,ch6_min,ch6_mean,ch6_max,ch7_min,ch7_mean,ch7_max,ch8_min,ch8_mean,ch8_max,ch9_min,ch9_mean,ch9_max\n"; response->sendChunk((uint8_t*)header, strlen(header));
        
        char buf[256];
        size_t start = (_rideLogCount < kRideLogMaxRows) ? 0 : _rideLogHead;
        for (size_t i = 0; i < _rideLogCount; i++) {
            size_t idx = (start + i) % kRideLogMaxRows;
            const auto& row = _rideLogBuffer[idx];
            int len = snprintf(buf, sizeof(buf), "%u", row.timestamp_ms);
            for (int c = 0; c < 10; c++) {
                uint32_t mean = row.channels[c].count > 0 ? (row.channels[c].sum / row.channels[c].count) : 0;
                len += snprintf(buf + len, sizeof(buf) - len, ",%u,%u,%u", row.channels[c].min_val, mean, row.channels[c].max_val);
            }
            snprintf(buf + len, sizeof(buf) - len, "\n");
            response->sendChunk((uint8_t*)buf, strlen(buf));
        }
        response->finishChunking();
        return ESP_OK;
    });

    server.on("/api/echenon/events.csv", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        
        response->setContentType("text/csv");
        
        const char* header = "timestamp_us,type,channel,value\n"; response->sendChunk((uint8_t*)header, strlen(header));
        
        char buf[64];
        size_t start = (_eventLogCount < kEventLogMaxEvents) ? 0 : _eventLogHead;
        for (size_t i = 0; i < _eventLogCount; i++) {
            size_t idx = (start + i) % kEventLogMaxEvents;
            const auto& ev = _eventLogBuffer[idx];
            snprintf(buf, sizeof(buf), "%u,%u,%u,%u\n", ev.timestamp_us, ev.type, ev.channel, ev.value);
            response->sendChunk((uint8_t*)buf, strlen(buf));
        }
        response->finishChunking();
        return ESP_OK;
    });

    server.on("/api/echenon/burst.csv", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        
        response->setContentType("text/csv");
        
        const char* header = "sample_idx,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8,ch9\n"; response->sendChunk((uint8_t*)header, strlen(header));
        
        char buf[128];
        size_t samples = _burstBufferCount / kNumChannels;
        for (size_t i = 0; i < samples; i++) {
            int len = snprintf(buf, sizeof(buf), "%u", i);
            for (size_t c = 0; c < kNumChannels; c++) {
                len += snprintf(buf + len, sizeof(buf) - len, ",%u", _burstBuffer[i * kNumChannels + c]);
            }
            snprintf(buf + len, sizeof(buf) - len, "\n");
            response->sendChunk((uint8_t*)buf, strlen(buf));
        }
        response->finishChunking();
        return ESP_OK;
    });
}
