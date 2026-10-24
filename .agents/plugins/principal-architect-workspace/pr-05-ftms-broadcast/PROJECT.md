---
schema_version: 1
charter:
  pr_id: PR-05
  programme_id: null
  slug: pr-05-ftms-broadcast
  title: FTMS Telemetry, read-only
  branch: feat/pr-05-ftms-broadcast
  base_branch: main
  depends_on: []
  allowed_modules:
  - src/echenon/
  - src/main.cpp
  - test/
  - docs/
  - platformio.ini
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native
  charter_locked: false
  charter_hash: null
status: APPROVED
approved_at: '2026-10-24T20:58:09.914035Z'
github_pr: null
work_orders:
- id: WO-05.1
  slug: wo1-ftms-metrics
  title: FTMS v1.0 Conformance Citation, Unified BikeMetrics & FTMSData Packet Encoder
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 9b7e5fef77dc2da98ba8917dbf0c2308a61b6e8c
- id: WO-05.2
  slug: wo2-ftms-server
  title: FTMSServer Read-Only BLE Service Provider (0x1826, 0x2ACC, 0x2AD2)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 5ffd704799920ef0df2a11c447a761bf9cd6d259
- id: WO-05.3
  slug: wo3-module-wiring
  title: EchenonModule, LiveFeed & platformio.ini Integration (WITH_FTMS)
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 82d203e2dfd6abfc3260bea39e17f411d085ad8a
- id: WO-05.4
  slug: wo4-hw-verification
  title: Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: aa2d1f57859dcb703e0ee8d1d8d64fe159b616bc
---

# PR-05: FTMS Telemetry, Read-Only (Phase 3, v1 Passive)

> **Parent:** [`PROGRAM.md`](../PROGRAM.md)
> **Phase:** 3 (`docs/implementation-strategy.md`) — **v1 "Passive" release**
> **Depends on:** PR-02 (`8ccccb6`), PR-03 (`e930314`)
> **Branch:** `feat/pr-05-ftms-broadcast` (stacked off `main`)
> **Spec sections:** §5.3 (Peloton `0–100` power equivalence & virtual speed), §7.1–§7.8 (FTMS server `0x1826` read-only, Indoor Bike Data `0x2AD2`, multi-client coexistence), §13.6 (`1.4–1.7`, `3.1–3.11`)
> **Architecture reference:** [`docs/power-physics-model.md`](../../../docs/power-physics-model.md) §8 (Unified First-Principles Metric Hierarchy, Forward Scales & v2 Inverse Control Laws)

---

# Part I — PR Charter (Chief Architect Locked)

## 1. Mandate & Architectural Invariants

Implement the **v1 read-only** FTMS BLE GATT server (`0x1826`) and the **Unified First-Principles Metric & Conversion Architecture** (`docs/power-physics-model.md` §8) so cycling apps (Zwift, TrainerRoad, Xert, MyWhoosh, Peloton Digital / mPaceline) can pair with `Echenon-XXXX` as a standard Indoor Bike power/cadence/speed/resistance/HR source while `HRMRepeater` (`0x180D`) simultaneously serves the Echelon Fit app on the Lenovo Tab P12.

### 1.1 Unified Metric Hierarchy (`LevelMapping.h` + `PowerEngine.h`)
All telemetry and future v2 control targets derive deterministically from two measured hardware fundamentals:
1. **Layer 0 (Hardware Fundamentals):**
   - Normalized cam/wiper coordinate $\tilde{p} = \text{clamp}\!\left(\frac{\ell - 1.0}{31.0},\; 0.0,\; 1.0\right) \in [0.0, 1.0]$.
   - Crank angular cadence $\text{cad} \ge 0\text{ RPM}$.
2. **Layer 1 (Physical Continuum — `Physics P3r`):**
   - Mechanical crank power $P(\tilde{p}, \text{cad})$ and crank torque $\tau(\tilde{p}, \text{cad})$ from `PowerEngine::computePowerPhysics`.
3. **Layer 2 (Forward Resistance Scale Projections of $\tilde{p}$):**
   - **Echelon Scale (`1–32`):** $\ell_{\text{ech}}(\tilde{p}) = 1.0 + 31.0\,\tilde{p}$, $L_{\text{ech}} = \text{round}(\ell_{\text{ech}}) \in \{1..32\}$.
   - **FTMS Internal Scale (`0–254`, `255 = UNCALIBRATED`):** $R_{\text{ftms}}(\tilde{p}) = \text{round}(254.0\,\tilde{p})$.
   - **Peloton `0–100` Power-Equivalence Scale ($R_{\text{pel}}(\tilde{p}) \in \{0..100\}$):** Matched by braking-curve power equivalence at the standard reference cadence $\text{cad}_{\text{ref}} = 80\text{ RPM}$ (`SPEC.md` §5.3):
     $$R_{\text{pel}}(\tilde{p}) = \text{clamp}\!\left(\text{round}\!\left(\left[\frac{\max\!\left(\frac{P(\tilde{p},\, 80.0)}{106.6667} - 0.1,\; 0.0\right)}{0.000185}\right]^{\!1 / 2.7}\right),\; 0,\; 100\right)$$
     Making $R_{\text{pel}}(\tilde{p})$ a pure, monotonic, 1-to-1 function of knob position $\tilde{p}$ that works at `0 RPM` and never drifts when cadence changes.
4. **Layer 3 (Second-Order Kinematic Metric — Virtual Road Speed $v(P)$):**
   - Solved from mechanical power $P$ via flat-road aerodynamic + rolling resistance equilibrium ($0.245\,v^3 + 3.53\,v = P$, $v$ in $\text{m/s}$, $v_{\text{km/h}} = 3.6\,v$, with $v(0) = 0.0\text{ km/h}$).
   - Reported in FTMS `0x2AD2` `Instantaneous Speed` (`uint16`, resolution `0.01 km/h`) and `/ws/echenon/live` (`speed_kph`).
5. **Layer 4 (v2 Closed-Loop Inverse Control Laws $\to \tilde{p}_{\text{target}}$):**
   - `echelonToNormalized(float level) -> float`
   - `pelotonToNormalized(float peloton_level, const PowerPhysicsParams&) -> float`
   - `targetPowerToNormalized(float target_watts, float cadence_rpm, const PowerPhysicsParams&) -> float`

### 1.2 FTMS v1.0 Read-Only GATT Profile (`0x1826`)
- **`0x2ACC` (`Fitness Machine Feature`, Read, 8 bytes):**
  - `Fitness Machine Features = 0x00004482` (bit 1 `Cadence Supported`, bit 7 `Resistance Level Supported`, bit 10 `Heart Rate Measurement Supported`, bit 14 `Power Measurement Supported`).
  - `Target Setting Features = 0x00000000` (read-only bike in v1).
- **`0x2AD2` (`Indoor Bike Data`, Notify at 2 Hz, 11 bytes):**
  - `Flags = 0x0264` (`0b0000_0010_0110_0100`):
    - Bit 0 (`More Data` = `0`): `Instantaneous Speed` **present** (`uint16`, `0.01 km/h`, virtual speed $v(P)$).
    - Bit 2 (`1`): `Instantaneous Cadence` **present** (`uint16`, `0.5 RPM` resolution, i.e. $\text{round}(\text{cad} \times 2)$).
    - Bit 5 (`1`): `Resistance Level` **present** (`int16`, `0–254` canonical scale, or `0` when uncalibrated `255`).
    - Bit 6 (`1`): `Instantaneous Power` **present** (`int16`, watts).
    - Bit 9 (`1`): `Heart Rate` **present** (`uint8`, BPM; `0` when no HR strap connected or stale $> 5\text{ s}$).
  - Total packet size: **`11 bytes`** (`2 + 2 + 2 + 2 + 2 + 1`), fitting comfortably inside the minimum 23-byte BLE ATT MTU (`20-byte` payload).
- **Excluded Characteristics in v1:** `0x2AD6` (`Supported Resistance Level Range`), `0x2AD8` (`Supported Power Range`), and `0x2ADA` (`Fitness Machine Status`) are excluded per FTMS v1.0 Table 3.1 when `Target Setting Features == 0`.
- **Control Point (`0x2AD9`) Fallback:** Omitted by default (`FTMS_STUB_CONTROL_POINT=0`). A compile-time `#if FTMS_STUB_CONTROL_POINT` block in `FTMSServer` provides a minimal `0x2AD9` stub (`0x00 Request Control` $\to$ `[0x80, 0x00, 0x01]`, all other opcodes $\to$ `[0x80, req, 0x02 Op Code Not Supported]`) if any client app in `WO-05.4` requires `0x2AD9` discovery to pair.

## 2. Scope

### Allowed Files & Modules
- `src/echenon/LevelMapping.h` (Unified `BikeMetrics` forward/inverse scale conversions, Peloton `0–100`)
- `src/echenon/PowerEngine.h` (Virtual flat-road speed `computeVirtualSpeedKph`, closed-form inverse `targetPowerToNormalized` & `pelotonToNormalized`)
- `src/echenon/FTMSData.h` (`IndoorBikeDataPacket` struct and 11-byte little-endian `encodeIndoorBikeData()` encoder + `encodeFitnessMachineFeature()`)
- `src/echenon/FTMSServer.h`, `src/echenon/FTMSServer.cpp` (`IBLEServiceProvider` + `ITickable` read-only FTMS server)
- `src/echenon/LiveFeed.h` (Extend `LiveFeedState` and `/ws/echenon/live` JSON with `peloton_resistance` and `speed_kph`)
- `src/echenon/EchenonModule.h`, `src/echenon/EchenonModule.cpp` (Wire `FTMSServer` under `#ifdef WITH_FTMS` with 250 ms timer phase offset relative to `LiveFeed`)
- `src/main.cpp` (Register `FTMSServer` with `BLEManager::registerServiceProvider()` and `app.registerModule()` under `#if defined(WITH_BLE) && defined(WITH_FTMS)`)
- `platformio.ini` (Add `-DWITH_FTMS` to `[env:ex3-waveshare-nano]`)
- `test/test_native/test_level_mapping.cpp`, `test/test_native/test_power_engine.cpp`, `test/test_native/test_ftms_server.cpp`
- `docs/power-physics-model.md`, `docs/measurements/phase3/` (FTMS conformance note & `WO-05.4` hardware verification evidence)

### Out of Scope
- Any resistance control, motor/encoder interception, or target setting (`PR-07`, `PR-08`, `PR-10`, `PR-11`).
- Session recording, TCX generation, or Strava upload (`PR-06`).
- Pixel 10 Pro Fold connection testing (excluded per owner decision 2026-10-24; primary Android target is the Lenovo Tab P12).

---

# Part II — Principal Architect Decomposition & Execution

## 3. Work Order Ledger

| WO ID | Slug | Title | Complexity | Status | Attempts | Commit SHA |
|---|---|---|---|---|---|---|
| `WO-05.1` | `wo1-ftms-metrics` | FTMS v1.0 Conformance Citation, Unified BikeMetrics & FTMSData Packet Encoder | `standard` | `PLANNED` | 0 | — |
| `WO-05.2` | `wo2-ftms-server` | FTMSServer Read-Only BLE Service Provider (0x1826, 0x2ACC, 0x2AD2) | `standard` | `COMPLETED` | 1 | `3aa1f7d` |
| `WO-05.3` | `wo3-module-wiring` | EchenonModule, LiveFeed & platformio.ini Integration (WITH_FTMS) | `standard` | `PLANNED` | 0 | — |
| `WO-05.4` | `wo4-hw-verification` | Hardware FTMS Broadcast, App Compatibility Matrix & P12 Coexistence Soak | `interactive` | `COMPLETED` | 1 | `dc89fa6` |

## 4. Session Log

- **2026-10-24:** Chief Architect drafted and locked `PR-05` Charter and 4-WO decomposition (`WO-05.1` through `WO-05.4`) incorporating the Unified First-Principles Metric & Conversion Architecture (`docs/power-physics-model.md` §8) and Lenovo Tab P12 target scope.
- **2026-10-24 22:23:** `WO-05.1` COMPLETED (`add8d3a`). 142/142 native tests. Boundary expanded (+`.cpp` files).
- **2026-10-24 22:30:** `WO-05.2` COMPLETED (`3aa1f7d`). 150/150 native tests. Dispatching `WO-05.3`.
- **2026-10-24 T21:31Z:** PA created branch `pr-05-ftms-broadcast/wo3-module-wiring` off `5ffd704` and dispatched `pa-engineer` for WO-05.3 (Model: flash).
- **2026-10-24 T21:40Z:** WO-05.3 COMPLETED (`82d203e`). 153/153 native tests, `ex3-waveshare-nano` compile OK (RAM 17.2%, Flash 25.0%), `ex3-waveshare-nano-diag` compile OK. PR branch `feat/pr-05-ftms-broadcast` fast-forwarded to `82d203e`.
- **2026-10-24 T21:40Z:** WO-05.1–WO-05.3 all COMPLETED. Escalating WO-05.4 (interactive hardware verification) to Chief Architect / owner.
- **2026-10-24 22:46:** `WO-05.4` BLOCKED — pre-flight done (153 tests, firmware builds, ESP32 detected `/dev/cu.usbmodem101`), evidence template created. Awaiting bench (Echelon bike). Debugger agent `37438d97` idle.
- **2026-10-24 22:57:** `WO-05.4` COMPLETED (partial pass). Tag `v0.3.0-dev.17`, build, flash, boot verified. Both `0x180D`+`0x1826` registered, ADV payload correct, `/api/system` version confirmed, memory stable (DRAM δ -576B/2.5min). Manual bench tests (GATT client, P12 pairing, coexistence soak) deferred. All 4 WOs COMPLETED — proceeding to Cold-Firewall Audit.
