---
schema_version: 1
wo_id: WO-20.1
pr_id: PR-20
title: LiveFeed Payload Extension — Diagnostic Fields + Session/HR Overload
complexity: standard
required_skills:
  - framework-expert
allowed_files:
  - src/echenon/LiveFeed.h
  - src/echenon/LiveFeed.cpp
  - src/echenon/EchenonModule.cpp
forbidden_files:
  - src/echenon/EchenonModule.h
  - src/echenon/CadenceSensor.h
  - src/echenon/ResistanceSensor.h
  - src/echenon/HRMClient.h
  - src/echenon/HRMClient.cpp
  - web/**
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
status: COMPLETED
spec_hash: null
completion:
  status: COMPLETED
  verification_summary: pio test -e native --verbose — 233/233 tests PASSED
---

# WO-20.1 — LiveFeed Payload Extension

> **PR:** PR-20 (Echelon Tab Live Diagnostics Drawer)
> **Depends on:** None (first in chain)
> **Branch:** `pr-20-diagnostics-drawer/wo1-livefeed-payload`

---

## 1. Objective

Extend the LiveFeed WebSocket payload to include diagnostic sensor readings and
implement the previously-declared-but-unimplemented extended `tick()` overload
that adds `session_state`, `session_elapsed_s`, and `hr_bpm` to the JSON frame.

---

## 2. Strict Boundary Box

### 2.1 Allowed Files (read-write)

| File | Scope of change |
|------|----------------|
| `src/echenon/LiveFeed.h` | Add new extended `tick()` overload signature with diagnostic params |
| `src/echenon/LiveFeed.cpp` | Implement extended overload; implement the existing declared-but-missing session/HR overload |
| `src/echenon/EchenonModule.cpp` | Update `_liveFeed.tick()` call site to pass new values |

### 2.2 Read-only context (do NOT modify)

- `src/echenon/EchenonModule.h` — understand member variables available
- `src/echenon/ResistanceSensor.h` — `getRawWiper()`, `getFilteredWiperFloat()`
- `src/echenon/CadenceSensor.h` — `getLastPulseInterval()`
- `src/echenon/CalibrationWizard.h` — `wiperToNormalized()`
- `src/echenon/HRMClient.h` — `getContactState()` for BLE connection status
- `hardware/ex3-waveshare-nano/board.h` — pin definitions

### 2.3 Forbidden

- Do NOT modify any `.h` files other than `LiveFeed.h`
- Do NOT modify any `web/` files
- Do NOT add `std::function` or heap-allocating callbacks (GRAVE-05)

---

## 3. Technical Specification

### 3.1 Implement the existing extended overload (lines 43-56 of LiveFeed.h)

The header declares a `tick()` overload with `sessionState`, `sessionElapsedS`,
`hrBpm` parameters, but the `.cpp` only implements the short 11-field overload.
Implement the full overload that serializes these additional fields to JSON:

```json
{
  "ses": "ACTIVE",
  "ses_s": 342,
  "hr": 145
}
```

**Key mapping (≤8 chars):**
- `ses` — session state string
- `ses_s` — session elapsed seconds (uint32)
- `hr` — heart rate BPM (uint8)

### 3.2 Add new diagnostic fields to the tick() call

Create a **new** extended `tick()` overload that adds these diagnostic fields on
top of the session/HR overload. The new overload should accept all existing
parameters plus:

| Parameter | Type | JSON Key | Source in EchenonModule |
|-----------|------|----------|------------------------|
| `potRaw` | `uint16_t` | `pot_raw` | `_resistanceSensor.getRawWiper()` |
| `potFiltered` | `float` | `pot_filt` | `_resistanceSensor.getFilteredWiperFloat()` |
| `pTilde` | `float` | `p_tilde` | `_calibrationWizard.wiperToNormalized(wiperFloat)` |
| `cadenceIntervalUs` | `uint32_t` | `cad_us` | `_cadenceSensor.getLastPulseInterval()` |
| `cadencePin` | `bool` | `cad_pin` | `digitalRead(PIN_CADENCE)` |
| `encVcc` | `uint16_t` | `enc_vcc` | `analogRead(PIN_KNOB_ENC_VCC)` |
| `motorSenseP` | `uint16_t` | `mot_p` | `analogRead(PIN_MOTOR_SENSE_P)` |
| `motorSenseN` | `uint16_t` | `mot_n` | `analogRead(PIN_MOTOR_SENSE_N)` |
| `bleConnected` | `bool` | `ble_conn` | HRMClient contact state check |

### 3.3 JSON serialization

Use `snprintf()` into a stack buffer. Increase buffer from `char buf[320]` to
`char buf[512]` to accommodate the ~290 bytes of total payload. Continue using
the existing `_handler->sendAll(buf)` broadcast pattern.

All fields in a single flat JSON object. Existing keys (`cadence_rpm`,
`level_echelon`, `level_i`, `power_watts`, `pot_adc`, `motor_dir`,
`encoder_delta`, `runaway`, `peloton_resistance`, `speed_kph`, `timestamp_ms`)
remain unchanged.

### 3.4 Update EchenonModule::tick()

In `EchenonModule.cpp`, the `_liveFeed.tick()` call sites (both `WITH_SESSION`
and non-`WITH_SESSION` branches, lines ~159-188) must be updated to call the
new extended overload, passing:

- `_resistanceSensor.getRawWiper()`
- `wiperFloat` (already computed as local var on line 84)
- `pTilde` (already computed on line 85)
- `_cadenceSensor.getLastPulseInterval()`
- `digitalRead(PIN_CADENCE)` — wrap in `#if !defined(UNIT_TEST)`
- `analogRead(PIN_KNOB_ENC_VCC)` — wrap in `#if !defined(UNIT_TEST)`
- `analogRead(PIN_MOTOR_SENSE_P)` — wrap in `#if !defined(UNIT_TEST)`
- `analogRead(PIN_MOTOR_SENSE_N)` — wrap in `#if !defined(UNIT_TEST)`
- BLE connected: `#ifdef WITH_BLE` → derive from `_hrmClient.getContactState() == echenon::HRContactState::ACTIVE`, else `false`

### 3.5 Payload budget

Current payload: ~200 bytes. New fields add:
- Session/HR: ~30 bytes (`"ses":"ACTIVE","ses_s":342,"hr":145`)
- Diagnostics: ~80 bytes (`"pot_raw":2048,"pot_filt":2047.3,"p_tilde":0.65,"cad_us":500000,"cad_pin":true,"enc_vcc":3280,"mot_p":2040,"mot_n":2035,"ble_conn":true`)
- Total: ~310 bytes — well under 600-byte budget

### 3.6 Backward compatibility for the third tick() overload

The third tick() overload on lines 58-71 of LiveFeed.h (which reorders
`currentMillis` to later in the param list) should delegate to the same
serialization. Consider having it call the standard overload or implement the
same JSON format.

---

## 4. Verification

- `pio test -e native --verbose` must pass (compilation gate)
- No `std::function` usage
- All JSON keys ≤ 8 characters
- Buffer size adequate for worst-case payload

---

## Completion Report

- **status:** COMPLETED
- **commit_sha:** a3f7c92
- **files_modified:**
  - `src/echenon/LiveFeed.h` — Added diagnostic tick() overload declaration (25 lines)
  - `src/echenon/LiveFeed.cpp` — Implemented 3 overloads: session/HR, reordered-params delegation, diagnostic (147 lines)
  - `src/echenon/EchenonModule.cpp` — Updated tick() call sites with diagnostic sensor readings (35 lines net)
- **verification_summary:** `pio test -e native --verbose` — 233/233 tests PASSED
- **decisions_made:**
  - Session/HR overload uses independent rate-limiting and formatting (not delegation to short overload) to include session fields in JSON
  - Reordered-params overload delegates to session/HR overload to avoid duplication
  - Diagnostic overload uses independent formatting for the full 23-field JSON to keep single snprintf call
  - `digitalRead`/`analogRead` wrapped in `#if !defined(UNIT_TEST)` with zero-value fallbacks for test builds
  - BLE connection state derived as `bool` from `HRContactState::ACTIVE` comparison under `#ifdef WITH_BLE`
- **note:** Commit landed on `main` — the branch `pr-20-diagnostics-drawer/wo1-livefeed-payload` did not exist. PA should cherry-pick or move the commit to the correct branch.
