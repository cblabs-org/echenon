# Echenon PCB Fabrication Package (`ex3-waveshare-nano`)

**KiCad 10** PCB projects, **Makera Carvera Air** home-CNC bundles, and **JLCPCB 2-Layer SMD PCBA** turnkey assembly bundles for the Echenon hardware ([beta-build-guide.md](../../../docs/beta-build-guide.md), [hardware_specification.md](../../../docs/hardware_specification.md), [board.h](../board.h)).

- **Deliverables A & B (Carvera Air Home CNC):** **100% single-sided bottom-copper (`B.Cu`)** through-hole layouts — zero copper via rivets, zero double-sided board flips, and zero inaccessible solder joints beneath `J5` headers.
- **Deliverable C (JLCPCB 2-Layer SMD PCBA — Hybrid Carrier):** **2-layer FR-4 (`F.Cu` + solid `B.Cu` ground plane)** layout where JLCPCB factory-populates all **18 surface-mount passives + Schottky diode** using **100% \$0-setup JLCPCB Basic Parts**, leaving only the `2.54 mm` headers/pigtails (`J1–J4`, `SW0`, `U1` socket) for quick bench soldering.

---

## 1. Repository Layout

| Path | Purpose | Dimensions | Status |
|---|---|---|---|
| [generate_pcbs.py](generate_pcbs.py) | Deterministic KiCad 10 S-expression generator + `kicad-cli` ERC/DRC, Gerber, BOM/CPL, 3D PNG, SVG & PDF runner | — | Verified |
| [coupon/](coupon/) | **Deliverable A:** Carvera Air calibration test coupon (`carvera_calibration_coupon.kicad_*`, [schematic PDF](coupon/carvera_calibration_coupon.pdf), [PCB PDF](coupon/carvera_calibration_coupon_pcb.pdf)) | `25 × 20 mm` | ERC=0, DRC=0 |
| [v1-passive-tap/](v1-passive-tap/) | **Deliverable B:** Carvera Air v1 Passive Tap Board (`echenon_v1_passive_tap.kicad_*`, [schematic PDF](v1-passive-tap/echenon_v1_passive_tap.pdf), [PCB PDF](v1-passive-tap/echenon_v1_passive_tap_pcb.pdf)) | `70 × 50 mm` | ERC=0, DRC=0 |
| [v1-passive-tap-jlcpcb/](v1-passive-tap-jlcpcb/) | **Deliverable C:** JLCPCB 2-Layer SMD PCBA v1 Passive Tap Board (`echenon_v1_passive_tap_jlcpcb.kicad_*`, [schematic PDF](v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb.pdf), [PCB PDF](v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_pcb.pdf)) | `70 × 50 mm` | ERC=0, DRC=0, SMD=18 |
| [gerbers/coupon/](gerbers/coupon/) | Ready-to-mill Gerbers, Excellon drills, [`carvera_calibration_coupon_gerbers.zip`](gerbers/coupon/carvera_calibration_coupon_gerbers.zip), SVGs | `25 × 20 mm` | 2 drills (`0.9`, `1.0 mm`) |
| [gerbers/v1-passive-tap/](gerbers/v1-passive-tap/) | Ready-to-mill Gerbers, Excellon drills, [`echenon_v1_passive_tap_gerbers.zip`](gerbers/v1-passive-tap/echenon_v1_passive_tap_gerbers.zip), SVGs | `70 × 50 mm` | 2 drills (`0.9`, `1.0 mm`) |
| [gerbers/v1-passive-tap-jlcpcb/](gerbers/v1-passive-tap-jlcpcb/) | Turnkey JLCPCB bundle: [`echenon_v1_passive_tap_jlcpcb_gerbers.zip`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_gerbers.zip), [`_bom.csv`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_bom.csv), [`_cpl.csv`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_cpl.csv), [3D Top PNG](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_3d_top.png), [3D Bottom PNG](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_3d_bottom.png) | `70 × 50 mm` | 2-Layer + Economic PCBA |

---

## 2. Carvera Air CNC Design Rules (Built Into Both Boards)

- **Copper Layer:** `B.Cu` only (no `F.Cu` tracks, no vias).
- **Minimum Track Width:** `0.50 mm` signals (`0.60–0.90 mm` for motor sense pass-throughs, `GND`, and `+9 V`).
- **Minimum Copper Clearance:** `0.45 mm` everywhere (sized for `0.2 mm 30°` V-bits with 2–3 isolation passes).
- **Standardized Tooling (Only 2 Drill Sizes):**
  - `T1 = 0.90 mm`: Axial resistors (`RS1–RS8`, `R1a/b`, `R2a/b`), Schottky diode (`D1`), ceramic capacitors (`C1_1–C1_5`).
  - `T2 = 1.00 mm`: Waveshare ESP32-S3-Nano socket headers (`J5` / `U1`), pigtail solder pads (`J1–J4`), and `SW0` interlock pins.
  - `0.80 mm` corn endmill (`Edge.Cuts`): Board perimeter, corner `M2.5` mounting holes (`2.70 mm` dia), and pigtail strain-relief tie holes (`2.20 mm` dia) — milled automatically on the contour pass with zero extra drill bits.

---

## 3. Deliverable A: Calibration Coupon (`25 × 20 mm`)

Mill the coupon first in the corner of a **single-sided blank** (`100 × 150 mm`) to dial in your V-bit cut depth and drill fit before cutting the full `70 × 50 mm` board:

- Tests one `÷4` motor-sense divider cell (`R_DIV1 30 kΩ`, `R_DIV2 10 kΩ`) + `C_FILT1 100 nF` cap + `J_LOOM1` (`1×2` pigtail pads) + `J_TEST1` (`1×3` Nano socket strip) + one `2.2 mm` strain-relief hole.
- **Check after milling:**
  1. Confirm `0.50 mm` & `1.00 mm` copper traces are crisp with clean isolation channels and no copper whiskers.
  2. Test-fit your `2.54 mm` female socket strip into `J_TEST1` (`1.00 mm` holes), `1/4 W` axial resistor leads into `R_DIV1` (`0.90 mm` holes), and stripped `22 AWG` JST pigtail wire into `J_LOOM1` (`1.00 mm` holes).

---

## 4. Deliverable B: Echenon v1 Passive Tap Board (`70 × 50 mm`)

Matches the DR-1 frozen schematic in [beta-build-guide.md](../../../docs/beta-build-guide.md) and [board.h](../board.h):

- **Top-Side Component Placement (`F.SilkS`):**
  - `J4` (Console) ↔ `J3` (Resistance Knob), 3-pin JST pigtail banks (`1: Black ENC_CHA`, `2: Orange/Yellow ENC_VCC`, `3: Red ENC_CHB`).
  - `J2` (Console) ↔ `J1` (Bike Frame), 9-pin JST-SM pigtail banks (`1: Red MOT+`, `2: Black MOT−`, `3: Yellow POT_HI`, `4: Blue POT_WIPER`, `5: Green POT_LO`, `6: Orange CAD`, `7: Grey CAD_RTN`, `8: White +9V`, `9: Brown GND`).
  - **Strain Relief:** Loop a small nylon zip-tie through the `2.2 mm` holes beside `J1/J2` and `J3/J4` around each pigtail cable jacket so pulling the plugs never stresses the solder joints.
  - **`SW0` Physical USB-C Blocker (`x = 4.80 mm`):** Mount a `2-pin` header or socket/jumper block straddling the USB-C port entrance of `U1`. When the `+9 V` bridge jumper is installed across `SW0`, it **physically blocks insertion of a USB-C cable**.
  - **`D1` (`1N5819` Schottky Diode):** Bridges vertically over the bottom `A0–A3` tracks (`Pad 1` square cathode toward `U1` `VIN`, `Pad 2` round anode toward `SW0`). Match the diode silver/white cathode band to the square pad!
  - **Reserved Pins Left Unconnected:** `U1` `D8` (`GPIO 17`), `D1/TX1` (`GPIO 43`), `D0/RX0` (`GPIO 44`).
- **Schematics & Off-Board Reference Circuitry:** Both schematics use official KiCad 10 IEC symbols (`Device:R`, `Device:C`, `Device:D_Schottky`, `Switch:SW_SPST`, `Connector_Generic:Conn_01x*`, `power:GND`) with continuous wired dividers, RC filters, and harnesses. [echenon_v1_passive_tap.kicad_sch](v1-passive-tap/echenon_v1_passive_tap.kicad_sch) also includes dashed off-board EX-3 bike and stock console reference blocks (`M1`, `RV1`, `SW_CAD1`, `PS1`, `ENC1`, `CON1`) marked `on_board no`, `in_bom no`, and `in_pos_files no` so end-to-end signal flow is documented without entering the PCB or BOM.

---

## 5. Makera CAM & Carvera Air Production Runbook

### Step 1: Stock Setup
1. Secure a clean **single-sided copper-clad blank (`100 × 150 mm`)** **copper-side UP** onto your Carvera Air wasteboard/sub-spoiler using flat double-sided tape across the full underside (essential so the board does not vibrate or lift during V-bit isolation).
2. Register the lower-left corner against the L-bracket origin.
3. Put **bit collars** on your 4 tools (matching your Makera PCB Fabrication Pack):
   - **Isolation:** `1/8"` `0.2 mm 30°` V-bit (or `0.1 mm 60°` V-bit)
   - **Drill 1:** `0.90 mm` PCB drill bit
   - **Drill 2:** `1.00 mm` PCB drill bit
   - **Contour / Cutout:** `0.80 mm` corn endmill

### Step 2: Import & Mirror in Makera CAM (CRITICAL FOR SINGLE-SIDED `B.Cu`)
1. Open **Makera CAM** → create a **3-Axis Project**.
2. Click **Import PCB** and select the unzipped Gerbers + `.drl` files from [gerbers/coupon/](gerbers/coupon/) or [gerbers/v1-passive-tap/](gerbers/v1-passive-tap/) (`*-bottom_layer.gbl`, `*-B_Mask.gbs`, `*-Edge_Cuts.gm1`, `*-PTH.drl`).
3. **CRITICAL — Mirror Together Before Generating Toolpaths:**
   - Standard KiCad `B.Cu` Gerbers and `.drl` files are exported in **top-down X-ray coordinates**, whereas your single-sided copper blank sits **copper-side UP** on the Carvera bed.
   - Select **all imported PCB layers together** (`B.Cu`, `B.Mask`, `Edge.Cuts`, `PTH.drl`) → **Adjust Object → Mirror (Horizontal / across Y-axis around Stock Center)**.
   - **Visual Sanity Check (Do Not Skip!):** Compare the Makera CAM workspace view against the mirrored bottom-copper preview SVG:
     - Coupon: [gerbers/coupon/carvera_calibration_coupon_bottom_copper.svg](gerbers/coupon/carvera_calibration_coupon_bottom_copper.svg) (`J_LOOM1` on top-right, strain-relief hole on top-right corner).
     - v1 Board: [gerbers/v1-passive-tap/echenon_v1_passive_tap_bottom_copper.svg](gerbers/v1-passive-tap/echenon_v1_passive_tap_bottom_copper.svg) (**`SW0` on the RIGHT**, **`J1/J2` 9-pin bank on the TOP-LEFT**, **`J3/J4` 3-pin bank on the TOP-RIGHT**, and `ECHENON v1 B.Cu` text readable left-to-right).

### Step 3: Toolpaths & Milling Parameters
1. **Isolation Routing (`*-bottom_layer.gbl`):**
   - Bit: `0.2 mm 30°` V-bit (`12,000–13,000 RPM`, feed `150–250 mm/min`, plunge `60 mm/min`).
   - Cut depth: `-0.08 mm` to `-0.10 mm` (`1 oz` copper is `0.035 mm` thick; `0.08 mm` cleanly cuts through copper + minor surface variation without digging deep into substrate).
   - Isolation passes: **2–3 passes** (`~0.25–0.35 mm` total moat width; our `0.45 mm` minimum DRC clearance gives plenty of margin).
2. **Drilling (`*-PTH.drl`):**
   - `T1 = 0.90 mm` (`10,000 RPM`, plunge `120–150 mm/min`, depth `-1.85 mm` for `1.5 mm` FR-4/FR-1 blank).
   - `T2 = 1.00 mm` (same speed/feed/depth).
3. **Contour & Mounting/Strain-Relief Holes (`*-Edge_Cuts.gm1`):**
   - Bit: `0.80 mm` corn endmill (`10,000–12,000 RPM`, `2–3` step-downs to `-1.85 mm` depth, feed `150–200 mm/min`).
   - Add **4 small holding tabs** (`1.5 mm` wide × `0.6 mm` high) on the outer board rectangle (not inside the mounting/strain-relief circles).

### Step 4: Carvera Controller Execution & Safety
1. Enable **Auto-Leveling (`G32` wireless Z-probe mesh)** across the PCB bounding box (e.g., `5 × 4` or `7 × 5` grid points) before cutting traces.
2. Follow **Carvera Controller** prompts during **`M6` Quick Tool Changes** (each bit swap automatically runs the tool-length offset setter).
3. **Dust Safety (especially for FR-4 blanks):**
   - Keep the Carvera Air enclosure closed and run vacuum extraction throughout the cut.
   - Wear an N95/P100 mask when opening the lid, **never** use compressed air, and wipe the finished board and bed clean with a damp paper towel or isopropyl alcohol (IPA) wipe.
4. *(Optional)* **UV Solder Mask (Final Bike Board Only):** Skip UV mask on your first test board so every trace is easy to inspect and probe. For the final bike-installed board, roll a thin coat of UV solder mask onto the copper side, cure under the `365 nm` lamp, and run Makera CAM's pad-clearing pass from `*-B_Mask.gbs` using the spring-loaded mask removal bit.

---

## 6. Post-Mill Pre-Power Verification (With `U1` Unseated!)

Before inserting the **Waveshare ESP32-S3-Nano** (`U1`) into `J5`:
1. Visually inspect all isolation moats under bright light/magnifier and scrub lightly with Scotch-Brite/IPA to remove any fine copper burrs.
2. Buzz out continuity on all 12 straight pass-through pins (`J1-1..9 ↔ J2-1..9`, `J3-1..3 ↔ J4-1..3`).
3. Verify **zero continuity (open circuit)** between adjacent pads and between `+9 V` (`J1-8`), `VIN` (`U1-30`), and `GND` (`J1-9`).
4. Solder `RS1–RS8`, `R1a/b`, `R2a/b`, `C1_1–C1_5`, `D1`, `SW0`, `J5` headers, and `J1–J4` pigtails.
5. **Interlock Check:** With `U1` still unseated, apply `+9 V` to `J1-8`/`J1-9`:
   - `SW0` **OPEN (USB position):** `0.0 V` at `U1` `VIN` (`Pad 30`).
   - `SW0` **BRIDGED (BIKE position):** `~8.6–8.7 V` at `U1` `VIN` (`Pad 30`, accounting for `D1` `~0.35 V` Schottky drop), and USB-C port physically blocked.
6. Proceed with [beta-build-guide.md §5–§7](../../../docs/beta-build-guide.md) to seat `U1` and commission on the EX-3.

---

## 7. Deliverable C: JLCPCB 2-Layer SMD PCBA Edition (`70 × 50 mm`)

[v1-passive-tap-jlcpcb/](v1-passive-tap-jlcpcb/) implements the identical DR-1 frozen circuit as a **2-layer FR-4 (`1.6 mm`) Hybrid SMD Carrier**:

- **2-Layer Signal Integrity:** Top-side (`F.Cu`) `0805`/`SMA` SMD placement over an unbroken thermal-relieved **`B.Cu` `GND` ground plane** with dedicated ground vias right beside every `100 nF` RC filter capacitor (`C1_1–C1_5`) and `÷4` divider leg (`R1b`, `R2b`).
- **Optical & Mechanical Features:** 3 asymmetric `1.0 mm` optical fiducials (`FID1–FID3`, `>= 5 mm` from edges), 4 corner `M2.5` (`2.70 mm` NPTH) mounting holes (`MH1–MH4`), and 3 `2.20 mm` NPTH zip-tie strain-relief holes (`SR1–SR3`). All 18 SMD components sit well inside JLCPCB's `5 mm` conveyor rail margin.
- **100% JLCPCB Basic Parts (\$0 Feeder Setup Fee on Economic PCBA):**

| Designators | Value / Spec | Package | MPN | JLCPCB Part # | Tier | Qty |
|---|---|---|---|---|---|---|
| `RS4–RS8` | `4.7 kΩ` ±1% 1/8W | `0805` | `0805W8F4701T5E` | **`C17673`** | Basic | 5 |
| `R1b, R2b` | `10 kΩ` ±1% 1/8W | `0805` | `0805W8F1002T5E` | **`C17414`** | Basic | 2 |
| `RS1–RS3` | `22 kΩ` ±1% 1/8W | `0805` | `0805W8F2202T5E` | **`C17560`** | Basic | 3 |
| `R1a, R2a` | `30 kΩ` ±1% 1/8W | `0805` | `0805W8F3002T5E` | **`C17621`** | Basic | 2 |
| `C1_1–C1_5` | `100 nF` 50V X7R | `0805` | `CL21B104KBCNNNC` | **`C49678`** | Basic | 5 |
| `D1` | `SS14` 40V 1A Schottky | `SMA (DO-214AC)` | `SS14` | **`C2480`** | Basic | 1 |

### JLCPCB Ordering Steps
1. Go to [jlcpcb.com](https://jlcpcb.com) → **Order Now** → upload [`echenon_v1_passive_tap_jlcpcb_gerbers.zip`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_gerbers.zip).
2. Keep standard 2-layer defaults (`FR-4`, `2 Layers`, `1.6 mm` thickness, `1 oz` copper, `HASL (with lead)` or `LeadFree HASL` / `ENIG`, `5 pcs`).
3. Scroll down and toggle **PCB Assembly** → **ON**:
   - **PCBA Type:** `Economic`
   - **Assembly Side:** `Top Side`
   - **PCBA Qty:** `2` (or `5` to populate the entire batch)
4. Click **Next**, then upload:
   - **BOM File:** [`echenon_v1_passive_tap_jlcpcb_bom.csv`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_bom.csv)
   - **CPL File:** [`echenon_v1_passive_tap_jlcpcb_cpl.csv`](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_cpl.csv)
5. **DFM / Polarity Sanity Check (10 seconds):** On JLCPCB's 2D/3D component placement preview, confirm that **`D1` (`SS14`)** has its cathode band facing **LEFT** over the **`K`** silkscreen bar (matching [echenon_v1_passive_tap_jlcpcb_3d_top.png](gerbers/v1-passive-tap-jlcpcb/echenon_v1_passive_tap_jlcpcb_3d_top.png)). If a reel change ever flips `D1`, click `D1` in the preview and rotate `180°`.
6. **Bench Finish Upon Arrival (~5 minutes):** Solder the `2×15` `2.54 mm` female socket strips (`U1`), the `2-pin` USB-C blocker header/jumper (`SW0`), and either `2.54 mm` headers or pre-wired JST pigtails (`J1–J4`). Run the `SW0` interlock check in §6 Step 5 before seating your Waveshare ESP32-S3-Nano!

---

## 8. Regenerating After Changes

```bash
python3 hardware/ex3-waveshare-nano/pcb/generate_pcbs.py
```

The script regenerates all three KiCad 10 projects (`coupon`, `v1-passive-tap`, `v1-passive-tap-jlcpcb`), runs headless `kicad-cli sch erc` and `kicad-cli pcb drc --schematic-parity --severity-all` (with automatic `B.Cu` zone filling), verifies 1-to-1 BOM/CPL designator parity, and rebuilds all Gerbers, drill files, CSVs, 3D PNG renders, SVGs, printable PDFs, and `.zip` archives.
