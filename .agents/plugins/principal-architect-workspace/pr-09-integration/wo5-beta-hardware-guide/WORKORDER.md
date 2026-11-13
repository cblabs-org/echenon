---
schema_version: 1
wo_id: WO-09.5
pr_id: PR-09
title: Standalone v1 Passive Tap Board Build Guide & Hardware Spec Errata
complexity: standard
status: COMPLETED
allowed_files:
- docs/beta-build-guide.md
- docs/hardware_specification.md
forbidden_files:
- framework/**
- src/**
- web/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 4aa530a1257e4766e618ff75d8a2d7694e9a9672250f886fdc4cf34528d146fc
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_011b654df3ad
  commit_sha: 07b89b5fb9aa30f4da3b8e8acfaa012d70545231
  files_modified:
  - docs/beta-build-guide.md
  - docs/hardware_specification.md
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.563 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-09.5 — Standalone v1 Passive Tap Board Build Guide & Hardware Spec Errata

## 1. Objective

Create a standalone, builder-ready hardware construction and commissioning guide ([docs/beta-build-guide.md](docs/beta-build-guide.md)) for early beta testers building the **v1 "Passive" Tap Board** for the Echelon EX-3, and fix the `C1 ×3` -> `C1 ×5` errata and mechanical `VIN`/`USB-C` interlock + OTA beta deferral notes in [docs/hardware_specification.md](docs/hardware_specification.md).

## 2. Strict Boundary Box

- **Allowed files:** `docs/beta-build-guide.md`, `docs/hardware_specification.md`.
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, or any file outside the two listed docs.
- **No invented parts/pins:** Cross-check every resistor value, capacitor count, pin number, and wire colour against `docs/hardware_specification.md` and `hardware/ex3-waveshare-nano/board.h`. Do NOT mention an external `U3` buck converter for v1 — v1 powers `U1` directly on its onboard MP2322 `VIN` (6–21 V) pin.
- **Shell hygiene:** Never use bash `$VAR` loops, `/tmp`, or raw `pio` CLI in `run_command`.

## 3. Technical Specification

### 3.1 `docs/beta-build-guide.md` (New Standalone Builder Guide)
Structure the guide cleanly for capable DIY electronics hobbyists / cyclists joining the `v0.3.0-beta.1` beta:

1. **Top `[!CAUTION]` Banner — Power Supply Mechanical Interlock (`SW0`):**
   - Explicitly warn that the board MUST be powered by **either** bike `+9 V VIN` (via `SW0` mechanical interlock) **or** `USB-C`, **NEVER BOTH simultaneously**.
   - Provide a bold step-by-step **Bench / Flashing vs. Bike Riding Switching Checklist**:
     - Before plugging USB-C into a computer: verify `SW0` is in the **USB / VIN-OFF** position (or physical barrel/JST `VIN` interlock is disconnected).
     - Before connecting `VIN` / sliding `SW0` to **BIKE (VIN-ON)**: verify the USB-C cable is completely unplugged from the Waveshare ESP32-S3-Nano.

2. **What This Board Does (v1 Passive Tap Architecture):**
   - Non-destructive inline harness Y-tap between the EX-3 Main Control Board (`J1`) and Resistance Knob (`J3`), keeping the bike's stock console and motor fully intact.
   - Reads reed-switch cadence, potentiometer wiper voltage (with ÷4 divider + `C1` RC filter), and motor H-bridge PWM signals.
   - Notes clearly that **v1 is read-only / passive** (no active BLE FTMS resistance motor override — see Known Limitations).

3. **Master Bill of Materials (BOM):**
   Exact match to `docs/hardware_specification.md` §1.1 & §6.1:
   - `U1`: **Waveshare ESP32-S3-Nano** (`ESP32-S3R8`, 16 MB Flash / 8 MB OPI PSRAM, integrated `VIN` 6–21 V MP2322 switching regulator — **no external `U3` buck regulator needed**).
   - `SW0`: Mechanical **VIN / USB-C Power Interlock** (SPDT toggle/slide switch in series with `J1-8` `+9 V` to `U1 VIN`, or slotted mechanical gate preventing USB-C insertion while `VIN` is closed).
   - `RS1–RS3`: **3× 22 kΩ** 1% axial resistors (1/4 W) — on `GPIO 1`, `GPIO 2`, `GPIO 3` (motor driver high-side taps `M+ / M-` and cadence reed).
   - `RS4–RS8`: **5× 4.7 kΩ** 1% axial resistors (1/4 W) — series protection on `GPIO 4`, `GPIO 5`, `GPIO 8`, `GPIO 9`, `GPIO 10`.
   - `R1a`, `R2a`: **2× 30 kΩ** 1% axial resistors (1/4 W) — upper leg of ÷4 voltage dividers for `GPIO 6`, `GPIO 7`.
   - `R1b`, `R2b`: **2× 10 kΩ** 1% axial resistors (1/4 W) — lower leg to `GND` of ÷4 voltage dividers for `GPIO 6`, `GPIO 7`.
   - `C1 ×5`: **5× 100 nF (0.1 µF)** X7R ceramic capacitors — from the **ESP32 GPIO side** of `RS4–RS8` to `GND` on `GPIO 4, 5, 8, 9, 10` (low-pass filtering ~338 Hz PWM/switch noise).
   - `J1`, `J2`: **JST-SM 9-pin** male + female housings & crimp pins (inline pass-through for main console harness).
   - `J3`, `J4`: **JST 3-pin** male + female housings & crimp pins (inline pass-through for handlebar knob encoder).
   - `J5`: **2× 15-pin 2.54 mm female headers** (socket for `U1`, making MCU removable).
   - Prototype board: **70 × 50 mm** double-sided FR-4 perfboard + 22 AWG hookup wire.
   - Explicitly note **leave unconnected** in v1: `GPIO 17`, `GPIO 43` (UART0 TX), `GPIO 44` (UART0 RX) — reserved for future ANT+ / v2 expansion.

4. **Wiring Table & ASCII/Mermaid Schematic:**
   - Complete signal-to-GPIO-to-wire-colour mapping table verified against `hardware/ex3-waveshare-nano/board.h` and `docs/hardware_specification.md` §2–§6.
   - Clear ASCII wiring diagram showing pass-through continuity, `SW0` interlock on `+9 V` (`J1-8`), `RS1–RS3`, `RS4–RS8 + C1 ×5`, and `R1a/R1b` / `R2a/R2b` dividers (`ADC_ATTEN_DB_11`).

5. **Pre-Power Sanity Checks (CRITICAL — Before Inserting `U1` or Connecting the Bike):**
   - Visual solder-bridge inspection.
   - Multimeter continuity checks across pass-through pins (`J1 <-> J2`, `J3 <-> J4`), zero short between `VIN`/`5V`/`3V3` and `GND`.
   - Multimeter resistance verification of both ÷4 dividers (`~40 kΩ` input-to-GND, `10 kΩ` GPIO-pin-to-GND), `22 kΩ` and `4.7 kΩ` series legs, and `SW0` open/closed continuity.

6. **Initial USB-C Flash & First Boot (Beta Procedure):**
   - Reminder that **OTA is disabled in `v0.3.0-beta.1`**; all beta firmware updates happen over **USB-C with `SW0` isolating `VIN`**.
   - Exact PlatformIO steps (`pio run -e ex3-waveshare-nano -t upload` and `pio run -e ex3-waveshare-nano -t uploadfs` — or pre-built binary flash instructions if using `esptool`).
   - WiFi AP onboarding (`Echenon-Setup` captive portal), portal login/auth credentials setup, and verifying live cadence/ADC signals on `/` before running the 4-point **Calibration Wizard** (`/calibration`).

7. **Known v1 Beta Limitations:**
   - Passive read-only tap (no BLE FTMS ERG / SIM resistance motor control; stock EX-3 console retains motor control including EX-3 factory runaway quirk if powered down mid-level).
   - Firmware updates via USB-C only in `v0.3.0-beta.1` (`WITH_OTA` deferred pending framework hardening).
   - Tested specifically on Echelon EX-3.

### 3.2 `docs/hardware_specification.md`
1. Inspect lines ~620–730 in `docs/hardware_specification.md`.
2. Fix every occurrence where `C1` is listed as `×3` / `GPIO 4, 5, 10` in §6.2 (lines ~637, ~638, ~702, ~721) so it matches §1.1 & §6.1: **`C1 ×5` (`100 nF` to GND on `GPIO 4, 5, 8, 9, 10`)**.
3. Ensure §6.2 / §1.1 power notes explicitly mention the `SW0` mechanical `VIN` (`J1-8` +9 V) / `USB-C` interlock (**one or the other, NEVER both**) and note that `v0.3.0-beta.1` flashes over USB-C (`WITH_OTA` deferred post-beta).

## 4. Verification

1. Verify every BOM part count, resistor value, and GPIO pin in `docs/beta-build-guide.md` matches `docs/hardware_specification.md` and `hardware/ex3-waveshare-nano/board.h`.
2. Check that no `U3` external buck converter or `<REDACTED_SSID>` / personal details appear in the new/edited docs.
3. Run `~/.local/bin/pa wo verify WO-09.5` (executes `pio test -e native --verbose`, stamps receipt, commits on `pr-09-integration/wo5-beta-hardware-guide`).
4. Post completion summary to `@coordinator` via `post_message`.

## Completion Report

_Pending execution._
