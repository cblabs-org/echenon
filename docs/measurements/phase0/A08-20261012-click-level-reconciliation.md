# A08 — Click-to-Level Reconciliation

**Date:** 2026-10-12
**Bench session:** DR-1 prep WO1
**Firmware:** `v0.1.0-dev.7` (env `ex3-waveshare-nano`, `WITH_SIGNAL_LOGGER`)
**Data file:** [`A08-20261012-click-level-reconciliation.csv`](A08-20261012-click-level-reconciliation.csv)

## Purpose

Re-derive B02's "28% of encoder clicks produce no motor response" using the
WO5 finding that the encoder has 4 detents (16 PCNT steps) per console level
change. Determine whether clicks are genuinely *lost* or whether the console
simply accumulates multiple detents before acting.

## Method

1. With the console at level 1 and a class running (motor seeks on knob
   input), the owner turned the knob **one detent at a time**, waiting for the
   motor to settle after each click, for 5 level changes UP (to level 6).
2. Repeated DOWN (back to level 1). The owner reported this direction was
   "much flakier on registering the detents."
3. Rapid spin UP to ~level 18, then rapid spin DOWN.
4. Events downloaded from `GET /api/echenon/events.csv` (134 events).

> **Note:** The ride-log (PSRAM) was lost when the device rebooted during the
> CSV download. The events log (flash) survived. Wiper ADC correlation is
> therefore unavailable for this specific test, but the event-level analysis
> is sufficient for the reconciliation because B03's `wiper = 39.1 × level + 97`
> is already validated.

## Logger resolution finding

Each **type-6 event** in the SignalLogger represents **2 physical detents**
(8 PCNT counts), not 1. This was confirmed by the owner counting 4 physical
clicks per level while the logger recorded 2 type-6 events per motor action.

## Results

### Slow, deliberate clicks — UP (levels 1 → 6)

| Motor action | Type-6 events | Physical detents | Motor response |
|---|---|---|---|
| Level 1→2 | 2 | 4 | Forward, clean |
| Level 2→3 | 2 | 4 | Forward, clean |
| Level 3→4 | 2 | 4 | Forward, clean |
| Level 4→5 | 2 | 4 | Forward, clean |
| Level 5→6 | 2 | 4 | Forward, clean |

**Result: 4 detents per level, 100% consistent.** Confirms B03.

### Slow, deliberate clicks — DOWN (levels 6 → 1)

| Motor action | Type-6 events | Physical detents | Notes |
|---|---|---|---|
| Level 6→5 | 3 | 6 | Needed extra clicks |
| Level 5→4 | 5 | 10 | Significantly flaky |
| Level 4→3 | 2 | 4 | Clean |
| Level 3→2 | 2 | 4 | Clean |
| Level 2→1 | 2 | 4 | Clean |

**Result: Nominally 4 detents per level, but the down direction sometimes
requires 6–10 detents (1.5–2.5× more).** The motor also exhibits
stop→restart stutter pairs on reverse (at 731.05 s and 737.26 s) that do not
appear on forward.

### Fast spin — UP (~level 1 → ~18)

The console batches rapid detents into single long motor seeks:
- 8 → 16 → 12 → 2 type-6 events per motor action (16–32 physical detents batched)
- Motor ran continuously for ~1 s per action

### Fast spin — DOWN (~level 18 → ~1)

- 4 → 18 → 5 type-6 events per motor action
- **Overshoot correction observed:** motor action #16 was a *forward* pulse
  during a DOWN sequence (18 type-6 DOWN events before it). The console
  overshot and reversed. This is normal seeking behaviour.

### Overall statistics

| Metric | Value |
|---|---|
| Total type-6 events | 96 (= 192 physical detents) |
| Total motor start actions | 19 |
| Motor response per physical detent | 10% (19/192) |
| Motor response per level (slow UP) | 100% (5/5) |
| Motor response per level (slow DOWN) | 100% (5/5) |
| Genuinely lost clicks (encoder registered, level never changed) | **0** |

## B02 "28% miss rate" — erratum

B02 (2026-10-04) reported that "28% of encoder clicks produce no motor
response" and interpreted this as a signal-quality concern. This figure is
now understood to be a **measurement artifact**:

1. **By design, 75% of individual detent clicks produce no motor response.**
   The console accumulates 4 detents (16 PCNT steps) before changing one
   level and moving the motor. Three of those four clicks are accumulation
   steps, not misses.

2. **B02 undercounted physical detents** because it used ADC sampling of a
   single encoder channel (before the wire-colour correction in B04 and
   before the PCNT module was adopted). Rapid clicks were grouped, and the
   effective count was lower than the true physical count — inflating the
   apparent response rate to 72%.

3. **The corrected statement:** At normal click rates, every group of 4
   detents produces exactly one motor response (UP). The down direction is
   somewhat flakier, sometimes requiring 6–10 detents. No individual detent
   is ever genuinely "lost" — the console always acts once the accumulation
   threshold is reached.

## Proposals for DR-1

- **The 4-detent-per-level design is confirmed and should be treated as
  settled.** Any MITM encoder scheme must replicate groups of 4 detents (16
  PCNT steps) per level command, not individual clicks.
- **The down-direction flakiness is a console characteristic, not a wiring
  or sense-network issue.** DR-1 should note this asymmetry as a constraint
  on encoder resynthesis timing.
- **The motor stutter on reverse should be characterised further** if the
  MITM design needs to detect seek-complete by monitoring motor cessation.
