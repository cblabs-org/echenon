---
schema_version: 1
wo_id: WO-05.1
pr_id: PR-05
title: FTMS v1.0 Conformance Citation, Unified BikeMetrics & FTMSData Packet Encoder
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/LevelMapping.h
- src/echenon/LevelMapping.cpp
- src/echenon/PowerEngine.h
- src/echenon/PowerEngine.cpp
- src/echenon/FTMSData.h
- test/test_native/test_level_mapping.cpp
- test/test_native/test_power_engine.cpp
- test/test_native/test_ftms_server.cpp
- docs/power-physics-model.md
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: aa9a454413d7ae4074da14fd2a063618207d0dbbb062e5a94f4d3f9484cd9f46
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_89357444404f
  commit_sha: 9b7e5fef77dc2da98ba8917dbf0c2308a61b6e8c
  files_modified:
  - docs/power-physics-model.md
  - src/echenon/FTMSData.h
  - src/echenon/LevelMapping.cpp
  - src/echenon/LevelMapping.h
  - src/echenon/PowerEngine.cpp
  - src/echenon/PowerEngine.h
  - test/test_native/test_ftms_server.cpp
  - test/test_native/test_level_mapping.cpp
  - test/test_native/test_power_engine.cpp
  verification_summary: 'pio test -e native: ================ 142 test cases: 142
    succeeded in 00:00:00.309 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-05.1: FTMS v1.0 Conformance Citation, Unified BikeMetrics & FTMSData Packet Encoder

## 1. Objective & Scope

Implement the **Unified First-Principles Metric & Conversion Architecture** (`docs/power-physics-model.md` §8) across `src/echenon/LevelMapping.h` and `src/echenon/PowerEngine.h`, and create the pure header-only FTMS v1.0 binary packet encoder in `src/echenon/FTMSData.h` with full native unit test coverage.

## 2. Detailed Requirements

### 2.1 Forward & Inverse Metric Conversions (`src/echenon/LevelMapping.h` & `src/echenon/PowerEngine.h`)
1. **Normalized Wiper/Cam Position ($\tilde{p} \in [0.0, 1.0]$) Helpers (`LevelMapping.h`):**
   - `float echelonContinuousToNormalized(float continuous_level)`: returns $\text{clamp}\!\left(\frac{\ell - 1.0\text{f}}{31.0\text{f}},\; 0.0\text{f},\; 1.0\text{f}\right)$ (returns `0.0f` for non-finite inputs).
   - `float normalizedToEchelonContinuous(float p_tilde)`: returns $1.0\text{f} + 31.0\text{f} \cdot \text{clamp}(p_{\text{tilde}}, 0.0\text{f}, 1.0\text{f})$.
   - `uint8_t normalizedToInternal(float p_tilde)`: returns $\text{round}(254.0\text{f} \cdot \text{clamp}(p_{\text{tilde}}, 0.0\text{f}, 1.0\text{f})) \in \{0..254\}$.
   - `float internalToNormalized(uint8_t internal)`: returns `0.0f` if `internal == INTERNAL_RESISTANCE_UNCALIBRATED` (`255`), else $\text{clamp}(\text{internal} / 254.0\text{f},\; 0.0\text{f},\; 1.0\text{f})$.
2. **Peloton `0–100` Power-Equivalence Mapping (`PowerEngine.h` & `LevelMapping.h`, `SPEC.md` §5.3):**
   - Reference cadence constant: `constexpr float PELOTON_REF_CADENCE_RPM = 80.0f;` ($\left(\frac{80}{60}\right)^2 \times 60 = 106.66667\text{f}$).
   - `uint8_t computePelotonResistanceFromNormalized(float p_tilde, const PowerPhysicsParams& params = PowerPhysicsParams{})`:
     - Evaluates `Physics P3r` reference power $P_{\text{ref}} = P(\text{clamp}(p_{\text{tilde}}, 0.0\text{f}, 1.0\text{f}),\; 80.0\text{f})$.
     - Solves Peloton's power equation $P_{\text{pel}}(R_{\text{pel}}, 80) = (0.000185 \cdot R_{\text{pel}}^{2.7} + 0.1) \times 106.66667 = P_{\text{ref}}$:
       $$R_{\text{pel}} = \text{clamp}\!\left(\text{round}\!\left(\left[\frac{\max(P_{\text{ref}} / 106.66667\text{f} - 0.1\text{f},\; 0.0\text{f})}{0.000185\text{f}}\right]^{\!1.0\text{f} / 2.7\text{f}}\right),\; 0,\; 100\right)$$
   - `uint8_t computePelotonResistance(float continuous_level, const PowerPhysicsParams& params = PowerPhysicsParams{})`: convenience wrapper taking continuous Echelon level $\ell \in [1.0, 32.0]$ (returns `0` if $\ell < 1.0\text{f}$ or non-finite).
   - `uint8_t internalToPeloton(uint8_t internal)` in `LevelMapping.h`: returns `0` if `internal == 255`, else evaluates the Peloton equivalence formula for $\tilde{p} = \text{internal} / 254.0\text{f}$.
3. **Virtual Road Speed $v(P)$ (`PowerEngine.h`):**
   - `float computeVirtualSpeedKph(float power_watts)`:
     - Solves the flat-road cycling equilibrium cubic $0.245\,v^3 + 3.53\,v = P$ ($v$ in $\text{m/s}$, $v_{\text{km/h}} = 3.6\,v$).
     - For $P \le 0.0\text{f}$ or non-finite $P$, returns `0.0f`.
     - Uses Newton-Raphson (6 iterations starting from $v_0 = \text{cbrtf}(P / 0.245\text{f})$) and returns $3.6\text{f} \cdot v \ge 0.0\text{f}$.
4. **v2 Closed-Loop Inverse Control Laws (`PowerEngine.h`):**
   - `float targetPowerToNormalized(float target_watts, float cadence_rpm, const PowerPhysicsParams& params = PowerPhysicsParams{})`:
     - Exact closed-form analytical inverse of `Physics P3r` (§8.4 of `docs/power-physics-model.md`). Returns `0.0f` if `cadence_rpm <= 0.0f`, `target_watts <= 0.0f`, or non-finite.
   - `float pelotonToNormalized(float peloton_level, const PowerPhysicsParams& params = PowerPhysicsParams{})`:
     - Evaluates $P_{\text{ref}} = (0.000185\text{f} \cdot \text{powf}(\text{clamp}(R_{\text{pel}}, 0.0\text{f}, 100.0\text{f}), 2.7\text{f}) + 0.1\text{f}) \times 106.66667\text{f}$ and returns `targetPowerToNormalized(P_ref, 80.0f, params)`.

### 2.2 FTMS v1.0 Binary Packet Encoder (`src/echenon/FTMSData.h`)
Create `src/echenon/FTMSData.h` (pure C++ header, no NimBLE dependency so it compiles cleanly in `pio test -e native`):
1. **FTMS UUID & Feature Constants (`SPEC.md` §7.2–§7.4):**
   - `constexpr uint16_t FTMS_SERVICE_UUID = 0x1826;`
   - `constexpr uint16_t FTMS_FEATURE_CHAR_UUID = 0x2ACC;`
   - `constexpr uint16_t FTMS_INDOOR_BIKE_DATA_UUID = 0x2AD2;`
   - `constexpr uint16_t FTMS_CONTROL_POINT_UUID = 0x2AD9;`
   - `constexpr uint32_t FTMS_MACHINE_FEATURES_V1 = 0x00004482u;` (Bit 1 Cadence | Bit 7 Resistance Level | Bit 10 Heart Rate | Bit 14 Power Measurement)
   - `constexpr uint32_t FTMS_TARGET_FEATURES_V1 = 0x00000000u;` (Read-only in v1)
   - `constexpr uint16_t FTMS_INDOOR_BIKE_FLAGS_V1 = 0x0264u;` (Bit 0 = 0 Instantaneous Speed present, Bit 2 = 1 Instantaneous Cadence present, Bit 5 = 1 Resistance Level present, Bit 6 = 1 Instantaneous Power present, Bit 9 = 1 Heart Rate present)
   - `constexpr size_t FTMS_FEATURE_PACKET_SIZE = 8;`
   - `constexpr size_t FTMS_INDOOR_BIKE_PACKET_SIZE = 11;`
2. **Packet Encoding Functions:**
   - `void encodeFitnessMachineFeature(uint8_t out[8], uint32_t machine_features = FTMS_MACHINE_FEATURES_V1, uint32_t target_features = FTMS_TARGET_FEATURES_V1)`: writes `[0x82, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]` in little-endian order.
   - `struct IndoorBikeTelemetry { float speed_kph = 0.0f; float cadence_rpm = 0.0f; uint8_t internal_resistance = 0; int16_t power_watts = 0; uint8_t heart_rate_bpm = 0; };`
   - `size_t encodeIndoorBikeData(const IndoorBikeTelemetry& t, uint8_t out[11])`:
     - Bytes `0..1`: `Flags = 0x0264` (`[0x64, 0x02]`)
     - Bytes `2..3`: `Instantaneous Speed` (`uint16_t`, resolution `0.01 km/h`, i.e. $\text{round}(\text{clamp}(\text{speed\_kph}, 0.0\text{f}, 655.35\text{f}) \times 100.0\text{f})$)
     - Bytes `4..5`: `Instantaneous Cadence` (`uint16_t`, resolution `0.5 RPM`, i.e. $\text{round}(\text{clamp}(\text{cadence\_rpm}, 0.0\text{f}, 32767.5\text{f}) \times 2.0\text{f})$)
     - Bytes `6..7`: `Resistance Level` (`int16_t`, `0` if `internal_resistance == 255`, else `static_cast<int16_t>(internal_resistance)`)
     - Bytes `8..9`: `Instantaneous Power` (`int16_t`, clamped to `[0, 4000]`)
     - Byte `10`: `Heart Rate` (`uint8_t`, BPM; `0` when no strap / stale)
     - Returns `11`.

### 2.3 Native Unit Tests
- Extend `test/test_native/test_level_mapping.cpp` and `test/test_native/test_power_engine.cpp` to verify:
  - Monotonicity and boundary values (`L1 -> 28`, `L16 -> 42`, `L20 -> 51`, `L24 -> 62`, `L28 -> 74`, `L32 -> 84`) of `computePelotonResistance` and `internalToPeloton`.
  - Virtual road speed `computeVirtualSpeedKph` at `0 W` (`0.0 km/h`), `100 W` (`~23.6 km/h`), `200 W` (`~31.2 km/h`), `500 W` (`~43.8 km/h`), and verify $0.245(v/3.6)^3 + 3.53(v/3.6) \approx P$ within $10^{-3}\text{ W}$.
  - Round-trip accuracy of `targetPowerToNormalized` and `pelotonToNormalized` against `computePowerPhysics` and `computePelotonResistanceFromNormalized`.
- Create `test/test_native/test_ftms_server.cpp` testing `encodeFitnessMachineFeature` and `encodeIndoorBikeData` across normal riding, coasting (`0 RPM`, `0 W`), uncalibrated resistance (`255 -> 0`), missing HR (`0 BPM`), and clamping bounds.

## 3. Acceptance Criteria
- `pio test -e native` compiles cleanly and passes 100% of unit tests including the new `BikeMetrics`, Peloton `0–100`, Virtual Speed $v(P)$, v2 inverse laws, and `FTMSData` packet encoding tests.

## Completion Report

**Status:** `ESCALATED_TO_PA`

### Escalation Request

All implementation code is complete and all **142 native unit tests pass** (0 failures). However, `pa wo verify WO-05.1` fails the scope hygiene check because two `.cpp` implementation files that were necessarily modified are **not listed in `allowed_files`**:

- `src/echenon/LevelMapping.cpp` — contains `internalToPeloton()` implementation
- `src/echenon/PowerEngine.cpp` — contains `computePelotonResistanceFromNormalized()`, `computePelotonResistance()`, `computeVirtualSpeedKph()`, `targetPowerToNormalized()`, `pelotonToNormalized()` implementations

These are the non-inline function bodies for declarations in `LevelMapping.h` and `PowerEngine.h` (both in `allowed_files`). The `.cpp` files must be included in `allowed_files` for verification to pass.

Additionally, `.agents/plugins/principal-architect-workspace/pr-05-ftms-broadcast/PROJECT.md` is flagged as modified in the working tree.

**Requested Action:** Add `src/echenon/LevelMapping.cpp` and `src/echenon/PowerEngine.cpp` to `allowed_files` in the WORKORDER frontmatter, then re-run verification.

### Implementation Summary
- **Files Modified:** `LevelMapping.h`, `LevelMapping.cpp`, `PowerEngine.h`, `PowerEngine.cpp`, `FTMSData.h` (NEW), `test_level_mapping.cpp`, `test_power_engine.cpp`, `test_ftms_server.cpp` (NEW)
- **Test Results:** 142/142 passed (`pio test -e native`, exit code 0)
