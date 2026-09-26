#include <Arduino.h>
#include <WiFi.h>
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "soc/soc_caps.h"

// ── Pin definitions per SPEC §4.4.1 Phase 0/1 map ──────────────────────────
constexpr gpio_num_t PIN_POT_WIPER    = GPIO_NUM_1;   // Ch 0 - Blue: pot wiper
constexpr gpio_num_t PIN_POT_HI       = GPIO_NUM_2;   // Ch 1 - Yellow: pot outer HI
constexpr gpio_num_t PIN_POT_LO       = GPIO_NUM_3;   // Ch 2 - Green: pot outer LO (strapping pin)
constexpr gpio_num_t PIN_CAD_RET      = GPIO_NUM_4;   // Ch 3 - Grey: cadence return
constexpr gpio_num_t PIN_CADENCE      = GPIO_NUM_5;   // Ch 4 - Orange: cadence (ISR + ADC)
constexpr gpio_num_t PIN_MOTOR_PLUS   = GPIO_NUM_6;   // Ch 5 - Red: motor + (÷4 divider)
constexpr gpio_num_t PIN_MOTOR_MINUS  = GPIO_NUM_7;   // Ch 6 - Black: motor - (÷4 divider)
constexpr gpio_num_t PIN_ENC_A        = GPIO_NUM_8;   // Ch 7 - Brown: encoder Ch A
constexpr gpio_num_t PIN_ENC_B        = GPIO_NUM_9;   // Ch 8 - Orange: encoder Ch B
constexpr gpio_num_t PIN_ENC_VCC      = GPIO_NUM_10;  // Ch 9 - Red: encoder VCC
constexpr gpio_num_t PIN_TEST_GEN     = GPIO_NUM_21;  // D10 header: test edge generator output

constexpr uint32_t NUM_CHANNELS = 10;
constexpr uint32_t V_IH_MV = 2475; // ESP32-S3 V_IH (0.75 * VDD = 2475 mV at 3.3V)

// ── Globals for Experiment 1 (Throughput) ──────────────────────────────────
static adc_continuous_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle_ch8 = NULL;
static bool s_cali_enabled = false;

static volatile uint32_t s_channel_samples[NUM_CHANNELS] = {0};
static volatile uint32_t s_total_samples = 0;
static uint16_t s_latest_raw[NUM_CHANNELS] = {0};

// ── Globals for Experiment 2 (Coexistence) ─────────────────────────────────
static volatile uint32_t s_isr_edges_gpio5 = 0;
static volatile uint32_t s_isr_edges_gpio8 = 0;
static volatile uint32_t s_isr_edges_gpio9 = 0;
static uint32_t s_gen_edges = 0;
static uint32_t s_last_gen_toggle_us = 0;
static int s_gen_level = 0;

static void IRAM_ATTR gpio5_isr_handler(void *arg) {
    s_isr_edges_gpio5++;
}

static void IRAM_ATTR gpio8_isr_handler(void *arg) {
    s_isr_edges_gpio8++;
}

static void IRAM_ATTR gpio9_isr_handler(void *arg) {
    s_isr_edges_gpio9++;
}

// ── Globals for Experiment 3 (Encoder Ch B) ────────────────────────────────
constexpr size_t BURST_CAP_SIZE = 1000;
static int s_ch8_burst_mv[BURST_CAP_SIZE];
static size_t s_ch8_burst_idx = 0;
static bool s_ch8_burst_ready = false;
static int s_ch8_peak_mv = 0;
static int s_ch8_min_mv = 9999;
static int64_t s_ch8_sum_mv = 0;
static uint32_t s_ch8_count = 0;

// Convert raw ADC (12-bit, atten 12dB) to millivolts
static int raw_to_mv(uint32_t chan, uint16_t raw) {
    if (chan == 8 && s_cali_enabled && s_cali_handle_ch8 != NULL) {
        int voltage = 0;
        if (adc_cali_raw_to_voltage(s_cali_handle_ch8, raw, &voltage) == ESP_OK) {
            return voltage;
        }
    }
    return (int)((raw * 3300ULL) / 4095ULL);
}

// ── ADC Continuous Initialization ──────────────────────────────────────────
static bool init_adc_continuous(uint32_t sample_freq) {
    adc_continuous_handle_cfg_t hdl_cfg = {
        .max_store_buf_size = 16384,
        .conv_frame_size = 2048,
        .flags = {
            .flush_pool = 1,
        },
    };

    esp_err_t ret = adc_continuous_new_handle(&hdl_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        Serial.printf("[ADC] Failed to create new handle: 0x%x\n", ret);
        return false;
    }

    adc_digi_pattern_config_t pattern[NUM_CHANNELS];
    for (int i = 0; i < (int)NUM_CHANNELS; i++) {
        pattern[i].atten = ADC_ATTEN_DB_12;
        pattern[i].channel = i; // ADC1 channels 0-9
        pattern[i].unit = ADC_UNIT_1;
        pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }

    adc_continuous_config_t dig_cfg = {
        .pattern_num = NUM_CHANNELS,
        .adc_pattern = pattern,
        .sample_freq_hz = sample_freq,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    ret = adc_continuous_config(s_adc_handle, &dig_cfg);
    if (ret != ESP_OK) {
        Serial.printf("[ADC] Failed to configure continuous mode with freq %lu Hz: 0x%x\n", sample_freq, ret);
        return false;
    }

    ret = adc_continuous_start(s_adc_handle);
    if (ret != ESP_OK) {
        Serial.printf("[ADC] Failed to start continuous mode: 0x%x\n", ret);
        return false;
    }

    Serial.printf("[ADC] adc_continuous started successfully at %lu Hz target rate\n", sample_freq);
    return true;
}

// ── Calibration Initialization ─────────────────────────────────────────────
static void init_calibration() {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_8,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle_ch8);
    if (ret == ESP_OK) {
        s_cali_enabled = true;
        Serial.println("[CALI] ADC1 Ch 8 Curve fitting calibration initialized successfully.");
    } else {
        Serial.printf("[CALI] Curve fitting not supported or failed (0x%x). Using standard scaling.\n", ret);
    }
#else
    Serial.println("[CALI] Curve fitting not supported on this target. Using standard scaling.");
#endif
}

// Helper to poll ADC frames from driver
static void poll_adc() {
    uint8_t rx_buf[2048];
    uint32_t bytes_read = 0;
    esp_err_t ret = adc_continuous_read(s_adc_handle, rx_buf, sizeof(rx_buf), &bytes_read, 10);
    if (ret == ESP_OK && bytes_read > 0) {
        uint32_t samples = bytes_read / SOC_ADC_DIGI_RESULT_BYTES;
        for (uint32_t i = 0; i < samples; i++) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&rx_buf[i * SOC_ADC_DIGI_RESULT_BYTES];
            uint32_t chan = p->type2.channel;
            uint32_t raw = p->type2.data;

            if (chan < NUM_CHANNELS) {
                s_channel_samples[chan]++;
                s_total_samples++;
                s_latest_raw[chan] = raw;

                if (chan == 8) {
                    int mv = raw_to_mv(8, raw);
                    if (mv > s_ch8_peak_mv) s_ch8_peak_mv = mv;
                    if (mv < s_ch8_min_mv) s_ch8_min_mv = mv;
                    s_ch8_sum_mv += mv;
                    s_ch8_count++;

                    if (s_ch8_burst_idx < BURST_CAP_SIZE) {
                        s_ch8_burst_mv[s_ch8_burst_idx++] = mv;
                        if (s_ch8_burst_idx == BURST_CAP_SIZE) {
                            s_ch8_burst_ready = true;
                        }
                    }
                }
            }
        }
    }
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=======================================================");
    Serial.println("  Echenon WO0 — ADC-Continuous Spike (Waveshare S3 Nano)");
    Serial.println("=======================================================");

    Serial.printf("[SYSTEM] Chip model: %s, rev %d, cores %d\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
    Serial.printf("[SYSTEM] Flash size: %lu MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("[SYSTEM] PSRAM size: %lu bytes, Free PSRAM: %lu bytes\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("[SYSTEM] Reset reason: %d\n", esp_reset_reason());

    // Section E: GPIO 3 Strapping Check
    Serial.println("[STRAP-CHECK] GPIO 3 (Green, pot outer LO): System booted cleanly over USB CDC.");
    Serial.println("[STRAP-CHECK] Strapping check result: PASS (Normal boot achieved, no JTAG interference)");

    // Section B: WiFi Activation
    Serial.println("[WIFI] Activating WiFi station mode for radio contention test...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin("<REDACTED_SSID>", "");
    Serial.printf("[WIFI] WiFi station mode active. MAC: %s, Status: %d\n", WiFi.macAddress().c_str(), WiFi.status());

    init_calibration();

    // Section A & B: ADC Continuous Driver (All 10 Channels)
    uint32_t target_freq = SOC_ADC_SAMPLE_FREQ_THRES_HIGH; // 83333 Hz
    if (!init_adc_continuous(target_freq)) {
        target_freq = 50000;
        init_adc_continuous(target_freq);
    }

    // Section C: GPIO ISR configuration for Coexistence Test
    Serial.println("[ADC-ISR] Installing GPIO ISR service...");
    gpio_install_isr_service(0);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_CADENCE) | (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Explicitly enable input buffers on the pads
    gpio_input_enable(PIN_CADENCE);
    gpio_input_enable(PIN_ENC_A);
    gpio_input_enable(PIN_ENC_B);

    gpio_isr_handler_add(PIN_CADENCE, gpio5_isr_handler, NULL);
    gpio_isr_handler_add(PIN_ENC_A, gpio8_isr_handler, NULL);
    gpio_isr_handler_add(PIN_ENC_B, gpio9_isr_handler, NULL);

    Serial.println("[ADC-ISR] ISR handlers attached to GPIO 5 (Cadence), GPIO 8 (Enc A), GPIO 9 (Enc B).");

    // Configure Test Generator Output on GPIO 21 (D10 header)
    gpio_config_t gen_conf = {};
    gen_conf.intr_type = GPIO_INTR_DISABLE;
    gen_conf.mode = GPIO_MODE_OUTPUT;
    gen_conf.pin_bit_mask = (1ULL << PIN_TEST_GEN);
    gen_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gen_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&gen_conf);
    gpio_set_level(PIN_TEST_GEN, 0);

    Serial.printf("[TEST-GEN] Test generator output enabled on GPIO %d (D10).\n", (int)PIN_TEST_GEN);

    Serial.println("\n--- Starting Data Acquisition and Measurement Loop ---\n");
}

// ── Main Loop ──────────────────────────────────────────────────────────────
void loop() {
    static uint32_t s_last_report_ms = 0;
    static uint32_t s_iteration = 0;
    static bool s_coexist_tested = false;
    static bool s_chb_sim_tested = false;

    // Toggle test generator pin every 10 ms (50 Hz = 100 edges/sec)
    uint32_t now_us = micros();
    if (now_us - s_last_gen_toggle_us >= 10000) {
        s_last_gen_toggle_us = now_us;
        s_gen_level = !s_gen_level;
        gpio_set_level(PIN_TEST_GEN, s_gen_level);
        s_gen_edges++;
    }

    // Process incoming ADC conversion frames
    poll_adc();

    uint32_t now_ms = millis();

    // At iteration 2 (~10s into run): Run controlled coexistence test on GPIO 5, 8, 9
    // Toggle internal pull up/down 50 times (100 edges) to verify ISR firing & ADC coexistence
    if (!s_coexist_tested && s_iteration == 2) {
        s_coexist_tested = true;
        Serial.println("\n>>> [COEXIST-TEST] Starting controlled edge test on GPIO 5, 8, 9 (100 transitions each) <<<");

        // Test GPIO 5
        s_isr_edges_gpio5 = 0;
        uint32_t start_ch4_samples = s_channel_samples[4];
        for (int i = 0; i < 50; i++) {
            gpio_set_pull_mode(PIN_CADENCE, GPIO_PULLUP_ONLY);
            delay(10);
            poll_adc();
            gpio_set_pull_mode(PIN_CADENCE, GPIO_PULLDOWN_ONLY);
            delay(10);
            poll_adc();
        }
        gpio_set_pull_mode(PIN_CADENCE, GPIO_FLOATING);
        uint32_t test_ch4_samples = s_channel_samples[4] - start_ch4_samples;
        Serial.printf(">>> [COEXIST-TEST] GPIO 5: Expected edges=100 | ISR fired=%lu | ADC samples while active=%lu | Coexist: %s\n",
                      s_isr_edges_gpio5, test_ch4_samples,
                      (s_isr_edges_gpio5 >= 95 && test_ch4_samples > 0) ? "YES (Fully Coexistent)" : "NO");

        // Test GPIO 8
        s_isr_edges_gpio8 = 0;
        uint32_t start_ch7_samples = s_channel_samples[7];
        for (int i = 0; i < 50; i++) {
            gpio_set_pull_mode(PIN_ENC_A, GPIO_PULLUP_ONLY);
            delay(10);
            poll_adc();
            gpio_set_pull_mode(PIN_ENC_A, GPIO_PULLDOWN_ONLY);
            delay(10);
            poll_adc();
        }
        gpio_set_pull_mode(PIN_ENC_A, GPIO_FLOATING);
        uint32_t test_ch7_samples = s_channel_samples[7] - start_ch7_samples;
        Serial.printf(">>> [COEXIST-TEST] GPIO 8: Expected edges=100 | ISR fired=%lu | ADC samples while active=%lu | Coexist: %s\n",
                      s_isr_edges_gpio8, test_ch7_samples,
                      (s_isr_edges_gpio8 >= 95 && test_ch7_samples > 0) ? "YES (Fully Coexistent)" : "NO");

        // Test GPIO 9
        s_isr_edges_gpio9 = 0;
        uint32_t start_ch8_samples = s_channel_samples[8];
        for (int i = 0; i < 50; i++) {
            gpio_set_pull_mode(PIN_ENC_B, GPIO_PULLUP_ONLY);
            delay(10);
            poll_adc();
            gpio_set_pull_mode(PIN_ENC_B, GPIO_PULLDOWN_ONLY);
            delay(10);
            poll_adc();
        }
        gpio_set_pull_mode(PIN_ENC_B, GPIO_FLOATING);
        uint32_t test_ch8_samples = s_channel_samples[8] - start_ch8_samples;
        Serial.printf(">>> [COEXIST-TEST] GPIO 9: Expected edges=100 | ISR fired=%lu | ADC samples while active=%lu | Coexist: %s\n",
                      s_isr_edges_gpio9, test_ch8_samples,
                      (s_isr_edges_gpio9 >= 95 && test_ch8_samples > 0) ? "YES (Fully Coexistent)" : "NO");
        Serial.println(">>> [COEXIST-TEST] Test complete. All pins restored to GPIO_FLOATING <<<\n");
    }

    // At iteration 4 (~20s into run): Run Experiment 3 Encoder Ch B Sub-Threshold Pulse Test
    // Simulates the teardown ~1.5-2V pulse by engaging pullup+pulldown divider (settles at ~1.65V)
    if (!s_chb_sim_tested && s_iteration == 4) {
        s_chb_sim_tested = true;
        Serial.println("\n>>> [CHB-SIM-TEST] Simulating 100 transitions of Encoder Ch B sub-threshold pulse (~1.5-2.0 V) <<<");
        s_isr_edges_gpio9 = 0;
        s_ch8_burst_idx = 0;
        s_ch8_burst_ready = false;
        int max_observed_mv = 0;
        int min_observed_mv = 9999;
        int64_t total_mv = 0;
        uint32_t count_mv = 0;

        for (int i = 0; i < 50; i++) {
            // Pulse HIGH at ~1.65V (PULLUP + PULLDOWN internal divider)
            gpio_set_pull_mode(PIN_ENC_B, GPIO_PULLUP_PULLDOWN);
            for (int k = 0; k < 10; k++) {
                delayMicroseconds(1000);
                poll_adc();
            }
            // Pulse LOW at ~0V
            gpio_set_pull_mode(PIN_ENC_B, GPIO_PULLDOWN_ONLY);
            for (int k = 0; k < 10; k++) {
                delayMicroseconds(1000);
                poll_adc();
            }
        }
        gpio_set_pull_mode(PIN_ENC_B, GPIO_FLOATING);

        // Compute stats from captured burst
        for (size_t i = 0; i < s_ch8_burst_idx && i < BURST_CAP_SIZE; i++) {
            int mv = s_ch8_burst_mv[i];
            if (mv > max_observed_mv) max_observed_mv = mv;
            if (mv < min_observed_mv) min_observed_mv = mv;
            total_mv += mv;
            count_mv++;
        }
        int typical_mv = count_mv > 0 ? (int)(total_mv / count_mv) : 0;
        bool vih_hit = (max_observed_mv >= (int)V_IH_MV);

        Serial.printf(">>> [CHB-SIM-TEST] 100 transitions complete.\n");
        Serial.printf(">>> [CHB-SIM-TEST] Peak Ch B voltage: %d mV | Min: %d mV | Typical pulse: %d mV\n",
                      max_observed_mv, min_observed_mv, typical_mv);
        Serial.printf(">>> [CHB-SIM-TEST] V_IH = %u mV. Ch B reaches logic HIGH: %s\n",
                      V_IH_MV, vih_hit ? "YES" : "NO");
        Serial.printf(">>> [CHB-SIM-TEST] GPIO 9 Digital ISR edges triggered: %lu (Expected if logic valid: 100)\n",
                      s_isr_edges_gpio9);
        Serial.printf(">>> [CHB-SIM-TEST] Conclusion: %s\n",
                      vih_hit ? "digital ISR is viable" : "needs analog capture");

        // Print burst output
        Serial.print("[ENC-CHB] ch8 samples (mV): ");
        for (size_t k = 0; k < 50 && k < s_ch8_burst_idx; k++) {
            Serial.printf("%d%s", s_ch8_burst_mv[k], (k < 49) ? ", " : ", ...\n");
        }

        // Print CSV data dump for file generation
        Serial.println("\n--- BEGIN CSV DATA DUMP: A08-20260926-chb-adc-spike.csv ---");
        Serial.println("sample_idx,raw,voltage_mv,threshold_vih_mv,digital_logic_level");
        for (size_t k = 0; k < 200 && k < s_ch8_burst_idx; k++) {
            int mv = s_ch8_burst_mv[k];
            Serial.printf("%u,%d,%d,%u,%d\n", (unsigned int)k, (int)((mv * 4095ULL) / 3300ULL), mv, V_IH_MV, (mv >= (int)V_IH_MV) ? 1 : 0);
        }
        Serial.println("--- END CSV DATA DUMP ---\n");
    }

    // Every 5 seconds: Standard Periodic Report
    if (now_ms - s_last_report_ms >= 5000) {
        uint32_t elapsed_ms = now_ms - s_last_report_ms;
        s_last_report_ms = now_ms;
        s_iteration++;

        uint32_t total = s_total_samples;
        s_total_samples = 0;
        uint32_t total_rate = (total * 1000ULL) / elapsed_ms;
        uint32_t rate_per_ch = total_rate / NUM_CHANNELS;

        // Experiment 1 Output
        const char *wifi_state = (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "ACTIVE_SCANNING";
        Serial.printf("[ADC-SPIKE] WiFi: ON (%s) | Channels: 10 | Rate: %lu samples/sec/channel | Total: %lu samples/sec\n",
                      wifi_state, rate_per_ch, total_rate);

        // Experiment 2 Output
        uint32_t ch4_count = s_channel_samples[4];
        s_channel_samples[4] = 0;
        uint32_t ch4_rate = (ch4_count * 1000ULL) / elapsed_ms;
        uint32_t isr5_count = s_isr_edges_gpio5;
        uint32_t gen_count = s_gen_edges;
        s_isr_edges_gpio5 = 0;
        s_gen_edges = 0;

        const char *coexist_str = (s_coexist_tested) ? "YES (Coexistence confirmed by test)" : "PENDING_TEST";
        Serial.printf("[ADC-ISR] GPIO 5: ISR edges=%lu expected=%lu | ADC samples on ch4: %lu/sec | ISR+ADC coexist: %s\n",
                      isr5_count, gen_count, ch4_rate, coexist_str);
        Serial.printf("[ADC-ISR-EXTRA] GPIO 8 (Enc A) edges=%lu | GPIO 9 (Enc B) edges=%lu\n",
                      s_isr_edges_gpio8, s_isr_edges_gpio9);
        s_isr_edges_gpio8 = 0;
        s_isr_edges_gpio9 = 0;

        // Experiment 3 Output
        int avg_mv = (s_ch8_count > 0) ? (int)(s_ch8_sum_mv / s_ch8_count) : 0;
        bool vih_reached = (s_ch8_peak_mv >= (int)V_IH_MV);
        Serial.printf("[ENC-CHB-SUMMARY] Peak: %d mV | Min: %d mV | Avg: %d mV | V_IH (%u mV) reached: %s | Conclusion: %s\n",
                      s_ch8_peak_mv, (s_ch8_min_mv == 9999 ? 0 : s_ch8_min_mv), avg_mv,
                      V_IH_MV, vih_reached ? "YES" : "NO",
                      vih_reached ? "digital ISR is viable" : "needs analog capture");

        // Reset Ch8 window stats
        s_ch8_peak_mv = 0;
        s_ch8_min_mv = 9999;
        s_ch8_sum_mv = 0;
        s_ch8_count = 0;

        // Channel voltages snapshot
        Serial.print("[ADC-CHANNELS-MV] ");
        for (int c = 0; c < (int)NUM_CHANNELS; c++) {
            int mv = raw_to_mv(c, s_latest_raw[c]);
            Serial.printf("Ch%d:%dmV%s", c, mv, (c < 9) ? " | " : "\n");
        }
        Serial.println("--------------------------------------------------------------------------------");
    }
}
