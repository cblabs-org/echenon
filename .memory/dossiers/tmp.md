# tmp

## Invariants & contracts

### [docseed-01e8b9f5] **Root Cause:** During extended bench operation (`uptime > 71.58 min` = `4,294,967 ms`, observed at `uptime = 9,237 s` in `tmp/ftms_multi_channel_monitor.json`), `uint32_t pulseMicros` wraps modulo `2^32 µs` (`0 .. 4,294,967 ms`), whereas `currentMillis` (`millis()`) continues advancing past `4,294,967 ms`. Because `handlePulse()` set `_lastPulseMs = pulseMicros / 1000` and `tick()` checked `c...
`invariant · kind: semantic · anchors: tmp/ftms_multi_channel_monitor.json · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-7a8e1850] **Live Capture Proof (`tmp/ftms_bleak_hr_knob_v21.json` & `v0.3.0-dev.22` live ride):** Continuous rock-solid HR reporting (`68–79 BPM`) across both `0x180D` (`HRMRepeater`) and `0x2AD2` (`FTMSServer`) alongside full resistance knob sweeps (`Level 1` / `R_ftms=0` / `Peloton=13` through `Level 32` / `R_ftms=254` / `Peloton=42`).
`invariant · kind: semantic · anchors: tmp/ftms_bleak_hr_knob_v21.json · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`

### [docseed-b9c34743] **Live Capture Proof (`tmp/ftms_bleak_pedal_v21.json`):** Continuous physical pedalling from `35.5 RPM` to `84.5 RPM` (`84–146 W`, `24.16–29.50 km/h`) with **zero** `200.0 RPM` spikes.
`invariant · kind: semantic · anchors: tmp/ftms_bleak_pedal_v21.json · source_doc: docs/measurements/phase3/C08-20261024-ftms-verification.md · evidence: doc:docs/measurements/phase3/C08-20261024-ftms-verification.md`
