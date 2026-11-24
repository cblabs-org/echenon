---
schema_version: 1
charter:
  pr_id: PR-20
  programme_id: prog-default
  slug: pr-20-diagnostics-drawer
  title: Echelon Tab Live Diagnostics Drawer
  branch: feat/pr-20-diagnostics-drawer
  base_branch: main
  depends_on:
  - PR-16
  allowed_modules:
  - src/echenon/
  - web/
  - test/
  - include/portal_assets/
  - docs/
  required_skills:
  - framework-expert
  verification_floor:
  - pio test -e native --verbose
  charter_locked: true
  charter_hash: 2cfe9f44515bf9eb1e2b67fd876235190df70286542d4480b13371ae102c8892
status: GRADUATED
approved_at: '2026-10-27T23:07:00Z'
github_pr: 15
work_orders:
- id: WO-20.1
  slug: wo1-livefeed-payload
  title: LiveFeed Payload Extension
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: 475145d5362b4c18048efae4039c4e4e34fc74a8
- id: WO-20.2
  slug: wo2-dashboard-drawer
  title: Dashboard Diagnostics Drawer
  complexity: standard
  status: COMPLETED
  attempts: 1
  commit_sha: d2dfee989ebd310dac082478aa8b24f60542dd15
- id: WO-20.3
  slug: wo3-hw-verification
  title: Hardware Verification
  complexity: interactive
  status: COMPLETED
  attempts: 1
  commit_sha: 61e95efbbaf500d11ba02e13310699a08984a4a6
- id: WO-20.4
  slug: wo4-linker-fix
  title: Remove duplicate LiveFeed tick() overloads (linker fix)
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: cf8a0cdc85375ab13436268a8a16d0aacd595dac
- id: WO-20.5
  slug: wo5-cal-fix
  title: Fix Calibration Status — Default/Custom from CalibrationWizard
  complexity: mechanical
  status: COMPLETED
  attempts: 1
  commit_sha: 138156c0096fd141f717da762903c0a009b58150
---

# PR-20 — Echelon Tab Live Diagnostics Drawer

> **Programme:** Echenon Firmware
> **Target Base:** `main` (post PR-16)
> **Depends On:** PR-16 ✅
> **Severity:** Enhancement — hardware connection diagnostics for beta testers

---

## 1. PR Charter: Scope, Goals, & Non-Goals

### 1.1 Problem Statement

The Echelon dashboard tab currently has a static "Echenon Status" card showing
Bike Connection, Calibration Status, and Session State. These values are
fetched once on tab open via REST and never refreshed — they go stale
immediately. There is no way to inspect raw sensor signals (ADC voltages,
cadence pulses, encoder state, motor sense) from the portal, making it
impossible to diagnose bad hardware connections without a serial console.

Additionally, `LiveFeed.h` declares an extended `tick()` overload that
includes `session_state`, `session_elapsed_s`, and `hr_bpm` in the WebSocket
payload, but the `.cpp` only implements the shorter 11-field overload. These
fields never reach the browser via WebSocket.

### 1.2 Core Scope & Goals

1. **Remove the static Echenon Status card** from the dashboard scaffold.

2. **Add a collapsible Hardware Diagnostics info-drawer** at the bottom of
   the Echelon dashboard tab using the framework's `<details>/<summary>`
   `article.info-drawer` CSS pattern:
   - **Drawer lid:** Single health-indicator dot + "Bike Connected" /
     "Not Connected" label. Collapsed by default.
   - **Expanded content** (three grouped sections):
     - **Status:** Calibration state (32/32 levels), Session state (Idle/Active/Paused)
     - **Resistance Knob:** `pot_raw`, `pot_filt`, `p_tilde`, `enc_vcc`, `enc_d`
     - **Cadence Sensor:** `cad_us`, `cad_pin`
     - **Motor Sense:** `mot_p`, `mot_n`, `mot_dir`
   - Values update live from the 2 Hz WebSocket stream (not REST polling).
   - Colour indicators: green when values look sane, grey/red when they
     suggest a disconnected plug.

3. **Extend the LiveFeed WebSocket payload** with ~10 new short-keyed fields
   (~60 bytes additional per frame):

   | Key | Type | Source |
   |-----|------|--------|
   | `pot_raw` | uint16 | `ResistanceSensor::getRawWiper()` |
   | `pot_filt` | float | `ResistanceSensor::getFilteredWiperFloat()` |
   | `p_tilde` | float | `CalibrationWizard::wiperToNormalized()` |
   | `cad_us` | uint32 | `CadenceSensor::getLastPulseInterval()` |
   | `cad_pin` | bool | `digitalRead(PIN_CADENCE)` |
   | `enc_vcc` | uint16 | ADC channel 7 (encoder VCC rail) |
   | `mot_p` | uint16 | ADC channel 5 (motor sense +) |
   | `mot_n` | uint16 | ADC channel 6 (motor sense −) |

   Fields already present (`encoder_delta`, `motor_dir`, `pot_adc`) remain
   with existing keys.

4. **Fix the missing LiveFeed overload:** Implement the extended `tick()`
   that includes `session_state`, `session_elapsed_s`, and `hr_bpm` in the
   WebSocket JSON payload. This resolves a gap where the `/ride` page had
   to poll `/api/session/status` separately to get session state.

5. **Feed the drawer lid health dot from the WS stream**, not from a
   one-shot `/api/ble/status` REST call. Add a `ble_conn` boolean field
   to the LiveFeed payload.

### 1.3 Non-Goals

- Changes to BLE connection logic or pairing flow.
- Changes to the `/ride` standalone page (it has its own UI).
- Full 8-channel SignalLogger decimation data (that's a `WITH_SIGNAL_LOGGER`
  debug feature with CSV endpoints, not a portal UI concern).
- Calibration wizard UI changes (separate module).
- Data sink / upload changes (deferred to PR-19).

---

## 2. Architectural Requirements & Invariants

1. **GRAVE-05:** No `std::function` heap allocations in firmware code.
   Use raw function pointers or static lambdas for any new callbacks.

2. **LiveFeed payload budget:** Total payload must stay under 600 bytes
   per frame. Current ~200 bytes + ~60 bytes new fields + ~30 bytes for
   session/HR/BLE = ~290 bytes. Well within budget.

3. **Design System:** Use the framework's `article.info-drawer` with
   `summary.info-summary` CSS classes. No custom drawer CSS.

4. **Asset Pipeline:** Changes to `web/*.js` require firmware rebuild
   (`pio run`) — assets are PROGMEM gzip via `portalBuilder.py`, NOT
   served from LittleFS.

5. **Tag → Build → Flash:** Rule 03 versioning policy applies.

6. **Sensor access:** The new WS fields require access to sensor objects
   that `EchenonModule` already holds references to. Pass values through
   the existing `LiveFeed::tick()` call in `EchenonModule::tick()`.
   Do NOT create new cross-module dependencies.

7. **Short keys only:** All new JSON keys must be ≤8 characters to
   minimize serialization overhead at 2 Hz.

---

## 3. Implementation Plan & Work Order Breakdown

### WO-20.1 — LiveFeed Payload Extension (Firmware)
- **Complexity:** `standard`
- **Files:** `src/echenon/LiveFeed.h`, `src/echenon/LiveFeed.cpp`,
  `src/echenon/EchenonModule.cpp`
- **Changes:**
  1. Implement the extended `tick()` overload adding `session_state`,
     `session_elapsed_s`, `hr_bpm` to the JSON payload.
  2. Add new diagnostic fields (`pot_raw`, `pot_filt`, `p_tilde`,
     `cad_us`, `cad_pin`, `enc_vcc`, `mot_p`, `mot_n`, `ble_conn`)
     to the `tick()` call and JSON serialization.
  3. Update `EchenonModule::tick()` to pass the new values from existing
     sensor references.
  4. Ensure total payload stays within 600-byte budget.
- **Verification:** `pio test -e native --verbose`

### WO-20.2 — Dashboard Diagnostics Drawer (Frontend)
- **Complexity:** `standard`
- **Files:** `web/echenon-dashboard.js`
- **Changes:**
  1. Remove the static "Echenon Status" card HTML from `renderScaffold()`.
  2. Add `article.info-drawer` HTML with `<details>/<summary>` below
     the "Open Ride Page" button.
  3. Drawer lid: status dot + "Bike Connected" / "Not Connected" label,
     driven by `ble_conn` field from WS.
  4. Expanded content: three sections (Status, Resistance Knob, Cadence,
     Motor Sense) as key-value rows updated from `handleLiveMessage()`.
  5. Colour indicators: green/grey/red based on value sanity thresholds
     (e.g., `enc_vcc < 100` → red = disconnected).
  6. Remove one-shot `fetchBikeStatus()`, `fetchCalibrationStatus()` calls
     (replaced by WS-driven updates). Keep `fetchSessionStatus()` as
     fallback for initial load before first WS frame arrives.
- **Verification:** `pio test -e native --verbose`

### WO-20.3 — Hardware Verification: Tag, Build, Flash & Live Test
- **Complexity:** `interactive`
- **Steps:**
  1. Tag next dev version.
  2. Build via PlatformIO MCP.
  3. Flash firmware (PROGMEM assets rebuilt automatically).
  4. Open Echelon dashboard tab — verify drawer lid shows connection status.
  5. Expand drawer — verify raw sensor values update at 2 Hz.
  6. Disconnect a sensor plug — verify values go to 0/red.
  7. Verify serial log shows extended WS payload with new fields.

### Dependency Chain
```
WO-20.1 (firmware) → WO-20.2 (frontend) → WO-20.3 (HW verify)
```

---

## 4. Outcome & Session Log

| Timestamp | Event |
|-----------|-------|
| 2026-10-27T23:07Z | PA decomposed charter into WO-20.1, WO-20.2, WO-20.3 |
| 2026-10-27T23:07Z | Plan approved (user pre-approved via Chief Architect dispatch) |
| 2026-10-27T23:08Z | Dispatching WO-20.1 (LiveFeed Payload Extension) |
| 2026-10-27T23:12Z | WO-20.1 COMPLETED — fc2fd86 — 233/233 tests passed |
| 2026-10-27T23:12Z | Branch topology repaired (cherry-pick from main to WO branch) |
| 2026-10-27T23:13Z | Dispatching WO-20.2 (Dashboard Diagnostics Drawer) |
| 2026-10-27T23:16Z | WO-20.2 COMPLETED — 38d7a22 — 233/233 tests passed |
| 2026-10-27T23:16Z | WO-20.3 (interactive HW verification) — escalated to user |
| 2026-10-28 | Merged to `main` via PR #14 (`109cf4a`), PR #15 (`cb643c5`), and PR #16 (`16ab9eb`), including WO-20.4 linker fix (`cf8a0cd`) and WO-20.5 calibration status & drawer rework (`5d4176e`, `138156c`, `61e95ef`). Reconciled as `GRADUATED` on 2026-11-24. |
