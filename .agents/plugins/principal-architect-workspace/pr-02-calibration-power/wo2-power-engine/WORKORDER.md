---
wo_id: WO-02.2
pr_id: PR-02
programme_id: prog-echenon
title: "PowerEngine + Default Table Structure"
slug: wo2-power-engine
branch: "calibration-power/wo2-power-engine"
base_branch: "calibration-power/wo1-sensors"
complexity: standard
status: IN_PROGRESS
attempts: 1
spec_hash: null
allowed_files:
  - src/echenon/PowerEngine.h
  - src/echenon/PowerEngine.cpp
  - src/echenon/PowerTable.h
  - src/echenon/EchenonModule.h
  - src/echenon/EchenonModule.cpp
  - test/test_native/test_power_engine.cpp
  - platformio.ini                            # native test deps only
verification_command: "pio test -e native && pio run -e ex3-waveshare-nano"
required_skills:
  - /Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md
---

# WO-02.2 — PowerEngine + Default Table Structure

> **Parent:** [PR-02 Calibration + Power Engine](../PROJECT.md)
> **Branch:** `calibration-power/wo2-power-engine` from `calibration-power/wo1-sensors`
> **Charter sections:** §4 of the PR-02 Work Order Ledger

## 1. Objective

Build the per-level × cadence power table engine with bilinear interpolation,
a table data structure with load/save/CSV-parse API, a placeholder default
table, and native unit tests.

## 2. Strict Boundary Box

### Allowed files (create or modify)
- `src/echenon/PowerEngine.h` — new
- `src/echenon/PowerEngine.cpp` — new
- `src/echenon/PowerTable.h` — new (header-only data structure)
- `src/echenon/EchenonModule.h` — modify (add PowerEngine member, getter)
- `src/echenon/EchenonModule.cpp` — modify (call PowerEngine in tick)
- `test/test_native/test_power_engine.cpp` — new
- `platformio.ini` — modify only if native test deps needed

### Read only (reference)
- `src/echenon/LevelMapping.h` — uses the continuous level ℓ
- `src/echenon/CadenceSensor.h` — gets live RPM
- `src/echenon/ResistanceSensor.h` — gets filtered wiper
- `hardware/ex3-waveshare-nano/board.h`

### Out of scope
- Calibration UI (WO-02.3)
- NVS persistence of the table (store the API, but actual NVS calls are
  deferred to WO-02.3 or WO-02.9 where ConfigManager integration lands)
- Peloton mapping (deferred to PR-05)
- WebSocket feed (WO-02.4)

## 3. Specification

### 3.1 PowerTable (SPEC §6.2, §6.3, §6.4)

A header-only data structure in `src/echenon/PowerTable.h`.

```cpp
namespace echenon {

// Table dimensions
constexpr int POWER_TABLE_LEVELS = 32;       // Echelon levels 1–32
constexpr int POWER_TABLE_RPM_BINS = 31;     // 0, 5, 10, ..., 150 RPM
constexpr float POWER_TABLE_RPM_STEP = 5.0f;
constexpr float POWER_TABLE_RPM_MAX = 150.0f;

// Table tier IDs (SPEC §6.3)
enum class PowerTableTier : uint8_t {
    DEFAULT = 1,       // Compiled-in default
    SELECTED = 2,      // Selected built-in or custom CSV upload
    RECALIBRATED = 3   // Updated by recalibration UI
};

struct PowerTable {
    int16_t watts[POWER_TABLE_LEVELS][POWER_TABLE_RPM_BINS];  // 32 × 31
    PowerTableTier tier = PowerTableTier::DEFAULT;
    char id[32] = "";   // Table ID string (e.g. "ex3-default")

    // Reset all watts to 0
    void clear();

    // Parse from CSV blob (level,rpm_0,rpm_5,...,rpm_150 format)
    // Returns true on success
    bool parseCSV(const char* csvData, size_t length);

    // Serialise to a flat byte buffer for NVS storage
    size_t serialise(uint8_t* buf, size_t bufLen) const;

    // Deserialise from a flat byte buffer
    bool deserialise(const uint8_t* buf, size_t bufLen);
};

} // namespace echenon
```

### 3.2 PowerEngine (SPEC §6.2)

```cpp
namespace echenon {

class PowerEngine {
public:
    PowerEngine();

    // Set the active power table
    void setTable(const PowerTable& table);

    // Get the active power table (for inspection, serialisation)
    const PowerTable& getTable() const;

    // Load the compiled-in default table
    void loadDefault();

    // Compute power using bilinear interpolation
    // level: continuous level ℓ (1.0–32.0) from LevelMapping
    // cadenceRPM: current cadence in RPM
    // Returns watts, clamped to max(0, P)
    float computePower(float level, float cadenceRPM) const;

    // Get the last computed power (cached from tick)
    float getLastPower() const { return _lastPower; }

    // Tick: compute power from current sensor readings
    void tick(float level, float cadenceRPM);

private:
    PowerTable _table;
    float _lastPower = 0.0f;
};

} // namespace echenon
```

**Bilinear interpolation:**
- Level axis: Echelon levels 1–32 (array indices 0–31). The continuous
  level ℓ maps to a fractional index `levelIdx = ℓ - 1.0` (clamped 0–31).
- RPM axis: 0, 5, 10, ..., 150 RPM (array indices 0–30). Live cadence maps
  to `rpmIdx = cadenceRPM / 5.0` (clamped 0–30).
- Standard bilinear formula between the four surrounding grid points.
- Zero cadence → 0 W (regardless of table values).
- Result clamped to `max(0, P)`.

### 3.3 Default Table (placeholder)

Generate the default table from the physics heuristic:
`P ≈ k × level × RPM²` with `k = 0.0004` (or a coefficient that gives
~100–200 W at level 16 / 80 RPM, which is a reasonable sanity range).

This is explicitly a **placeholder** — the real table comes from the rig
sweep in WO-07. The placeholder is used for testing the engine and for
development before the rig runs.

### 3.4 EchenonModule Integration

**Additions to `EchenonModule.h`:**
- Add `#include "echenon/PowerEngine.h"` (conditionally or unconditionally)
- Add `echenon::PowerEngine _powerEngine;` member
- Add `float getPowerWatts() const` getter
- Add `echenon::PowerEngine& powerEngine()` accessor

**Additions to `EchenonModule.cpp`:**
- In `tick()`, after sensor reads: compute the continuous level from the
  filtered wiper via `LevelMapping::wiperToLevel()`, then call
  `_powerEngine.tick(level, _cadenceSensor.getRPM())`.
- Load the default table on first tick (guarded).

### 3.5 Native Tests (test_power_engine.cpp)

Required test cases:
1. **Known grid point**: set a table cell to a known value, query at that
   exact level and RPM — should return that value.
2. **Bilinear interpolation**: query between four grid points with known
   values, verify the interpolated result.
3. **Zero cadence**: always returns 0 W regardless of level.
4. **Boundary: level 1 / RPM 0**: returns 0.
5. **Boundary: level 32 / RPM 150**: returns the table corner value.
6. **Clamping**: power is never negative (if table has negative entries).
7. **Level interpolation**: continuous level 16.5 interpolates between
   level 16 and level 17 rows.
8. **CSV parse**: parse a valid CSV string, verify table values.
9. **Serialise/deserialise round-trip**: serialise a table, deserialise,
   compare all cells.
10. **Default table sanity**: load the default table, verify level 16 / 80
    RPM gives a reasonable value (~50–200 W range).

## 4. Verification

1. `pio test -e native` — all native tests pass (including existing tests
   and the new test file)
2. `pio run -e ex3-waveshare-nano` — target compiles with zero errors

## 5. Standing Instructions

- Read `.agents/rules/` files 00–04 as first-class law
- Rule 03: Tag → Build → Flash (no flashing in this WO)
- Rule 04: use PlatformIO MCP tools, not direct CLI
- `board.h` is read-only
- Framework submodule drift is expected — NEVER commit the submodule
- Known gotcha: include `<cstddef>` before `BOARD_HEADER` in standalone
  headers (board.h uses `size_t` without including it)
