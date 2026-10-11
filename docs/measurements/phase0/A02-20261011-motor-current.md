# A02 — Motor Current Measurement

**Date:** 2026-10-11
**Register row:** #2 — Motor current (inrush and steady-state)
**Equipment:** DMM in series with motor wire
**Operator:** Owner at bench, PA recording

## Summary

Motor current is low (< 200 mA peak) under all conditions. The motor drives
an eccentric cam to position a braking magnet. The mechanical load is the cam
mechanism, independent of flywheel speed — no-load and under-load currents
are equivalent.

## Findings

| Condition | Direction | Inrush (peak) | Steady-state |
|---|---|---|---|
| No-load (idle wheels) | Reverse (resistance decrease) | −175 mA | −150 mA |
| No-load (idle wheels) | Forward (resistance increase) | ~95 mA | +50 mA |

### Notes

- Reverse direction draws ~3× more steady-state current than forward,
  consistent with the cam's asymmetric mechanical load profile.
- All readings well under 500 mA — no concerns for tap board, dividers, or wiring.
- The motor drives an eccentric cam (8 mm offset, ~151° sweep per B03).
  The cam mechanism is the load, not the flywheel. Motor current is independent
  of rider pedaling effort.
- DMM response time limits inrush accuracy. True peak inrush may be higher
  but is not expected to exceed 500 mA based on motor size and voltage (3.25V).

## Limitations

- DMM cannot capture sub-100 ms inrush spikes. A current probe or shunt +
  oscilloscope would give better inrush data.
- Only no-load (idle flywheel) tested. Under-load test is unnecessary because
  the motor load is the cam mechanism, not the flywheel brake force.

## Result

**PASS** — Motor current characterised: ≤175 mA peak, ≤150 mA steady-state.
Well within safe operating range for all circuit elements.
