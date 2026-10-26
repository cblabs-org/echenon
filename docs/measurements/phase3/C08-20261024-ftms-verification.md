# C08: FTMS Broadcast Hardware Verification Report

**Date:** 2026-10-26
**Work Order:** WO-05.4 (PR-05)
**Device Under Test:** ESP32-S3-Nano (Waveshare) — `3C:0F:02:DE:72:26` (BLE) / `3C:0F:02:DE:72:24` (WiFi)
**Firmware Version:** `0.3.0-dev.22` (`v0.3.0-dev.18` through `v0.3.0-dev.22` hardware verification sequence)
**Git Tag:** `v0.3.0-dev.22`
**SPEC Reference:** §13.6 Tests 1.4–1.7, 3.1–3.11

---

## Pre-Flight & Audit Round 1 + Hardware Remediation Verification

| Check | Result |
|-------|--------|
| Native tests (`pio test -e native`, **162/162** including BLOCKING-1 L32, reed-switch debounce, `micros()` wraparound, and cross-task `millis()` HR underflow regressions) | ✅ PASS |
| `BLOCKING-1` remediation: Echelon Level 32 maps to `R_ftms = 254` (`0x00FE`), uncalibrated `255` maps to `0` | ✅ PASS (Unit + Live Rig & Bike Hardware) |
| `NON-BLOCKING-1/2`: `getProviderId() == "ftms"`, `Priority::Required`, `kIndoorBikeAppearance = 0x0485`, 3-byte `0x1826` Service Data (`01 20 00`) | ✅ PASS (Unit + Live BLE Adv) |
| `NON-BLOCKING-3/4`: `updateTelemetry` main-loop contract documented; `onStackPaused()` / `onStackResumed()` + `0x2AD9` stub callback wired | ✅ PASS |
| Hardware Bug 1: Physical reed-switch rising-edge RC threshold bounce (`CadenceSensor` IO-MUX glitch filter + immediate leading-edge ISR + `250 ms` min interval + `50 ms` HIGH release hysteresis) | ✅ PASS (Unit + Live Bike Ride) |
| Hardware Bug 2: Optical HR armband brief `0 BPM` drops & cross-core `millis()` underflow (`HRMClient` `10 s` grace window + `FTMSServer` non-zero BPM filter + signed `int32_t` elapsed-time checks) | ✅ PASS (Unit + Live Bike Ride) |
| Hardware Bug 3: `CadenceSensor` 71.58-minute `uint32_t micros()` wraparound vs `millis()` (`_lastPulseMs = currentMillis` latched in `tick()` with `_newPulse` evaluated before signed `int32_t` timeout) | ✅ PASS (Unit + Live Bike Ride > 2.5 h uptime) |
| Firmware build (`ex3-waveshare-nano-rig` & `ex3-waveshare-nano`) | ✅ SUCCESS |
| RAM usage | 17.2% (56,404 / 327,680 bytes) |
| Flash usage | 25.0% (1,569,863 / 6,291,456 bytes) |
| ESP32-S3-Nano USB connected | ✅ `/dev/cu.usbmodem101` |

---

## Step 1: Tag, Build, Flash & Boot Verification

**Dev Tag:** `v0.3.0-dev.18`
**Build Timestamp:** `Oct 24 2026 23:25:34`

### Serial Boot Log (key lines)

```
I (808) Application: ESP32 App Foundation v0.3.0-dev.18 — Echenon — booting
I (808) Application: State → BOOT
I (925) BLEManager: BLEManager wired to Application. Providers locked.
I (6583) HRMRepeater: Registering Heart Rate Service (0x180D)...
I (6584) HRMRepeater: Heart Rate Service (0x180D) registered successfully
I (6584) FTMSServer: Registering FTMS Service (0x1826)...
I (6585) FTMSServer: FTMS Service (0x1826) registered successfully
I (6585) DeviceInfoService: Device Information Service (0x180A) registered
I (6590) NimBLE: GAP procedure initiated: advertise; disc_mode=2
I (6592) BLEManager: BLE stack active and ready.
I (926) FTMSServer: FTMSServer started — subscribed to HR topic
I (6562) Application: State → RUNNING
I (6563) Application: All startup preconditions satisfied — entering RUNNING state
```

### Boot Verification Checklist

- [x] Clean boot without panics or watchdog resets
- [x] `HRMRepeater (0x180D)` registered on BLEManager
- [x] `FTMSServer (0x1826)` registered on BLEManager (`getProviderId() == "ftms"`)
- [x] WiFi connects and portal reachable (`192.168.1.100`)
- [x] Firmware version matches dev tag (`/api/system` → `firmware_version: "0.3.0-dev.18"`)

---

## Step 2: Live BLE (`bleak`) GATT Structure, Payload & Rig Sweep (§13.6 Tests 3.1–3.5)

Live over-the-air BLE verification was performed from macOS CoreBluetooth using `tmp/verify_ftms_bleak.py --mode rig --out tmp/ftms_bleak_rig.json` (`bleak` + `websockets`).

### 2.1 BLE Advertisement & Scan Discovery (`v0.3.0-dev.18`)

**Primary ADV payload (`20/31` bytes, hex):** `02010605030d1826180319850406162618012000`
- `02 01 06` — AD Type `0x01` (Flags): LE General Discoverable + BR/EDR Not Supported (3 bytes)
- `05 03 0d 18 26 18` — AD Type `0x03` (Complete List of 16-bit Service UUIDs): `0x180D` (Heart Rate), `0x1826` (FTMS) (6 bytes)
- `03 19 85 04` — AD Type `0x19` (Appearance): `0x0485` (`Indoor Bike`, little-endian `[0x85, 0x04]`) (4 bytes)
- `06 16 26 18 01 20 00` — AD Type `0x16` (Service Data - 16-bit UUID): UUID `0x1826` (FTMS), `Flags = 0x01` (Fitness Machine Available), `Fitness Machine Type = 0x0020` (Indoor Bike, little-endian `[0x20, 0x00]`) (7 bytes)

**Scan Response (`14/31` bytes, hex):** `0d09456368656e6f6e2d37323236`
- `0d 09 456368656e6f6e2d37323236` — AD Type `0x09` (Complete Local Name): `Echenon-7226`

**Over-the-air `BleakScanner` Discovery (`tmp/ftms_bleak_rig.json`):**
```json
{
  "name": "Echenon",
  "address": "18FDE83C-F155-1C41-3441-BD04BA736B77",
  "rssi": -45,
  "service_uuids": [
    "0000180d-0000-1000-8000-00805f9b34fb",
    "00001826-0000-1000-8000-00805f9b34fb"
  ],
  "service_data": {
    "00001826-0000-1000-8000-00805f9b34fb": "01 20 00"
  }
}
```

| Expected AD / Service Element | Present Over-the-Air | Notes |
|-------------------------------|----------------------|-------|
| `0x1826` (FTMS UUID) | ✅ | Discovered via `BleakScanner` (`Priority::Required`) |
| `0x1826` Service Data (`01 20 00`) | ✅ | Verified in `primaryAdvHex` & `BleakScanner` `service_data` |
| Appearance `0x0485` (Indoor Bike) | ✅ | Verified in `primaryAdvHex` (`03 19 85 04`) |
| `0x180D` (Heart Rate UUID) | ✅ | Discovered via `BleakScanner` & GATT `0x2A38` read (`00`) |
| `0x180A` (Device Info) | ✅ | Registered on shared GATT server |

### 2.2 Fitness Machine Feature (`0x2ACC`) — Live GATT READ

Captured over BLE via `await client.read_gatt_char("00002acc-0000-1000-8000-00805f9b34fb")`:

```json
{
  "hex": "82 44 00 00 00 00 00 00",
  "len": 8,
  "machine_features_hex": "0x00004482",
  "target_features_hex": "0x00000000"
}
```

- [x] *(On-device BLE read)* Payload is exactly 8 bytes: `82 44 00 00 00 00 00 00`
- [x] *(On-device BLE read)* `Fitness Machine Features = 0x00004482` (Bit 1 Cadence, Bit 7 Resistance Level, Bit 10 Heart Rate, Bit 14 Power Measurement)
- [x] *(On-device BLE read)* `Target Setting Features = 0x00000000` (read-only v1 bike)

### 2.3 Calibration Rig Multi-Level Sweep (`0x2AD2` BLE + `/ws/echenon/live` WebSocket)

Using `/api/echenon/rig/command` (`SET level=<L> cadence=<RPM>`), the Calibration Rig stepped the bike's physical resistance motor and potentiometer across **Echelon Levels 1, 16, and 32** while `bleak` captured live over-the-air `0x2AD2` notifications and `websockets` captured simultaneous `/ws/echenon/live` frames:

| Rig State (`command`) | Settled Pot ADC (`derived_level`) | Live `0x2AD2` Notification (11 B Hex) | `0x2AD2` Decoded `resistance` (`int16`) | `/ws/echenon/live` `level_echelon` / `level_i` | `/ws/echenon/live` `peloton_resistance` | Status |
|-----------------------|-----------------------------------|---------------------------------------|-----------------------------------------|------------------------------------------------|----------------------------------------|--------|
| `SET level=1 cadence=0` (`SETTLED`) | `166` (`L1`) | `64 02 00 00 00 00 02 00 00 00 00` | `2` (`0x0002`) | `1` / `2` | `13` | ✅ PASS |
| `SET level=1 cadence=80` (`SETTLED`) | `170` (`L1`) | `64 02 00 00 00 00 04 00 00 00 00` | `4` (`0x0004`) | `1` / `3` | `13` | ✅ PASS |
| `SET level=16 cadence=80` (`SETTLED`) | `721` (`L16`) | `64 02 00 00 00 00 7b 00 00 00 00` | `123` (`0x007B`) | `16` / `123` | `21` | ✅ PASS |
| **`SET level=32 cadence=80` (`SETTLED`)** | **`1327` (`L32`)** | **`64 02 00 00 00 00 fe 00 00 00 00`** | **`254` (`0x00FE`)** | **`32` / `255`** | **`42`** | **✅ PASS (`BLOCKING-1` Verified)** |

#### Level 32 Capture Detail (`L32_80rpm` from `tmp/ftms_bleak_rig.json`)

```json
{
  "command": "SET level=32 cadence=80",
  "rig_status": {
    "state": "SETTLED",
    "level": 32,
    "target": 32,
    "cadence": 80,
    "steps_remaining": 0,
    "settle_timeout": false,
    "pot": 1327,
    "derived_level": 32
  },
  "ble_packets_count": 6,
  "ble_last_packet": {
    "hex": "64 02 00 00 00 00 fe 00 00 00 00",
    "flags": "0x0264",
    "speed_kph": 0.0,
    "cadence_rpm": 0.0,
    "resistance": 254,
    "power_watts": 0,
    "hr_bpm": 0
  },
  "ws_live": {
    "cadence_rpm": 0.0,
    "level_echelon": 32,
    "level_i": 255,
    "power_watts": 0.0,
    "pot_adc": 1333,
    "motor_dir": 0,
    "encoder_delta": 0,
    "runaway": false,
    "peloton_resistance": 42,
    "speed_kph": 0.0,
    "timestamp_ms": 216764
  }
}
```
> **Hardware Proof for `BLOCKING-1`:** At Echelon Level 32 (`pot = 1327..1333`, `derived_level = 32`), the legacy tracker field `level_i` is `255` (which previously collided with `INTERNAL_RESISTANCE_UNCALIBRATED = 255` and caused `0x2AD2` to transmit `0`). With the `WO-05.4` remediation in `EchenonModule.cpp`, all 6/6 consecutive `0x2AD2` BLE notifications at Level 32 transmitted `fe 00` (`resistance = 254`) over the air while `/ws/echenon/live` simultaneously reported `peloton_resistance: 42`.

### 2.4 Indoor Bike Data (`0x2AD2`) — 2 Hz Notification Timing

Subscribed over BLE via `await client.start_notify("00002ad2-0000-1000-8000-00805f9b34fb", callback)` during a 6.0 s timing window (`tmp/ftms_bleak_rig.json`):

```json
{
  "packet_count": 13,
  "duration_s": 6.0,
  "intervals_ms": [480.04, 510.13, 479.92, 480.08, 540.08, 479.7, 510.51, 540.83, 478.74, 450.31, 540.05, 510.05],
  "mean_interval_ms": 500.04,
  "stdev_interval_ms": 29.73,
  "min_interval_ms": 450.31,
  "max_interval_ms": 540.83
}
```

| Field | Offset | Raw Bytes (LE) | Decoded Value | Status |
|-------|--------|----------------|---------------|--------|
| `Flags` | `0..1` | `64 02` | `0x0264` (Speed, Cadence, Resistance, Power, HR) | ✅ PASS |
| `Instantaneous Speed` | `2..3` | `00 00` | `0.00 km/h` (`0.01 km/h` res) | ✅ PASS |
| `Instantaneous Cadence` | `4..5` | `00 00` | `0.0 RPM` (`0.5 RPM` res) | ✅ PASS |
| `Resistance Level` | `6..7` | `02 00` .. `fe 00` | `2` (L1), `123` (L16), `254` (L32) | ✅ PASS |
| `Instantaneous Power` | `8..9` | `00 00` | `0 W` (`int16`) | ✅ PASS |
| `Heart Rate` | `10` | `00` | `0 BPM` (`uint8`) | ✅ PASS |
| **Notification Rate** | — | 11 bytes / pkt | **Mean `500.04 ms` (`2.00 Hz`), σ = `29.73 ms`** | ✅ PASS |

### 2.5 WebSocket `/ws/echenon/live` Timing (Simultaneous with BLE)

Captured 13 JSON frames in 6.0 s at exact `500 ms` (`2.00 Hz`) firmware intervals alongside the BLE `0x2AD2` notification stream:
- **Consecutive Frame Timestamps (`timestamp_ms`):** `256302` → `256802` (+500 ms) → `257303` (+501 ms) → `257804` (+501 ms) → `258304` (+500 ms) → `258804` (+500 ms) → `259305` (+501 ms) → `259805` (+500 ms) → `260305` (+500 ms) → `260805` (+500 ms) → `261306` (+501 ms) → `261807` (+501 ms) → `262308` (+501 ms).
- **Host Arrival Intervals (`ms`):** `[512.3, 447.6, 502.1, 506.1, 497.7, 504.0, 513.6, 511.6, 521.5, 504.0, 445.0, 502.2]` (mean `497.3 ms`).

---

## Step 2.6: API & Active BLE Connection Inspection (`v0.3.0-dev.18`)

### `/api/ble/status` While Connected to `bleak` FTMS Central

```json
{
  "state": "Active",
  "isScanning": true,
  "isAdvertising": true,
  "advertisedName": "Echenon",
  "bleAddress": "3C:0F:02:DE:72:26",
  "advertising": {
    "budget": {
      "advUsed": 20,
      "advMax": 31,
      "scanRespUsed": 14,
      "scanRespMax": 31
    },
    "primaryAdvHex": "02010605030d1826180319850406162618012000",
    "scanResponseHex": "0d09456368656e6f6e2d37323236",
    "hasFault": false,
    "faults": []
  },
  "connections": [
    {
      "handle": 1,
      "address": "FC:B2:14:B4:11:32",
      "addressRedacted": "**:**:**:**:11:32",
      "mtu": 185,
      "encrypted": false,
      "params": {
        "minIntervalUnits": 24,
        "maxIntervalUnits": 24,
        "latency": 0,
        "supervisionTimeoutUnits": 400,
        "minIntervalMs": 30,
        "maxIntervalMs": 30,
        "intervalMs": 30,
        "supervisionTimeoutMs": 4000
      }
    }
  ],
  "counters": {
    "droppedEvents": 0,
    "scanInhibitCount": 0
  }
}
```

### `/api/system` Heap & Uptime Stability

```json
{
  "heap_free": 114471,
  "heap_total": 336747,
  "heap_min_free": 76052,
  "heap_max_alloc": 40960,
  "psram_free": 8385184,
  "psram_total": 8388608,
  "uptime": 262,
  "firmware_version": "0.3.0-dev.18",
  "build_serial": "Oct 24 2026 23:25:34",
  "status": "ok"
}
```

---

## Step 3: Live Physical Bike Verification, Hardware Bug Remediations & Client Coexistence (§13.6 Tests 1.4–1.7, 3.7–3.11)

### 3.1 Client Compatibility Matrix

| Client | OS/Version | App / Stack | Pairs? | Sees FTMS? | Live Data? | Notes |
|--------|-----------|-------------|--------|-----------|-----------|-------|
| macOS | macOS CoreBluetooth | `bleak` FTMS Central | ✅ | ✅ (`0x1826` + `0x180D` + `01 20 00` svc data) | ✅ (`2.00 Hz` `0x2AD2`, L1=2, L16=123, L32=254) | MTU 185, 30 ms conn interval, 4000 ms supervision timeout |
| iOS / Android / macOS | Live Rider Session (`v0.3.0-dev.22`) | **Xert** | ✅ | ✅ (`0x1826` FTMS + `0x180D` HR) | ✅ **Exact match with Echelon Fit** (Power, Cadence, Speed, Resistance, Heart Rate) | Paired as Power / Cadence / HR sensor with Trainer Control unchecked for v1 read-only (`Target Setting Features == 0`) |
| Tablet (Lenovo Tab P12 / Echelon Tablet) | Android | **Echelon Fit** | ✅ | ✅ (`0x180D` HR Repeater + Bike Console) | ✅ Simultaneous coexistence with **Xert** + **Echelon Beat HR armband** | Zero dropouts across multi-client ride soak |

### 3.2 Live Physical Bike Pedalling, Resistance Sweep & HR Armband Captures

Across tags `v0.3.0-dev.19` through `v0.3.0-dev.22`, live physical riding on the Echelon EX-3 bike with the **Echelon Beat optical HR armband** (`DC:58:95:C2:6A:61`), **Echelon Fit**, **Xert**, `bleak`, and `/ws/echenon/live` (`tmp/ftms_bleak_pedal_v21.json`, `tmp/ftms_bleak_hr_knob_v21.json`, `tmp/ftms_multi_channel_monitor.json`) uncovered and resolved three real-world hardware/timing edge cases:

1. **Hardware Bug 1 — Physical Reed Switch Rising-Edge RC Threshold Bounce (`src/echenon/CadenceSensor.h`, `src/echenon/CadenceSensor.cpp`):**
   - **Root Cause:** During physical pedalling (`~41–85 RPM`), the crank reed switch stays closed (`LOW`) for `100–200 ms` while the crank magnet passes, then opens (`LOW → HIGH`), charging the `100 nF` RC filter capacitor (`τ ≈ 1.5 ms`). As the slow rising ramp crosses the ESP32-S3 GPIO digital input threshold (`~1.65 V`), threshold oscillation triggered `attachInterrupt(..., FALLING)` a second time at `dt ≈ 150 ms` (`> DEBOUNCE_LOCKOUT_US = 25 ms`), causing `CadenceSensor` to alternate between true RPM and `200.0 RPM` (`MAX_RPM`).
   - **Fix Verified (`v0.3.0-dev.21` / `v0.3.0-dev.22`):** Enabled the ESP32-S3 hardware IO-MUX pin glitch filter (`driver/gpio_filter.h`) in `CadenceSensor::begin()`, paired with immediate leading-edge ISR timestamp commit in `onFallingEdgeISR()`, a `MIN_HW_INTERVAL_US = 250000` (`250 ms` = `240 RPM`) hardware lockout, and `REARM_HIGH_US = 50000` (`50 ms` continuous `HIGH`) open-state release hysteresis in `processHardwareState()`.
   - **Live Capture Proof (`tmp/ftms_bleak_pedal_v21.json`):** Continuous physical pedalling from `35.5 RPM` to `84.5 RPM` (`84–146 W`, `24.16–29.50 km/h`) with **zero** `200.0 RPM` spikes.

2. **Hardware Bug 2 — Optical HR Armband Transient `0 BPM` Drops & Cross-Core `millis()` Underflow (`src/echenon/HRMClient.cpp`, `src/echenon/FTMSServer.cpp`):**
   - **Root Cause:** (a) The Echelon Beat optical armband periodically emits brief `0 BPM` / sensor-adjust packets during arm movement; (b) When the NimBLE host task on Core 0 received an HR notification 1–2 ms after `Application::tick()` on Core 1 sampled `currentMillis = millis()`, `n.receivedAtMs` was `currentMillis + 1` (or `+ 2`), causing unsigned `currentMillis - _lastValidHrMs` in `HRMClient::tick()` and `now_ms - _lastHrMs` in `FTMSServer::getEffectiveHrBpm()` to underflow to `0xFFFFFFFF` (`4,294,967,295`) and trigger an immediate false stale timeout (`0 BPM`) on the same tick.
   - **Fix Verified (`v0.3.0-dev.21` / `v0.3.0-dev.22`):** Added a `10 s` grace window in `HRMClient` for transient `0 BPM` packets while connected, filtered `sample->bpm > 0` in `FTMSServer::onHrEvent()`, and replaced unsigned subtraction with signed `static_cast<int32_t>(currentMillis - _lastValidHrMs) >= 10000` in `HRMClient::tick()` and `static_cast<int32_t>(now_ms - _lastHrMs) > static_cast<int32_t>(HR_STALE_TIMEOUT_MS)` in `FTMSServer::getEffectiveHrBpm()`.
   - **Live Capture Proof (`tmp/ftms_bleak_hr_knob_v21.json` & `v0.3.0-dev.22` live ride):** Continuous rock-solid HR reporting (`68–79 BPM`) across both `0x180D` (`HRMRepeater`) and `0x2AD2` (`FTMSServer`) alongside full resistance knob sweeps (`Level 1` / `R_ftms=0` / `Peloton=13` through `Level 32` / `R_ftms=254` / `Peloton=42`).

3. **Hardware Bug 3 — `CadenceSensor` 71.58-Minute `uint32_t micros()` Wraparound vs `millis()` (`src/echenon/CadenceSensor.cpp`):**
   - **Root Cause:** During extended bench operation (`uptime > 71.58 min` = `4,294,967 ms`, observed at `uptime = 9,237 s` in `tmp/ftms_multi_channel_monitor.json`), `uint32_t pulseMicros` wraps modulo `2^32 µs` (`0 .. 4,294,967 ms`), whereas `currentMillis` (`millis()`) continues advancing past `4,294,967 ms`. Because `handlePulse()` set `_lastPulseMs = pulseMicros / 1000` and `tick()` checked `currentMillis - _lastPulseMs >= TIMEOUT_MS` *before* `if (_newPulse)`, every tick after 71.58 minutes immediately evaluated as timed out and wiped `_newPulse = false` and `_rpm = 0.0f`.
   - **Fix Verified (`v0.3.0-dev.22`):** Updated `CadenceSensor::tick(uint32_t currentMillis)` to evaluate `if (_newPulse)` **first**—computing `_rpm` from pure `uint32_t` microsecond difference (`_lastIntervalUs`) and latching `_lastPulseMs = currentMillis` in the `millis()` clock domain—before checking `else if (_hasFirstPulse && static_cast<int32_t>(currentMillis - _lastPulseMs) >= static_cast<int32_t>(TIMEOUT_MS))`.
   - **Live Ride Proof (`v0.3.0-dev.22`):** Verified live on the Echelon EX-3 bike with **Xert** + **Echelon Fit** + **Echelon Beat HR armband** running simultaneously; Xert displayed the exact same live Power, Cadence, Speed, Resistance, and Heart Rate as the Echelon Fit app.

### 3.3 FTMS Stub Control Point (`0x2AD9`)

- `FTMS_STUB_CONTROL_POINT` defaults to `0` (`0x2AD9` omitted per FTMS v1.0 Table 3.1 when `Target Setting Features == 0`).
- In **Xert**, pairing `Echenon` as a Power / Cadence / Heart Rate source with *Trainer Control* unchecked works natively with `FTMS_STUB_CONTROL_POINT = 0`.
- Compile-time fallback `#if FTMS_STUB_CONTROL_POINT` in `FTMSServer.cpp` includes full `NimBLECharacteristicCallbacks` write/indicate handling (`0x00` → `[0x80, 0x00, 0x01]`, other opcodes → `[0x80, req, 0x02]`).

---

## Step 4: GO / NO-GO Verdict

| Criterion | Status |
|-----------|--------|
| Boot clean with both services (`0x180D` + `0x1826`) registered (`v0.3.0-dev.22`) | ✅ PASS |
| Native unit test suite (`pio test -e native`, **162/162** tests passing) | ✅ PASS |
| ADV payload (`20/31` B) includes `0x1826` + `0x180D` + Appearance `0x0485` + FTMS Service Data `01 20 00`, no budget faults | ✅ PASS |
| `0x2ACC` on-device BLE GATT read (`82 44 00 00 00 00 00 00`) | ✅ PASS |
| `0x2AD2` on-device BLE notifications at 2 Hz (`500.04 ms` mean, 11 bytes, `Flags=0x0264`) | ✅ PASS |
| `BLOCKING-1` live Calibration Rig & physical knob sweep across L1 (`0..2`), L16 (`123`), L32 (`254` / `0x00FE`) verified over BLE | ✅ PASS |
| Physical reed-switch debounce, optical HR armband stability, and >71.58 min `micros()` wraparound verified on live bike | ✅ PASS |
| Live multi-client ride coexistence (**Xert** + **Echelon Fit** + **Echelon Beat HR armband** + `/ws/echenon/live`) with matching telemetry | ✅ PASS |

### **Verdict: ✅ PASS — Live Bike Ride (Xert + Echelon Fit + HR Armband), Hardware GATT, Calibration Rig Sweep, Timing, WebSocket & All Remediations Verified (`v0.3.0-dev.22`)**
