---
schema_version: 1
wo_id: WO-20.2
pr_id: PR-20
title: Dashboard Diagnostics Drawer — Replace Static Status Card
complexity: standard
required_skills:
  - framework-expert
allowed_files:
  - web/echenon-dashboard.js
forbidden_files:
  - src/**
  - include/**
  - hardware/**
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
status: COMPLETED
spec_hash: null
completion:
  status: COMPLETED
  verification_summary: pio test -e native --verbose — 233/233 tests PASSED
---

# WO-20.2 — Dashboard Diagnostics Drawer

> **PR:** PR-20 (Echelon Tab Live Diagnostics Drawer)
> **Depends on:** WO-20.1 (LiveFeed payload must include new fields)
> **Branch:** `pr-20-diagnostics-drawer/wo2-dashboard-drawer`

---

## 1. Objective

Replace the static "Echenon Status" card in the dashboard with a collapsible
diagnostics info-drawer that updates live from the WebSocket stream. Remove the
one-shot REST fetches that are now superseded by WS-driven updates.

---

## 2. Strict Boundary Box

### 2.1 Allowed Files (read-write)

| File | Scope of change |
|------|----------------|
| `web/echenon-dashboard.js` | All changes in this WO are in this single file |

### 2.2 Read-only context (do NOT modify)

- `framework/web/portal/app.css` — the `article.info-drawer` CSS pattern (lines 603-660)
- `framework/web/portal/wifi.js` — reference implementation of info-drawer HTML (lines 116-130)
- `src/echenon/LiveFeed.h` / `.cpp` — to understand the new JSON keys added by WO-20.1

### 2.3 Forbidden

- Do NOT modify any firmware source files
- Do NOT create custom CSS — use the framework's `article.info-drawer` pattern exclusively
- Do NOT add external libraries or dependencies

---

## 3. Technical Specification

### 3.1 Remove the static "Echenon Status" card

In `renderScaffold()`, remove the entire "Echenon Status" card HTML block
(currently lines ~195-216 in the scaffold string). This card contains:
- Bike Connection row with `echenon-bike-dot` / `echenon-bike-status`
- Calibration Status row with `echenon-calibration-status` / `echenon-calibration-link`
- Session State row with `echenon-session-status`

### 3.2 Add the info-drawer after the "Open Ride Page" button

Insert an `<article class="info-drawer">` block after the ride page link card.
Follow the exact framework pattern from `wifi.js`:

```html
<article class="info-drawer">
  <details id="echenon-diag-drawer">
    <summary class="info-summary">
      <span id="echenon-diag-dot" style="
        width:8px; height:8px; border-radius:50%; background:var(--border);
        display:inline-block; margin-right:0.5rem; flex-shrink:0;
      "></span>
      <span id="echenon-diag-label">Bike Status</span>
    </summary>
    <div class="info-drawer-content">
      <!-- Section: Status -->
      <!-- Section: Resistance Knob -->
      <!-- Section: Cadence Sensor -->
      <!-- Section: Motor Sense -->
    </div>
  </details>
</article>
```

### 3.3 Drawer lid (collapsed state)

- **Dot colour:** Driven by `ble_conn` from WS payload
  - `true` → `dot--green` (or `background:var(--success)`) + label "Bike Connected"
  - `false` → `background:var(--border)` + label "Not Connected"
- **Initial state:** Show "Not Connected" until first WS frame arrives
- Update the dot/label in `handleLiveMessage()` on every WS frame

### 3.4 Expanded content — four grouped sections

Use the framework's `info-row` CSS class for key-value rows. Group with
lightweight `<div>` section headers.

#### Section: Status
| Label | Element ID | WS Field | Format |
|-------|-----------|----------|--------|
| Calibration | `echenon-diag-cal` | derived from `p_tilde` | "Calibrated" (green) if non-zero / "Not Calibrated" (yellow) |
| Session | `echenon-diag-ses` | `ses` | "Idle" / "Active" (green) / "Paused" (yellow) / "Ended" (grey) |

#### Section: Resistance Knob
| Label | Element ID | WS Field | Format |
|-------|-----------|----------|--------|
| Raw ADC | `echenon-diag-pot-raw` | `pot_raw` | integer |
| Filtered | `echenon-diag-pot-filt` | `pot_filt` | 1 decimal |
| Normalized | `echenon-diag-ptilde` | `p_tilde` | 3 decimals |
| Enc VCC | `echenon-diag-enc-vcc` | `enc_vcc` | integer + health indicator |
| Enc Δ | `echenon-diag-enc-delta` | `encoder_delta` | integer |

**Health indicator for `enc_vcc`:** If `enc_vcc < 100`, show red (disconnected).
If `100 ≤ enc_vcc < 500`, show yellow (weak signal). Otherwise green.

#### Section: Cadence Sensor
| Label | Element ID | WS Field | Format |
|-------|-----------|----------|--------|
| Pulse μs | `echenon-diag-cad-us` | `cad_us` | integer |
| Pin State | `echenon-diag-cad-pin` | `cad_pin` | "HIGH" / "LOW" |

#### Section: Motor Sense
| Label | Element ID | WS Field | Format |
|-------|-----------|----------|--------|
| Sense + | `echenon-diag-mot-p` | `mot_p` | integer |
| Sense − | `echenon-diag-mot-n` | `mot_n` | integer |
| Direction | `echenon-diag-mot-dir` | `motor_dir` | "FWD" / "REV" / "IDLE" |

### 3.5 Update handleLiveMessage()

Extend `handleLiveMessage(data)` to update all drawer elements. Guard each
update with `if (el && data.field !== undefined)` to avoid errors when elements
are not yet rendered.

Also keep the existing cadence/power/level/HR metric card updates — don't break them.

Update the session state display within the drawer from the `ses` field (replaces
the static `echenon-session-status` element that was in the removed card).

### 3.6 Remove superseded REST fetches

- **Remove `fetchBikeStatus()`** and `updateBikeUI()` — replaced by `ble_conn` from WS
- **Remove `fetchCalibrationStatus()`** and `updateCalibrationUI()` — replaced by `p_tilde` from WS  
- **Keep `fetchSessionStatus()`** — used for initial load before first WS frame, as fallback
- **Keep `fetchLastRide()`** — ride history is not part of the WS stream
- Update `fetchAll()` to remove the deleted fetch calls

### 3.7 Colour indicator helper

Add a small helper function for health-coloured values:

```javascript
function healthColour(value, thresholds) {
  // thresholds = { red: N, yellow: N }
  // Returns CSS colour variable string
  if (value < thresholds.red) return 'var(--error)';
  if (value < thresholds.yellow) return 'var(--warning)';
  return 'var(--success)';
}
```

### 3.8 Session state display

Since the static status card is removed, the session state display moves into
the drawer's Status section. `updateSessionUI()` should be updated to target the
new element ID (`echenon-diag-ses`).

---

## 4. Verification

- `pio test -e native --verbose` must pass (compilation gate — firmware rebuild
  required since PROGMEM assets include this JS file)
- Drawer uses framework `article.info-drawer` CSS pattern — no custom CSS
- All WS field references match WO-20.1 key names
- No external library dependencies
- Existing live metrics card (cadence/power/level/HR) must continue to work


---

## Completion Report

- **Status:** COMPLETED
- **Commit:** `38d7a22`
- **Files modified:** `web/echenon-dashboard.js`
- **Verification:** `pio test -e native --verbose` — 233/233 tests PASSED

### Summary

Replaced the static "Echenon Status" card with a collapsible diagnostics
info-drawer following the framework's `article.info-drawer` CSS pattern.
The drawer lid shows BLE connection state via `ble_conn` from the WebSocket
stream. Four expandable sections (Status, Resistance Knob, Cadence Sensor,
Motor Sense) display raw diagnostic fields from the WO-20.1 LiveFeed payload.
Removed `fetchBikeStatus()`/`updateBikeUI()` and `fetchCalibrationStatus()`/
`updateCalibrationUI()` — both replaced by WS-driven updates. Added
`healthColour()` helper for `enc_vcc` threshold colouring. Existing Live
Metrics card and WS connection logic are unchanged.
