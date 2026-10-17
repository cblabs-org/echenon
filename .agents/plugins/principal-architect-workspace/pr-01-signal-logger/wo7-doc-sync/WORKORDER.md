---
schema_version: 1
wo_id: WO-01.7
pr_id: PR-01
title: Doc-Sync
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/
- docs/
- hardware/
- platformio.ini
verification_command: echo legacy-migrated
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

### G. E-01 Pot Polarity (CA review addendum)

B01 measured **Yellow ("POT_REF_HI") = 0 V** and **Green ("POT_REF_LO") ≈ 1.67 V**.
This is the opposite of what the SPEC assumes.

- [ ] Record the measured polarity as a **fact** in SPEC §4.4.1 (add a note):
  - Green (GPIO 2): console pot reference, measured ≈ 1.67 V (≈ 2,084 ADC counts)
  - Yellow (GPIO 3): pot reference, measured ≈ 0 V
- [ ] Update signal names if they say "HI"/"LO" incorrectly
- [ ] Add a note that the provisional Phase 3 standalone network is incorrect
  and DR-1 §2.2/§2.9 must redesign
- [ ] Do NOT redesign the circuit — leave that to DR-1

### H. E-03 A08 Superseded (CA review addendum)

WO0/A08 concluded encoder Ch B needed analog thresholding. After the wire-colour
correction, PCNT decodes both channels with 0% bounce and 100% detection (B04).
WO0 almost certainly probed the wrong wire.

- [ ] Mark A08 as *superseded* in the register (`docs/measurements/phase0/README.md`),
  with a note: "Superseded by B04 — wire-colour correction shows PCNT decodes
  both channels cleanly"
- [ ] Remove "Ch B conditioning" from SPEC §5.4 if present
- [ ] Remove or annotate the "1.5–2 V Ch B pulse" claim from the teardown/design
  notes if present

### I. E-04 Single Level Formula (CA review addendum)

B03 gives `wiper = 39.1 × level + 97`. Design notes §4 give
`level = (wiper − 125) / 37.7 + 1`. These are two different fits.

- [ ] Adopt B03's fit (`wiper = 39.1 × level + 97`) as the single reference
- [ ] Update any references in design notes, SPEC §5.3, or ex3 spec to cite
  B03 and use the single formula
- [ ] Remove or annotate the design notes §4 formula

### J. E-05 B-Report → Register Row Mapping (CA review addendum)

The B-series bench reports use a different naming prefix from the register's
`A<row>` convention. Keep the B-series as "bench reports" and have the register
cite them per row:

- [ ] Update `docs/measurements/phase0/README.md` register:
  - B01 → rows #6, #7, #8 (partial)
  - B02 and B04 → row #8
  - B03 → row #4
- [ ] Add a note at the top of the register explaining the dual prefix scheme:
  `A<row>-…` for row-specific evidence, `B<session>-…` for bench session reports

### K. Commit Bench Wrap-Up (CA review addendum — process item P-D)

The bench session wrap-up report currently lives outside the repo. Commit it for
provenance.

- [ ] Copy the wrap-up from:
  `/Users/mcneillm/.gemini/antigravity/brain/d327814e-863a-49b9-b7e7-96c160bf0725/session-wrapup.md`
  into `.agents/plugins/principal-architect-workspace/pr-01-signal-logger/session-wrapup.md`
- [ ] Commit with message: `docs: commit bench session wrap-up for provenance`

## Verification

- [ ] `grep -rn '/Users/\|file:///' docs/` returns no matches
- [ ] All modified files are valid Markdown with no broken links
- [ ] No changes to excluded files (PROGRAM.md, reviews/*, etc.)
- [ ] Build still succeeds (`build_project` on `env:ex3-waveshare-nano`)
- [ ] A08 is marked superseded in the register
- [ ] Pot polarity is recorded as a measured fact
- [ ] B03 fit is the single level formula reference
- [ ] B-reports are cited in the register per E-05 mapping

---

## Completion Report

_(To be filled in by the worker at task completion)_
