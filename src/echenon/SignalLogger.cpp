#include "SignalLogger.h"
#include "EventBus.hpp"
#include "EncoderModule/EncoderConfig.h"
#include <PsychicHttp.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <cstring>

static const char* TAG = "SignalLogger";
static SignalLogger* s_instance = nullptr;
static portMUX_TYPE eventLogSpinlock = portMUX_INITIALIZER_UNLOCKED;

// Channels to scan: 0-6 (pot, cadence, motor) + 9 (encoder VCC)
// Channels 7,8 (encoder A,B) released for PCNT EncoderModule
static const int adc_channels[] = {0, 1, 2, 3, 4, 5, 6, 9};
static constexpr int kAdcChannelCount = sizeof(adc_channels) / sizeof(adc_channels[0]);

static bool IRAM_ATTR s_on_pool_ovf(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    (void)handle;
    (void)edata;
    auto* self = static_cast<SignalLogger*>(user_data);
    if (self) {
        self->incrementOverflowCount();
    }
    return false;
}

static void onEncoderRotateCallback(const char* topic, const void* payload, void* ctx) {
    (void)topic;
    auto* self = static_cast<SignalLogger*>(ctx);
    if (!self || !payload) return;
    const auto* ev = static_cast<const EncoderEvent*>(payload);
    self->logEncoderEvent(ev->delta);
}

void SignalLogger::setEventBus(EventBus* bus) {
    _eventBus = bus;
    if (_eventBus && _initialized) {
        _eventBus->subscribe("encoder.rotate", onEncoderRotateCallback, this);
    }
}

void SignalLogger::logEncoderEvent(int32_t delta) {
    if (!_eventLogBuffer) return;
    uint32_t ts_us = static_cast<uint32_t>(esp_timer_get_time());
    // Direction: 1 for CW (delta > 0), 0 for CCW (delta < 0)
    uint16_t dir = (delta > 0) ? 1 : 0;

    portENTER_CRITICAL(&eventLogSpinlock);
    size_t idx = _eventLogHead;
    _eventLogBuffer[idx] = { ts_us, EDGE_EVENT_ENCODER, 8, dir };
    _eventLogHead = (idx + 1) % kEventLogMaxEvents;
    if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
    portEXIT_CRITICAL(&eventLogSpinlock);
}

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
    if (!_rideLogBuffer) {
        _rideLogBuffer = (DecimationRow*)heap_caps_malloc(kRideLogMaxRows * sizeof(DecimationRow), MALLOC_CAP_SPIRAM);
    }
    if (!_eventLogBuffer) {
        _eventLogBuffer = (EdgeEvent*)heap_caps_malloc(kEventLogMaxEvents * sizeof(EdgeEvent), MALLOC_CAP_SPIRAM);
    }
    
    // Burst buffer: 1.5M packed uint16_t samples (~3 MB PSRAM)
    _burstBufferCapacity = 1500000;
    if (!_burstBuffer) {
        _burstBuffer = (uint16_t*)heap_caps_malloc(_burstBufferCapacity * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    }

    if (!_rideLogBuffer || !_eventLogBuffer || !_burstBuffer) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers!");
        return;
    }
    ESP_LOGI(TAG, "PSRAM allocated successfully");

    // Initialize ADC continuous driver
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
    
    adc_digi_pattern_config_t adc_pattern[kAdcChannelCount] = {};
    for (int i = 0; i < kAdcChannelCount; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = adc_channels[i];
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = ADC_BITWIDTH_12;
    }
    dig_cfg.pattern_num = kAdcChannelCount;
    dig_cfg.adc_pattern = adc_pattern;

    if (!_adcConfigured) {
        if (adc_continuous_config(_adcHandle, &dig_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config ADC");
            return;
        }
        adc_continuous_evt_cbs_t cbs = {
            .on_conv_done = nullptr,
            .on_pool_ovf = s_on_pool_ovf,
        };
        if (adc_continuous_register_event_callbacks(_adcHandle, &cbs, this) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register ADC event callbacks");
        }
        _adcConfigured = true;
    }

    // ADC Cali scheme curve fitting
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

    // Reset row accumulators for all 8 scanned channel slots
    memset(&_currentRow, 0, sizeof(_currentRow));
    for (size_t s = 0; s < kNumChannels; s++) {
        _currentRow.channels[s].min_val = 0xFFFF;
    }

    // Subscribe to EncoderModule events if EventBus is configured
    if (_eventBus) {
        _eventBus->subscribe("encoder.rotate", onEncoderRotateCallback, this);
    }

    startBurst("power-on", kBurstDurationDefaultMs);
}

void SignalLogger::stop() {
    if (_adcHandle) {
        adc_continuous_stop(_adcHandle);
        adc_continuous_deinit(_adcHandle);
        _adcHandle = nullptr;
    }
    _adcConfigured = false;
    _capturing = false;
}

void SignalLogger::tick(uint32_t now) {
    if (!_capturing) return;

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
        if (_rideLogHead == 0 && _rideLogCount >= kRideLogMaxRows) {
            _rideLogWrapped = true;
        }
        if (_rideLogCount < kRideLogMaxRows) {
            _rideLogCount++;
        } else {
            _rideLogWrapped = true;
        }
        
        static uint32_t last_log_ms = 0;
        if (now - last_log_ms >= 60000 || last_log_ms == 0) {
            ESP_LOGI(TAG, "Decimation row count: %u", (unsigned int)_rideLogCount);
            last_log_ms = now;
        }

        // Reset accumulators for next decimation window
        memset(&_currentRow, 0, sizeof(_currentRow));
        for (size_t s = 0; s < kNumChannels; s++) {
            _currentRow.channels[s].min_val = 0xFFFF;
        }
        _lastWindowMs = now;
    }

    if (_burstActive && (now - _burstStartMs >= _burstDurationMs || _burstBufferCount >= _burstBufferCapacity)) {
        _burstActive = false;
        ESP_LOGI(TAG, "Burst capture ended. Samples: %u", (unsigned int)_burstBufferCount);
    }
}

void SignalLogger::processAdcFrame(uint8_t* buf, uint32_t length) {
    uint64_t now_us = esp_timer_get_time();
    uint32_t num_samples = length / sizeof(adc_digi_output_data_t);
    if (num_samples == 0) return;

    uint64_t frame_duration_us = (static_cast<uint64_t>(num_samples) * 1000000ULL) / kAdcSampleFreq;
    uint64_t frame_start_ts = (now_us >= frame_duration_us) ? (now_us - frame_duration_us) : 0;

    for (uint32_t i = 0; i < num_samples; i++) {
        adc_digi_output_data_t* p = (adc_digi_output_data_t*)&buf[i * sizeof(adc_digi_output_data_t)];
        uint8_t chan = p->type2.channel;
        uint16_t data = p->type2.data;

        int slot = echenon::channelToSlot(chan);
        if (slot < 0 || slot >= static_cast<int>(kNumChannels)) continue;

        // Accumulate min/mean/max
        if (data < _currentRow.channels[slot].min_val) _currentRow.channels[slot].min_val = data;
        if (data > _currentRow.channels[slot].max_val) _currentRow.channels[slot].max_val = data;
        _currentRow.channels[slot].sum += data;
        _currentRow.channels[slot].count++;

        if (_burstActive && _burstBufferCount < _burstBufferCapacity) {
            _burstBuffer[_burstBufferCount++] = echenon::packSample(chan, data);
        }

        // Per-sample timestamp calculation
        uint64_t sample_ts_us = echenon::calculateSampleTimestampUs(frame_start_ts, i, kAdcSampleFreq);

        int mv = data;
        if (_adcCaliHandle) {
            adc_cali_raw_to_voltage(_adcCaliHandle, data, &mv);
        }

        handleSoftwareThresholds(chan, static_cast<uint16_t>(mv), static_cast<uint32_t>(sample_ts_us));
    }
}

void SignalLogger::handleSoftwareThresholds(uint8_t channel, uint16_t mv, uint32_t ts_us) {
    if (channel == 4) { // Cadence (ADC1_CH4 = GPIO 5): HIGH idle, LOW on magnet
        if (_cadenceHigh && mv < kCadenceThresholdMv - 200) {
            _cadenceHigh = false;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, EDGE_EVENT_CADENCE, channel, 0 }; // type=0: cadence falling
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        } else if (!_cadenceHigh && mv > kCadenceThresholdMv + 200) {
            _cadenceHigh = true;
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = { ts_us, EDGE_EVENT_CADENCE, channel, 1 }; // type=0: cadence rising
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        }
    } else if (channel == 5 || channel == 6) { // Motor P (ch 5), Motor N (ch 6)
        _motorFsm.processSample(channel, mv, ts_us, [this](echenon::MotorDirection dir, uint64_t eventTsUs) {
            portENTER_CRITICAL(&eventLogSpinlock);
            size_t idx = _eventLogHead;
            _eventLogBuffer[idx] = {
                static_cast<uint32_t>(eventTsUs),
                EDGE_EVENT_MOTOR_DIRECTION,
                5,
                static_cast<uint16_t>(dir)
            };
            _eventLogHead = (idx + 1) % kEventLogMaxEvents;
            if (_eventLogCount < kEventLogMaxEvents) _eventLogCount++;
            portEXIT_CRITICAL(&eventLogSpinlock);
        });
    }
}

bool SignalLogger::startBurst(const char* trigger, uint32_t durationMs) {
    if (_burstActive) return false;
    _burstDurationMs = (durationMs > kBurstDurationMaxMs) ? kBurstDurationMaxMs : durationMs;
    _burstStartMs = esp_timer_get_time() / 1000;
    _burstStartUs = esp_timer_get_time();
    _burstBufferCount = 0;
    _burstActive = true;
    ESP_LOGI(TAG, "Burst started: %s (%u ms)", trigger, (unsigned int)_burstDurationMs);
    return true;
}

bool SignalLogger::isCapturing() const { return _capturing; }
bool SignalLogger::isBurstActive() const { return _burstActive; }

void SignalLogger::registerRoutes(PsychicHttpServer& server) {
    server.on("/api/echenon/status", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        JsonDocument doc;
        doc["ride_log_rows"] = _rideLogCount;
        doc["ride_log_wrapped"] = _rideLogWrapped;
        doc["burst_active"] = _burstActive;
        doc["uptime_ms"] = esp_timer_get_time() / 1000;
        doc["sample_rate"] = kAdcSampleFreq;
        doc["overflow_count"] = _overflowCount;
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
        
        const char* header = echenon::kRideLogCsvHeader;
        response->sendChunk((uint8_t*)header, strlen(header));
        
        char buf[256];
        size_t start = (_rideLogCount < kRideLogMaxRows) ? 0 : _rideLogHead;
        for (size_t i = 0; i < _rideLogCount; i++) {
            size_t idx = (start + i) % kRideLogMaxRows;
            const auto& row = _rideLogBuffer[idx];
            int len = echenon::formatRideLogRow(row, buf, sizeof(buf));
            response->sendChunk((uint8_t*)buf, len);
        }
        response->finishChunking();
        return ESP_OK;
    });

    server.on("/api/echenon/events.csv", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        response->setContentType("text/csv");
        
        const char* header = echenon::kEventsCsvHeader;
        response->sendChunk((uint8_t*)header, strlen(header));
        
        char buf[64];
        size_t start = (_eventLogCount < kEventLogMaxEvents) ? 0 : _eventLogHead;
        for (size_t i = 0; i < _eventLogCount; i++) {
            size_t idx = (start + i) % kEventLogMaxEvents;
            const auto& ev = _eventLogBuffer[idx];
            int len = echenon::formatEventRow(ev, buf, sizeof(buf));
            response->sendChunk((uint8_t*)buf, len);
        }
        response->finishChunking();
        return ESP_OK;
    });

    server.on("/api/echenon/burst.csv", HTTP_GET, [this](PsychicRequest *request, PsychicResponse *response) -> esp_err_t {
        response->setContentType("text/csv");
        
        char meta[256];
        uint32_t perChanRate = kAdcSampleFreq / kNumChannels; // 83333 / 8 ≈ 10416
        int metaLen = echenon::formatBurstCsvHeader(perChanRate, _burstStartUs, _overflowCount, meta, sizeof(meta));
        response->sendChunk((uint8_t*)meta, metaLen);
        
        char buf[128];
        echenon::deinterleaveBurstStream(_burstBuffer, _burstBufferCount,
            [response, &buf](size_t rowIdx, const echenon::DeinterleavedRow& row) {
                int len = echenon::formatBurstRow(rowIdx, row, buf, sizeof(buf));
                response->sendChunk((uint8_t*)buf, len);
            });
        response->finishChunking();
        return ESP_OK;
    });
}
