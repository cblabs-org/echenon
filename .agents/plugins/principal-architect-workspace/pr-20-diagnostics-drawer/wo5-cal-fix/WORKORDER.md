---
schema_version: 1
wo_id: WO-20.5
pr_id: PR-20
title: Fix Calibration Status — Default/Custom from CalibrationWizard
complexity: mechanical
status: COMPLETED
allowed_files:
- src/echenon/LiveFeed.h
- src/echenon/LiveFeed.cpp
- src/echenon/EchenonModule.cpp
- web/echenon-dashboard.js
forbidden_files:
- src/echenon/CalibrationWizard.h
- src/echenon/CalibrationWizard.cpp
- src/echenon/SessionOverlay.cpp
required_skills: []
verification_command: pio test -e native --verbose
max_attempts: 3
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-20.5 — Fix Calibration Status Row (Default/Custom)

## 1. Objective

The diagnostics drawer calibration row currently derives its state from
`p_tilde !== 0`, which is wrong — `p_tilde` is the normalized knob position
(0.0–1.0) and reads 0 at minimum position, falsely showing "Not Calibrated".

Fix: Add a `cal` boolean field to the LiveFeed WS payload derived from
`_calibrationWizard.isPosDone()`, then update the JS to display
"Default" or "Custom".

## 2. Exact Changes Required

### 2.1 Firmware: Add `calCustom` param to diagnostic tick

**`src/echenon/LiveFeed.h`** — Add `bool calCustom` as the **last** parameter
of the diagnostic tick overload (line ~96, after `bool bleConnected`):

```cpp
              bool bleConnected,
              bool calCustom);
```

**`src/echenon/LiveFeed.cpp`** — In the diagnostic tick implementation:

1. Add `bool calCustom` to the function signature (matching the header).
2. In the `snprintf` format string, append `,\"cal\":%s` just before the
   closing `}`. Use `calCustom ? "true" : "false"` as the format argument.

### 2.2 Call Site: Pass wizard state

**`src/echenon/EchenonModule.cpp`** — In both tick call sites (the
`#ifdef WITH_SESSION` and `#else` blocks, lines ~180 and ~204):

Add `_calibrationWizard.isPosDone()` as the final argument after
`liveBleConn`.

### 2.3 Frontend: Fix JS calibration display

**`web/echenon-dashboard.js`** — Replace the calibration status block
(lines ~175-184):

**Before:**
```js
    var calEl = document.getElementById('echenon-diag-cal');
    if (calEl && data.p_tilde !== undefined) {
      if (data.p_tilde !== 0) {
        calEl.textContent = 'Calibrated';
        calEl.style.color = 'var(--success)';
      } else {
        calEl.textContent = 'Not Calibrated';
        calEl.style.color = 'var(--warning)';
      }
    }
```

**After:**
```js
    var calEl = document.getElementById('echenon-diag-cal');
    if (calEl && data.cal !== undefined) {
      if (data.cal) {
        calEl.textContent = 'Custom';
        calEl.style.color = 'var(--success)';
      } else {
        calEl.textContent = 'Default';
        calEl.style.color = 'var(--text-muted)';
      }
    }
```

## 3. Strict Boundary Box

- ONLY modify the four files listed in `allowed_files`
- Do NOT modify CalibrationWizard, SessionOverlay, or any other files
- JSON key `cal` is ≤8 chars ✓
- Boolean value keeps payload within 600-byte budget ✓
- No `std::function` (GRAVE-05) ✓

## 4. Verification

Run `pio test -e native --verbose` — all 233+ tests must pass (no new tests
required; this is a pass-through field change).
## Completion Report

_No completion report recorded._
