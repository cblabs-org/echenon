---
name: signal-logger-wo0
description: Handoff instructions for Work Order 0 — ADC-continuous spike
complexity: standard
---

# WO0 — ADC-Continuous Spike

## Role Definition
> You are an expert senior software engineer and execution-focused worker agent.
> You are NOT the Principal Architect. Do not make high-level architectural
> decisions, do not create additional branches, and do not plan beyond this WO.
> Your task is strictly limited to executing the checklist below.

## Objective

Run three experiments on the Waveshare ESP32-S3-Nano to answer questions that
shape the PR-01 SignalLogger design:

1. **ADC-continuous throughput** on all 10 ADC1 channels with WiFi on.
2. **ADC + GPIO-ISR coexistence** on the same pad (GPIO 5, 8 or 9).
3. **Encoder Ch B analog capture** at kHz rate on GPIO 9.

This is a **spike** — the code need not be production quality. It will be
superseded by WO1 (project scaffold) and WO2 (capture engine). The spike's
value is the data in the Completion Report, not the code.

## Context

- **Mandate:** [PROJECT.md](../PROJECT.md)
- **SPEC pin map:** [SPEC §4.4.1](../../../../../SPEC.md) — GPIO 1–10 = ADC1 ch 0–9
- **Hardware spec §5.5:** Every direct tap (GPIO 1–5, 8–10) has a 22 kΩ series
  resistor. GPIO 6/7 are behind the ÷4 divider. No internal pulls on any tap pin.
- **Framework submodule:** Currently at `v0.0.12-dev.57-19` (`0268974a`). Do NOT
  bump it — WO1 handles that. The spike can bypass the framework entirely.
- **Board:** Waveshare ESP32-S3-Nano (board ID `um_nanos3` in PlatformIO).
  16 MB flash, 8 MB PSRAM (octal).
- **GPIO 3 is a strapping pin** (JTAG signal source). Confirm that the Green
  tap does not change boot behaviour.

## Strict Boundary Box

- **IN SCOPE:** A minimal PlatformIO project in `src/` (spike code), a
  `platformio.ini` for the spike, serial output, evidence files under
  `docs/measurements/phase0/`.
- **OUT OF SCOPE:** Framework integration, `EchenonModule`, portal/dashboard,
  board.h, any file under `hardware/`, the framework submodule pointer, any
  BLE code. Do not touch `SPEC.md` or `docs/` (except evidence files).
- **Do not modify or bump the framework submodule.**
- **Do not create additional branches.** All work stays on
  `signal-logger/wo0-adc-spike`.

## Tooling Constraints

- Use the `framework-expert` skill for any ESP32 App Foundation questions.
- Use the `pio-manager` skill and the PlatformIO MCP tools for building and
  flashing. Specifically: `build_project`, `upload_firmware`, `start_monitor`.
- Read the `pio-manager` skill (`SKILL.md`) before your first MCP build call.
- Serial output is the primary observation tool. No portal needed.
- The spike does NOT need the framework's `Application` class, `AppFoundation.h`,
  or any framework modules. A bare `setup()` / `loop()` with ESP-IDF calls is
  fine. However, the PlatformIO project still needs `framework = arduino, espidf`
  and a valid `board` entry.

## Line-Item Instructions

### A. Project setup (spike-only `platformio.ini`)

- [ ] Create `platformio.ini` at the repo root with a single environment
  `[env:adc-spike]`:
  ```ini
  [platformio]
  default_envs = adc-spike

  [env:adc-spike]
  platform = espressif32
  framework = arduino, espidf
  board = um_nanos3
  monitor_speed = 115200
  board_build.arduino.memory_type = qio_opi
  board_upload.flash_size = 16MB
  board_build.cmake_extra_args =
      -DSDKCONFIG_DEFAULTS="sdkconfig.defaults"
  build_flags =
      -std=gnu++17
      -DCORE_DEBUG_LEVEL=3
      -DARDUINO_USB_MODE=1
      -DARDUINO_USB_CDC_ON_BOOT=1
      -DBOARD_HAS_PSRAM
  build_unflags = -std=gnu++11
  ```
- [ ] Create `sdkconfig.defaults` at the repo root with PSRAM and USB console
  settings appropriate for the ESP32-S3 with octal PSRAM.
- [ ] Create `src/main.cpp` with a basic `setup()` and `loop()`.

### B. Experiment 1 — ADC-continuous throughput with WiFi on

- [ ] In `setup()`, connect to WiFi using the Arduino `WiFi.begin()` API
  (hardcode an SSID/password pair or use `WiFi.beginSmartConfig()`; the
  exact method is not important — what matters is that the WiFi radio is
  active during the ADC measurement). If WiFi connection is impractical for
  a throwaway spike, simply call `WiFi.mode(WIFI_STA); WiFi.begin("dummy","dummy");`
  to put the radio into active mode — the ADC contention with the radio is
  what we're measuring, not actual connectivity.
- [ ] Configure the ESP-IDF `adc_continuous` driver on **all 10 ADC1 channels**
  (channels 0–9, which are GPIO 1–10 on the ESP32-S3).
  - Use `ADC_ATTEN_DB_12` (full-scale 0–3.3 V).
  - Use `ADC_BITWIDTH_12`.
  - Set the conversion frame size and buffer size generously.
  - Set `sample_freq_hz` to the maximum the driver allows (try 83333 Hz
    or `SOC_ADC_SAMPLE_FREQ_THRES_HIGH`).
- [ ] In `loop()`, read completed conversion frames and count samples per
  second per channel. Every 5 seconds, print:
  ```
  [ADC-SPIKE] WiFi: ON | Channels: 10 | Rate: XXXXX samples/sec/channel | Total: XXXXXX samples/sec
  ```
- [ ] Run for at least 30 seconds and record the sustained rate.
- [ ] **Key question:** Is 1–5 kHz per channel on all 10 channels achievable
  with WiFi on? (Total throughput needed: 10 × 5000 = 50 kSa/s.)

### C. Experiment 2 — ADC-continuous + GPIO ISR on the same pad

- [ ] Choose GPIO 5 (Orange/cadence) for the test — it is both an ADC1
  channel and needs edge interrupts for cadence counting.
- [ ] With the ADC continuous driver running on all 10 channels (including
  channel 4 = GPIO 5), also attach a GPIO ISR to GPIO 5 using
  `gpio_isr_handler_add()`. Use `GPIO_INTR_ANYEDGE`. The ISR increments a
  volatile counter.
- [ ] Generate edges on GPIO 5: wire it to another GPIO that toggles in
  `loop()`, or use an external signal. Count edges in the ISR and compare
  with the toggle count.
- [ ] Print every 5 seconds:
  ```
  [ADC-ISR] GPIO 5: ISR edges=XXXX expected=XXXX | ADC samples on ch4: XXXX/sec | ISR+ADC coexist: YES/NO
  ```
- [ ] Also test GPIO 8 and GPIO 9 if time permits.
- [ ] **Key question:** Can we sample a pin with ADC-continuous AND get GPIO
  interrupts on the same pad? If not, note which fails (ISR never fires, or
  ADC samples are corrupted / absent).

### D. Experiment 3 — Encoder Ch B analog capture

- [ ] With the ADC continuous driver running, capture GPIO 9 (ADC1 channel 8)
  data at the highest achievable rate.
- [ ] While the encoder knob is turned (manually, or simulate by toggling a
  wire), capture at least 100 transitions.
- [ ] Print the raw ADC values for channel 8 in a burst (e.g., 1000 samples)
  so the voltage levels are visible:
  ```
  [ENC-CHB] ch8 samples (mV): 0, 0, 0, 1523, 1687, 1734, 1702, 0, 0, ...
  ```
- [ ] Record the peak voltage and compare against V_IH = 2.475 V (ESP32-S3).
- [ ] **Key question:** Is Ch B consistently below V_IH (confirming the
  teardown observation of ~1.5–2 V), or does it reach valid logic HIGH?

### E. GPIO 3 strapping check

- [ ] Confirm that GPIO 3 (Green tap, pot outer LO) does not interfere with
  boot. The ESP32-S3 uses GPIO 3 as a JTAG signal source selection strapping
  pin. If the Green wire applies a voltage at boot time, it could change the
  JTAG configuration. Simply observe whether the board boots normally with
  GPIO 3 connected to the Green tap wire (through the 22 kΩ series resistor).
  Note the result.

### F. Evidence

- [ ] Commit any encoder Ch B capture data as
  `docs/measurements/phase0/A08-YYYYMMDD-chb-adc-spike.csv` (or `.md` with
  the key numbers).
- [ ] All experimental results go into the Completion Report below.

## Verification Requirement

- The spike builds and flashes successfully to the Waveshare ESP32-S3-Nano.
- All three experiments produce serial output with the data requested.
- Before committing, ask the user if they are happy with the changes.

## Issue Surfacing

If you discover a defect, upstream bug, or concern *outside your boundary box*,
surface it in your Completion Report below — do not file issues directly. The
Principal Architect will triage and file them under the correct author identity.

---

## Completion Report (written by the worker at task completion)

<!-- Append the completion report here when done. Use this template: -->

<!--
### Status
complete | partial | blocked

### Changes Made
- `path/to/file` — what changed and why

### Experiment Results

#### Experiment 1: ADC-continuous throughput
- Sustained rate per channel: _____ Sa/s
- Total rate (10 channels): _____ Sa/s
- WiFi state: ON / active
- 1–5 kHz burst feasible: YES / NO

#### Experiment 2: ADC + ISR coexistence
- GPIO 5: ISR fires: YES / NO | ADC samples: OK / corrupt
- GPIO 8: ISR fires: YES / NO | ADC samples: OK / corrupt  (if tested)
- GPIO 9: ISR fires: YES / NO | ADC samples: OK / corrupt  (if tested)
- Conclusion: coexist / derive edges from analog stream

#### Experiment 3: Encoder Ch B
- Peak Ch B voltage: _____ mV
- Typical Ch B voltage: _____ mV
- V_IH = 2475 mV. Ch B reaches logic HIGH: YES / NO
- Conclusion: needs analog capture / digital ISR is viable

#### GPIO 3 strapping
- Boot behaviour with GPIO 3 connected: normal / abnormal

### Decisions Made
(list any non-obvious decisions)

### Dead Ends & Warnings
(things that didn't work)

### Out-of-Scope Findings
(defects or concerns outside the boundary box)

### Blockers
(none if status is 'complete')
-->
