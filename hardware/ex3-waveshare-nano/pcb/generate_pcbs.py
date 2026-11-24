#!/usr/bin/env python3
"""
Deterministic KiCad 10 generator and verification script for:
1. Deliverable A: Makera Carvera Air Calibration Coupon (25 x 20 mm, B.Cu single-sided)
2. Deliverable B: Echenon v1 Passive Tap Board (70 x 50 mm, B.Cu single-sided, DR-1 frozen)
3. Deliverable C: Echenon v1 Passive Tap Board — JLCPCB 2-Layer SMD PCBA Edition (70 x 50 mm)

Generates .kicad_pro, real IEC wired .kicad_sch (including off-board EX-3 reference
circuitry excluded from PCB/BOM), and .kicad_pcb, runs kicad-cli sch erc and
kicad-cli pcb drc --schematic-parity --severity-all, and exports Gerbers,
Excellon drill files, JLCPCB BOM/CPL CSVs, .zip archives, SVG previews, and PDFs.
"""

from __future__ import annotations

import csv
import hashlib
import json
import subprocess
import sys
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


PCB_ROOT = Path(__file__).resolve().parent
REPO_ROOT = PCB_ROOT.parents[2]
TMP_DIR = REPO_ROOT / "tmp"
KICAD_CLI = "/opt/homebrew/bin/kicad-cli"
KICAD_SYM_DIR = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols")


def det_uuid(seed: str) -> str:
    """Return a deterministic UUIDv4-formatted identifier from a seed string."""
    h = hashlib.sha256(seed.encode("utf-8")).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-a{h[17:20]}-{h[20:32]}"


@dataclass
class PadSpec:
    number: str
    dx: float
    dy: float
    shape: str  # "circle" | "oval" | "rect" | "roundrect"
    w: float
    h: float
    drill: float
    net: str = ""
    pintype: str = "passive"
    unconnected_net: str = ""
    pad_type: str = "thru_hole"  # "thru_hole" | "smd" | "np_thru_hole"
    layers: Optional[str] = None
    rratio: float = 0.0
    solder_mask_margin: Optional[float] = None
    clearance: Optional[float] = None
    pad_prop: str = ""


@dataclass
class ComponentSpec:
    ref: str
    value: str
    lib_id: str
    fp_name: str
    x: float
    y: float
    pads: List[PadSpec]
    sch_x: float
    sch_y: float
    descr: str = ""
    extra_fp_graphics: List[str] = field(default_factory=list)
    ref_offset: Tuple[float, float] = (0.0, -3.2)
    val_offset: Tuple[float, float] = (0.0, 3.2)
    ref_angle: float = 0.0
    hide_ref: bool = False
    hide_val: bool = False
    attr: str = "through_hole"
    lcsc: str = ""
    mpn: str = ""
    model_3d: str = ""

    @property
    def sym_uuid(self) -> str:
        return det_uuid(f"sym:{self.ref}")

    @property
    def fp_uuid(self) -> str:
        return det_uuid(f"fp:{self.ref}")

    def pad_board_xy(self, pad_num: str) -> Tuple[float, float]:
        for p in self.pads:
            if p.number == pad_num:
                return (round(self.x + p.dx, 4), round(self.y + p.dy, 4))
        raise KeyError(f"Pad {pad_num} not found on {self.ref}")


@dataclass
class TrackSegment:
    x1: float
    y1: float
    x2: float
    y2: float
    width: float
    net: str
    layer: str = "B.Cu"


@dataclass
class ViaSpec:
    x: float
    y: float
    net: str
    size: float = 0.80
    drill: float = 0.40


@dataclass
class ZoneSpec:
    net: str
    layer: str
    x1: float
    y1: float
    x2: float
    y2: float
    clearance: float = 0.30
    min_thickness: float = 0.25
    thermal_gap: float = 0.35
    thermal_bridge_width: float = 0.40


@dataclass
class SilkText:
    text: str
    x: float
    y: float
    layer: str = "F.SilkS"
    size: float = 1.0
    thickness: float = 0.15
    angle: float = 0.0
    mirror: bool = False


@dataclass
class SchSymbolInstance:
    ref: str
    value: str
    lib_id: str
    sx: float
    sy: float
    rot: int = 0
    mirror: Optional[str] = None
    pins: List[str] = field(default_factory=list)
    footprint: str = ""
    descr: str = ""
    on_board: bool = True
    in_bom: bool = True
    in_pos_files: Optional[bool] = None
    dnp: bool = False
    sym_uuid: Optional[str] = None
    ref_at: Optional[Tuple[float, float, int]] = None
    val_at: Optional[Tuple[float, float, int]] = None
    hide_ref: bool = False
    hide_val: bool = False
    ref_justify: str = ""
    val_justify: str = ""
    lcsc: str = ""
    mpn: str = ""


@dataclass
class SchSpec:
    paper: str
    lib_symbols: List[str]
    symbols: List[SchSymbolInstance]
    wires: List[Tuple[float, float, float, float]] = field(default_factory=list)
    junctions: List[Tuple[float, float]] = field(default_factory=list)
    labels: List[Tuple[str, float, float, int, str]] = field(default_factory=list)  # (text, x, y, angle, justify)
    no_connects: List[Tuple[float, float]] = field(default_factory=list)
    rects: List[Tuple[float, float, float, float, str, str]] = field(default_factory=list)  # (x1, y1, x2, y2, stroke_type, color)
    polylines: List[Tuple[List[Tuple[float, float]], str, str]] = field(default_factory=list)  # (pts, stroke_type, color)
    texts: List[Tuple[str, float, float, float, str]] = field(default_factory=list)  # (text, x, y, size, color)
    sch_title: str = ""


def extract_kicad_symbol(lib_file: str, sym_name: str, prefixed_name: str) -> str:
    """Extract a balanced S-expression symbol block from KiCad's installed symbol library."""
    txt = (KICAD_SYM_DIR / lib_file).read_text(encoding="utf-8")
    marker = f'(symbol "{sym_name}"'
    start = txt.find(marker)
    if start == -1:
        raise KeyError(f"Symbol {sym_name} not found in {lib_file}")
    depth = 0
    end = -1
    for idx in range(start, len(txt)):
        ch = txt[idx]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                end = idx + 1
                break
    if end == -1:
        raise RuntimeError(f"Unbalanced S-expression for {sym_name} in {lib_file}")
    block = txt[start:end]
    return block.replace(marker, f'(symbol "{prefixed_name}"', 1)


def sym_pin_to_sheet(
    sx: float,
    sy: float,
    px: float,
    py: float,
    rot: int = 0,
    mirror: Optional[str] = None,
) -> Tuple[float, float]:
    """Convert KiCad symbol-space pin coords (+y UP) to schematic sheet coords (+y DOWN)."""
    if mirror == "y":
        px = -px
    elif mirror == "x":
        py = -py
    if rot == 90:
        px, py = -py, px
    elif rot == 180:
        px, py = -px, -py
    elif rot == 270:
        px, py = py, -px
    return (round(sx + px, 4), round(sy - py, 4))


def add_sch_wire_chain(wires: List[Tuple[float, float, float, float]], pts: List[Tuple[float, float]]) -> None:
    for i in range(len(pts) - 1):
        x1, y1 = pts[i]
        x2, y2 = pts[i + 1]
        if abs(x1 - x2) > 1e-6 or abs(y1 - y2) > 1e-6:
            wires.append((round(x1, 4), round(y1, 4), round(x2, 4), round(y2, 4)))


NANO_LEFT_PINS: List[Tuple[str, str, float]] = [
    ("6",  "D3/GPIO6_MOT+",       91.44),
    ("7",  "D4/GPIO7_MOT-",       71.12),
    ("20", "A1/GPIO2_POT_HI",     48.26),
    ("19", "A0/GPIO1_WIPER",      40.64),
    ("21", "A2/GPIO3_POT_LO",     33.02),
    ("5",  "D2/GPIO5_CADENCE",    17.78),
    ("22", "A3/GPIO4_CAD_RTN",    -2.54),
    ("30", "VIN(6-21V/MP2322)",  -27.94),
    ("4",  "GND",                -38.10),
    ("29", "GND",                -43.18),
    ("8",  "D5/GPIO8_ENC_A",     -60.96),
    ("10", "D7/GPIO10_ENC_VCC",  -81.28),
    ("9",  "D6/GPIO9_ENC_B",    -101.60),
]

NANO_RIGHT_PINS: List[Tuple[str, str, float]] = [
    ("1",  "D1/TX1/GPIO43[RSVD]",  86.36),
    ("2",  "D0/RX0/GPIO44[RSVD]",  76.20),
    ("11", "D8/GPIO17[RSVD]",      66.04),
    ("17", "3V3",                  50.80),
    ("27", "+5V",                  40.64),
    ("18", "AREF",                 30.48),
    ("3",  "~{RESET}_1",           20.32),
    ("28", "~{RESET}_2",           10.16),
    ("12", "D9/GPIO18",            -5.08),
    ("13", "D10/GPIO21",          -15.24),
    ("14", "D11/GPIO38",          -25.40),
    ("15", "D12/GPIO47",          -35.56),
    ("16", "D13/GPIO48",          -45.72),
    ("23", "A4/GPIO11",           -60.96),
    ("24", "A5/GPIO12",           -71.12),
    ("25", "A6/GPIO13",           -81.28),
    ("26", "A7/GPIO14",           -91.44),
]

NANO_PIN_NAMES: Dict[str, str] = {num: name for num, name, _ in NANO_LEFT_PINS + NANO_RIGHT_PINS}


def kicad_unconnected_net(ref: str, pin_name: str, pad_num: str) -> str:
    """Format KiCad 10 automatic unconnected pad net name from schematic symbol pin name."""
    escaped = pin_name.replace(" ", "_").replace("/", "{slash}")
    return f"unconnected-({ref}-{escaped}-Pad{pad_num})"


def build_custom_nano_symbol() -> Tuple[str, Dict[str, Tuple[float, float]]]:
    """Build a crisp functional schematic symbol for Waveshare ESP32-S3-Nano (U1) and return pin sym coords."""
    left_pins = NANO_LEFT_PINS
    right_pins = NANO_RIGHT_PINS

    pin_coords: Dict[str, Tuple[float, float]] = {}
    lines = [
        '(symbol "echenon:Waveshare_ESP32_S3_Nano"',
        "\t(pin_names (offset 1.016))",
        "\t(exclude_from_sim no)",
        "\t(in_bom yes)",
        "\t(on_board yes)",
        "\t(in_pos_files yes)",
        "\t(duplicate_pin_numbers_are_jumpers no)",
        '\t(property "Reference" "U" (at 0 100.33 0) (effects (font (size 1.524 1.524) (bold yes))))',
        '\t(property "Value" "Waveshare_ESP32-S3-Nano" (at 0 -110.49 0) (effects (font (size 1.27 1.27))))',
        '\t(property "Footprint" "echenon:Waveshare_ESP32_S3_Nano_Socket_CNC" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(property "Datasheet" "" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(property "Description" "Waveshare ESP32-S3-Nano (J5 2x15 Socket)" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(symbol "Waveshare_ESP32_S3_Nano_0_1"',
        "\t\t(rectangle (start -19.05 96.52) (end 19.05 -106.68) (stroke (width 0.381) (type default)) (fill (type background)))",
        "\t)",
        '\t(symbol "Waveshare_ESP32_S3_Nano_1_1"',
    ]
    for num, name, py in left_pins:
        pin_coords[num] = (-24.13, py)
        lines.extend(
            [
                "\t\t(pin passive line",
                f"\t\t\t(at -24.13 {py:.2f} 0)",
                "\t\t\t(length 5.08)",
                f'\t\t\t(name "{name}" (effects (font (size 1.143 1.143))))',
                f'\t\t\t(number "{num}" (effects (font (size 1.016 1.016))))',
                "\t\t)",
            ]
        )
    for num, name, py in right_pins:
        pin_coords[num] = (24.13, py)
        lines.extend(
            [
                "\t\t(pin passive line",
                f"\t\t\t(at 24.13 {py:.2f} 180)",
                "\t\t\t(length 5.08)",
                f'\t\t\t(name "{name}" (effects (font (size 1.016 1.016))))',
                f'\t\t\t(number "{num}" (effects (font (size 1.016 1.016))))',
                "\t\t)",
            ]
        )
    lines.extend(["\t)", "\t(embedded_fonts no)", ")"])
    return "\n".join(lines), pin_coords


def build_offboard_ps_symbol() -> str:
    """Off-board 9V/3A DC wall adapter supply symbol."""
    return "\n".join(
        [
            '(symbol "echenon:EX3_Power_Supply_9V"',
            "\t(pin_names (offset 1.016))",
            "\t(exclude_from_sim yes)",
            "\t(in_bom no)",
            "\t(on_board no)",
            "\t(in_pos_files no)",
            "\t(duplicate_pin_numbers_are_jumpers no)",
            '\t(property "Reference" "PS" (at 0 7.62 0) (effects (font (size 1.27 1.27))))',
            '\t(property "Value" "9V_3A_DC_SUPPLY" (at 0 -8.89 0) (effects (font (size 1.143 1.143))))',
            '\t(property "Footprint" "" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
            '\t(property "Datasheet" "" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
            '\t(property "Description" "Off-board EX-3 9V 3A Wall Supply" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
            '\t(symbol "EX3_Power_Supply_9V_0_1"',
            "\t\t(rectangle (start -7.62 5.08) (end 7.62 -5.08) (stroke (width 0.254) (type dash)) (fill (type background)))",
            "\t)",
            '\t(symbol "EX3_Power_Supply_9V_1_1"',
            '\t\t(pin passive line (at 11.43 2.54 180) (length 3.81) (name "+9V" (effects (font (size 1.143 1.143)))) (number "1" (effects (font (size 1.016 1.016)))))',
            '\t\t(pin passive line (at 11.43 -2.54 180) (length 3.81) (name "GND" (effects (font (size 1.143 1.143)))) (number "2" (effects (font (size 1.016 1.016)))))',
            "\t)",
            "\t(embedded_fonts no)",
            ")",
        ]
    )


def build_offboard_console_symbol() -> Tuple[str, Dict[str, Tuple[float, float]]]:
    """Off-board Stock Echelon EX-3 Console PCB reference symbol."""
    pin_defs = [
        ("1",  "J2-1_MOT+_RED",     20.32),
        ("2",  "J2-2_MOT-_BLK",     17.78),
        ("3",  "J2-3_POT_HI_YEL",   15.24),
        ("4",  "J2-4_WIPER_BLU",    12.70),
        ("5",  "J2-5_POT_LO_GRN",   10.16),
        ("6",  "J2-6_CAD_ORG",       7.62),
        ("7",  "J2-7_CAD_RTN_GRY",   5.08),
        ("8",  "J2-8_+9V_WHT",       2.54),
        ("9",  "J2-9_GND_BRN",       0.00),
        ("10", "J4-1_ENC_A_BLK",   -12.70),
        ("11", "J4-2_ENC_3V3_ORG", -15.24),
        ("12", "J4-3_ENC_B_RED",   -17.78),
    ]
    pin_coords: Dict[str, Tuple[float, float]] = {}
    lines = [
        '(symbol "echenon:EX3_Console_PCB"',
        "\t(pin_names (offset 1.016))",
        "\t(exclude_from_sim yes)",
        "\t(in_bom no)",
        "\t(on_board no)",
        "\t(in_pos_files no)",
        "\t(duplicate_pin_numbers_are_jumpers no)",
        '\t(property "Reference" "CON" (at 0 25.40 0) (effects (font (size 1.27 1.27))))',
        '\t(property "Value" "STOCK_EX3_CONSOLE" (at 0 -22.86 0) (effects (font (size 1.143 1.143))))',
        '\t(property "Footprint" "" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(property "Datasheet" "" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(property "Description" "Off-board Stock EX-3 Console PCB Reference" (at 0 0 0) (hide yes) (effects (font (size 1.27 1.27))))',
        '\t(symbol "EX3_Console_PCB_0_1"',
        "\t\t(rectangle (start -13.97 22.86) (end 13.97 -20.32) (stroke (width 0.254) (type dash)) (fill (type background)))",
        "\t)",
        '\t(symbol "EX3_Console_PCB_1_1"',
    ]
    for num, name, py in pin_defs:
        pin_coords[num] = (-19.05, py)
        lines.append(
            f'\t\t(pin passive line (at -19.05 {py:.2f} 0) (length 5.08) (name "{name}" (effects (font (size 1.016 1.016)))) (number "{num}" (effects (font (size 0.889 0.889)))))'
        )
    lines.extend(["\t)", "\t(embedded_fonts no)", ")"])
    return "\n".join(lines), pin_coords


def make_kicad_pro(project_name: str, sch_uuid: str) -> str:
    """Create KiCad 10 .kicad_pro JSON with strict Carvera Air CNC DRC rules."""
    pro = {
        "board": {
            "3dviewports": [],
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.15,
                    "copper_line_width": 0.50,
                    "courtyard_line_width": 0.05,
                    "fab_line_width": 0.10,
                    "other_line_width": 0.10,
                    "pads": {"drill": 0.90, "height": 2.10, "width": 2.10},
                    "silk_line_width": 0.15,
                    "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0,
                    "silk_text_thickness": 0.15,
                    "zones": {"45_degree_only": False, "min_clearance": 0.45},
                },
                "diff_pair_dimensions": [],
                "drc_exclusions": [],
                "meta": {"version": 2},
                "rule_severities": {
                    "annular_width": "error",
                    "clearance": "error",
                    "connection_width": "error",
                    "copper_edge_clearance": "error",
                    "copper_sliver": "error",
                    "courtyards_overlap": "ignore",
                    "creepage": "ignore",
                    "diff_pair_gap_out_of_range": "error",
                    "diff_pair_uncoupled_length_too_long": "error",
                    "drill_out_of_range": "error",
                    "duplicate_footprints": "error",
                    "extra_footprint": "error",
                    "footprint": "error",
                    "footprint_filters_mismatch": "ignore",
                    "footprint_symbol_field_mismatch": "ignore",
                    "footprint_symbol_mismatch": "ignore",
                    "footprint_type_mismatch": "ignore",
                    "hole_clearance": "error",
                    "hole_near_hole": "error",
                    "hole_to_hole": "error",
                    "holes_co_located": "error",
                    "invalid_outline": "error",
                    "isolated_copper": "error",
                    "item_on_disabled_layer": "error",
                    "items_not_allowed": "error",
                    "length_out_of_range": "error",
                    "lib_footprint_issues": "ignore",
                    "lib_footprint_mismatch": "ignore",
                    "malformed_courtyard": "error",
                    "microvia_drill_out_of_range": "error",
                    "mirrored_text_on_front_layer": "error",
                    "missing_courtyard": "ignore",
                    "missing_footprint": "error",
                    "missing_tuning_profile": "error",
                    "net_conflict": "error",
                    "nonmirrored_text_on_back_layer": "error",
                    "npth_inside_courtyard": "ignore",
                    "overlapping_pads": "error",
                    "padstack": "error",
                    "pth_inside_courtyard": "ignore",
                    "shorting_items": "error",
                    "silk_edge_clearance": "ignore",
                    "silk_over_copper": "ignore",
                    "silk_overlap": "ignore",
                    "skew_out_of_range": "error",
                    "solder_mask_bridge": "ignore",
                    "starved_thermal": "error",
                    "text_height": "ignore",
                    "text_on_edge_cuts": "error",
                    "text_thickness": "ignore",
                    "through_hole_pad_without_hole": "error",
                    "too_many_vias": "error",
                    "track_angle": "ignore",
                    "track_dangling": "error",
                    "track_on_post_machined_layer": "error",
                    "track_segment_length": "error",
                    "track_width": "error",
                    "tracks_crossing": "error",
                    "tuning_profile_track_geometries": "ignore",
                    "unconnected_items": "error",
                    "unresolved_variable": "error",
                    "via_dangling": "error",
                    "zones_intersect": "error",
                },
                "rules": {
                    "allow_blind_buried_vias": False,
                    "allow_microvias": False,
                    "max_error": 0.01,
                    "min_clearance": 0.45,
                    "min_connection": 0.25,
                    "min_copper_edge_clearance": 0.50,
                    "min_groove_width": 0.0,
                    "min_hole_clearance": 0.25,
                    "min_hole_to_hole": 0.50,
                    "min_microvia_diameter": 0.50,
                    "min_microvia_drill": 0.20,
                    "min_resolved_spokes": 1,
                    "min_silk_clearance": 0.0,
                    "min_text_height": 0.8,
                    "min_text_thickness": 0.12,
                    "min_through_hole_diameter": 0.85,
                    "min_track_width": 0.50,
                    "min_via_annular_width": 0.35,
                    "min_via_diameter": 1.60,
                    "solder_mask_to_copper_clearance": 0.0,
                    "use_height_for_length_calcs": True,
                },
                "track_widths": [0.5, 0.6, 0.8, 1.0, 1.2],
                "via_dimensions": [],
            },
            "ipc2581": {},
            "layer_pairs": [],
            "layer_presets": [],
            "viewports": [],
        },
        "boards": [],
        "cvpcb": {"equivalence_files": []},
        "erc": {
            "erc_exclusions": [],
            "meta": {"version": 0},
            "pin_map": [
                [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2],
                [0, 2, 0, 1, 0, 0, 1, 0, 2, 2, 2, 2],
                [0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 2],
                [0, 1, 0, 0, 0, 0, 1, 0, 2, 1, 1, 2],
                [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2],
                [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2],
                [1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 2],
                [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2],
                [0, 2, 1, 2, 0, 0, 1, 0, 2, 2, 2, 2],
                [0, 2, 0, 1, 0, 0, 1, 0, 2, 0, 0, 2],
                [0, 2, 1, 1, 0, 0, 1, 0, 2, 0, 0, 2],
                [2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2],
            ],
            "rule_severities": {
                "bus_definition_conflict": "error",
                "bus_entry_needed": "error",
                "bus_to_bus_conflict": "error",
                "bus_to_net_conflict": "error",
                "conflicting_netclasses": "error",
                "different_unit_footprint": "error",
                "different_unit_net": "error",
                "duplicate_reference": "error",
                "DuplicateSheetNames": "error",
                "endpoint_off_grid": "ignore",
                "extra_units": "error",
                "footprint_link_issues": "ignore",
                "four_way_junction": "ignore",
                "global_label_dangling": "error",
                "hier_label_mismatch": "error",
                "label_dangling": "error",
                "lib_symbol_issues": "ignore",
                "lib_symbol_mismatch": "ignore",
                "missing_bidi_pin": "error",
                "missing_input_pin": "error",
                "missing_power_pin": "error",
                "missing_unit": "error",
                "multiple_net_names": "error",
                "net_not_bus_member": "error",
                "no_connect_connected": "error",
                "no_connect_dangling": "error",
                "pin_not_connected": "error",
                "pin_not_driven": "ignore",
                "pin_to_pin": "error",
                "power_pin_not_driven": "ignore",
                "same_local_global_label": "error",
                "similar_labels": "ignore",
                "simulation_model_issue": "ignore",
                "single_global_label": "ignore",
                "unannotated": "error",
                "unit_value_mismatch": "error",
                "unresolved_variable": "error",
                "wire_dangling": "error",
            },
        },
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "meta": {"filename": f"{project_name}.kicad_pro", "version": 1},
        "net_settings": {
            "classes": [
                {
                    "bus_width": 12,
                    "clearance": 0.45,
                    "diff_pair_gap": 0.45,
                    "diff_pair_via_gap": 0.45,
                    "diff_pair_width": 0.50,
                    "line_style": 0,
                    "microvia_diameter": 0.50,
                    "microvia_drill": 0.20,
                    "name": "Default",
                    "pcb_color": "rgba(0, 0, 0, 0.000)",
                    "priority": 2147483647,
                    "schematic_color": "rgba(0, 0, 0, 0.000)",
                    "track_width": 0.50,
                    "tuning_profile": "",
                    "via_diameter": 1.60,
                    "via_drill": 0.80,
                    "wire_width": 6,
                }
            ],
            "meta": {"version": 5},
            "net_colors": {},
            "netclass_assignments": {},
            "netclass_patterns": [],
        },
        "pcbnew": {"page_layout_descr_file": ""},
        "schematic": {
            "meta": {"version": 1},
            "top_level_sheets": [
                {
                    "filename": f"{project_name}.kicad_sch",
                    "name": project_name,
                    "uuid": sch_uuid,
                }
            ],
        },
        "sheets": [[sch_uuid, ""]],
        "text_variables": {},
    }
    return json.dumps(pro, indent=2) + "\n"


def make_kicad_pro_jlcpcb(project_name: str, sch_uuid: str) -> str:
    """Create KiCad 10 .kicad_pro JSON with 2-layer JLCPCB PCBA DRC rules."""
    pro = json.loads(make_kicad_pro(project_name, sch_uuid))
    pro["board"]["design_settings"]["defaults"]["copper_line_width"] = 0.35
    pro["board"]["design_settings"]["defaults"]["zones"] = {"45_degree_only": False, "min_clearance": 0.30}
    pro["board"]["design_settings"]["rule_severities"]["courtyards_overlap"] = "error"
    pro["board"]["design_settings"]["rules"].update(
        {
            "min_clearance": 0.20,
            "min_connection": 0.20,
            "min_copper_edge_clearance": 0.40,
            "min_hole_clearance": 0.25,
            "min_hole_to_hole": 0.40,
            "min_through_hole_diameter": 0.35,
            "min_track_width": 0.20,
            "min_via_annular_width": 0.15,
            "min_via_diameter": 0.70,
        }
    )
    pro["board"]["design_settings"]["track_widths"] = [0.25, 0.35, 0.50, 0.60, 0.75, 0.90]
    pro["board"]["design_settings"]["via_dimensions"] = [{"diameter": 0.80, "drill": 0.40}]
    pro["net_settings"]["classes"][0].update(
        {
            "clearance": 0.20,
            "track_width": 0.35,
            "via_diameter": 0.80,
            "via_drill": 0.40,
        }
    )
    return json.dumps(pro, indent=2) + "\n"


def make_kicad_sch(
    project_name: str,
    title: str,
    sch_uuid: str,
    sch_spec: SchSpec,
    rev: str = "v1.0-cnc",
) -> str:
    """Serialize a complete KiCad 10 .kicad_sch circuit diagram."""
    sch_title = sch_spec.sch_title or title
    lines: List[str] = [
        "(kicad_sch",
        "\t(version 20260101)",
        '\t(generator "eeschema")',
        '\t(generator_version "10.0")',
        f'\t(uuid "{sch_uuid}")',
        f'\t(paper "{sch_spec.paper}")',
        "\t(title_block",
        f'\t\t(title "{sch_title}")',
        '\t\t(date "2026-11-24")',
        f'\t\t(rev "{rev}")',
        '\t\t(company "Echenon Hardware")',
        "\t)",
        "\t(lib_symbols",
    ]
    for sym_def in sch_spec.lib_symbols:
        for raw_line in sym_def.splitlines():
            lines.append(f"\t\t{raw_line}")
    lines.append("\t)")

    # Annotation frames (dashed / solid boxes & polylines)
    for idx, (x1, y1, x2, y2, stype, color) in enumerate(sch_spec.rects):
        lines.extend(
            [
                "\t(rectangle",
                f"\t\t(start {x1:.2f} {y1:.2f})",
                f"\t\t(end {x2:.2f} {y2:.2f})",
                f"\t\t(stroke (width 0.3048) (type {stype}) (color {color}))",
                "\t\t(fill (type none))",
                f'\t\t(uuid "{det_uuid(f"sch_rect:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, (pts, stype, color) in enumerate(sch_spec.polylines):
        pts_str = " ".join(f"(xy {x:.2f} {y:.2f})" for x, y in pts)
        lines.extend(
            [
                "\t(polyline",
                f"\t\t(pts {pts_str})",
                f"\t\t(stroke (width 0.3048) (type {stype}) (color {color}))",
                "\t\t(fill (type none))",
                f'\t\t(uuid "{det_uuid(f"sch_poly:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    # Annotation texts
    for idx, (text, x, y, size, color) in enumerate(sch_spec.texts):
        lines.extend(
            [
                f'\t(text "{text}"',
                f"\t\t(at {x:.2f} {y:.2f} 0)",
                f"\t\t(effects (font (size {size:.3f} {size:.3f}) (bold yes) (color {color})) (justify left top))",
                f'\t\t(uuid "{det_uuid(f"sch_text:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    # Placed symbols
    for sym in sch_spec.symbols:
        suuid = sym.sym_uuid if sym.sym_uuid else det_uuid(f"sym:{sym.ref}")
        rx, ry, rrot = sym.ref_at if sym.ref_at else (sym.sx, sym.sy - 5.08, 0)
        vx, vy, vrot = sym.val_at if sym.val_at else (sym.sx, sym.sy + 5.08, 0)
        mirror_line = f"\t\t(mirror {sym.mirror})" if sym.mirror else ""
        hide_ref_str = " (hide yes)" if sym.hide_ref else ""
        hide_val_str = " (hide yes)" if sym.hide_val else ""
        rj = f" (justify {sym.ref_justify})" if sym.ref_justify else ""
        vj = f" (justify {sym.val_justify})" if sym.val_justify else ""
        in_pos = sym.in_pos_files if sym.in_pos_files is not None else sym.on_board

        lines.extend(
            [
                "\t(symbol",
                f'\t\t(lib_id "{sym.lib_id}")',
                f"\t\t(at {sym.sx:.2f} {sym.sy:.2f} {sym.rot})",
                mirror_line,
                "\t\t(unit 1)",
                "\t\t(body_style 1)",
                f'\t\t(exclude_from_sim {"yes" if not sym.on_board else "no"})',
                f'\t\t(in_bom {"yes" if sym.in_bom else "no"})',
                f'\t\t(on_board {"yes" if sym.on_board else "no"})',
                f'\t\t(in_pos_files {"yes" if in_pos else "no"})',
                f'\t\t(dnp {"yes" if sym.dnp else "no"})',
                f'\t\t(uuid "{suuid}")',
                f'\t\t(property "Reference" "{sym.ref}" (at {rx:.2f} {ry:.2f} {rrot}){hide_ref_str} (effects (font (size 1.27 1.27)){rj}))',
                f'\t\t(property "Value" "{sym.value}" (at {vx:.2f} {vy:.2f} {vrot}){hide_val_str} (effects (font (size 1.143 1.143)){vj}))',
                f'\t\t(property "Footprint" "{sym.footprint}" (at {sym.sx:.2f} {sym.sy:.2f} 0) (hide yes) (effects (font (size 1.27 1.27))))',
                f'\t\t(property "Datasheet" "" (at {sym.sx:.2f} {sym.sy:.2f} 0) (hide yes) (effects (font (size 1.27 1.27))))',
                f'\t\t(property "Description" "{sym.descr}" (at {sym.sx:.2f} {sym.sy:.2f} 0) (hide yes) (effects (font (size 1.27 1.27))))',
            ]
        )
        if sym.lcsc:
            lines.append(f'\t\t(property "LCSC" "{sym.lcsc}" (at {sym.sx:.2f} {sym.sy:.2f} 0) (hide yes) (effects (font (size 1.27 1.27))))')
        if sym.mpn:
            lines.append(f'\t\t(property "MPN" "{sym.mpn}" (at {sym.sx:.2f} {sym.sy:.2f} 0) (hide yes) (effects (font (size 1.27 1.27))))')
        for pnum in sym.pins:
            lines.append(f'\t\t(pin "{pnum}" (uuid "{det_uuid(f"sch_pin:{sym.ref}:{pnum}")}"))')
        lines.extend(
            [
                "\t\t(instances",
                f'\t\t\t(project "{project_name}"',
                f'\t\t\t\t(path "/{sch_uuid}"',
                f'\t\t\t\t\t(reference "{sym.ref}")',
                "\t\t\t\t\t(unit 1)",
                "\t\t\t\t)",
                "\t\t\t)",
                "\t\t)",
                "\t)",
            ]
        )

    # Wires, junctions, net labels, no-connects
    for idx, (x1, y1, x2, y2) in enumerate(sch_spec.wires):
        lines.extend(
            [
                "\t(wire",
                f"\t\t(pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f}))",
                "\t\t(stroke (width 0) (type solid))",
                f'\t\t(uuid "{det_uuid(f"wire:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, (jx, jy) in enumerate(sch_spec.junctions):
        lines.extend(
            [
                "\t(junction",
                f"\t\t(at {jx:.2f} {jy:.2f})",
                "\t\t(diameter 0)",
                "\t\t(color 0 0 0 0)",
                f'\t\t(uuid "{det_uuid(f"junc:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, (lbl, lx, ly, ang, just) in enumerate(sch_spec.labels):
        lines.extend(
            [
                f'\t(label "{lbl}"',
                f"\t\t(at {lx:.2f} {ly:.2f} {ang})",
                f"\t\t(effects (font (size 1.016 1.016)) (justify {just}))",
                f'\t\t(uuid "{det_uuid(f"lbl:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, (nx, ny) in enumerate(sch_spec.no_connects):
        lines.extend(
            [
                "\t(no_connect",
                f"\t\t(at {nx:.2f} {ny:.2f})",
                f'\t\t(uuid "{det_uuid(f"nc:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    lines.extend(
        [
            "\t(sheet_instances",
            '\t\t(path "/"',
            '\t\t\t(page "1")',
            "\t\t)",
            "\t)",
            "\t(embedded_fonts no)",
            ")",
        ]
    )
    return "\n".join(l for l in lines if l != "") + "\n"


def make_kicad_pcb(
    project_name: str,
    title: str,
    width_mm: float,
    height_mm: float,
    components: List[ComponentSpec],
    segments: List[TrackSegment],
    edge_circles: List[Tuple[float, float, float]],
    silk_texts: List[SilkText],
    vias: Optional[List[ViaSpec]] = None,
    zones: Optional[List[ZoneSpec]] = None,
    allow_f_cu: bool = False,
    rev: str = "v1.0-carvera-air",
) -> str:
    """Generate a valid KiCad 10 .kicad_pcb file."""
    ox = round((297.0 - width_mm) / 2.0, 2)
    oy = round((210.0 - height_mm) / 2.0, 2)

    lines: List[str] = [
        "(kicad_pcb",
        "\t(version 20260206)",
        '\t(generator "pcbnew")',
        '\t(generator_version "10.0")',
        "\t(general",
        "\t\t(thickness 1.6)",
        "\t\t(legacy_teardrops no)",
        "\t)",
        '\t(paper "A4")',
        "\t(title_block",
        f'\t\t(title "{title}")',
        '\t\t(date "2026-11-24")',
        f'\t\t(rev "{rev}")',
        '\t\t(company "Echenon Hardware")',
        "\t)",
        "\t(layers",
        '\t\t(0 "F.Cu" signal "top_layer")',
        '\t\t(2 "B.Cu" signal "bottom_layer")',
        '\t\t(9 "F.Adhes" user "F.Adhesive")',
        '\t\t(11 "B.Adhes" user "B.Adhesive")',
        '\t\t(13 "F.Paste" user)',
        '\t\t(15 "B.Paste" user)',
        '\t\t(5 "F.SilkS" user "F.Silkscreen")',
        '\t\t(7 "B.SilkS" user "B.Silkscreen")',
        '\t\t(1 "F.Mask" user)',
        '\t\t(3 "B.Mask" user)',
        '\t\t(17 "Dwgs.User" user "User.Drawings")',
        '\t\t(19 "Cmts.User" user "User.Comments")',
        '\t\t(21 "Eco1.User" user "User.Eco1")',
        '\t\t(23 "Eco2.User" user "User.Eco2")',
        '\t\t(25 "Edge.Cuts" user)',
        '\t\t(27 "Margin" user)',
        '\t\t(31 "F.CrtYd" user "F.Courtyard")',
        '\t\t(29 "B.CrtYd" user "B.Courtyard")',
        '\t\t(35 "F.Fab" user)',
        '\t\t(33 "B.Fab" user)',
        "\t)",
        "\t(setup",
        "\t\t(pad_to_mask_clearance 0.05)",
        "\t\t(allow_soldermask_bridges_in_footprints no)",
        "\t\t(tenting (front no) (back no))",
        f"\t\t(aux_axis_origin {ox:.4f} {oy + height_mm:.4f})",
        f"\t\t(grid_origin {ox:.4f} {oy:.4f})",
        "\t)",
    ]

    for comp in components:
        non_empty_pins = [f'"{p.number}"' for p in comp.pads if p.number]
        pins_line = f"\t\t\t\t(pins {' '.join(non_empty_pins)})" if non_empty_pins else ""
        rx, ry = comp.ref_offset
        vx, vy = comp.val_offset
        lines.extend(
            [
                f'\t(footprint "{comp.fp_name}"',
                '\t\t(layer "F.Cu")',
                f'\t\t(uuid "{comp.fp_uuid}")',
                f"\t\t(at {comp.x + ox:.4f} {comp.y + oy:.4f})",
                f'\t\t(descr "{comp.descr}")',
                f'\t\t(property "Reference" "{comp.ref}"',
                f"\t\t\t(at {rx:.4f} {ry:.4f} {comp.ref_angle:.0f})",
                '\t\t\t(layer "F.SilkS")',
                '\t\t\t(hide yes)' if comp.hide_ref else "",
                f'\t\t\t(uuid "{det_uuid(f"prop_ref:{comp.ref}")}")',
                "\t\t\t(effects (font (size 0.80 0.80) (thickness 0.12)))",
                "\t\t)",
                f'\t\t(property "Value" "{comp.value}"',
                f"\t\t\t(at {vx:.4f} {vy:.4f} 0)",
                '\t\t\t(layer "F.Fab")',
                '\t\t\t(hide yes)' if comp.hide_val else "",
                f'\t\t\t(uuid "{det_uuid(f"prop_val:{comp.ref}")}")',
                "\t\t\t(effects (font (size 0.75 0.75) (thickness 0.12)))",
                "\t\t)",
                f'\t\t(property "Datasheet" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{det_uuid(f"prop_ds:{comp.ref}")}") (effects (font (size 1 1))))',
                f'\t\t(property "Description" "{comp.descr}" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{det_uuid(f"prop_desc:{comp.ref}")}") (effects (font (size 1 1))))',
            ]
        )
        if comp.lcsc:
            lines.append(
                f'\t\t(property "LCSC" "{comp.lcsc}" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{det_uuid(f"prop_lcsc:{comp.ref}")}") (effects (font (size 1 1))))'
            )
        if comp.mpn:
            lines.append(
                f'\t\t(property "MPN" "{comp.mpn}" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{det_uuid(f"prop_mpn:{comp.ref}")}") (effects (font (size 1 1))))'
            )
        lines.extend(
            [
                f'\t\t(path "/{comp.sym_uuid}")',
                '\t\t(sheetname "/")',
                f'\t\t(sheetfile "{project_name}.kicad_sch")',
                "\t\t(units",
                "\t\t\t(unit",
                '\t\t\t\t(name "A")',
                pins_line,
                "\t\t\t)",
                "\t\t)",
                f"\t\t(attr {comp.attr})",
                "\t\t(duplicate_pad_numbers_are_jumpers no)",
            ]
        )
        for gfx in comp.extra_fp_graphics:
            lines.append(f"\t\t{gfx}")
        for idx_p, pad in enumerate(comp.pads):
            pad_uuid = det_uuid(f"pad:{comp.ref}:{pad.number or idx_p}")
            pad_layers = pad.layers if pad.layers else ('"F.Cu" "F.Mask" "F.Paste"' if pad.pad_type == "smd" else '"*.Cu" "*.Mask"')
            lines.extend(
                [
                    f'\t\t(pad "{pad.number}" {pad.pad_type} {pad.shape}',
                    f"\t\t\t(at {pad.dx:.4f} {pad.dy:.4f})",
                    f"\t\t\t(size {pad.w:.4f} {pad.h:.4f})",
                ]
            )
            if pad.pad_type in ("thru_hole", "np_thru_hole") and pad.drill > 0:
                lines.append(f"\t\t\t(drill {pad.drill:.4f})")
            if pad.pad_prop:
                lines.append(f"\t\t\t(property {pad.pad_prop})")
            lines.append(f"\t\t\t(layers {pad_layers})")
            if pad.pad_type == "thru_hole":
                lines.append("\t\t\t(remove_unused_layers no)")
            if pad.shape == "roundrect" and pad.rratio > 0:
                lines.append(f"\t\t\t(roundrect_rratio {pad.rratio:.6f})")
            if pad.solder_mask_margin is not None:
                lines.append(f"\t\t\t(solder_mask_margin {pad.solder_mask_margin:.4f})")
            if pad.clearance is not None:
                lines.append(f"\t\t\t(clearance {pad.clearance:.4f})")
            if pad.number:
                if pad.net:
                    lines.append(f'\t\t\t(net "{pad.net}")')
                    pt = pad.pintype
                else:
                    unconn = pad.unconnected_net or f"unconnected-({comp.ref}-P{pad.number}-Pad{pad.number})"
                    lines.append(f'\t\t\t(net "{unconn}")')
                    pt = "passive+no_connect"
                lines.append(f'\t\t\t(pintype "{pt}")')
            lines.extend(
                [
                    f'\t\t\t(uuid "{pad_uuid}")',
                    "\t\t)",
                ]
            )
        if comp.model_3d:
            lines.extend(
                [
                    f'\t\t(model "{comp.model_3d}"',
                    "\t\t\t(offset (xyz 0 0 0))",
                    "\t\t\t(scale (xyz 1 1 1))",
                    "\t\t\t(rotate (xyz 0 0 0))",
                    "\t\t)",
                ]
            )
        lines.extend(["\t\t(embedded_fonts no)", "\t)"])

    lines.extend(
        [
            "\t(gr_rect",
            f"\t\t(start {ox:.4f} {oy:.4f})",
            f"\t\t(end {ox + width_mm:.4f} {oy + height_mm:.4f})",
            "\t\t(stroke (width 0.15) (type solid))",
            "\t\t(fill no)",
            '\t\t(layer "Edge.Cuts")',
            f'\t\t(uuid "{det_uuid(f"edge_rect:{project_name}")}")',
            "\t)",
        ]
    )

    for idx, (cx, cy, r) in enumerate(edge_circles):
        lines.extend(
            [
                "\t(gr_circle",
                f"\t\t(center {cx + ox:.4f} {cy + oy:.4f})",
                f"\t\t(end {cx + ox + r:.4f} {cy + oy:.4f})",
                "\t\t(stroke (width 0.15) (type solid))",
                "\t\t(fill no)",
                '\t\t(layer "Edge.Cuts")',
                f'\t\t(uuid "{det_uuid(f"edge_circle:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, st in enumerate(silk_texts):
        justify = " (justify mirror)" if st.mirror else ""
        lines.extend(
            [
                f'\t(gr_text "{st.text}"',
                f"\t\t(at {st.x + ox:.4f} {st.y + oy:.4f} {st.angle:.1f})",
                f'\t\t(layer "{st.layer}")',
                f'\t\t(uuid "{det_uuid(f"silk_text:{project_name}:{idx}")}")',
                f"\t\t(effects (font (size {st.size:.2f} {st.size:.2f}) (thickness {st.thickness:.2f})){justify})",
                "\t)",
            ]
        )

    for idx, seg in enumerate(segments):
        if not allow_f_cu:
            assert seg.layer == "B.Cu", f"Forbidden non-B.Cu segment on {seg.layer}"
        if abs(seg.x1 - seg.x2) < 1e-6 and abs(seg.y1 - seg.y2) < 1e-6:
            continue
        lines.extend(
            [
                "\t(segment",
                f"\t\t(start {seg.x1 + ox:.4f} {seg.y1 + oy:.4f})",
                f"\t\t(end {seg.x2 + ox:.4f} {seg.y2 + oy:.4f})",
                f"\t\t(width {seg.width:.4f})",
                f'\t\t(layer "{seg.layer}")',
                f'\t\t(net "{seg.net}")',
                f'\t\t(uuid "{det_uuid(f"seg:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, v in enumerate(vias or []):
        lines.extend(
            [
                "\t(via",
                f"\t\t(at {v.x + ox:.4f} {v.y + oy:.4f})",
                f"\t\t(size {v.size:.4f})",
                f"\t\t(drill {v.drill:.4f})",
                '\t\t(layers "F.Cu" "B.Cu")',
                f'\t\t(net "{v.net}")',
                f'\t\t(uuid "{det_uuid(f"via:{project_name}:{idx}")}")',
                "\t)",
            ]
        )

    for idx, z in enumerate(zones or []):
        lines.extend(
            [
                "\t(zone",
                f'\t\t(net "{z.net}")',
                f'\t\t(layer "{z.layer}")',
                f'\t\t(uuid "{det_uuid(f"zone:{project_name}:{idx}")}")',
                "\t\t(hatch edge 0.508)",
                "\t\t(connect_pads",
                f"\t\t\t(clearance {z.clearance:.4f})",
                "\t\t)",
                f"\t\t(min_thickness {z.min_thickness:.4f})",
                "\t\t(filled_areas_thickness no)",
                "\t\t(fill yes",
                f"\t\t\t(thermal_gap {z.thermal_gap:.4f})",
                f"\t\t\t(thermal_bridge_width {z.thermal_bridge_width:.4f})",
                "\t\t)",
                "\t\t(polygon",
                "\t\t\t(pts",
                f"\t\t\t\t(xy {z.x1 + ox:.4f} {z.y1 + oy:.4f}) (xy {z.x2 + ox:.4f} {z.y1 + oy:.4f}) (xy {z.x2 + ox:.4f} {z.y2 + oy:.4f}) (xy {z.x1 + ox:.4f} {z.y2 + oy:.4f})",
                "\t\t\t)",
                "\t\t)",
                "\t)",
            ]
        )

    lines.extend(["\t(embedded_fonts no)", ")"])
    return "\n".join(l for l in lines if l != "") + "\n"


def add_chain(
    segments: List[TrackSegment],
    pts: List[Tuple[float, float]],
    width: float,
    net: str,
    layer: str = "B.Cu",
) -> None:
    for i in range(len(pts) - 1):
        x1, y1 = pts[i]
        x2, y2 = pts[i + 1]
        if abs(x1 - x2) > 1e-6 or abs(y1 - y2) > 1e-6:
            segments.append(TrackSegment(x1, y1, x2, y2, width, net, layer))


def build_coupon_project():
    """Deliverable A: 25 x 20 mm Carvera Air Calibration Coupon."""
    proj_name = "carvera_calibration_coupon"
    title = "Echenon Carvera Air Calibration Coupon (25x20mm)"
    w_mm, h_mm = 25.0, 20.0

    j_loom1 = ComponentSpec(
        ref="J_LOOM1",
        value="PIGTAIL_2P",
        lib_id="Connector_Generic:Conn_01x02",
        fp_name="echenon:Pigtail_1x02_P2.54mm_CNC",
        x=5.00,
        y=4.50,
        pads=[
            PadSpec("1", 0.0, 0.0, "oval", 1.75, 2.30, 1.00, "/MOT_RAW"),
            PadSpec("2", 2.54, 0.0, "oval", 1.75, 2.30, 1.00, "/MOT_RAW"),
        ],
        sch_x=70.0,
        sch_y=50.0,
        descr="2-pin JST pigtail pass-through test pads (1.0mm drill)",
        ref_offset=(1.27, -2.0),
        hide_val=True,
    )

    j_test1 = ComponentSpec(
        ref="J_TEST1",
        value="NANO_HDR_3P",
        lib_id="Connector_Generic:Conn_01x03",
        fp_name="echenon:NanoSocket_1x03_P2.54mm_CNC",
        x=15.00,
        y=4.50,
        pads=[
            PadSpec("1", 0.0, 0.0, "oval", 1.75, 2.20, 1.00, "GND"),
            PadSpec("2", 2.54, 0.0, "oval", 1.75, 2.20, 1.00, "/TAP_OUT"),
            PadSpec("3", 5.08, 0.0, "oval", 1.75, 2.20, 1.00, "/MOT_RAW"),
        ],
        sch_x=70.0,
        sch_y=80.0,
        descr="3-pin Waveshare Nano socket test strip (1.0mm drill)",
        ref_offset=(2.54, -2.0),
        hide_val=True,
    )

    r_div1 = ComponentSpec(
        ref="R_DIV1",
        value="30k",
        lib_id="Device:R",
        fp_name="echenon:R_Axial_P7.62mm_CNC",
        x=7.54,
        y=8.50,
        pads=[
            PadSpec("1", 0.0, 0.0, "circle", 2.10, 2.10, 0.90, "/MOT_RAW"),
            PadSpec("2", 0.0, 7.62, "circle", 2.10, 2.10, 0.90, "/TAP_OUT"),
        ],
        sch_x=130.0,
        sch_y=50.0,
        descr="30k 1/4W axial resistor (0.9mm drill)",
        extra_fp_graphics=['(fp_rect (start -1.25 1.3) (end 1.25 6.32) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))'],
        ref_offset=(-2.4, 3.81),
        val_offset=(0.0, 3.81),
        ref_angle=90.0,
    )

    r_div2 = ComponentSpec(
        ref="R_DIV2",
        value="10k",
        lib_id="Device:R",
        fp_name="echenon:R_Axial_P7.62mm_CNC",
        x=12.50,
        y=8.50,
        pads=[
            PadSpec("1", 0.0, 0.0, "circle", 2.10, 2.10, 0.90, "GND"),
            PadSpec("2", 0.0, 7.62, "circle", 2.10, 2.10, 0.90, "/TAP_OUT"),
        ],
        sch_x=130.0,
        sch_y=80.0,
        descr="10k 1/4W axial resistor (0.9mm drill)",
        extra_fp_graphics=['(fp_rect (start -1.25 1.3) (end 1.25 6.32) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))'],
        ref_offset=(-2.0, 3.81),
        val_offset=(0.0, 3.81),
        ref_angle=90.0,
    )

    c_filt1 = ComponentSpec(
        ref="C_FILT1",
        value="100nF",
        lib_id="Device:C",
        fp_name="echenon:C_Disc_P5.08mm_CNC",
        x=16.50,
        y=11.04,
        pads=[
            PadSpec("1", 0.0, 0.0, "circle", 2.10, 2.10, 0.90, "GND"),
            PadSpec("2", 0.0, 5.08, "circle", 2.10, 2.10, 0.90, "/TAP_OUT"),
        ],
        sch_x=130.0,
        sch_y=110.0,
        descr="100nF ceramic capacitor (0.9mm drill)",
        extra_fp_graphics=['(fp_rect (start -1.1 1.2) (end 1.1 3.88) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))'],
        ref_offset=(2.8, 2.54),
        val_offset=(0.0, 2.54),
        ref_angle=90.0,
        hide_val=True,
    )

    components = [j_loom1, j_test1, r_div1, r_div2, c_filt1]
    segments: List[TrackSegment] = []

    add_chain(segments, [j_loom1.pad_board_xy("1"), j_loom1.pad_board_xy("2"), r_div1.pad_board_xy("1")], 1.00, "/MOT_RAW")
    add_chain(
        segments,
        [j_loom1.pad_board_xy("2"), (7.54, 1.60), (20.08, 1.60), j_test1.pad_board_xy("3")],
        0.50,
        "/MOT_RAW",
    )
    add_chain(segments, [j_test1.pad_board_xy("1"), (15.00, 8.50), r_div2.pad_board_xy("1")], 1.00, "GND")
    add_chain(segments, [(15.00, 8.50), (16.50, 8.50), c_filt1.pad_board_xy("1")], 1.00, "GND")
    add_chain(
        segments,
        [
            r_div1.pad_board_xy("2"),
            r_div2.pad_board_xy("2"),
            c_filt1.pad_board_xy("2"),
            (19.50, 16.12),
            (19.50, 7.20),
            (17.54, 7.20),
            j_test1.pad_board_xy("2"),
        ],
        0.50,
        "/TAP_OUT",
    )

    edge_circles = [(2.40, 4.50, 1.10)]
    silk_texts = [
        SilkText("CARVERA AIR COUPON", 12.5, 18.4, "F.SilkS", 0.95, 0.14),
        SilkText("ECHENON V1 TEST", 12.5, 18.4, "B.SilkS", 0.95, 0.14, mirror=True),
    ]

    # Build clean wired IEC schematic for Coupon (A4)
    lib_syms = [
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x02", "Connector_Generic:Conn_01x02"),
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x03", "Connector_Generic:Conn_01x03"),
        extract_kicad_symbol("Device.kicad_sym", "R", "Device:R"),
        extract_kicad_symbol("Device.kicad_sym", "C", "Device:C"),
        extract_kicad_symbol("power.kicad_sym", "GND", "power:GND"),
    ]

    wires: List[Tuple[float, float, float, float]] = []
    junctions: List[Tuple[float, float]] = []
    labels: List[Tuple[str, float, float, int, str]] = []

    # J_LOOM1 (mirror y, pins face RIGHT at x=60.96, y=88.90 and 91.44)
    jl_sx, jl_sy = 55.88, 88.90
    r1_sx, r1_sy = 114.30, 88.90   # R_DIV1 rot=90 (Pin 1=110.49, Pin 2=118.11)
    r2_sx, r2_sy = 139.70, 106.68  # R_DIV2 rot=180 (Pin 2=139.70,102.87, Pin 1=139.70,110.49)
    c1_sx, c1_sy = 157.48, 106.68  # C_FILT1 rot=180 (Pin 2=157.48,102.87, Pin 1=157.48,110.49)
    jt_sx, jt_sy = 190.50, 88.90   # J_TEST1 mirror=x (Pin 3=185.42,86.36, Pin 2=185.42,88.90, Pin 1=185.42,91.44)

    # Wire J_LOOM1 Pin 1 & Pin 2 -> MOT_RAW junction -> R_DIV1.1 and upper bypass to J_TEST1.3
    add_sch_wire_chain(wires, [(60.96, 91.44), (66.04, 91.44), (66.04, 88.90), (60.96, 88.90)])
    junctions.append((66.04, 88.90))
    add_sch_wire_chain(wires, [(66.04, 88.90), (83.82, 88.90), (110.49, 88.90)])
    junctions.append((83.82, 88.90))
    add_sch_wire_chain(wires, [(83.82, 88.90), (83.82, 76.20), (175.26, 76.20), (175.26, 86.36), (185.42, 86.36)])
    labels.append(("MOT_RAW", 96.52, 88.90, 0, "left bottom"))

    # Wire R_DIV1.2 (118.11, 88.90) -> TAP_OUT -> R_DIV2.2, C_FILT1.2, J_TEST1.2
    add_sch_wire_chain(wires, [(118.11, 88.90), (139.70, 88.90), (157.48, 88.90), (185.42, 88.90)])
    junctions.extend([(139.70, 88.90), (157.48, 88.90)])
    add_sch_wire_chain(wires, [(139.70, 88.90), (139.70, 102.87)])
    add_sch_wire_chain(wires, [(157.48, 88.90), (157.48, 102.87)])
    labels.append(("TAP_OUT", 167.64, 88.90, 0, "left bottom"))

    # GND connections: R_DIV2.1, C_FILT1.1, J_TEST1.1
    add_sch_wire_chain(wires, [(139.70, 110.49), (139.70, 116.84)])
    add_sch_wire_chain(wires, [(157.48, 110.49), (157.48, 116.84)])
    add_sch_wire_chain(wires, [(185.42, 91.44), (177.80, 91.44), (177.80, 116.84)])

    sch_symbols = [
        SchSymbolInstance("J_LOOM1", "PIGTAIL_2P", "Connector_Generic:Conn_01x02", jl_sx, jl_sy, 0, "y", ["1", "2"], j_loom1.fp_name, j_loom1.descr, True, True, False, j_loom1.sym_uuid, (jl_sx, jl_sy - 5.08, 0), (jl_sx, jl_sy + 6.35, 0)),
        SchSymbolInstance("R_DIV1", "30k", "Device:R", r1_sx, r1_sy, 90, None, ["1", "2"], r_div1.fp_name, r_div1.descr, True, True, False, r_div1.sym_uuid, (r1_sx, r1_sy - 3.302, 90), (r1_sx, r1_sy + 3.302, 90)),
        SchSymbolInstance("R_DIV2", "10k", "Device:R", r2_sx, r2_sy, 180, None, ["1", "2"], r_div2.fp_name, r_div2.descr, True, True, False, r_div2.sym_uuid, (r2_sx - 2.54, r2_sy, 0), (r2_sx + 2.54, r2_sy, 0), ref_justify="left", val_justify="right"),
        SchSymbolInstance("C_FILT1", "100nF", "Device:C", c1_sx, c1_sy, 180, None, ["1", "2"], c_filt1.fp_name, c_filt1.descr, True, True, False, c_filt1.sym_uuid, (c1_sx - 3.048, c1_sy, 0), (c1_sx + 3.048, c1_sy, 0), ref_justify="left", val_justify="right"),
        SchSymbolInstance("J_TEST1", "NANO_HDR_3P", "Connector_Generic:Conn_01x03", jt_sx, jt_sy, 0, "x", ["1", "2", "3"], j_test1.fp_name, j_test1.descr, True, True, False, j_test1.sym_uuid, (jt_sx, jt_sy - 6.35, 0), (jt_sx, jt_sy + 6.35, 0)),
        SchSymbolInstance("#PWR01", "GND", "power:GND", 139.70, 116.84, 0, None, ["1"], "", "GND", False, False, False, None, (139.70, 123.19, 0), (139.70, 120.65, 0), hide_ref=True),
        SchSymbolInstance("#PWR02", "GND", "power:GND", 157.48, 116.84, 0, None, ["1"], "", "GND", False, False, False, None, (157.48, 123.19, 0), (157.48, 120.65, 0), hide_ref=True),
        SchSymbolInstance("#PWR03", "GND", "power:GND", 177.80, 116.84, 0, None, ["1"], "", "GND", False, False, False, None, (177.80, 123.19, 0), (177.80, 120.65, 0), hide_ref=True),
    ]

    sch_spec = SchSpec(
        paper="A4",
        lib_symbols=lib_syms,
        symbols=sch_symbols,
        wires=wires,
        junctions=junctions,
        labels=labels,
        rects=[(40.64, 63.50, 208.28, 132.08, "solid", "0 90 150 1")],
        texts=[
            ("CARVERA AIR CALIBRATION COUPON (25x20 mm SINGLE-SIDED B.Cu)", 43.18, 66.04, 1.778, "0 90 150 1"),
            ("DIV4 MOTOR SENSE TEST CELL (30k / 10k + 100nF)", 109.22, 71.12, 1.27, "80 80 80 1"),
        ],
        sch_title="Carvera Calibration Coupon (25x20mm)",
    )

    return proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts, sch_spec


def build_v1_passive_tap_project():
    """Deliverable B: 70 x 50 mm Echenon v1 Passive Tap Board (100% B.Cu single-sided)."""
    proj_name = "echenon_v1_passive_tap"
    title = "Echenon v1 Passive Tap Board (EX-3 / Waveshare ESP32-S3-Nano)"
    w_mm, h_mm = 70.0, 50.0

    # U1 (Waveshare ESP32-S3-Nano in J5 2x15 socket)
    u1_top_nets: Dict[int, str] = {
        10: "/GPIO10_ENC_VCC",
        9: "/GPIO9_ENC_B",
        8: "/GPIO8_ENC_A",
        7: "/GPIO7_MOT_N",
        6: "/GPIO6_MOT_P",
        5: "/GPIO5_CADENCE",
        4: "GND",
    }
    u1_bot_nets: Dict[int, str] = {
        19: "/GPIO1_POT_WIPER",
        20: "/GPIO2_POT_HI",
        21: "/GPIO3_POT_LO",
        22: "/GPIO4_CAD_RTN",
        29: "GND",
        30: "/NET_VIN_9V",
    }

    u1_pads: List[PadSpec] = []
    for dy, row_nets, pnum_fn in (
        (0.0, u1_top_nets, lambda i: 15 - i),
        (15.24, u1_bot_nets, lambda i: 16 + i),
    ):
        for i in range(15):
            pnum = pnum_fn(i)
            dx = round(i * 2.54, 4)
            net = row_nets.get(pnum, "")
            unconn = "" if net else kicad_unconnected_net("U1", NANO_PIN_NAMES[str(pnum)], str(pnum))
            u1_pads.append(PadSpec(str(pnum), dx, dy, "oval", 1.75, 2.20, 1.00, net, unconnected_net=unconn))

    u1 = ComponentSpec(
        ref="U1",
        value="Waveshare_ESP32-S3-Nano",
        lib_id="echenon:Waveshare_ESP32_S3_Nano",
        fp_name="echenon:Waveshare_ESP32_S3_Nano_Socket_CNC",
        x=11.00,
        y=24.50,
        pads=u1_pads,
        sch_x=317.50,
        sch_y=142.24,
        descr="Waveshare ESP32-S3-Nano in J5 2x15 female header strips (1.0mm drill)",
        extra_fp_graphics=[
            '(fp_rect (start -1.8 -1.8) (end 37.36 17.04) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))',
            '(fp_rect (start -3.0 4.12) (end -0.4 11.12) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))',
        ],
        ref_offset=(17.78, 5.40),
        val_offset=(17.78, 10.20),
    )

    j3_nets = ["/NET_J3_1_ENC_A", "/NET_J3_2_ENC_VCC", "/NET_J3_3_ENC_B"]
    j4 = ComponentSpec(
        ref="J4",
        value="JST_3P_CONSOLE",
        lib_id="Connector_Generic:Conn_01x03",
        fp_name="echenon:Pigtail_1x03_P2.54mm_CNC",
        x=11.00,
        y=3.80,
        pads=[PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "oval", 1.75, 2.30, 1.00, j3_nets[i]) for i in range(3)],
        sch_x=152.40,
        sch_y=266.70,
        descr="3-pin JST plug pigtail to stock console encoder socket",
        ref_offset=(-2.2, 0.0),
        hide_val=True,
    )
    j3 = ComponentSpec(
        ref="J3",
        value="JST_3P_KNOB",
        lib_id="Connector_Generic:Conn_01x03",
        fp_name="echenon:Pigtail_1x03_P2.54mm_CNC",
        x=11.00,
        y=7.00,
        pads=[PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "oval", 1.75, 2.30, 1.00, j3_nets[i]) for i in range(3)],
        sch_x=114.30,
        sch_y=266.70,
        descr="3-pin JST socket pigtail from handlebar resistance knob",
        ref_offset=(-2.2, 0.0),
        hide_val=True,
    )

    j1_nets = [
        "/NET_J1_1_MOT_P",
        "/NET_J1_2_MOT_N",
        "/NET_J1_3_POT_HI",
        "/NET_J1_4_POT_WIPER",
        "/NET_J1_5_POT_LO",
        "/NET_J1_6_CADENCE",
        "/NET_J1_7_CAD_RTN",
        "/NET_J1_8_V9_RAW",
        "GND",
    ]
    j2 = ComponentSpec(
        ref="J2",
        value="JST_SM_9P_CONSOLE",
        lib_id="Connector_Generic:Conn_01x09",
        fp_name="echenon:Pigtail_1x09_P2.54mm_CNC",
        x=35.00,
        y=3.80,
        pads=[PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "oval", 1.75, 2.30, 1.00, j1_nets[i]) for i in range(9)],
        sch_x=152.40,
        sch_y=187.96,
        descr="9-pin JST-SM plug pigtail to stock EX-3 console PCB",
        ref_offset=(-2.4, 0.0),
        hide_val=True,
    )
    j1 = ComponentSpec(
        ref="J1",
        value="JST_SM_9P_BIKE",
        lib_id="Connector_Generic:Conn_01x09",
        fp_name="echenon:Pigtail_1x09_P2.54mm_CNC",
        x=35.00,
        y=7.00,
        pads=[PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "oval", 1.75, 2.30, 1.00, j1_nets[i]) for i in range(9)],
        sch_x=114.30,
        sch_y=187.96,
        descr="9-pin JST-SM socket pigtail from lower bike frame",
        ref_offset=(-2.4, 0.0),
        hide_val=True,
    )

    sw0 = ComponentSpec(
        ref="SW0",
        value="VIN_USBC_INTERLOCK",
        lib_id="Switch:SW_SPST",
        fp_name="echenon:Interlock_USB_Blocker_2P_CNC",
        x=4.80,
        y=28.12,
        pads=[
            PadSpec("1", 0.0, 0.0, "circle", 2.30, 2.30, 1.00, "/NET_J1_8_V9_RAW"),
            PadSpec("2", 0.0, 8.00, "circle", 2.30, 2.30, 1.00, "/NET_SW0_D1"),
        ],
        sch_x=223.52,
        sch_y=165.10,
        descr="Mechanical +9V VIN interlock jumper straddling USB-C port",
        extra_fp_graphics=['(fp_rect (start -1.4 -1.6) (end 1.4 9.6) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))'],
        ref_offset=(0.0, -2.5),
        hide_val=True,
    )

    d1 = ComponentSpec(
        ref="D1",
        value="1N5819",
        lib_id="Device:D_Schottky",
        fp_name="echenon:D_DO41_Vertical_CNC",
        x=49.40,
        y=36.60,
        pads=[
            PadSpec("1", 0.0, 0.0, "rect", 2.10, 2.10, 0.90, "/NET_VIN_9V"),
            PadSpec("2", 0.0, 10.16, "circle", 2.10, 2.10, 0.90, "/NET_SW0_D1"),
        ],
        sch_x=248.92,
        sch_y=165.10,
        descr="1N5819 Schottky diode on +9V VIN feed (bridges A0..A3 bus)",
        extra_fp_graphics=[
            '(fp_rect (start -1.35 1.4) (end 1.35 8.76) (stroke (width 0.14) (type solid)) (fill no) (layer "F.SilkS"))',
            '(fp_line (start -1.35 2.4) (end 1.35 2.4) (stroke (width 0.30) (type solid)) (layer "F.SilkS"))',
        ],
        ref_offset=(2.3, 5.08),
        val_offset=(0.0, 5.08),
        ref_angle=90.0,
        hide_val=True,
    )

    def make_v_res(ref: str, val: str, x: float, y1: float, y2: float, net1: str, net2: str, ref_off: Optional[Tuple[float, float]] = None) -> ComponentSpec:
        mid_y = round((y2 - y1) / 2.0, 3)
        return ComponentSpec(
            ref=ref,
            value=val,
            lib_id="Device:R",
            fp_name="echenon:R_Axial_Vertical_CNC",
            x=x,
            y=y1,
            pads=[
                PadSpec("1", 0.0, 0.0, "circle", 2.10, 2.10, 0.90, net1),
                PadSpec("2", 0.0, round(y2 - y1, 4), "circle", 2.10, 2.10, 0.90, net2),
            ],
            sch_x=223.52,
            sch_y=40.0,
            descr=f"{val} 1% 1/4W metal film axial resistor (0.9mm drill)",
            extra_fp_graphics=[f'(fp_rect (start -1.20 1.4) (end 1.20 {y2 - y1 - 1.4:.2f}) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))'],
            ref_offset=ref_off if ref_off else (0.0, mid_y),
            val_offset=(0.0, mid_y),
            ref_angle=90.0,
            hide_val=True,
        )

    def make_v_cap(ref: str, val: str, x: float, y1: float, y2: float, net1: str, net2: str, ref_off: Optional[Tuple[float, float]] = None) -> ComponentSpec:
        mid_y = round((y2 - y1) / 2.0, 3)
        return ComponentSpec(
            ref=ref,
            value=val,
            lib_id="Device:C",
            fp_name="echenon:C_Disc_Vertical_CNC",
            x=x,
            y=y1,
            pads=[
                PadSpec("1", 0.0, 0.0, "circle", 2.10, 2.10, 0.90, net1),
                PadSpec("2", 0.0, round(y2 - y1, 4), "circle", 2.10, 2.10, 0.90, net2),
            ],
            sch_x=248.92,
            sch_y=40.0,
            descr=f"{val} X7R ceramic capacitor (0.9mm drill)",
            extra_fp_graphics=[f'(fp_rect (start -1.10 1.2) (end 1.10 {y2 - y1 - 1.2:.2f}) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))'],
            ref_offset=ref_off if ref_off else (0.0, mid_y),
            val_offset=(0.0, mid_y),
            ref_angle=90.0,
            hide_val=True,
        )

    rs8 = make_v_res("RS8", "4.7k", 10.40, 10.20, 17.82, "/NET_J3_2_ENC_VCC", "/GPIO10_ENC_VCC")
    c1_5 = make_v_cap("C1_5", "100nF", 13.10, 14.01, 17.82, "GND", "/GPIO10_ENC_VCC", ref_off=(0.0, -2.0))
    rs7 = make_v_res("RS7", "4.7k", 15.80, 10.20, 17.82, "/NET_J3_3_ENC_B", "/GPIO9_ENC_B")
    c1_4 = make_v_cap("C1_4", "100nF", 18.50, 14.01, 17.82, "GND", "/GPIO9_ENC_B", ref_off=(0.0, -2.0))
    rs6 = make_v_res("RS6", "4.7k", 21.50, 10.20, 17.82, "/NET_J3_1_ENC_A", "/GPIO8_ENC_A")
    c1_3 = make_v_cap("C1_3", "100nF", 24.20, 14.01, 17.82, "GND", "/GPIO8_ENC_A", ref_off=(0.0, -2.0))

    r2a = make_v_res("R2a", "30k", 28.60, 9.70, 17.32, "/NET_J1_2_MOT_N", "/GPIO7_MOT_N")
    r2b = make_v_res("R2b", "10k", 31.32, 13.65, 18.73, "GND", "/GPIO7_MOT_N", ref_off=(0.0, -2.0))
    r1a = make_v_res("R1a", "30k", 35.00, 9.70, 17.32, "/NET_J1_1_MOT_P", "/GPIO6_MOT_P")
    r1b = make_v_res("R1b", "10k", 37.70, 13.65, 18.73, "GND", "/GPIO6_MOT_P", ref_off=(0.0, -2.0))

    rs5 = make_v_res("RS5", "4.7k", 47.70, 11.60, 21.76, "/NET_J1_6_CADENCE", "/GPIO5_CADENCE")
    rs4 = make_v_res("RS4", "4.7k", 52.00, 11.60, 21.76, "/NET_J1_7_CAD_RTN", "/GPIO4_CAD_RTN")
    c1_2 = make_v_cap("C1_2", "100nF", 49.40, 23.20, 27.01, "/GPIO5_CADENCE", "GND", ref_off=(2.3, 1.90))
    c1_1 = make_v_cap("C1_1", "100nF", 49.40, 29.80, 33.61, "GND", "/GPIO4_CAD_RTN", ref_off=(-2.3, 1.90))

    rs3 = make_v_res("RS3", "22k", 54.80, 13.60, 23.76, "/NET_J1_5_POT_LO", "/GPIO3_POT_LO")
    rs2 = make_v_res("RS2", "22k", 58.20, 17.60, 27.76, "/NET_J1_3_POT_HI", "/GPIO2_POT_HI")
    rs1 = make_v_res("RS1", "22k", 61.90, 15.60, 25.76, "/NET_J1_4_POT_WIPER", "/GPIO1_POT_WIPER")

    components = [
        u1, j4, j3, j2, j1, sw0, d1,
        rs8, c1_5, rs7, c1_4, rs6, c1_3,
        r2a, r2b, r1a, r1b,
        rs5, c1_2, rs4, c1_1,
        rs3, rs2, rs1,
    ]

    # PCB B.Cu routing (unchanged verified geometry; /GND -> GND)
    segments: List[TrackSegment] = []
    for i in range(3):
        p = str(i + 1)
        add_chain(segments, [j4.pad_board_xy(p), j3.pad_board_xy(p)], 0.60, j3_nets[i])
    for i in range(9):
        p = str(i + 1)
        w = 0.90 if i in (0, 1, 7, 8) else 0.60
        add_chain(segments, [j2.pad_board_xy(p), j1.pad_board_xy(p)], w, j1_nets[i])

    add_chain(segments, [j4.pad_board_xy("1"), (11.00, 1.50), (21.50, 1.50), rs6.pad_board_xy("1")], 0.50, "/NET_J3_1_ENC_A")
    add_chain(segments, [j3.pad_board_xy("2"), (13.54, 9.10), (10.40, 9.10), rs8.pad_board_xy("1")], 0.50, "/NET_J3_2_ENC_VCC")
    add_chain(segments, [j3.pad_board_xy("3"), (16.08, 9.10), (15.80, 10.20)], 0.50, "/NET_J3_3_ENC_B")

    add_chain(segments, [rs8.pad_board_xy("2"), c1_5.pad_board_xy("2"), (13.10, 22.00), (23.70, 22.00), u1.pad_board_xy("10")], 0.50, "/GPIO10_ENC_VCC")
    add_chain(segments, [rs7.pad_board_xy("2"), c1_4.pad_board_xy("2"), (18.50, 20.90), (26.24, 20.90), u1.pad_board_xy("9")], 0.50, "/GPIO9_ENC_B")
    add_chain(segments, [rs6.pad_board_xy("2"), c1_3.pad_board_xy("2"), (24.20, 19.80), (28.78, 19.80), u1.pad_board_xy("8")], 0.50, "/GPIO8_ENC_A")

    add_chain(segments, [j1.pad_board_xy("1"), r1a.pad_board_xy("1")], 0.60, "/NET_J1_1_MOT_P")
    add_chain(segments, [j1.pad_board_xy("2"), (37.54, 11.75), (28.60, 11.75), r2a.pad_board_xy("1")], 0.60, "/NET_J1_2_MOT_N")
    add_chain(segments, [r2a.pad_board_xy("2"), (28.60, 18.73), r2b.pad_board_xy("2"), u1.pad_board_xy("7")], 0.50, "/GPIO7_MOT_N")
    add_chain(segments, [r1a.pad_board_xy("2"), (35.00, 17.32), (33.86, 17.32), u1.pad_board_xy("6")], 0.50, "/GPIO6_MOT_P")
    add_chain(segments, [r1b.pad_board_xy("2"), (35.00, 18.73), r1a.pad_board_xy("2")], 0.50, "/GPIO6_MOT_P")

    add_chain(segments, [j1.pad_board_xy("5"), (45.16, 13.60), rs3.pad_board_xy("1")], 0.50, "/NET_J1_5_POT_LO")
    add_chain(segments, [j1.pad_board_xy("4"), (42.62, 15.60), rs1.pad_board_xy("1")], 0.50, "/NET_J1_4_POT_WIPER")
    add_chain(segments, [j1.pad_board_xy("3"), (40.08, 17.60), rs2.pad_board_xy("1")], 0.50, "/NET_J1_3_POT_HI")

    add_chain(segments, [j1.pad_board_xy("6"), rs5.pad_board_xy("1")], 0.50, "/NET_J1_6_CADENCE")
    add_chain(segments, [rs5.pad_board_xy("2"), (47.70, 22.40), (49.40, 23.20)], 0.50, "/GPIO5_CADENCE")
    add_chain(segments, [(47.70, 22.40), (36.40, 22.40), u1.pad_board_xy("5")], 0.50, "/GPIO5_CADENCE")

    add_chain(segments, [j1.pad_board_xy("7"), (50.24, 11.60), rs4.pad_board_xy("1")], 0.50, "/NET_J1_7_CAD_RTN")
    add_chain(segments, [rs4.pad_board_xy("2"), (52.00, 33.61), c1_1.pad_board_xy("2")], 0.50, "/GPIO4_CAD_RTN")
    add_chain(segments, [(52.00, 33.61), (52.00, 41.70), (26.24, 41.70), u1.pad_board_xy("22")], 0.50, "/GPIO4_CAD_RTN")

    add_chain(segments, [rs3.pad_board_xy("2"), (54.80, 42.75), (23.70, 42.75), u1.pad_board_xy("21")], 0.50, "/GPIO3_POT_LO")
    add_chain(segments, [rs2.pad_board_xy("2"), (58.20, 43.80), (21.16, 43.80), u1.pad_board_xy("20")], 0.50, "/GPIO2_POT_HI")
    add_chain(segments, [rs1.pad_board_xy("2"), (61.90, 44.85), (18.62, 44.85), u1.pad_board_xy("19")], 0.50, "/GPIO1_POT_WIPER")

    add_chain(
        segments,
        [j1.pad_board_xy("8"), (52.78, 9.60), (64.40, 9.60), (64.40, 48.90), (1.50, 48.90), (1.50, 28.12), sw0.pad_board_xy("1")],
        0.70,
        "/NET_J1_8_V9_RAW",
    )
    add_chain(segments, [sw0.pad_board_xy("2"), (7.40, 38.72), (7.40, 46.76), d1.pad_board_xy("2")], 0.70, "/NET_SW0_D1")
    add_chain(segments, [d1.pad_board_xy("1"), (47.40, 36.60), (46.56, 37.44), u1.pad_board_xy("30")], 0.70, "/NET_VIN_9V")

    add_chain(segments, [j2.pad_board_xy("9"), (55.32, 1.50), (26.40, 1.50), (26.40, 13.65)], 0.70, "GND")
    add_chain(segments, [c1_5.pad_board_xy("1"), c1_4.pad_board_xy("1"), c1_3.pad_board_xy("1"), (26.40, 14.01), (26.40, 13.65), r2b.pad_board_xy("1"), r1b.pad_board_xy("1")], 0.70, "GND")
    add_chain(
        segments,
        [c1_5.pad_board_xy("1"), (6.20, 14.01), (6.20, 24.50), (8.20, 26.50), (8.20, 31.20), (38.94, 31.20), (44.02, 31.20), (47.40, 31.20), (47.40, 29.80), c1_1.pad_board_xy("1")],
        0.70,
        "GND",
    )
    add_chain(segments, [c1_1.pad_board_xy("1"), c1_2.pad_board_xy("2")], 0.70, "GND")
    add_chain(segments, [(38.94, 31.20), u1.pad_board_xy("4")], 0.70, "GND")
    add_chain(segments, [(44.02, 31.20), u1.pad_board_xy("29")], 0.70, "GND")

    edge_circles = [
        (3.00, 3.00, 1.35),
        (67.00, 3.00, 1.35),
        (4.20, 43.20, 1.35),
        (67.20, 46.80, 1.35),
        (19.00, 5.40, 1.10),
        (30.80, 5.40, 1.10),
        (59.00, 5.40, 1.10),
    ]
    silk_texts = [
        SilkText("ECHENON v1 PASSIVE TAP (EX-3)", 28.78, 28.20, "F.SilkS", 1.0, 0.15),
        SilkText("SW0 BLOCKS USB-C WHEN +9V ON", 28.78, 31.20, "F.SilkS", 0.80, 0.12),
        SilkText("RSVD: D8 TX1 RX0", 41.50, 27.20, "F.SilkS", 0.65, 0.11),
        SilkText("USB", 9.60, 32.12, "F.SilkS", 0.70, 0.11, angle=90.0),
        SilkText("1:BLK 2:ORG 3:RED", 13.54, 1.80, "F.SilkS", 0.65, 0.11),
        SilkText("1:RED 2:BLK 3:YEL 4:BLU 5:GRN 6:ORG 7:GRY 8:WHT 9:BRN", 45.16, 1.80, "F.SilkS", 0.65, 0.11),
        SilkText("ECHENON v1 B.Cu (CARVERA AIR)", 28.78, 30.00, "B.SilkS", 1.0, 0.15, mirror=True),
    ]

    # =========================================================================
    # Build real IEC wired schematic (A3: 420 x 297 mm) with off-board EX-3 ref
    # =========================================================================
    nano_sym_def, nano_pins = build_custom_nano_symbol()
    ps_sym_def = build_offboard_ps_symbol()
    con_sym_def, con_pins = build_offboard_console_symbol()

    lib_syms = [
        extract_kicad_symbol("Device.kicad_sym", "R", "Device:R"),
        extract_kicad_symbol("Device.kicad_sym", "C", "Device:C"),
        extract_kicad_symbol("Device.kicad_sym", "D_Schottky", "Device:D_Schottky"),
        extract_kicad_symbol("Device.kicad_sym", "R_Potentiometer", "Device:R_Potentiometer"),
        extract_kicad_symbol("Device.kicad_sym", "RotaryEncoder", "Device:RotaryEncoder"),
        extract_kicad_symbol("Switch.kicad_sym", "SW_SPST", "Switch:SW_SPST"),
        extract_kicad_symbol("Switch.kicad_sym", "SW_Reed", "Switch:SW_Reed"),
        extract_kicad_symbol("Motor.kicad_sym", "Motor_DC", "Motor:Motor_DC"),
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x09", "Connector_Generic:Conn_01x09"),
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x03", "Connector_Generic:Conn_01x03"),
        extract_kicad_symbol("power.kicad_sym", "GND", "power:GND"),
        nano_sym_def,
        ps_sym_def,
        con_sym_def,
    ]

    wires: List[Tuple[float, float, float, float]] = []
    junctions: List[Tuple[float, float]] = []
    labels: List[Tuple[str, float, float, int, str]] = []
    no_connects: List[Tuple[float, float]] = []
    sch_syms: List[SchSymbolInstance] = []
    pwr_counter = [0]

    def add_gnd(gx: float, gy: float) -> None:
        pwr_counter[0] += 1
        ref = f"#PWR{pwr_counter[0]:02d}"
        sch_syms.append(
            SchSymbolInstance(
                ref=ref,
                value="GND",
                lib_id="power:GND",
                sx=gx,
                sy=gy,
                rot=0,
                pins=["1"],
                descr="GND",
                on_board=False,
                in_bom=False,
                dnp=False,
                ref_at=(gx, gy + 6.35, 0),
                val_at=(gx, gy + 3.81, 0),
                hide_ref=True,
            )
        )

    # U1 center on A3 sheet (shifted left & tightened vertically to clear bottom-right Title Block)
    u1_sx, u1_sy = 299.72, 124.46
    sch_syms.append(
        SchSymbolInstance(
            ref="U1",
            value="Waveshare_ESP32-S3-Nano",
            lib_id="echenon:Waveshare_ESP32_S3_Nano",
            sx=u1_sx,
            sy=u1_sy,
            rot=0,
            pins=[str(i) for i in range(1, 31)],
            footprint=u1.fp_name,
            descr=u1.descr,
            on_board=True,
            in_bom=True,
            dnp=False,
            sym_uuid=u1.sym_uuid,
            ref_at=(u1_sx, u1_sy - 100.33, 0),
            val_at=(u1_sx, u1_sy + 110.49, 0),
        )
    )

    u1_sheet = {p: sym_pin_to_sheet(u1_sx, u1_sy, px, py) for p, (px, py) in nano_pins.items()}
    # Add no_connect markers on all 17 unconnected U1 pins
    for nc_pin in ["1", "2", "3", "11", "12", "13", "14", "15", "16", "17", "18", "23", "24", "25", "26", "27", "28"]:
        no_connects.append(u1_sheet[nc_pin])

    # U1 GND pins (Pin 4 & Pin 29) -> tie together to a clean GND symbol
    p4_x, p4_y = u1_sheet["4"]
    p29_x, p29_y = u1_sheet["29"]
    add_sch_wire_chain(wires, [(p4_x, p4_y), (p4_x - 10.16, p4_y), (p29_x - 10.16, p29_y), (p29_x, p29_y)])
    junctions.append((p29_x - 10.16, p29_y))
    add_sch_wire_chain(wires, [(p29_x - 10.16, p29_y), (p29_x - 10.16, p29_y + 6.35)])
    add_gnd(p29_x - 10.16, p29_y + 6.35)

    row_y = {
        1: u1_sheet["6"][1],    # 33.02  MOT_P
        2: u1_sheet["7"][1],    # 53.34  MOT_N
        3: u1_sheet["20"][1],   # 76.20  POT_HI
        4: u1_sheet["19"][1],   # 83.82  POT_WIPER
        5: u1_sheet["21"][1],   # 91.44  POT_LO
        6: u1_sheet["5"][1],    # 106.68 CADENCE
        7: u1_sheet["22"][1],   # 127.00 CAD_RTN
        8: u1_sheet["30"][1],   # 152.40 V9_RAW
        9: 160.02,              # 160.02 GND bus
        10: u1_sheet["8"][1],   # 185.42 ENC_A
        11: u1_sheet["10"][1],  # 205.74 ENC_VCC
        12: u1_sheet["9"][1],   # 226.06 ENC_B
    }

    bus_right_x = 208.28

    # --- Zone 1: Off-Board EX-3 Bike Reference Components (clean, on_board=False, in_bom=False, no red DNP cross) ---
    # 1. M1 (Motor_DC): Pin 1(+) at (sx, sy-5.08), Pin 2(-) at (sx, sy+7.62)
    m1_sx, m1_sy = 60.96, 43.18
    sch_syms.append(
        SchSymbolInstance(
            ref="M1",
            value="EX3_MOTOR (~3.3V, <=175mA)",
            lib_id="Motor:Motor_DC",
            sx=m1_sx,
            sy=m1_sy,
            rot=0,
            pins=["1", "2"],
            descr="Off-board EX-3 Resistance DC Motor",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(m1_sx - 6.35, m1_sy - 2.54, 0),
            val_at=(m1_sx - 6.35, m1_sy + 2.54, 0),
            ref_justify="right",
            val_justify="right",
        )
    )
    add_sch_wire_chain(wires, [(m1_sx, m1_sy - 5.08), (m1_sx, row_y[1]), (bus_right_x, row_y[1])])
    add_sch_wire_chain(wires, [(m1_sx, m1_sy + 7.62), (m1_sx, row_y[2]), (bus_right_x, row_y[2])])

    # 2. RV1 (R_Potentiometer IEC): Pin 1 at (sx, sy-3.81), Pin 2 at (sx+3.81, sy), Pin 3 at (sx, sy+3.81)
    rv1_sx, rv1_sy = 60.96, row_y[4]
    sch_syms.append(
        SchSymbolInstance(
            ref="RV1",
            value="CAM_POT (~5.4k, 0..1.67V)",
            lib_id="Device:R_Potentiometer",
            sx=rv1_sx,
            sy=rv1_sy,
            rot=0,
            pins=["1", "2", "3"],
            descr="Off-board EX-3 Resistance Cam Potentiometer",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(rv1_sx - 5.08, rv1_sy - 2.54, 0),
            val_at=(rv1_sx - 5.08, rv1_sy + 2.54, 0),
            ref_justify="right",
            val_justify="right",
        )
    )
    add_sch_wire_chain(wires, [(rv1_sx, rv1_sy - 3.81), (rv1_sx, row_y[3]), (bus_right_x, row_y[3])])
    add_sch_wire_chain(wires, [(rv1_sx + 3.81, rv1_sy), (bus_right_x, row_y[4])])
    add_sch_wire_chain(wires, [(rv1_sx, rv1_sy + 3.81), (rv1_sx, row_y[5]), (bus_right_x, row_y[5])])

    # 3. SW_CAD1 (SW_Reed vertical rot=90 -> rrot=90 flips justify right<->left)
    sw_cad_sx, sw_cad_sy = 60.96, 116.84
    sch_syms.append(
        SchSymbolInstance(
            ref="SW_CAD1",
            value="CRANK_REED_SW",
            lib_id="Switch:SW_Reed",
            sx=sw_cad_sx,
            sy=sw_cad_sy,
            rot=90,
            pins=["1", "2"],
            descr="Off-board EX-3 Crank Cadence Reed Switch",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(sw_cad_sx - 5.08, sw_cad_sy - 2.54, 90),
            val_at=(sw_cad_sx - 5.08, sw_cad_sy + 2.54, 90),
            ref_justify="left",
            val_justify="left",
        )
    )
    add_sch_wire_chain(wires, [(sw_cad_sx, sw_cad_sy - 5.08), (sw_cad_sx, row_y[6]), (bus_right_x, row_y[6])])
    add_sch_wire_chain(wires, [(sw_cad_sx, sw_cad_sy + 5.08), (sw_cad_sx, row_y[7]), (bus_right_x, row_y[7])])

    # 4. PS1 (EX3_Power_Supply_9V): Pin 1(+9V) at (sx+11.43, sy-2.54), Pin 2(GND) at (sx+11.43, sy+2.54)
    ps1_sx, ps1_sy = 48.26, row_y[8] + 2.54
    sch_syms.append(
        SchSymbolInstance(
            ref="PS1",
            value="9V_3A_DC_SUPPLY",
            lib_id="echenon:EX3_Power_Supply_9V",
            sx=ps1_sx,
            sy=ps1_sy,
            rot=0,
            pins=["1", "2"],
            descr="Off-board EX-3 9V 3A Wall Supply",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(ps1_sx, ps1_sy - 7.62, 0),
            val_at=(ps1_sx, ps1_sy + 8.89, 0),
        )
    )
    add_sch_wire_chain(wires, [(ps1_sx + 11.43, ps1_sy - 2.54), (198.12, row_y[8])])
    add_sch_wire_chain(wires, [(ps1_sx + 11.43, ps1_sy + 2.54), (66.04, ps1_sy + 2.54), (66.04, row_y[9]), (162.56, row_y[9])])
    add_gnd(162.56, row_y[9] + 5.08)
    add_sch_wire_chain(wires, [(162.56, row_y[9]), (162.56, row_y[9] + 5.08)])

    # 5. ENC1 (RotaryEncoder mirror=y): Pin A=(sx+7.62, sy-2.54), Pin C=(sx+7.62, sy), Pin B=(sx+7.62, sy+2.54)
    enc1_sx, enc1_sy = 53.34, row_y[11]
    sch_syms.append(
        SchSymbolInstance(
            ref="ENC1",
            value="KNOB_ENCODER",
            lib_id="Device:RotaryEncoder",
            sx=enc1_sx,
            sy=enc1_sy,
            rot=0,
            mirror="y",
            pins=["A", "B", "C"],
            descr="Off-board Handlebar Resistance Knob Quadrature Encoder",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(enc1_sx, enc1_sy - 10.16, 0),
            val_at=(enc1_sx, enc1_sy + 10.16, 0),
        )
    )
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy - 2.54), (68.58, enc1_sy - 2.54), (68.58, row_y[10]), (bus_right_x, row_y[10])])
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy), (bus_right_x, row_y[11])])
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy + 2.54), (68.58, enc1_sy + 2.54), (68.58, row_y[12]), (bus_right_x, row_y[12])])

    # --- Zone 2a: Inline Pass-Through Connectors J1/J2 (9-pin) & J3/J4 (3-pin) ---
    j12_sy = 170.18
    j1_sx, j2_sx = 109.22, 144.78
    sch_syms.append(
        SchSymbolInstance("J1", "JST_SM_9P_BIKE", "Connector_Generic:Conn_01x09", j1_sx, j12_sy, 270, None, [str(i) for i in range(1, 10)], j1.fp_name, j1.descr, True, True, False, j1.sym_uuid, (j1_sx, j12_sy + 4.318, 90), (j1_sx, j12_sy + 7.112, 90))
    )
    sch_syms.append(
        SchSymbolInstance("J2", "JST_SM_9P_CONSOLE", "Connector_Generic:Conn_01x09", j2_sx, j12_sy, 270, None, [str(i) for i in range(1, 10)], j2.fp_name, j2.descr, True, True, False, j2.sym_uuid, (j2_sx, j12_sy + 4.318, 90), (j2_sx, j12_sy + 7.112, 90))
    )
    for k in range(1, 10):
        j1_px, j1_py = sym_pin_to_sheet(j1_sx, j12_sy, -5.08, 10.16 - (k - 1) * 2.54, 270, None)
        j2_px, j2_py = sym_pin_to_sheet(j2_sx, j12_sy, -5.08, 10.16 - (k - 1) * 2.54, 270, None)
        ry = row_y[k]
        add_sch_wire_chain(wires, [(j1_px, ry), (j1_px, j1_py)])
        add_sch_wire_chain(wires, [(j2_px, ry), (j2_px, j2_py)])
        junctions.extend([(j1_px, ry), (j2_px, ry)])

    j34_sy = 248.92
    j3_sx, j4_sx = 109.22, 144.78
    sch_syms.append(
        SchSymbolInstance("J3", "JST_3P_KNOB", "Connector_Generic:Conn_01x03", j3_sx, j34_sy, 270, None, ["1", "2", "3"], j3.fp_name, j3.descr, True, True, False, j3.sym_uuid, (j3_sx, j34_sy + 4.318, 90), (j3_sx, j34_sy + 7.112, 90))
    )
    sch_syms.append(
        SchSymbolInstance("J4", "JST_3P_CONSOLE", "Connector_Generic:Conn_01x03", j4_sx, j34_sy, 270, None, ["1", "2", "3"], j4.fp_name, j4.descr, True, True, False, j4.sym_uuid, (j4_sx, j34_sy + 4.318, 90), (j4_sx, j34_sy + 7.112, 90))
    )
    for idx, k in enumerate([1, 2, 3]):
        j3_px, j3_py = sym_pin_to_sheet(j3_sx, j34_sy, -5.08, 2.54 - idx * 2.54, 270, None)
        j4_px, j4_py = sym_pin_to_sheet(j4_sx, j34_sy, -5.08, 2.54 - idx * 2.54, 270, None)
        ry = row_y[10 + idx]
        add_sch_wire_chain(wires, [(j3_px, ry), (j3_px, j3_py)])
        add_sch_wire_chain(wires, [(j4_px, ry), (j4_px, j4_py)])
        junctions.extend([(j3_px, ry), (j4_px, ry)])

    # Net labels along the 12 horizontal harness wires (matching wire colors & CON1)
    bus_labels = [
        ("NET_J1_1_MOT_P",     162.56, row_y[1]),
        ("NET_J1_2_MOT_N",     162.56, row_y[2]),
        ("NET_J1_3_POT_HI",    162.56, row_y[3]),
        ("NET_J1_4_POT_WIPER", 162.56, row_y[4]),
        ("NET_J1_5_POT_LO",    162.56, row_y[5]),
        ("NET_J1_6_CADENCE",   162.56, row_y[6]),
        ("NET_J1_7_CAD_RTN",   162.56, row_y[7]),
        ("NET_J1_8_V9_RAW",    162.56, row_y[8]),
        ("NET_J3_1_ENC_A",     162.56, row_y[10]),
        ("NET_J3_2_ENC_VCC",   162.56, row_y[11]),
        ("NET_J3_3_ENC_B",     162.56, row_y[12]),
    ]
    for lname, lx, ly in bus_labels:
        labels.append((lname, lx, ly, 0, "left bottom"))

    # --- Zone 2b: Real Passive Dividers, Series Taps, RC Filters, Interlock & Diode ---
    r_x = 220.98
    shunt_x = 243.84

    def place_series_r(comp: ComponentSpec, r_sx: float, ry: float, u1_pin: str, net_label: str) -> float:
        sch_syms.append(
            SchSymbolInstance(
                ref=comp.ref,
                value=comp.value,
                lib_id="Device:R",
                sx=r_sx,
                sy=ry,
                rot=90,
                pins=["1", "2"],
                footprint=comp.fp_name,
                descr=comp.descr,
                on_board=True,
                in_bom=True,
                dnp=False,
                sym_uuid=comp.sym_uuid,
                ref_at=(r_sx, ry - 3.302, 90),
                val_at=(r_sx, ry + 3.302, 90),
            )
        )
        add_sch_wire_chain(wires, [(bus_right_x, ry), (r_sx - 3.81, ry)])
        ux, uy = u1_sheet[u1_pin]
        add_sch_wire_chain(wires, [(r_sx + 3.81, ry), (ux, uy)])
        labels.append((net_label, ux - 25.40, ry, 0, "left bottom"))
        return r_sx + 3.81

    # Motor sense ÷4 dividers (R1a+R1b -> D3, R2a+R2b -> D4)
    place_series_r(r1a, r_x, row_y[1], "6", "GPIO6_MOT_P")
    sch_syms.append(SchSymbolInstance(r1b.ref, r1b.value, "Device:R", shunt_x, row_y[1] + 6.35, 180, None, ["1", "2"], r1b.fp_name, r1b.descr, True, True, False, r1b.sym_uuid, (shunt_x - 2.54, row_y[1] + 6.35, 0), (shunt_x + 2.54, row_y[1] + 6.35, 0), ref_justify="left", val_justify="right"))
    add_sch_wire_chain(wires, [(shunt_x, row_y[1]), (shunt_x, row_y[1] + 2.54)])
    junctions.append((shunt_x, row_y[1]))
    add_sch_wire_chain(wires, [(shunt_x, row_y[1] + 10.16), (shunt_x, row_y[1] + 11.43)])
    add_gnd(shunt_x, row_y[1] + 11.43)

    place_series_r(r2a, r_x, row_y[2], "7", "GPIO7_MOT_N")
    sch_syms.append(SchSymbolInstance(r2b.ref, r2b.value, "Device:R", shunt_x, row_y[2] + 6.35, 180, None, ["1", "2"], r2b.fp_name, r2b.descr, True, True, False, r2b.sym_uuid, (shunt_x - 2.54, row_y[2] + 6.35, 0), (shunt_x + 2.54, row_y[2] + 6.35, 0), ref_justify="left", val_justify="right"))
    add_sch_wire_chain(wires, [(shunt_x, row_y[2]), (shunt_x, row_y[2] + 2.54)])
    junctions.append((shunt_x, row_y[2]))
    add_sch_wire_chain(wires, [(shunt_x, row_y[2] + 10.16), (shunt_x, row_y[2] + 11.43)])
    add_gnd(shunt_x, row_y[2] + 11.43)

    # Potentiometer series taps (RS2->A1, stagger RS1->A0 right by 15.24mm, RS3->A2)
    place_series_r(rs2, r_x, row_y[3], "20", "GPIO2_POT_HI")
    place_series_r(rs1, r_x + 15.24, row_y[4], "19", "GPIO1_POT_WIPER")
    place_series_r(rs3, r_x, row_y[5], "21", "GPIO3_POT_LO")

    # Helper for RC shunt caps (remember C1_2 has Pad 1=GPIO / rot=0; others Pad 2=GPIO / rot=180)
    def place_rc_filter(res_comp: ComponentSpec, cap_comp: ComponentSpec, ry: float, u1_pin: str, net_label: str, cap_rot: int) -> None:
        place_series_r(res_comp, r_x, ry, u1_pin, net_label)
        cy = ry + 6.35
        rj_c, vj_c = ("left", "right") if cap_rot == 180 else ("right", "left")
        sch_syms.append(SchSymbolInstance(cap_comp.ref, cap_comp.value, "Device:C", shunt_x, cy, cap_rot, None, ["1", "2"], cap_comp.fp_name, cap_comp.descr, True, True, False, cap_comp.sym_uuid, (shunt_x - 3.048, cy, 0), (shunt_x + 3.048, cy, 0), ref_justify=rj_c, val_justify=vj_c))
        add_sch_wire_chain(wires, [(shunt_x, ry), (shunt_x, cy - 3.81)])
        junctions.append((shunt_x, ry))
        add_sch_wire_chain(wires, [(shunt_x, cy + 3.81), (shunt_x, cy + 5.08)])
        add_gnd(shunt_x, cy + 5.08)

    place_rc_filter(rs5, c1_2, row_y[6], "5", "GPIO5_CADENCE", 0)
    place_rc_filter(rs4, c1_1, row_y[7], "22", "GPIO4_CAD_RTN", 180)

    # Power Interlock SW0 + D1 (1N5819) -> VIN
    sw_sx, sw_sy = 203.20, row_y[8]
    d1_sx, d1_sy = 236.22, row_y[8]
    sch_syms.append(
        SchSymbolInstance(sw0.ref, sw0.value, "Switch:SW_SPST", sw_sx, sw_sy, 0, None, ["1", "2"], sw0.fp_name, sw0.descr, True, True, False, sw0.sym_uuid, (sw_sx, sw_sy - 3.556, 0), (sw_sx, sw_sy + 3.81, 0))
    )
    sch_syms.append(
        SchSymbolInstance(d1.ref, d1.value, "Device:D_Schottky", d1_sx, d1_sy, 180, None, ["1", "2"], d1.fp_name, d1.descr, True, True, False, d1.sym_uuid, (d1_sx, d1_sy - 3.556, 0), (d1_sx, d1_sy + 3.81, 0))
    )
    add_sch_wire_chain(wires, [(198.12, row_y[8]), (sw_sx - 5.08, row_y[8])])
    add_sch_wire_chain(wires, [(sw_sx + 5.08, row_y[8]), (d1_sx - 3.81, row_y[8])])
    labels.append(("NET_SW0_D1", sw_sx + 5.08, row_y[8], 0, "left bottom"))
    ux_vin, uy_vin = u1_sheet["30"]
    add_sch_wire_chain(wires, [(d1_sx + 3.81, row_y[8]), (ux_vin, uy_vin)])
    labels.append(("NET_VIN_9V", ux_vin - 25.40, row_y[8], 0, "left bottom"))

    # Encoder RC filters (RS6/C1_3 -> D5, RS8/C1_5 -> D7, RS7/C1_4 -> D6)
    place_rc_filter(rs6, c1_3, row_y[10], "8", "GPIO8_ENC_A", 180)
    place_rc_filter(rs8, c1_5, row_y[11], "10", "GPIO10_ENC_VCC", 180)
    place_rc_filter(rs7, c1_4, row_y[12], "9", "GPIO9_ENC_B", 180)

    # --- Zone 3: Off-Board Stock EX-3 Console PCB Reference (Top-Right, well above A3 Title Block) ---
    con_sx, con_sy = 391.16, 66.04
    sch_syms.append(
        SchSymbolInstance(
            ref="CON1",
            value="STOCK_EX3_CONSOLE",
            lib_id="echenon:EX3_Console_PCB",
            sx=con_sx,
            sy=con_sy,
            rot=0,
            pins=[str(i) for i in range(1, 13)],
            descr="Off-board Stock EX-3 Console PCB",
            on_board=False,
            in_bom=False,
            dnp=False,
            ref_at=(con_sx, con_sy - 25.40, 0),
            val_at=(con_sx, con_sy + 22.86, 0),
        )
    )
    con_net_map = {
        "1": "NET_J1_1_MOT_P",
        "2": "NET_J1_2_MOT_N",
        "3": "NET_J1_3_POT_HI",
        "4": "NET_J1_4_POT_WIPER",
        "5": "NET_J1_5_POT_LO",
        "6": "NET_J1_6_CADENCE",
        "7": "NET_J1_7_CAD_RTN",
        "8": "NET_J1_8_V9_RAW",
        "9": None,
        "10": "NET_J3_1_ENC_A",
        "11": "NET_J3_2_ENC_VCC",
        "12": "NET_J3_3_ENC_B",
    }
    for pnum, lname in con_net_map.items():
        px, py = sym_pin_to_sheet(con_sx, con_sy, *con_pins[pnum])
        if lname is not None:
            add_sch_wire_chain(wires, [(px, py), (px - 3.81, py)])
            labels.append((lname, px - 3.81, py, 180, "right bottom"))
        else:
            add_sch_wire_chain(wires, [(px, py), (px - 3.81, py), (px - 3.81, py + 5.08)])
            add_gnd(px - 3.81, py + 5.08)

    # Dashed & solid zone boundary boxes (stepped around bottom-right Title Block!) + engineering annotations
    rects = [
        (17.78, 20.32, 81.28, 266.70, "dash", "120 120 120 1"),   # Off-board Bike Frame & Knob
        (345.44, 20.32, 407.67, 101.60, "dash", "120 120 120 1"), # Off-board Stock Console PCB
    ]
    polylines = [
        (
            [
                (86.36, 20.32),
                (340.36, 20.32),
                (340.36, 238.76),
                (271.78, 238.76),
                (271.78, 266.70),
                (86.36, 266.70),
                (86.36, 20.32),
            ],
            "solid",
            "0 90 150 1",
        )
    ]
    texts = [
        ("OFF-BOARD REFERENCE: ECHELON EX-3 BIKE", 20.32, 22.86, 1.524, "90 90 90 1"),
        ("(EXCLUDED FROM PCB & BOM)", 20.32, 25.40, 1.143, "110 110 110 1"),
        ("ECHENON v1 PASSIVE TAP BOARD (70x50 mm SINGLE-SIDED B.Cu)", 88.90, 22.86, 1.651, "0 90 150 1"),
        ("MOTOR SENSE DIV4 DIVIDERS (0..9V -> 0..2.25V)", 190.50, 24.13, 1.143, "80 80 80 1"),
        ("POTENTIOMETER HIGH-Z TAPS (NO CAPS ON WIPER)", 172.72, 68.58, 1.143, "80 80 80 1"),
        ("CADENCE RC FILTERS (fc ~ 338 Hz)", 190.50, 99.06, 1.143, "80 80 80 1"),
        ("CRITICAL: +9V VIN OR USB-C (NEVER BOTH)", 177.80, 144.78, 1.143, "180 30 30 1"),
        ("INLINE PASS-THROUGH (J1<->J2)", 162.56, 173.99, 1.143, "0 90 150 1"),
        ("ENCODER RC FILTERS (fc ~ 338 Hz)", 195.58, 177.80, 1.143, "80 80 80 1"),
        ("ENCODER PASS-THROUGH (J3<->J4)", 162.56, 248.92, 1.143, "0 90 150 1"),
        ("OFF-BOARD: STOCK CONSOLE", 347.98, 22.86, 1.27, "90 90 90 1"),
        ("(J2 & J4 PASS-THROUGH)", 347.98, 25.40, 1.016, "110 110 110 1"),
    ]

    sch_spec = SchSpec(
        paper="A3",
        lib_symbols=lib_syms,
        symbols=sch_syms,
        wires=wires,
        junctions=junctions,
        labels=labels,
        no_connects=no_connects,
        rects=rects,
        polylines=polylines,
        texts=texts,
        sch_title="Echenon v1 Passive Tap (ESP32-S3-Nano)",
    )

    return proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts, sch_spec


def build_v1_passive_tap_jlcpcb_project():
    """Deliverable C: 70 x 50 mm Echenon v1 Passive Tap Board — 2-Layer JLCPCB SMD PCBA Edition."""
    proj_name = "echenon_v1_passive_tap_jlcpcb"
    title = "Echenon v1 Passive Tap Board (JLCPCB 2-Layer SMD PCBA)"
    w_mm, h_mm = 70.0, 50.0

    # U1 (Waveshare ESP32-S3-Nano in J5 2x15 socket — user-populated, excluded from SMT BOM/CPL)
    u1_top_nets: Dict[int, str] = {
        10: "/GPIO10_ENC_VCC",
        9: "/GPIO9_ENC_B",
        8: "/GPIO8_ENC_A",
        7: "/GPIO7_MOT_N",
        6: "/GPIO6_MOT_P",
        5: "/GPIO5_CADENCE",
        4: "GND",
    }
    u1_bot_nets: Dict[int, str] = {
        19: "/GPIO1_POT_WIPER",
        20: "/GPIO2_POT_HI",
        21: "/GPIO3_POT_LO",
        22: "/GPIO4_CAD_RTN",
        29: "GND",
        30: "/NET_VIN_9V",
    }

    u1_pads: List[PadSpec] = []
    for dy, row_nets, pnum_fn in (
        (0.0, u1_top_nets, lambda i: 15 - i),
        (15.24, u1_bot_nets, lambda i: 16 + i),
    ):
        for i in range(15):
            pnum = pnum_fn(i)
            dx = round(i * 2.54, 4)
            net = row_nets.get(pnum, "")
            unconn = "" if net else kicad_unconnected_net("U1", NANO_PIN_NAMES[str(pnum)], str(pnum))
            shape = "rect" if pnum == 1 else "oval"
            u1_pads.append(PadSpec(str(pnum), dx, dy, shape, 1.75, 2.20, 1.00, net, unconnected_net=unconn))

    u1 = ComponentSpec(
        ref="U1",
        value="Waveshare_ESP32-S3-Nano",
        lib_id="echenon:Waveshare_ESP32_S3_Nano",
        fp_name="echenon:Waveshare_ESP32_S3_Nano_Socket_THT",
        x=11.00,
        y=26.50,
        pads=u1_pads,
        sch_x=317.50,
        sch_y=142.24,
        descr="Waveshare ESP32-S3-Nano in J5 2x15 female header strips (user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.80 -1.80) (end 37.36 17.04) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))',
            '(fp_rect (start -3.00 4.12) (end -0.40 11.12) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))',
            '(fp_rect (start -1.80 -1.80) (end 37.36 17.04) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(17.78, 5.20),
        val_offset=(17.78, 10.20),
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )

    j3_nets = ["/NET_J3_1_ENC_A", "/NET_J3_2_ENC_VCC", "/NET_J3_3_ENC_B"]
    j4 = ComponentSpec(
        ref="J4",
        value="JST_3P_CONSOLE",
        lib_id="Connector_Generic:Conn_01x03",
        fp_name="echenon:Header_Pigtail_1x03_P2.54mm",
        x=11.00,
        y=3.80,
        pads=[
            PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "rect" if i == 0 else "oval", 1.75, 2.20, 1.00, j3_nets[i])
            for i in range(3)
        ],
        sch_x=152.40,
        sch_y=266.70,
        descr="3-pin encoder pass-through to stock console (2.54mm header or pigtail, user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.20 -1.35) (end 6.28 1.35) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(-2.3, 0.0),
        hide_val=True,
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )
    j3 = ComponentSpec(
        ref="J3",
        value="JST_3P_KNOB",
        lib_id="Connector_Generic:Conn_01x03",
        fp_name="echenon:Header_Pigtail_1x03_P2.54mm",
        x=11.00,
        y=7.00,
        pads=[
            PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "rect" if i == 0 else "oval", 1.75, 2.20, 1.00, j3_nets[i])
            for i in range(3)
        ],
        sch_x=114.30,
        sch_y=266.70,
        descr="3-pin encoder input from handlebar knob (2.54mm header or pigtail, user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.20 -1.35) (end 6.28 1.35) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(-2.3, 0.0),
        hide_val=True,
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )

    j1_nets = [
        "/NET_J1_1_MOT_P",
        "/NET_J1_2_MOT_N",
        "/NET_J1_3_POT_HI",
        "/NET_J1_4_POT_WIPER",
        "/NET_J1_5_POT_LO",
        "/NET_J1_6_CADENCE",
        "/NET_J1_7_CAD_RTN",
        "/NET_J1_8_V9_RAW",
        "GND",
    ]
    j2 = ComponentSpec(
        ref="J2",
        value="JST_SM_9P_CONSOLE",
        lib_id="Connector_Generic:Conn_01x09",
        fp_name="echenon:Header_Pigtail_1x09_P2.54mm",
        x=35.00,
        y=3.80,
        pads=[
            PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "rect" if i == 0 else "oval", 1.75, 2.20, 1.00, j1_nets[i])
            for i in range(9)
        ],
        sch_x=152.40,
        sch_y=187.96,
        descr="9-pin main loom pass-through to stock console (2.54mm header or pigtail, user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.20 -1.35) (end 21.52 1.35) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(-2.4, 0.0),
        hide_val=True,
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )
    j1 = ComponentSpec(
        ref="J1",
        value="JST_SM_9P_BIKE",
        lib_id="Connector_Generic:Conn_01x09",
        fp_name="echenon:Header_Pigtail_1x09_P2.54mm",
        x=35.00,
        y=7.00,
        pads=[
            PadSpec(str(i + 1), round(i * 2.54, 4), 0.0, "rect" if i == 0 else "oval", 1.75, 2.20, 1.00, j1_nets[i])
            for i in range(9)
        ],
        sch_x=114.30,
        sch_y=187.96,
        descr="9-pin main loom input from lower bike frame (2.54mm header or pigtail, user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.20 -1.35) (end 21.52 1.35) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(-2.4, 0.0),
        hide_val=True,
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )

    sw0 = ComponentSpec(
        ref="SW0",
        value="VIN_USBC_INTERLOCK",
        lib_id="Switch:SW_SPST",
        fp_name="echenon:Interlock_USB_Blocker_2P_THT",
        x=5.20,
        y=30.12,
        pads=[
            PadSpec("1", 0.0, 0.0, "rect", 2.10, 2.10, 1.00, "/NET_J1_8_V9_RAW"),
            PadSpec("2", 0.0, 8.00, "circle", 2.10, 2.10, 1.00, "/NET_SW0_D1"),
        ],
        sch_x=223.52,
        sch_y=165.10,
        descr="Mechanical +9V VIN interlock header/jumper straddling USB-C port (user-populated)",
        extra_fp_graphics=[
            '(fp_rect (start -1.35 -1.45) (end 1.35 9.45) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))',
            '(fp_rect (start -1.45 -1.55) (end 1.45 9.55) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
        ],
        ref_offset=(0.0, -2.5),
        hide_val=True,
        attr="through_hole exclude_from_pos_files exclude_from_bom",
    )

    # Populated JLCPCB Basic SMD Components (all at 0 deg IPC-7351B orientation)
    RES_LCSC = {
        "4.7k": ("C17673", "0805W8F4701T5E"),
        "10k":  ("C17414", "0805W8F1002T5E"),
        "22k":  ("C17560", "0805W8F2202T5E"),
        "30k":  ("C17621", "0805W8F3002T5E"),
    }

    def make_smd_r0805(ref: str, val: str, x: float, y: float, net1: str, net2: str) -> ComponentSpec:
        lcsc, mpn = RES_LCSC[val]
        return ComponentSpec(
            ref=ref,
            value=val,
            lib_id="Device:R",
            fp_name="Resistor_SMD:R_0805_2012Metric",
            x=x,
            y=y,
            pads=[
                PadSpec("1", -0.9125, 0.0, "roundrect", 1.025, 1.40, 0.0, net1, pad_type="smd", rratio=0.243902),
                PadSpec("2",  0.9125, 0.0, "roundrect", 1.025, 1.40, 0.0, net2, pad_type="smd", rratio=0.243902),
            ],
            sch_x=220.98,
            sch_y=40.0,
            descr=f"{val} 1% 1/8W 0805 thick-film resistor (JLCPCB Basic {lcsc})",
            extra_fp_graphics=[
                '(fp_line (start -0.227 -0.735) (end 0.227 -0.735) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
                '(fp_line (start -0.227 0.735) (end 0.227 0.735) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
                '(fp_rect (start -1.68 -0.95) (end 1.68 0.95) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
                '(fp_rect (start -1.00 -0.625) (end 1.00 0.625) (stroke (width 0.10) (type solid)) (fill no) (layer "F.Fab"))',
            ],
            ref_offset=(0.0, -1.60),
            val_offset=(0.0, 1.60),
            hide_val=True,
            attr="smd",
            lcsc=lcsc,
            mpn=mpn,
            model_3d="${KICAD10_3DMODEL_DIR}/Resistor_SMD.3dshapes/R_0805_2012Metric.step",
        )

    def make_smd_c0805(ref: str, val: str, x: float, y: float, net1: str, net2: str) -> ComponentSpec:
        return ComponentSpec(
            ref=ref,
            value=val,
            lib_id="Device:C",
            fp_name="Capacitor_SMD:C_0805_2012Metric",
            x=x,
            y=y,
            pads=[
                PadSpec("1", -0.95, 0.0, "roundrect", 1.00, 1.45, 0.0, net1, pad_type="smd", rratio=0.25),
                PadSpec("2",  0.95, 0.0, "roundrect", 1.00, 1.45, 0.0, net2, pad_type="smd", rratio=0.25),
            ],
            sch_x=243.84,
            sch_y=40.0,
            descr="100nF 50V X7R 0805 MLCC capacitor (JLCPCB Basic C49678)",
            extra_fp_graphics=[
                '(fp_line (start -0.261 -0.735) (end 0.261 -0.735) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
                '(fp_line (start -0.261 0.735) (end 0.261 0.735) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
                '(fp_rect (start -1.70 -0.98) (end 1.70 0.98) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
                '(fp_rect (start -1.00 -0.625) (end 1.00 0.625) (stroke (width 0.10) (type solid)) (fill no) (layer "F.Fab"))',
            ],
            ref_offset=(0.0, -1.62),
            val_offset=(0.0, 1.62),
            hide_val=True,
            attr="smd",
            lcsc="C49678",
            mpn="CL21B104KBCNNNC",
            model_3d="${KICAD10_3DMODEL_DIR}/Capacitor_SMD.3dshapes/C_0805_2012Metric.step",
        )

    # D1: MDD SS14 40V 1A Schottky Diode in SMA / DO-214AC (JLCPCB Basic C2480)
    # Pad 1 (Left, -2.0mm) = Cathode (K -> /NET_VIN_9V), Pad 2 (Right, +2.0mm) = Anode (A -> /NET_SW0_D1)
    d1 = ComponentSpec(
        ref="D1",
        value="SS14",
        lib_id="Device:D_Schottky",
        fp_name="Diode_SMD:D_SMA",
        x=54.50,
        y=37.20,
        pads=[
            PadSpec("1", -2.00, 0.0, "roundrect", 2.50, 1.80, 0.0, "/NET_VIN_9V", pad_type="smd", rratio=0.138889),
            PadSpec("2",  2.00, 0.0, "roundrect", 2.50, 1.80, 0.0, "/NET_SW0_D1", pad_type="smd", rratio=0.138889),
        ],
        sch_x=236.22,
        sch_y=152.40,
        descr="SS14 40V 1A Schottky diode SMA/DO-214AC (JLCPCB Basic C2480)",
        extra_fp_graphics=[
            '(fp_line (start -3.51 -1.65) (end -3.51 1.65) (stroke (width 0.25) (type solid)) (layer "F.SilkS"))',
            '(fp_line (start -3.51 -1.65) (end 2.00 -1.65) (stroke (width 0.14) (type solid)) (layer "F.SilkS"))',
            '(fp_line (start -3.51 1.65) (end 2.00 1.65) (stroke (width 0.14) (type solid)) (layer "F.SilkS"))',
            '(fp_rect (start -3.50 -1.75) (end 3.50 1.75) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
            '(fp_rect (start -2.30 -1.50) (end 2.30 1.50) (stroke (width 0.10) (type solid)) (fill no) (layer "F.Fab"))',
        ],
        ref_offset=(0.0, -2.40),
        val_offset=(0.0, 2.40),
        hide_val=True,
        attr="smd",
        lcsc="C2480",
        mpn="SS14",
        model_3d="${KICAD10_3DMODEL_DIR}/Diode_SMD.3dshapes/D_SMA.step",
    )

    # Top-side 6x2 0805 array above U1 (Cols at x = 11.50, 17.10, 22.70, 28.30, 33.90, 39.50)
    rs8  = make_smd_r0805("RS8",  "4.7k",  11.50, 13.00, "/NET_J3_2_ENC_VCC", "/GPIO10_ENC_VCC")
    c1_5 = make_smd_c0805("C1_5", "100nF", 11.50, 17.50, "GND", "/GPIO10_ENC_VCC")

    rs7  = make_smd_r0805("RS7",  "4.7k",  17.10, 13.00, "/NET_J3_3_ENC_B", "/GPIO9_ENC_B")
    c1_4 = make_smd_c0805("C1_4", "100nF", 17.10, 17.50, "GND", "/GPIO9_ENC_B")

    rs6  = make_smd_r0805("RS6",  "4.7k",  22.70, 13.00, "/NET_J3_1_ENC_A", "/GPIO8_ENC_A")
    c1_3 = make_smd_c0805("C1_3", "100nF", 22.70, 17.50, "GND", "/GPIO8_ENC_A")

    r2a  = make_smd_r0805("R2a",  "30k",   28.30, 13.00, "/NET_J1_2_MOT_N", "/GPIO7_MOT_N")
    r2b  = make_smd_r0805("R2b",  "10k",   28.30, 17.50, "GND", "/GPIO7_MOT_N")

    r1a  = make_smd_r0805("R1a",  "30k",   33.90, 13.00, "/NET_J1_1_MOT_P", "/GPIO6_MOT_P")
    r1b  = make_smd_r0805("R1b",  "10k",   33.90, 17.50, "GND", "/GPIO6_MOT_P")

    rs5  = make_smd_r0805("RS5",  "4.7k",  39.50, 13.00, "/NET_J1_6_CADENCE", "/GPIO5_CADENCE")
    c1_2 = make_smd_c0805("C1_2", "100nF", 39.50, 17.50, "GND", "/GPIO5_CADENCE")

    # Right-side 0805 column (x = 54.00) feeding U1 bottom analog row (A0..A3)
    rs1  = make_smd_r0805("RS1",  "22k",   54.00, 14.50, "/NET_J1_4_POT_WIPER", "/GPIO1_POT_WIPER")
    rs2  = make_smd_r0805("RS2",  "22k",   54.00, 19.00, "/NET_J1_3_POT_HI", "/GPIO2_POT_HI")
    rs3  = make_smd_r0805("RS3",  "22k",   54.00, 23.50, "/NET_J1_5_POT_LO", "/GPIO3_POT_LO")
    rs4  = make_smd_r0805("RS4",  "4.7k",  54.00, 28.00, "/NET_J1_7_CAD_RTN", "/GPIO4_CAD_RTN")
    c1_1 = make_smd_c0805("C1_1", "100nF", 54.00, 32.20, "GND", "/GPIO4_CAD_RTN")

    # Optical Fiducials (3-corner asymmetric L-pattern, >= 5mm from edges) & NPTH Mounting/Strain-Relief Holes
    def make_fiducial(ref: str, x: float, y: float) -> ComponentSpec:
        return ComponentSpec(
            ref=ref,
            value="Fiducial",
            lib_id="Mechanical:Fiducial",
            fp_name="Fiducial:Fiducial_1mm_Mask2mm",
            x=x,
            y=y,
            pads=[
                PadSpec(
                    "", 0.0, 0.0, "circle", 1.00, 1.00, 0.0,
                    pad_type="smd",
                    layers='"F.Cu" "F.Mask"',
                    solder_mask_margin=0.50,
                    clearance=0.60,
                    pad_prop="pad_prop_fiducial_glob",
                )
            ],
            sch_x=360.0,
            sch_y=120.0,
            descr="Optical SMT pick-and-place fiducial (1mm bare F.Cu, 2mm mask opening)",
            extra_fp_graphics=[
                '(fp_circle (center 0 0) (end 1.25 0) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
                '(fp_circle (center 0 0) (end 1.00 0) (stroke (width 0.10) (type solid)) (fill no) (layer "F.Fab"))',
            ],
            ref_offset=(0.0, -1.80),
            hide_ref=True,
            hide_val=True,
            attr="smd exclude_from_pos_files exclude_from_bom",
        )

    def make_npth_hole(ref: str, val: str, x: float, y: float, drill_mm: float, crtyd_r: float) -> ComponentSpec:
        return ComponentSpec(
            ref=ref,
            value=val,
            lib_id="Mechanical:MountingHole",
            fp_name=f"MountingHole:NPTH_{drill_mm:.1f}mm",
            x=x,
            y=y,
            pads=[PadSpec("", 0.0, 0.0, "circle", drill_mm, drill_mm, drill_mm, pad_type="np_thru_hole")],
            sch_x=360.0,
            sch_y=135.0,
            descr=f"Non-plated mechanical hole ({drill_mm:.2f}mm drill)",
            extra_fp_graphics=[
                f'(fp_circle (center 0 0) (end {drill_mm / 2.0 + 0.15:.2f} 0) (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))',
                f'(fp_circle (center 0 0) (end {crtyd_r:.2f} 0) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))',
            ],
            ref_offset=(0.0, -crtyd_r - 0.6),
            hide_ref=True,
            hide_val=True,
            attr="exclude_from_pos_files exclude_from_bom",
        )

    fid1 = make_fiducial("FID1", 5.00, 10.50)
    fid2 = make_fiducial("FID2", 65.00, 10.50)
    fid3 = make_fiducial("FID3", 65.00, 40.50)

    mh1 = make_npth_hole("MH1", "M2.5_2.7mm", 3.50, 3.50, 2.70, 1.65)
    mh2 = make_npth_hole("MH2", "M2.5_2.7mm", 66.50, 3.50, 2.70, 1.65)
    mh3 = make_npth_hole("MH3", "M2.5_2.7mm", 3.50, 46.50, 2.70, 1.65)
    mh4 = make_npth_hole("MH4", "M2.5_2.7mm", 66.50, 46.50, 2.70, 1.65)

    sr1 = make_npth_hole("SR1", "ZIP_2.2mm", 20.50, 5.40, 2.20, 1.35)
    sr2 = make_npth_hole("SR2", "ZIP_2.2mm", 30.50, 5.40, 2.20, 1.35)
    sr3 = make_npth_hole("SR3", "ZIP_2.2mm", 59.80, 5.40, 2.20, 1.35)

    components = [
        u1, j4, j3, j2, j1, sw0, d1,
        rs8, c1_5, rs7, c1_4, rs6, c1_3,
        r2a, r2b, r1a, r1b,
        rs5, c1_2,
        rs1, rs2, rs3, rs4, c1_1,
        fid1, fid2, fid3,
        mh1, mh2, mh3, mh4,
        sr1, sr2, sr3,
    ]

    segments: List[TrackSegment] = []
    vias: List[ViaSpec] = []

    # 1. Inline pass-through bars on F.Cu (J4<->J3 and J2<->J1)
    for i in range(3):
        p = str(i + 1)
        add_chain(segments, [j4.pad_board_xy(p), j3.pad_board_xy(p)], 0.50, j3_nets[i], "F.Cu")
    for i in range(9):
        p = str(i + 1)
        w = 0.85 if i in (0, 1, 7, 8) else 0.50
        add_chain(segments, [j2.pad_board_xy(p), j1.pad_board_xy(p)], w, j1_nets[i], "F.Cu")

    # 2. Top-bank inputs (J3 & J1 -> Cols 1..6)
    add_chain(segments, [j3.pad_board_xy("2"), (13.54, 10.30), (10.5875, 10.30), rs8.pad_board_xy("1")], 0.35, "/NET_J3_2_ENC_VCC", "F.Cu")
    add_chain(segments, [j3.pad_board_xy("3"), (16.08, 10.30), (16.1875, 10.30), rs7.pad_board_xy("1")], 0.35, "/NET_J3_3_ENC_B", "F.Cu")
    add_chain(segments, [j3.pad_board_xy("1"), (11.00, 9.20), (21.7875, 9.20)], 0.35, "/NET_J3_1_ENC_A", "B.Cu")
    vias.append(ViaSpec(21.7875, 9.20, "/NET_J3_1_ENC_A"))
    add_chain(segments, [(21.7875, 9.20), rs6.pad_board_xy("1")], 0.35, "/NET_J3_1_ENC_A", "F.Cu")

    add_chain(segments, [j1.pad_board_xy("1"), (35.00, 10.30), (32.9875, 10.30), r1a.pad_board_xy("1")], 0.45, "/NET_J1_1_MOT_P", "F.Cu")
    add_chain(segments, [j1.pad_board_xy("2"), (37.54, 9.20), (27.3875, 9.20)], 0.45, "/NET_J1_2_MOT_N", "B.Cu")
    vias.append(ViaSpec(27.3875, 9.20, "/NET_J1_2_MOT_N"))
    add_chain(segments, [(27.3875, 9.20), r2a.pad_board_xy("1")], 0.45, "/NET_J1_2_MOT_N", "F.Cu")

    add_chain(segments, [j1.pad_board_xy("6"), (47.70, 9.20), (38.5875, 9.20)], 0.35, "/NET_J1_6_CADENCE", "B.Cu")
    vias.append(ViaSpec(38.5875, 9.20, "/NET_J1_6_CADENCE"))
    add_chain(segments, [(38.5875, 9.20), rs5.pad_board_xy("1")], 0.35, "/NET_J1_6_CADENCE", "F.Cu")

    # 3. Top-bank outputs (Cols 1..6 -> U1 top row D7..D2) + short GND vias
    add_chain(segments, [rs8.pad_board_xy("2"), c1_5.pad_board_xy("2"), (12.45, 22.80), (23.70, 22.80), u1.pad_board_xy("10")], 0.35, "/GPIO10_ENC_VCC", "F.Cu")
    add_chain(segments, [rs7.pad_board_xy("2"), c1_4.pad_board_xy("2"), (18.05, 21.80), (26.24, 21.80), u1.pad_board_xy("9")], 0.35, "/GPIO9_ENC_B", "F.Cu")
    add_chain(segments, [rs6.pad_board_xy("2"), c1_3.pad_board_xy("2"), (23.65, 20.80), (28.78, 20.80), u1.pad_board_xy("8")], 0.35, "/GPIO8_ENC_A", "F.Cu")
    add_chain(segments, [r2a.pad_board_xy("2"), r2b.pad_board_xy("2"), (29.2125, 20.00), (31.32, 20.00), u1.pad_board_xy("7")], 0.35, "/GPIO7_MOT_N", "F.Cu")
    add_chain(segments, [r1a.pad_board_xy("2"), r1b.pad_board_xy("2"), (34.8125, 20.00), (33.86, 20.00), u1.pad_board_xy("6")], 0.35, "/GPIO6_MOT_P", "F.Cu")
    add_chain(segments, [rs5.pad_board_xy("2"), c1_2.pad_board_xy("2"), (40.45, 20.80), (36.40, 20.80), u1.pad_board_xy("5")], 0.35, "/GPIO5_CADENCE", "F.Cu")

    for shunt_comp, vx in (
        (c1_5, 9.15),
        (c1_4, 14.75),
        (c1_3, 20.35),
        (r2b,  25.95),
        (r1b,  31.55),
        (c1_2, 37.15),
    ):
        add_chain(segments, [shunt_comp.pad_board_xy("1"), (vx, 17.50)], 0.50, "GND", "F.Cu")
        vias.append(ViaSpec(vx, 17.50, "GND"))

    # 4. Right-column inputs & outputs (RS1..RS4 + C1_1 -> U1 bottom row A0..A3)
    add_chain(segments, [j1.pad_board_xy("4"), (42.62, 14.50), rs1.pad_board_xy("1")], 0.35, "/NET_J1_4_POT_WIPER", "F.Cu")
    add_chain(segments, [j1.pad_board_xy("3"), (41.60, 8.52), (41.60, 19.00), rs2.pad_board_xy("1")], 0.35, "/NET_J1_3_POT_HI", "F.Cu")

    add_chain(segments, [j2.pad_board_xy("5"), (45.16, 2.00), (57.40, 2.00), (57.40, 23.50), (51.40, 23.50)], 0.35, "/NET_J1_5_POT_LO", "B.Cu")
    vias.append(ViaSpec(51.40, 23.50, "/NET_J1_5_POT_LO"))
    add_chain(segments, [(51.40, 23.50), rs3.pad_board_xy("1")], 0.35, "/NET_J1_5_POT_LO", "F.Cu")

    add_chain(segments, [j1.pad_board_xy("7"), (50.24, 28.00)], 0.35, "/NET_J1_7_CAD_RTN", "B.Cu")
    vias.append(ViaSpec(50.24, 28.00, "/NET_J1_7_CAD_RTN"))
    add_chain(segments, [(50.24, 28.00), rs4.pad_board_xy("1")], 0.35, "/NET_J1_7_CAD_RTN", "F.Cu")

    add_chain(segments, [c1_1.pad_board_xy("1"), (50.80, 32.20)], 0.50, "GND", "F.Cu")
    vias.append(ViaSpec(50.80, 32.20, "GND"))

    add_chain(segments, [rs1.pad_board_xy("2"), (62.50, 14.50), (62.50, 47.20), (18.62, 47.20), u1.pad_board_xy("19")], 0.35, "/GPIO1_POT_WIPER", "F.Cu")
    add_chain(segments, [rs2.pad_board_xy("2"), (61.60, 19.00), (61.60, 46.30), (21.16, 46.30), u1.pad_board_xy("20")], 0.35, "/GPIO2_POT_HI", "F.Cu")
    add_chain(segments, [rs3.pad_board_xy("2"), (60.70, 23.50), (60.70, 45.40), (23.70, 45.40), u1.pad_board_xy("21")], 0.35, "/GPIO3_POT_LO", "F.Cu")
    add_chain(segments, [rs4.pad_board_xy("2"), c1_1.pad_board_xy("2"), (59.80, 32.20), (59.80, 44.50), (26.24, 44.50), u1.pad_board_xy("22")], 0.35, "/GPIO4_CAD_RTN", "F.Cu")

    # 5. +9V Power Interlock Path (J2-8 -> SW0 -> D1 SS14 -> U1-30 VIN)
    add_chain(segments, [j2.pad_board_xy("8"), (52.78, 1.40), (7.20, 1.40), (7.20, 30.12), sw0.pad_board_xy("1")], 0.75, "/NET_J1_8_V9_RAW", "F.Cu")
    add_chain(segments, [sw0.pad_board_xy("2"), (7.20, 38.12), (7.20, 48.40), (56.50, 48.40)], 0.75, "/NET_SW0_D1", "F.Cu")
    vias.append(ViaSpec(56.50, 48.40, "/NET_SW0_D1", size=0.90, drill=0.45))
    add_chain(segments, [(56.50, 48.40), (56.50, 42.80)], 0.75, "/NET_SW0_D1", "B.Cu")
    vias.append(ViaSpec(56.50, 42.80, "/NET_SW0_D1", size=0.90, drill=0.45))
    add_chain(segments, [(56.50, 42.80), d1.pad_board_xy("2")], 0.75, "/NET_SW0_D1", "F.Cu")
    add_chain(segments, [d1.pad_board_xy("1"), (49.50, 37.20), (46.56, 40.14), u1.pad_board_xy("30")], 0.75, "/NET_VIN_9V", "F.Cu")

    # Solid B.Cu Ground Plane across entire board (with 0.50mm inset from Edge.Cuts)
    zones = [
        ZoneSpec(net="GND", layer="B.Cu", x1=0.50, y1=0.50, x2=69.50, y2=49.50, clearance=0.30, min_thickness=0.25)
    ]

    silk_texts = [
        SilkText("ECHENON v1 PASSIVE TAP (JLCPCB SMD)", 28.78, 30.20, "F.SilkS", 0.95, 0.14),
        SilkText("SW0 BLOCKS USB-C WHEN +9V ON", 28.78, 33.20, "F.SilkS", 0.78, 0.12),
        SilkText("RSVD: D8 TX1 RX0", 41.50, 29.00, "F.SilkS", 0.65, 0.11),
        SilkText("USB", 8.60, 34.12, "F.SilkS", 0.62, 0.10, angle=90.0),
        SilkText("K", 49.90, 37.20, "F.SilkS", 0.80, 0.13),
        SilkText("1:BLK 2:ORG 3:RED", 13.54, 1.95, "F.SilkS", 0.62, 0.11),
        SilkText("1:RED 2:BLK 3:YEL 4:BLU 5:GRN 6:ORG 7:GRY 8:WHT 9:BRN", 45.16, 1.95, "F.SilkS", 0.58, 0.10),
        SilkText("ECHENON v1.0 JLCPCB 2L GND PLANE", 30.00, 34.00, "B.SilkS", 0.95, 0.14, mirror=True),
    ]

    # =========================================================================
    # Build real IEC wired schematic (A3) with LCSC Basic Part #s & parity
    # =========================================================================
    nano_sym_def, nano_pins = build_custom_nano_symbol()
    ps_sym_def = build_offboard_ps_symbol()
    con_sym_def, con_pins = build_offboard_console_symbol()

    lib_syms = [
        extract_kicad_symbol("Device.kicad_sym", "R", "Device:R"),
        extract_kicad_symbol("Device.kicad_sym", "C", "Device:C"),
        extract_kicad_symbol("Device.kicad_sym", "D_Schottky", "Device:D_Schottky"),
        extract_kicad_symbol("Device.kicad_sym", "R_Potentiometer", "Device:R_Potentiometer"),
        extract_kicad_symbol("Device.kicad_sym", "RotaryEncoder", "Device:RotaryEncoder"),
        extract_kicad_symbol("Switch.kicad_sym", "SW_SPST", "Switch:SW_SPST"),
        extract_kicad_symbol("Switch.kicad_sym", "SW_Reed", "Switch:SW_Reed"),
        extract_kicad_symbol("Motor.kicad_sym", "Motor_DC", "Motor:Motor_DC"),
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x09", "Connector_Generic:Conn_01x09"),
        extract_kicad_symbol("Connector_Generic.kicad_sym", "Conn_01x03", "Connector_Generic:Conn_01x03"),
        extract_kicad_symbol("Mechanical.kicad_sym", "Fiducial", "Mechanical:Fiducial"),
        extract_kicad_symbol("Mechanical.kicad_sym", "MountingHole", "Mechanical:MountingHole"),
        extract_kicad_symbol("power.kicad_sym", "GND", "power:GND"),
        nano_sym_def,
        ps_sym_def,
        con_sym_def,
    ]

    wires: List[Tuple[float, float, float, float]] = []
    junctions: List[Tuple[float, float]] = []
    labels: List[Tuple[str, float, float, int, str]] = []
    no_connects: List[Tuple[float, float]] = []
    sch_syms: List[SchSymbolInstance] = []
    pwr_counter = [0]

    def add_gnd(gx: float, gy: float) -> None:
        pwr_counter[0] += 1
        ref = f"#PWR{pwr_counter[0]:02d}"
        sch_syms.append(
            SchSymbolInstance(
                ref=ref,
                value="GND",
                lib_id="power:GND",
                sx=gx,
                sy=gy,
                rot=0,
                pins=["1"],
                descr="GND",
                on_board=False,
                in_bom=False,
                in_pos_files=False,
                dnp=False,
                ref_at=(gx, gy + 6.35, 0),
                val_at=(gx, gy + 3.81, 0),
                hide_ref=True,
            )
        )

    u1_sx, u1_sy = 299.72, 124.46
    sch_syms.append(
        SchSymbolInstance(
            ref="U1",
            value="Waveshare_ESP32-S3-Nano",
            lib_id="echenon:Waveshare_ESP32_S3_Nano",
            sx=u1_sx,
            sy=u1_sy,
            rot=0,
            pins=[str(i) for i in range(1, 31)],
            footprint=u1.fp_name,
            descr=u1.descr,
            on_board=True,
            in_bom=False,
            in_pos_files=False,
            dnp=False,
            sym_uuid=u1.sym_uuid,
            ref_at=(u1_sx, u1_sy - 100.33, 0),
            val_at=(u1_sx, u1_sy + 110.49, 0),
        )
    )

    u1_sheet = {p: sym_pin_to_sheet(u1_sx, u1_sy, px, py) for p, (px, py) in nano_pins.items()}
    for nc_pin in ["1", "2", "3", "11", "12", "13", "14", "15", "16", "17", "18", "23", "24", "25", "26", "27", "28"]:
        no_connects.append(u1_sheet[nc_pin])

    p4_x, p4_y = u1_sheet["4"]
    p29_x, p29_y = u1_sheet["29"]
    add_sch_wire_chain(wires, [(p4_x, p4_y), (p4_x - 10.16, p4_y), (p29_x - 10.16, p29_y), (p29_x, p29_y)])
    junctions.append((p29_x - 10.16, p29_y))
    add_sch_wire_chain(wires, [(p29_x - 10.16, p29_y), (p29_x - 10.16, p29_y + 6.35)])
    add_gnd(p29_x - 10.16, p29_y + 6.35)

    row_y = {
        1: u1_sheet["6"][1],
        2: u1_sheet["7"][1],
        3: u1_sheet["20"][1],
        4: u1_sheet["19"][1],
        5: u1_sheet["21"][1],
        6: u1_sheet["5"][1],
        7: u1_sheet["22"][1],
        8: u1_sheet["30"][1],
        9: 160.02,
        10: u1_sheet["8"][1],
        11: u1_sheet["10"][1],
        12: u1_sheet["9"][1],
    }
    bus_right_x = 208.28

    # Off-board EX-3 Bike Reference
    m1_sx, m1_sy = 60.96, 43.18
    sch_syms.append(SchSymbolInstance("M1", "EX3_MOTOR (~3.3V, <=175mA)", "Motor:Motor_DC", m1_sx, m1_sy, 0, None, ["1", "2"], "", "Off-board EX-3 Resistance DC Motor", False, False, False, False, None, (m1_sx - 6.35, m1_sy - 2.54, 0), (m1_sx - 6.35, m1_sy + 2.54, 0), ref_justify="right", val_justify="right"))
    add_sch_wire_chain(wires, [(m1_sx, m1_sy - 5.08), (m1_sx, row_y[1]), (bus_right_x, row_y[1])])
    add_sch_wire_chain(wires, [(m1_sx, m1_sy + 7.62), (m1_sx, row_y[2]), (bus_right_x, row_y[2])])

    rv1_sx, rv1_sy = 60.96, row_y[4]
    sch_syms.append(SchSymbolInstance("RV1", "CAM_POT (~5.4k, 0..1.67V)", "Device:R_Potentiometer", rv1_sx, rv1_sy, 0, None, ["1", "2", "3"], "", "Off-board EX-3 Resistance Cam Potentiometer", False, False, False, False, None, (rv1_sx - 5.08, rv1_sy - 2.54, 0), (rv1_sx - 5.08, rv1_sy + 2.54, 0), ref_justify="right", val_justify="right"))
    add_sch_wire_chain(wires, [(rv1_sx, rv1_sy - 3.81), (rv1_sx, row_y[3]), (bus_right_x, row_y[3])])
    add_sch_wire_chain(wires, [(rv1_sx + 3.81, rv1_sy), (bus_right_x, row_y[4])])
    add_sch_wire_chain(wires, [(rv1_sx, rv1_sy + 3.81), (rv1_sx, row_y[5]), (bus_right_x, row_y[5])])

    sw_cad_sx, sw_cad_sy = 60.96, 116.84
    sch_syms.append(SchSymbolInstance("SW_CAD1", "CRANK_REED_SW", "Switch:SW_Reed", sw_cad_sx, sw_cad_sy, 90, None, ["1", "2"], "", "Off-board EX-3 Crank Cadence Reed Switch", False, False, False, False, None, (sw_cad_sx - 5.08, sw_cad_sy - 2.54, 90), (sw_cad_sx - 5.08, sw_cad_sy + 2.54, 90), ref_justify="left", val_justify="left"))
    add_sch_wire_chain(wires, [(sw_cad_sx, sw_cad_sy - 5.08), (sw_cad_sx, row_y[6]), (bus_right_x, row_y[6])])
    add_sch_wire_chain(wires, [(sw_cad_sx, sw_cad_sy + 5.08), (sw_cad_sx, row_y[7]), (bus_right_x, row_y[7])])

    ps1_sx, ps1_sy = 48.26, row_y[8] + 2.54
    sch_syms.append(SchSymbolInstance("PS1", "9V_3A_DC_SUPPLY", "echenon:EX3_Power_Supply_9V", ps1_sx, ps1_sy, 0, None, ["1", "2"], "", "Off-board EX-3 9V 3A Wall Supply", False, False, False, False, None, (ps1_sx, ps1_sy - 7.62, 0), (ps1_sx, ps1_sy + 8.89, 0)))
    add_sch_wire_chain(wires, [(ps1_sx + 11.43, ps1_sy - 2.54), (198.12, row_y[8])])
    add_sch_wire_chain(wires, [(ps1_sx + 11.43, ps1_sy + 2.54), (66.04, ps1_sy + 2.54), (66.04, row_y[9]), (162.56, row_y[9])])
    add_gnd(162.56, row_y[9] + 5.08)
    add_sch_wire_chain(wires, [(162.56, row_y[9]), (162.56, row_y[9] + 5.08)])

    enc1_sx, enc1_sy = 53.34, row_y[11]
    sch_syms.append(SchSymbolInstance("ENC1", "KNOB_ENCODER", "Device:RotaryEncoder", enc1_sx, enc1_sy, 0, "y", ["A", "B", "C"], "", "Off-board Handlebar Resistance Knob Quadrature Encoder", False, False, False, False, None, (enc1_sx, enc1_sy - 10.16, 0), (enc1_sx, enc1_sy + 10.16, 0)))
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy - 2.54), (68.58, enc1_sy - 2.54), (68.58, row_y[10]), (bus_right_x, row_y[10])])
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy), (bus_right_x, row_y[11])])
    add_sch_wire_chain(wires, [(enc1_sx + 7.62, enc1_sy + 2.54), (68.58, enc1_sy + 2.54), (68.58, row_y[12]), (bus_right_x, row_y[12])])

    # Inline Pass-Through Connectors J1/J2 & J3/J4 (user-populated -> in_bom=False, in_pos_files=False)
    j12_sy = 170.18
    j1_sx, j2_sx = 109.22, 144.78
    sch_syms.append(SchSymbolInstance("J1", "JST_SM_9P_BIKE", "Connector_Generic:Conn_01x09", j1_sx, j12_sy, 270, None, [str(i) for i in range(1, 10)], j1.fp_name, j1.descr, True, False, False, False, j1.sym_uuid, (j1_sx, j12_sy + 4.318, 90), (j1_sx, j12_sy + 7.112, 90)))
    sch_syms.append(SchSymbolInstance("J2", "JST_SM_9P_CONSOLE", "Connector_Generic:Conn_01x09", j2_sx, j12_sy, 270, None, [str(i) for i in range(1, 10)], j2.fp_name, j2.descr, True, False, False, False, j2.sym_uuid, (j2_sx, j12_sy + 4.318, 90), (j2_sx, j12_sy + 7.112, 90)))
    for k in range(1, 10):
        j1_px, j1_py = sym_pin_to_sheet(j1_sx, j12_sy, -5.08, 10.16 - (k - 1) * 2.54, 270, None)
        j2_px, j2_py = sym_pin_to_sheet(j2_sx, j12_sy, -5.08, 10.16 - (k - 1) * 2.54, 270, None)
        ry = row_y[k]
        add_sch_wire_chain(wires, [(j1_px, ry), (j1_px, j1_py)])
        add_sch_wire_chain(wires, [(j2_px, ry), (j2_px, j2_py)])
        junctions.extend([(j1_px, ry), (j2_px, ry)])

    j34_sy = 248.92
    j3_sx, j4_sx = 109.22, 144.78
    sch_syms.append(SchSymbolInstance("J3", "JST_3P_KNOB", "Connector_Generic:Conn_01x03", j3_sx, j34_sy, 270, None, ["1", "2", "3"], j3.fp_name, j3.descr, True, False, False, False, j3.sym_uuid, (j3_sx, j34_sy + 4.318, 90), (j3_sx, j34_sy + 7.112, 90)))
    sch_syms.append(SchSymbolInstance("J4", "JST_3P_CONSOLE", "Connector_Generic:Conn_01x03", j4_sx, j34_sy, 270, None, ["1", "2", "3"], j4.fp_name, j4.descr, True, False, False, False, j4.sym_uuid, (j4_sx, j34_sy + 4.318, 90), (j4_sx, j34_sy + 7.112, 90)))
    for idx, k in enumerate([1, 2, 3]):
        j3_px, j3_py = sym_pin_to_sheet(j3_sx, j34_sy, -5.08, 2.54 - idx * 2.54, 270, None)
        j4_px, j4_py = sym_pin_to_sheet(j4_sx, j34_sy, -5.08, 2.54 - idx * 2.54, 270, None)
        ry = row_y[10 + idx]
        add_sch_wire_chain(wires, [(j3_px, ry), (j3_px, j3_py)])
        add_sch_wire_chain(wires, [(j4_px, ry), (j4_px, j4_py)])
        junctions.extend([(j3_px, ry), (j4_px, ry)])

    for lname, lx, ly in [
        ("NET_J1_1_MOT_P",     162.56, row_y[1]),
        ("NET_J1_2_MOT_N",     162.56, row_y[2]),
        ("NET_J1_3_POT_HI",    162.56, row_y[3]),
        ("NET_J1_4_POT_WIPER", 162.56, row_y[4]),
        ("NET_J1_5_POT_LO",    162.56, row_y[5]),
        ("NET_J1_6_CADENCE",   162.56, row_y[6]),
        ("NET_J1_7_CAD_RTN",   162.56, row_y[7]),
        ("NET_J1_8_V9_RAW",    162.56, row_y[8]),
        ("NET_J3_1_ENC_A",     162.56, row_y[10]),
        ("NET_J3_2_ENC_VCC",   162.56, row_y[11]),
        ("NET_J3_3_ENC_B",     162.56, row_y[12]),
    ]:
        labels.append((lname, lx, ly, 0, "left bottom"))

    r_x = 220.98
    shunt_x = 243.84

    def place_series_r(comp: ComponentSpec, r_sx: float, ry: float, u1_pin: str, net_label: str) -> None:
        sch_syms.append(
            SchSymbolInstance(
                ref=comp.ref,
                value=comp.value,
                lib_id="Device:R",
                sx=r_sx,
                sy=ry,
                rot=90,
                pins=["1", "2"],
                footprint=comp.fp_name,
                descr=comp.descr,
                on_board=True,
                in_bom=True,
                in_pos_files=True,
                dnp=False,
                sym_uuid=comp.sym_uuid,
                ref_at=(r_sx, ry - 3.302, 90),
                val_at=(r_sx, ry + 3.302, 90),
                lcsc=comp.lcsc,
                mpn=comp.mpn,
            )
        )
        add_sch_wire_chain(wires, [(bus_right_x, ry), (r_sx - 3.81, ry)])
        ux, uy = u1_sheet[u1_pin]
        add_sch_wire_chain(wires, [(r_sx + 3.81, ry), (ux, uy)])
        labels.append((net_label, ux - 25.40, ry, 0, "left bottom"))

    place_series_r(r1a, r_x, row_y[1], "6", "GPIO6_MOT_P")
    sch_syms.append(SchSymbolInstance(r1b.ref, r1b.value, "Device:R", shunt_x, row_y[1] + 6.35, 180, None, ["1", "2"], r1b.fp_name, r1b.descr, True, True, True, False, r1b.sym_uuid, (shunt_x - 2.54, row_y[1] + 6.35, 0), (shunt_x + 2.54, row_y[1] + 6.35, 0), ref_justify="left", val_justify="right", lcsc=r1b.lcsc, mpn=r1b.mpn))
    add_sch_wire_chain(wires, [(shunt_x, row_y[1]), (shunt_x, row_y[1] + 2.54)])
    junctions.append((shunt_x, row_y[1]))
    add_sch_wire_chain(wires, [(shunt_x, row_y[1] + 10.16), (shunt_x, row_y[1] + 11.43)])
    add_gnd(shunt_x, row_y[1] + 11.43)

    place_series_r(r2a, r_x, row_y[2], "7", "GPIO7_MOT_N")
    sch_syms.append(SchSymbolInstance(r2b.ref, r2b.value, "Device:R", shunt_x, row_y[2] + 6.35, 180, None, ["1", "2"], r2b.fp_name, r2b.descr, True, True, True, False, r2b.sym_uuid, (shunt_x - 2.54, row_y[2] + 6.35, 0), (shunt_x + 2.54, row_y[2] + 6.35, 0), ref_justify="left", val_justify="right", lcsc=r2b.lcsc, mpn=r2b.mpn))
    add_sch_wire_chain(wires, [(shunt_x, row_y[2]), (shunt_x, row_y[2] + 2.54)])
    junctions.append((shunt_x, row_y[2]))
    add_sch_wire_chain(wires, [(shunt_x, row_y[2] + 10.16), (shunt_x, row_y[2] + 11.43)])
    add_gnd(shunt_x, row_y[2] + 11.43)

    place_series_r(rs2, r_x, row_y[3], "20", "GPIO2_POT_HI")
    place_series_r(rs1, r_x + 15.24, row_y[4], "19", "GPIO1_POT_WIPER")
    place_series_r(rs3, r_x, row_y[5], "21", "GPIO3_POT_LO")

    def place_rc_filter(res_comp: ComponentSpec, cap_comp: ComponentSpec, ry: float, u1_pin: str, net_label: str) -> None:
        place_series_r(res_comp, r_x, ry, u1_pin, net_label)
        cy = ry + 6.35
        sch_syms.append(SchSymbolInstance(cap_comp.ref, cap_comp.value, "Device:C", shunt_x, cy, 180, None, ["1", "2"], cap_comp.fp_name, cap_comp.descr, True, True, True, False, cap_comp.sym_uuid, (shunt_x - 3.048, cy, 0), (shunt_x + 3.048, cy, 0), ref_justify="left", val_justify="right", lcsc=cap_comp.lcsc, mpn=cap_comp.mpn))
        add_sch_wire_chain(wires, [(shunt_x, ry), (shunt_x, cy - 3.81)])
        junctions.append((shunt_x, ry))
        add_sch_wire_chain(wires, [(shunt_x, cy + 3.81), (shunt_x, cy + 5.08)])
        add_gnd(shunt_x, cy + 5.08)

    # All 5 C1_* caps have Pad 1 = GND, Pad 2 = GPIO* (rot=180)
    place_rc_filter(rs5, c1_2, row_y[6], "5", "GPIO5_CADENCE")
    place_rc_filter(rs4, c1_1, row_y[7], "22", "GPIO4_CAD_RTN")

    sw_sx, sw_sy = 203.20, row_y[8]
    d1_sx, d1_sy = 236.22, row_y[8]
    sch_syms.append(SchSymbolInstance(sw0.ref, sw0.value, "Switch:SW_SPST", sw_sx, sw_sy, 0, None, ["1", "2"], sw0.fp_name, sw0.descr, True, False, False, False, sw0.sym_uuid, (sw_sx, sw_sy - 3.556, 0), (sw_sx, sw_sy + 3.81, 0)))
    sch_syms.append(SchSymbolInstance(d1.ref, d1.value, "Device:D_Schottky", d1_sx, d1_sy, 180, None, ["1", "2"], d1.fp_name, d1.descr, True, True, True, False, d1.sym_uuid, (d1_sx, d1_sy - 3.556, 0), (d1_sx, d1_sy + 3.81, 0), lcsc=d1.lcsc, mpn=d1.mpn))
    add_sch_wire_chain(wires, [(198.12, row_y[8]), (sw_sx - 5.08, row_y[8])])
    add_sch_wire_chain(wires, [(sw_sx + 5.08, row_y[8]), (d1_sx - 3.81, row_y[8])])
    labels.append(("NET_SW0_D1", sw_sx + 5.08, row_y[8], 0, "left bottom"))
    ux_vin, uy_vin = u1_sheet["30"]
    add_sch_wire_chain(wires, [(d1_sx + 3.81, row_y[8]), (ux_vin, uy_vin)])
    labels.append(("NET_VIN_9V", ux_vin - 25.40, row_y[8], 0, "left bottom"))

    place_rc_filter(rs6, c1_3, row_y[10], "8", "GPIO8_ENC_A")
    place_rc_filter(rs8, c1_5, row_y[11], "10", "GPIO10_ENC_VCC")
    place_rc_filter(rs7, c1_4, row_y[12], "9", "GPIO9_ENC_B")

    # Off-Board Stock EX-3 Console PCB Reference
    con_sx, con_sy = 391.16, 66.04
    sch_syms.append(SchSymbolInstance("CON1", "STOCK_EX3_CONSOLE", "echenon:EX3_Console_PCB", con_sx, con_sy, 0, None, [str(i) for i in range(1, 13)], "", "Off-board Stock EX-3 Console PCB", False, False, False, False, None, (con_sx, con_sy - 25.40, 0), (con_sx, con_sy + 22.86, 0)))
    con_net_map = {
        "1": "NET_J1_1_MOT_P",
        "2": "NET_J1_2_MOT_N",
        "3": "NET_J1_3_POT_HI",
        "4": "NET_J1_4_POT_WIPER",
        "5": "NET_J1_5_POT_LO",
        "6": "NET_J1_6_CADENCE",
        "7": "NET_J1_7_CAD_RTN",
        "8": "NET_J1_8_V9_RAW",
        "9": None,
        "10": "NET_J3_1_ENC_A",
        "11": "NET_J3_2_ENC_VCC",
        "12": "NET_J3_3_ENC_B",
    }
    for pnum, lname in con_net_map.items():
        px, py = sym_pin_to_sheet(con_sx, con_sy, *con_pins[pnum])
        if lname is not None:
            add_sch_wire_chain(wires, [(px, py), (px - 3.81, py)])
            labels.append((lname, px - 3.81, py, 180, "right bottom"))
        else:
            add_sch_wire_chain(wires, [(px, py), (px - 3.81, py), (px - 3.81, py + 5.08)])
            add_gnd(px - 3.81, py + 5.08)

    # Mechanical & Fiducial Symbols for 100% Schematic-to-PCB Parity
    for idx_f, fcomp in enumerate([fid1, fid2, fid3]):
        fx, fy = 355.60 + idx_f * 17.78, 124.46
        sch_syms.append(SchSymbolInstance(fcomp.ref, fcomp.value, "Mechanical:Fiducial", fx, fy, 0, None, [], fcomp.fp_name, fcomp.descr, True, False, False, False, fcomp.sym_uuid, (fx, fy - 3.81, 0), (fx, fy + 3.81, 0), hide_val=True))
    for idx_m, mcomp in enumerate([mh1, mh2, mh3, mh4]):
        mx, my = 353.06 + idx_m * 15.24, 142.24
        sch_syms.append(SchSymbolInstance(mcomp.ref, mcomp.value, "Mechanical:MountingHole", mx, my, 0, None, [], mcomp.fp_name, mcomp.descr, True, False, False, False, mcomp.sym_uuid, (mx, my - 3.81, 0), (mx, my + 3.81, 0), hide_val=True))
    for idx_s, scomp in enumerate([sr1, sr2, sr3]):
        sx_m, sy_m = 355.60 + idx_s * 17.78, 157.48
        sch_syms.append(SchSymbolInstance(scomp.ref, scomp.value, "Mechanical:MountingHole", sx_m, sy_m, 0, None, [], scomp.fp_name, scomp.descr, True, False, False, False, scomp.sym_uuid, (sx_m, sy_m - 3.81, 0), (sx_m, sy_m + 3.81, 0), hide_val=True))

    rects = [
        (17.78, 20.32, 81.28, 266.70, "dash", "120 120 120 1"),
        (345.44, 20.32, 407.67, 101.60, "dash", "120 120 120 1"),
        (345.44, 109.22, 407.67, 167.64, "dash", "0 90 150 1"),
    ]
    polylines = [
        (
            [
                (86.36, 20.32),
                (340.36, 20.32),
                (340.36, 238.76),
                (271.78, 238.76),
                (271.78, 266.70),
                (86.36, 266.70),
                (86.36, 20.32),
            ],
            "solid",
            "0 90 150 1",
        )
    ]
    texts = [
        ("OFF-BOARD REFERENCE: ECHELON EX-3 BIKE", 20.32, 22.86, 1.524, "90 90 90 1"),
        ("(EXCLUDED FROM PCB & BOM)", 20.32, 25.40, 1.143, "110 110 110 1"),
        ("ECHENON v1 PASSIVE TAP — JLCPCB 2-LAYER SMD PCBA (70x50 mm)", 88.90, 22.86, 1.600, "0 90 150 1"),
        ("MOTOR SENSE DIV4 DIVIDERS (0805 1% BASIC: C17621 / C17414)", 185.42, 24.13, 1.100, "80 80 80 1"),
        ("POTENTIOMETER HIGH-Z TAPS (0805 22k BASIC: C17560)", 172.72, 68.58, 1.100, "80 80 80 1"),
        ("CADENCE RC FILTERS (0805 C17673 + C49678)", 190.50, 99.06, 1.100, "80 80 80 1"),
        ("CRITICAL: +9V VIN OR USB-C (NEVER BOTH; D1=SS14 C2480)", 172.72, 144.78, 1.100, "180 30 30 1"),
        ("USER-POPULATED HEADERS (J1..J4, SW0, U1)", 154.94, 173.99, 1.050, "0 90 150 1"),
        ("ENCODER RC FILTERS (0805 C17673 + C49678)", 193.04, 177.80, 1.100, "80 80 80 1"),
        ("ENCODER PASS-THROUGH (J3<->J4)", 162.56, 248.92, 1.100, "0 90 150 1"),
        ("OFF-BOARD: STOCK CONSOLE", 347.98, 22.86, 1.27, "90 90 90 1"),
        ("(J2 & J4 PASS-THROUGH)", 347.98, 25.40, 1.016, "110 110 110 1"),
        ("PCB FIDUCIALS & NPTH HOLES", 347.98, 111.76, 1.200, "0 90 150 1"),
    ]

    sch_spec = SchSpec(
        paper="A3",
        lib_symbols=lib_syms,
        symbols=sch_syms,
        wires=wires,
        junctions=junctions,
        labels=labels,
        no_connects=no_connects,
        rects=rects,
        polylines=polylines,
        texts=texts,
        sch_title="Echenon v1 Passive Tap — JLCPCB 2L SMD PCBA",
    )

    return proj_name, title, w_mm, h_mm, components, segments, [], silk_texts, sch_spec, vias, zones


def run_cmd(cmd: List[str]) -> subprocess.CompletedProcess:
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"COMMAND FAILED ({res.returncode}): {' '.join(cmd)}", file=sys.stderr)
        print("STDOUT:", res.stdout, file=sys.stderr)
        print("STDERR:", res.stderr, file=sys.stderr)
        sys.exit(res.returncode)
    return res


def export_jlcpcb_bom_and_cpl(proj_name: str, sch_path: Path, pcb_path: Path, gerber_dir: Path) -> Tuple[Path, Path, int, int]:
    """Export JLCPCB-ready BOM and Pick-and-Place CPL CSV files via kicad-cli."""
    bom_csv = gerber_dir / f"{proj_name}_bom.csv"
    raw_pos_csv = TMP_DIR / f"{proj_name}_pos_raw.csv"
    cpl_csv = gerber_dir / f"{proj_name}_cpl.csv"

    run_cmd(
        [
            KICAD_CLI, "sch", "export", "bom",
            "--fields", "Value,Reference,Footprint,LCSC",
            "--labels", "Comment,Designator,Footprint,JLCPCB Part #",
            "--group-by", "Value,Footprint,LCSC",
            "--ref-range-delimiter", "",
            "--exclude-dnp",
            "-o", str(bom_csv),
            str(sch_path),
        ]
    )

    run_cmd(
        [
            KICAD_CLI, "pcb", "export", "pos",
            "--format", "csv",
            "--units", "mm",
            "--side", "front",
            "--use-drill-file-origin",
            "--smd-only",
            "--exclude-dnp",
            "-o", str(raw_pos_csv),
            str(pcb_path),
        ]
    )

    cpl_rows: List[List[str]] = []
    with raw_pos_csv.open("r", encoding="utf-8", newline="") as rf:
        reader = csv.DictReader(rf)
        for row in reader:
            ref = (row.get("Ref") or "").strip()
            if not ref or ref.startswith("FID"):
                continue
            side_raw = (row.get("Side") or "top").strip().lower()
            layer_str = "Top" if side_raw == "top" else "Bottom"
            pos_x = f"{float(row['PosX']):.4f}"
            pos_y = f"{float(row['PosY']):.4f}"
            rot = f"{float(row['Rot']) % 360.0:.1f}"
            cpl_rows.append([ref, pos_x, pos_y, layer_str, rot])

    with cpl_csv.open("w", encoding="utf-8", newline="") as wf:
        writer = csv.writer(wf)
        writer.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        writer.writerows(cpl_rows)

    # Normalize BOM CSV: strip KiCad's spurious '?' on lowercase sub-ref designators (e.g. R1a? -> R1a)
    # and verify 1-to-1 exact designator set parity between BOM and CPL.
    cleaned_bom_rows: List[List[str]] = []
    bom_desig_set = set()
    with bom_csv.open("r", encoding="utf-8", newline="") as bf:
        b_reader = csv.DictReader(bf)
        for brow in b_reader:
            comment = (brow.get("Comment") or "").strip()
            fp_col = (brow.get("Footprint") or "").strip()
            lcsc_code = (brow.get("JLCPCB Part #") or "").strip()
            assert lcsc_code.startswith("C"), f"Missing LCSC part code in BOM row: {brow}"
            desigs = [d.strip().rstrip("?") for d in (brow.get("Designator") or "").split(",") if d.strip()]
            for d in desigs:
                bom_desig_set.add(d)
            cleaned_bom_rows.append([comment, ",".join(desigs), fp_col, lcsc_code])

    with bom_csv.open("w", encoding="utf-8", newline="") as bwf:
        b_writer = csv.writer(bwf)
        b_writer.writerow(["Comment", "Designator", "Footprint", "JLCPCB Part #"])
        b_writer.writerows(cleaned_bom_rows)

    cpl_desig_set = {r[0] for r in cpl_rows}
    assert bom_desig_set == cpl_desig_set, f"BOM/CPL designator mismatch: BOM={sorted(bom_desig_set)} vs CPL={sorted(cpl_desig_set)}"
    return bom_csv, cpl_csv, len(bom_desig_set), len(cpl_rows)


def build_and_verify_board(sub_dir: str, builder_fn, mode: str = "cnc") -> Dict[str, str]:
    built = builder_fn()
    if mode == "jlcpcb":
        proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts, sch_spec, vias, zones = built
    else:
        proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts, sch_spec = built
        vias, zones = None, None

    proj_dir = PCB_ROOT / sub_dir
    gerber_dir = PCB_ROOT / "gerbers" / sub_dir
    proj_dir.mkdir(parents=True, exist_ok=True)
    gerber_dir.mkdir(parents=True, exist_ok=True)
    TMP_DIR.mkdir(parents=True, exist_ok=True)

    sch_uuid = det_uuid(f"root_sch:{proj_name}")
    pro_path = proj_dir / f"{proj_name}.kicad_pro"
    sch_path = proj_dir / f"{proj_name}.kicad_sch"
    pcb_path = proj_dir / f"{proj_name}.kicad_pcb"

    if mode == "jlcpcb":
        pro_path.write_text(make_kicad_pro_jlcpcb(proj_name, sch_uuid), encoding="utf-8")
        sch_path.write_text(make_kicad_sch(proj_name, title, sch_uuid, sch_spec, rev="v1.0-jlcpcb-smd"), encoding="utf-8")
        pcb_path.write_text(
            make_kicad_pcb(
                proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts,
                vias=vias, zones=zones, allow_f_cu=True, rev="v1.0-jlcpcb-smd",
            ),
            encoding="utf-8",
        )
    else:
        pro_path.write_text(make_kicad_pro(proj_name, sch_uuid), encoding="utf-8")
        sch_path.write_text(make_kicad_sch(proj_name, title, sch_uuid, sch_spec), encoding="utf-8")
        pcb_path.write_text(
            make_kicad_pcb(proj_name, title, w_mm, h_mm, components, segments, edge_circles, silk_texts),
            encoding="utf-8",
        )

    erc_json = TMP_DIR / f"{proj_name}_erc.json"
    run_cmd([KICAD_CLI, "sch", "erc", "--severity-all", "--exit-code-violations", "--format", "json", "-o", str(erc_json), str(sch_path)])
    erc_data = json.loads(erc_json.read_text(encoding="utf-8"))
    erc_violations = sum(len(sheet.get("violations", [])) for sheet in erc_data.get("sheets", []))
    assert erc_violations == 0, f"ERC failed with {erc_violations} violations"

    drc_json = TMP_DIR / f"{proj_name}_drc.json"
    drc_cmd = [KICAD_CLI, "pcb", "drc", "--schematic-parity", "--severity-all", "--exit-code-violations", "--format", "json"]
    if mode == "jlcpcb":
        drc_cmd.extend(["--refill-zones", "--save-board"])
    drc_cmd.extend(["-o", str(drc_json), str(pcb_path)])
    run_cmd(drc_cmd)
    drc_data = json.loads(drc_json.read_text(encoding="utf-8"))
    assert len(drc_data.get("violations", [])) == 0, f"DRC violations: {drc_data.get('violations')}"
    assert len(drc_data.get("unconnected_items", [])) == 0, f"Unconnected items: {drc_data.get('unconnected_items')}"
    assert len(drc_data.get("schematic_parity", [])) == 0, f"Schematic parity errors: {drc_data.get('schematic_parity')}"

    for old_f in gerber_dir.glob("*"):
        if old_f.is_file():
            old_f.unlink()

    if mode == "jlcpcb":
        gerber_layers = "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"
        run_cmd([KICAD_CLI, "pcb", "export", "gerbers", "--layers", gerber_layers, "--subtract-soldermask", "--check-zones", "--use-drill-file-origin", "-o", str(gerber_dir), str(pcb_path)])
    else:
        run_cmd([KICAD_CLI, "pcb", "export", "gerbers", "--layers", "B.Cu,B.Mask,F.SilkS,B.SilkS,Edge.Cuts", "--use-drill-file-origin", "-o", str(gerber_dir), str(pcb_path)])

    run_cmd([KICAD_CLI, "pcb", "export", "drill", "--format", "excellon", "--drill-origin", "plot", "--excellon-units", "mm", "--excellon-separate-th", "--generate-map", "--map-format", "svg", "-o", str(gerber_dir), str(pcb_path)])

    top_svg = gerber_dir / f"{proj_name}_top_view.svg"
    bot_svg = gerber_dir / f"{proj_name}_bottom_copper.svg"
    sch_svg_dir = gerber_dir / "schematic_svg"
    sch_svg_dir.mkdir(exist_ok=True)
    sch_pdf = proj_dir / f"{proj_name}.pdf"
    pcb_pdf = proj_dir / f"{proj_name}_pcb.pdf"

    top_layers = "F.Cu,F.Mask,F.SilkS,F.Fab,Edge.Cuts" if mode == "jlcpcb" else "F.SilkS,F.Fab,B.Cu,Edge.Cuts"
    pdf_layers = "B.Cu,F.Cu,F.Fab,F.SilkS,Edge.Cuts" if mode == "jlcpcb" else "F.SilkS,F.Fab,B.Cu,Edge.Cuts"
    run_cmd([KICAD_CLI, "pcb", "export", "svg", "--layers", top_layers, "--exclude-drawing-sheet", "--fit-page-to-board", "--page-size-mode", "2", "--mode-single", "-o", str(top_svg), str(pcb_path)])
    run_cmd([KICAD_CLI, "pcb", "export", "svg", "--layers", "B.Cu,B.Mask,B.SilkS,Edge.Cuts", "--mirror", "--exclude-drawing-sheet", "--fit-page-to-board", "--page-size-mode", "2", "--mode-single", "-o", str(bot_svg), str(pcb_path)])
    run_cmd([KICAD_CLI, "sch", "export", "svg", "-o", str(sch_svg_dir), str(sch_path)])
    run_cmd([KICAD_CLI, "sch", "export", "pdf", "--exclude-pdf-metadata", "-o", str(sch_pdf), str(sch_path)])
    run_cmd([KICAD_CLI, "pcb", "export", "pdf", "--layers", pdf_layers, "--drill-shape-opt", "2", "--scale", "0", "--no-property-popups", "--mode-single", "-o", str(pcb_pdf), str(pcb_path)])

    if mode == "jlcpcb":
        render_top = gerber_dir / f"{proj_name}_3d_top.png"
        render_bot = gerber_dir / f"{proj_name}_3d_bottom.png"
        run_cmd([KICAD_CLI, "pcb", "render", "--side", "top", "--quality", "high", "--background", "opaque", "-w", "1800", "-h", "1250", "-o", str(render_top), str(pcb_path)])
        run_cmd([KICAD_CLI, "pcb", "render", "--side", "bottom", "--quality", "high", "--background", "opaque", "-w", "1800", "-h", "1250", "-o", str(render_bot), str(pcb_path)])

    zip_path = gerber_dir / f"{proj_name}_gerbers.zip"
    gerber_exts = {".gbr", ".gtl", ".gbl", ".gts", ".gbs", ".gtp", ".gbp", ".gto", ".gbo", ".gm1", ".drl", ".gbrjob"}
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for gf in sorted(gerber_dir.iterdir()):
            if gf.is_file() and gf.suffix.lower() in gerber_exts:
                zf.write(gf, arcname=gf.name)

    result = {
        "proj_name": proj_name,
        "pcb_path": str(pcb_path),
        "sch_path": str(sch_path),
        "sch_pdf": str(sch_pdf),
        "pcb_pdf": str(pcb_pdf),
        "zip_path": str(zip_path),
        "top_svg": str(top_svg),
        "bot_svg": str(bot_svg),
    }

    if mode == "jlcpcb":
        bom_csv, cpl_csv, bom_cnt, cpl_cnt = export_jlcpcb_bom_and_cpl(proj_name, sch_path, pcb_path, gerber_dir)
        assert bom_cnt == 18, f"Expected 18 SMD parts in BOM, got {bom_cnt}"
        result["bom_csv"] = str(bom_csv)
        result["cpl_csv"] = str(cpl_csv)
        result["smd_part_count"] = str(bom_cnt)
        print(f"[OK] {proj_name}: ERC=0, DRC=0, Unconnected=0, Parity=0, SMD={bom_cnt} -> {zip_path}")
    else:
        print(f"[OK] {proj_name}: ERC=0, DRC=0, Unconnected=0, Parity=0 -> {zip_path}")

    return result


def main() -> None:
    coupon_info = build_and_verify_board("coupon", build_coupon_project, mode="cnc")
    v1_info = build_and_verify_board("v1-passive-tap", build_v1_passive_tap_project, mode="cnc")
    v1_jlcpcb_info = build_and_verify_board("v1-passive-tap-jlcpcb", build_v1_passive_tap_jlcpcb_project, mode="jlcpcb")
    print(json.dumps({"coupon": coupon_info, "v1_passive_tap": v1_info, "v1_passive_tap_jlcpcb": v1_jlcpcb_info}, indent=2))


if __name__ == "__main__":
    main()
