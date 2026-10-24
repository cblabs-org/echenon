---
schema_version: 1
wo_id: WO-02.7
pr_id: PR-02
title: "Rig Sweep (90-min)"
complexity: interactive
status: COMPLETED
allowed_files:
  - tools/calibration-rig/
  - src/echenon/
  - test/
  - docs/
verification_command: "./.pio/build/native/program"
---

# WO-02.7 — Rig Sweep (90-min)

## 1. Objective

Run the full 320-point automated calibration sweep (32 levels × 10 cadence steps, 40–130 RPM) against the Echelon EX-3 and Echelon Fit app via `calibration_sweep.py`, commit the empirical CSV, and update `LevelMapping` to the Phase 3 fit (`pot = 37.26 * level + 120.0`).

## 2. Completion Report

- **Commit:** `4ae2a9ff404d6c89e59fb14a5229e9aa445b77da` (and `cce3b5f8d32b2d4eb4ebf681ded79eb909996cf8`, tagged `v0.3.0-dev.16`)
- **Sweep result:** 320/320 points captured over 74 minutes (`docs/measurements/phase3/evidence/sweep_20261024.csv`), zero HTTP/ADB errors.
- **Power range:** 12 W (L1 / 40 RPM) to 861 W (L32 / 130 RPM).
- **Phase 3 fit applied:** `LevelMapping` updated to `B03_SLOPE = 37.26f`, `B03_INTERCEPT = 120.0f`, `WORKING_RANGE_MIN = 157`, `WORKING_RANGE_MAX = 1313` (R² = 0.9998).
