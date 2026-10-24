# Phase 3 — Calibration Rig Characterization (PR-02)

This directory holds the Phase 3 measurement evidence: automated
calibration rig characterization data from the ESP32-controlled encoder
synthesis and ADB-verified resistance sweep tests.

Phase 3 captures the empirical parameters needed for WO-02.7 (the full
90-minute calibration sweep) and validates the CalibrationRig firmware
module.

## Context

Phase 3 follows Phase 0 (PR-01 passive signal characterization) and
Phase 2 (PR-03 BLE coexistence). Where Phase 0 used oscilloscopes and
manual knob turns, Phase 3 uses the ESP32 itself as a signal generator
— synthesizing quadrature encoder edges and verifying the console's
response via ADC (pot wiper) and ADB (Echelon Fit app screen scraping).

## Evidence file naming

```
C<test>-20261024-<short-description>.<ext>
```

- `C<test>` denotes the characterization test number (`C01`…`C04`)
- `<ext>` is `.csv` for data, `.py` for test scripts, `.md` for reports

## Register

| # | Test | Purpose | Evidence files | Result |
|---|---|---|---|---|
| C01 | Edges per level | Verify how many quadrature edges = 1 console level change | `C01-20261024-edges-per-level-report.md`, `evidence/chartest1_edges_per_level.csv` | **8 edges = 1 level** (100% match across 4 levels) |
| C02 | Step interval | Find reliable inter-edge timing for synthesized quadrature | (inline in C01 report) | **150ms reliable** (100ms, 150ms, 200ms all work) |
| C03 | Full profile sweep | Complete 1→32→1 automated sweep with ADB verification | `C03-20261024-full-sweep-report.md`, `evidence/chartest3_full_sweep.csv` | **100% ADB match** both directions, 63 data points |
| C04 | Settle time + pulse width | Bang-bang settle profiling and minimum pulse width | `C04-20261024-settle-pulse-report.md`, `evidence/chartest4_settle_time.csv`, `evidence/char_test4_settle_time.py`, `evidence/char_test4c_pulse_width.py` | **2.8s max settle** (level 15), **25–50ms min pulse width**, 75ms anomaly (drops 1 level, 3/3 reproducible) |
