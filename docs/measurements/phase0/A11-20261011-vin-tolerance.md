# A11 — Waveshare ESP32-S3-Nano VIN Tolerance

**Date:** 2026-10-11
**Register row:** #11 — Waveshare VIN 9V tolerance
**Method:** Datasheet review
**Operator:** PA (no bench test required)

## Summary

The Waveshare ESP32-S3-Nano board accepts **6V–21V on VIN** via its onboard
MP2322GQH buck regulator. The EX-3 console's 9V supply is well within the
rated input range.

## Source

- Waveshare ESP32-S3-Nano product page and datasheet
- Onboard regulator: MPS MP2322GQH, input range 4.5V–24V, output 3.3V

## Findings

| Parameter | Value |
|---|---|
| VIN minimum | 6 V |
| VIN maximum | 21 V |
| Bike supply | 9 V |
| Margin to min | 3 V (50%) |
| Margin to max | 12 V (133%) |

## Owner Observation (2026-10-11)

> The motor drive lines (±3.25V DC) may be used as signal inputs only (desired
> movement direction/duration), with actual motor drive via a separate H-bridge
> board. This would change the power architecture — the ESP32 board may not
> need the bike's 9V directly. Power architecture deferred to DR-1.

## Result

**PASS** — 9V is within the board's 6–21V VIN range. Safe for Phase 0/1 use.
Power architecture for Phase 3+ deferred to DR-1.
