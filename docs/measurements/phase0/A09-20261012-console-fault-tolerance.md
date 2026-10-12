# A09 Addendum — Console Fault Tolerance (Safe Subset)

**Date:** 2026-10-12
**Bench session:** DR-1 prep WO1
**Firmware:** `v0.1.0-dev.7` (env `ex3-waveshare-nano`)

## Purpose

Determine whether the stock console tolerates the fault conditions relevant
to an MITM encoder interception design. These are the safe subset from
`docs/mitm-encoder-design-notes.md` §12.

## Results

### Test #1 — Encoder disconnect

**Method:** Unplugged the encoder at the Y-harness during a class, waited
~30 s, reconnected.

**Result: PASS — console tolerates encoder disconnect.**
- Console continued to display the current level.
- No fault, no error, no reset.
- On reconnection, the encoder worked immediately — clicks registered and
  the motor responded.
- The console does not appear to monitor encoder presence or health.

### Test #2 — Encoder stall (knob held still)

**Method:** Left the knob untouched for multiple minutes during a running
class.

**Result: PASS — this is normal operation.**
- The console holds the current level indefinitely when the knob is not
  turned. This is the standard class experience — the rider sets a level
  and leaves it for the duration of a song or interval.
- No timeout, no fault, no idle-reset behaviour observed.

### Test #3 — Fast burst (> 10 detents/sec)

**Method:** Spun the knob rapidly up (~10+ detents/sec) then rapidly down.
Data captured in Item 1's events.csv
([`A08-20261012-click-level-reconciliation.csv`](A08-20261012-click-level-reconciliation.csv)).

**Result: PASS (with caveat) — console batches rapid detents but can
overshoot into runaway.**

- At moderate speed: the console batches the accumulated encoder counts
  into one long motor seek. Motor runs continuously for ~1 s until the
  target level is reached. Normal behaviour.
- At extreme speed to maximum level: the console can **overshoot past the
  working range of the pot** into the non-working half. Once in the
  non-working half, the wiper voltage no longer corresponds to any valid
  level, and the motor runs continuously in 360° rotation without stopping.

> [!WARNING]
> **Runaway failure mode observed.** During rapid knob spin to maximum,
> the motor entered continuous 360° rotation and would not stop. Recovery
> required physically disconnecting the motor wire. The console resumed
> normal operation after motor reconnection and knob adjustment.

- **Trigger condition:** Very rapid clockwise spin to the top of the range.
  The console's seek algorithm overshot the level-32 ceiling and drove the
  pot past the dead zone into the non-working ramp.
- **Recovery:** Disconnect motor → reconnect → turn knob back to a valid
  range.
- **DMM observation:** Wiper voltage reached ~3.3 V during runaway
  (consistent with the pot being driven past the working range into the
  upper plateau / second working ramp).

### Test #7 — Reversed A/B

**Method:** Swapped encoder A and B channels using a plug-in adapter.

**Result: PASS — console operates normally with inverted direction.**
- Reversing A/B causes the encoder to register CW rotation as "down" and
  ACW as "up" (opposite of normal convention: CW = up, ACW = down).
- Console does not fault, does not detect the reversal, and continues to
  change levels correctly — just in the opposite rotational sense.
- Motor seek direction and level changes are correct relative to the
  encoder output; only the physical rotation direction mapping is inverted.

### Test #9 — Mid-seek (clicks during motor travel)

**Method:** Clicked 2–3 detents while the motor was still seeking from a
previous level change.

**Result: PASS — console updates target seamlessly.**
- When additional clicks arrive mid-seek, the console updates its internal
  target level and continues (or reverses) the motor seek to the new
  target.
- No fault, no confusion, no overshoot observed at normal click rates.
- This is standard user behaviour (turning the knob while the motor is
  still moving) and the console handles it cleanly.

## Summary

| Test | Result | Notes |
|---|---|---|
| #1 Encoder disconnect | **PASS** | No fault detection; immediate recovery on reconnect |
| #2 Encoder stall | **PASS** | Normal operation; no timeout |
| #3 Fast burst | **PASS*** | Batches correctly, but extreme speed can cause runaway |
| #7 Reversed A/B | **PASS** | Works normally, direction inverted |
| #9 Mid-seek | **PASS** | Seamless target update |

\* Conditional pass — extreme speed reveals a runaway failure mode.

## Proposals for DR-1

- **The console is highly tolerant of encoder faults.** It does not monitor
  encoder presence, does not timeout on idle, does not detect channel
  reversal, and handles mid-seek input cleanly. This is favourable for
  MITM interception.
- **An MITM encoder resynthesiser MUST rate-limit its quadrature output.**
  Rapid encoder input can drive the console past the pot's working range
  into a runaway 360° seek. The rate limit should prevent the console from
  overshooting level 32 (or level 1 in the down direction).
- **The reversed-A/B tolerance means the MITM can choose either rotational
  convention.** The console will work correctly either way; only the
  mapping to the app's "increase/decrease" changes.
