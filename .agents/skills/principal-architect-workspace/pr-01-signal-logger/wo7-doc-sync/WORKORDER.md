---
complexity: standard
---

# WO7 — Doc-Sync

**Parent:** [PR-01 Signal Logger](../PROJECT.md)
**Branch:** `signal-logger/wo7-doc-sync` (base: `signal-logger/wo6-native-tests`)

## Role Definition
> You are a technical documentation engineer. Your job is to update the project
> documentation so that it accurately reflects the firmware and hardware as built
> during PR-01. You do not change code — only documentation.

## Objective

Synchronise the documentation with what was actually built and measured during
PR-01. Four categories of changes:

1. **Wire colours, cadence tap values, ADC/PCNT split, EncoderModule decision**
2. **Test list updates** if any tests changed
3. **Remove all absolute OS paths** (`/Users/…`, `file:///`) from `docs/`
4. **Do NOT edit** `PROGRAM.md`, `PROGRAM-PLAN.md`, `reviews/*`, or
   `reviews/DR-1-design-freeze.md` — the Chief Architect owns those

## Context

**Owner-approved changes to document:**
- Encoder wire colours: Black = Ch A, Red = Ch B, Orange = VCC (measured in B01)
- Cadence tap: 4.7 kΩ series + 100 nF (not 22 kΩ). Bench-verified in B01.
- GPIO 8/9 released from ADC for PCNT (`EncoderModule`). 8 ADC channels remain.
- `stepsPerDetent=4` confirmed in B04.
- `WITH_ENCODER` enabled; `BOARD_ENCODERS[]` added to `board.h`.

**Known inconsistencies (from pre-flight audit):**
- Hardware spec §5.3 schematic: encoder wires still show old colours
  (Brown/Orange/Red should be Black/Red/Orange)
- Hardware spec §5.7 build checklist: says "RS1–RS8 (22 kΩ)" but cadence
  taps are now 4.7 kΩ
- SPEC §4.4.1: may need note updates for the cadence resistor change
- `docs/encoder-resistance-analysis.md` and `docs/mitm-encoder-design-notes.md`:
  contain `file:///Users/…` absolute paths that must be removed

## Strict Boundary Box

- **Modify:**
  - `SPEC.md` — sections §4.4.1, §5.1, §5.4 only
  - `docs/hardware_specification.md` — section §5 only
  - `docs/ex3_specification.md` — encoder and cadence sections
  - `SPEC.md` §13.2 — if any test descriptions need updating
  - `docs/encoder-resistance-analysis.md` — absolute path cleanup
  - `docs/mitm-encoder-design-notes.md` — absolute path cleanup
  - Any other `docs/*.md` file that contains `/Users/` or `file:///` paths
- **Read:** `board.h`, `B01` sensor bringup report, `B04` PCNT comparison,
  PROJECT.md session log (owner decisions)
- **Do NOT modify:**
  - `PROGRAM.md`, `PROGRAM-PLAN.md`
  - `reviews/*` or `reviews/DR-1-design-freeze.md`
  - Any source code (`src/`, `hardware/`, `platformio.ini`)
  - Evidence files under `docs/measurements/phase0/`
  - `PROJECT.md` or any file under `.agents/`
- **Do NOT create additional branches.**

## Tooling Constraints

- Use PlatformIO MCP tools for building (`build_project`) if verification is
  needed, not `pio` CLI
- Follow `.agents/rules/03-versioning.md` and `.agents/rules/04-mcp-tooling.md`

## Line-Item Instructions

### A. SPEC §4.4.1 — Pin Map

- [ ] Verify the pin map table is correct for the as-built state:
  - GPIO 4/5: cadence taps use 4.7 kΩ + 100 nF (not 22 kΩ)
  - GPIO 8/9: PCNT input (not ADC), wire colours Black/Red
  - GPIO 10: Encoder VCC, wire colour Orange
- [ ] Verify notes 2, 4, 5 accurately describe the cadence resistor change,
  encoder wire colours, and GPIO 8/9 reassignment
- [ ] Fix any remaining references to old wire colours or old resistor values

### B. SPEC §5.1 and §5.4

- [ ] If these sections exist and reference the tap network, update them
  consistently with §4.4.1

### C. Hardware Spec §5 — Phase 0 Build

- [ ] §5.3 schematic table: update encoder wire colours from
  Brown/Orange/Red to Black/Red/Orange
- [ ] §5.5 series resistors table: ensure cadence rows show 4.7 kΩ + 100 nF
- [ ] §5.6 observation taps: verify encoder connector table has correct colours
- [ ] §5.7 build checklist: update "RS1–RS8 (22 kΩ)" to reflect that
  cadence taps (RS4, RS5) use 4.7 kΩ + 100 nF
- [ ] §5.8 test points: verify expected readings are consistent with actual
  measurements from B01

### D. `docs/ex3_specification.md`

- [ ] Update any encoder or cadence sections to reflect the measured wire
  colours and resistor values
- [ ] Check for any stale references to GPIO 8/9 as ADC channels

### E. SPEC §13.2 — PR-01 Tests

- [ ] Verify the test table matches the actual test suites in WO6
- [ ] If WO6 added or changed test names, update accordingly
- [ ] Verify the on-device verification table matches the register row
  assignments

### F. Absolute Path Cleanup

- [ ] Search all `docs/*.md` files for `/Users/` and `file:///` patterns:
  ```
  grep -rn '/Users/\|file:///' docs/
  ```
- [ ] Replace absolute paths with relative paths within the repository
- [ ] Verify all links still resolve correctly after the change

## Verification

- [ ] `grep -rn '/Users/\|file:///' docs/` returns no matches
- [ ] All modified files are valid Markdown with no broken links
- [ ] No changes to excluded files (PROGRAM.md, reviews/*, etc.)
- [ ] Build still succeeds (`build_project` on `env:ex3-waveshare-nano`)

---

## Completion Report

_(To be filled in by the worker at task completion)_
