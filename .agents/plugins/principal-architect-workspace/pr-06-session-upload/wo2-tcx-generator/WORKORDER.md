---
schema_version: 1
wo_id: WO-06.2
pr_id: PR-06
title: Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps)
complexity: standard
status: PENDING
allowed_files:
- src/echenon/TCXGenerator.h
- src/echenon/TCXGenerator.cpp
- test/test_native/test_tcx_generator.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: f4275a9a628ad1a4b0ac92037d4711558b6e987a18ee1337010f53caa9e3d5ab
completion: null
---

# WO-06.2: Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps)

## 1. Objective & Scope

Implement `TCXGenerator` (`src/echenon/TCXGenerator.h` / `.cpp`) to stream stored `RideRecord` arrays into both uncompressed Training Center Database v2 XML (`.tcx`) and gzip-compressed `.tcx.gz` files using a bounded `4 KB` formatting buffer, integrating virtual flat-road speed $v(P)$ from `PowerEngine::computeVirtualSpeedKph` (`PR-05`) into `<DistanceMeters>` and `<ns3:Speed>` and preserving 1 Hz coasting continuity for Xert MPA (`SPEC.md` §7.5 & §9.4–§9.5).

## 2. Detailed Requirements

### 2.1 `TCXGenerator` Contract (`src/echenon/TCXGenerator.h` & `.cpp`)
1. **XML Schema & Structure (`SPEC.md` §9.4–§9.5):**
   - Root `<TrainingCenterDatabase xmlns="http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ns3="http://www.garmin.com/xmlschemas/ActivityExtension/v2">`.
   - `<Activities><Activity Sport="Biking">`.
   - `<Id>YYYY-MM-DDTHH:MM:SSZ</Id>` formatted in UTC from `start_epoch_s`.
   - `<Lap StartTime="YYYY-MM-DDTHH:MM:SSZ">` containing `<TotalTimeSeconds>`, `<DistanceMeters>` (integrated over all records from `PowerEngine::computeVirtualSpeedKph(record.power_watts) / 3.6f * 1.0f`), `<Calories>` ($\text{total\_work\_kj}$, assuming $\sim 24\%$ human metabolic efficiency where $1\text{ kJ mechanical} \approx 1\text{ kcal}$), `<AverageHeartRateBpm>` / `<MaximumHeartRateBpm>` (omitted if no valid HR records in session), `<Cadence>`, `<Intensity>Active</Intensity>`, `<TriggerMethod>Manual</TriggerMethod>`, `<Track>`, and `<Extensions><ns3:LX><ns3:AvgWatts>`, `<ns3:MaxWatts>`.
   - `<Creator xsi:type="Device_t"><Name>Echenon EX-3</Name>...</Creator>` and `<Author xsi:type="Application_t"><Name>Echenon Firmware</Name>...</Author>`.
2. **1 Hz `<Trackpoint>` Streaming & Virtual Speed Integration (`SPEC.md` §7.5 & §9.4):**
   - Each `RideRecord` computes virtual flat-road speed $v_{\text{m/s}} = \text{PowerEngine::computeVirtualSpeedKph}(record.power\_watts) / 3.6\text{f}$ (`0.245 v^3 + 3.53 v = P`), accumulates `cumulative_distance_m += v_mps * 1.0f`, and emits a `<Trackpoint>` with timestamp `start_epoch_s + record.timestamp_offset_s`:
     - `<Time>YYYY-MM-DDTHH:MM:SSZ</Time>`
     - `<DistanceMeters>...</DistanceMeters>` (cumulative virtual distance in meters, formatted `%.2f`)
     - `<HeartRateBpm><Value>...</Value></HeartRateBpm>` **only** if `(record.flags & RIDE_FLAG_HR_VALID)` and `record.heart_rate_bpm > 0`.
     - `<Cadence>...</Cadence>`
     - `<Extensions><ns3:TPX><ns3:Speed>...</ns3:Speed><ns3:Watts>...</ns3:Watts></ns3:TPX></Extensions>` (speed `v_mps` in `m/s` formatted `%.2f`, watts in integer `W`).
   - **Coasting & Xert MPA 1 Hz Continuity (`cadence == 0` while `ACTIVE`, `SPEC.md` §9.4):** `<Trackpoint>` IS ALWAYS emitted with `<Cadence>0</Cadence>`, `<ns3:Speed>0.00</ns3:Speed>`, and `<ns3:Watts>0</ns3:Watts>` (zero timestamp gaps so Xert MPA differential equations never interpolate nonzero watts across a coasting recovery).
   - **Pause Gaps:** Because `SessionManager` does not record samples while `PAUSED` and increments `timestamp_offset_s` by elapsed real time on `resume()`, `<Trackpoint>` timestamps jump cleanly across the pause gap with zero trackpoints emitted during the paused interval.
3. **Bounded `4 KB` Streaming Output, `.tcx.gz` Compression & Deadline Guard:**
   - Provide a chunk-sink interface `using WriteChunkFn = std::function<bool(const uint8_t* data, size_t len)>;` and an optional `using DeadlineCheckFn = std::function<bool()>;` (returning `false` if the `DeadlineToken` has expired) so the generator can stream directly to an HTTP response, a LittleFS file, or a streaming gzip compressor (`tdefl_compressor` / `zlib` with standard 10-byte RFC 1952 gzip header + CRC32 + ISIZE trailer) using at most a `4096-byte` scratch buffer.

### 2.2 Native Unit Tests (`test/test_native/test_tcx_generator.cpp`)
- Test XML generation for a synthetic ride containing normal pedalling (`power_watts > 0`, verifying `<ns3:Speed>` matches `PowerEngine::computeVirtualSpeedKph(W)/3.6` and `<DistanceMeters>` accumulates monotonically), active coasting (`0 RPM`, `0 W`, `0.00 m/s`), a 60-second pause gap, and missing-HR intervals.
- Verify well-formed XML tags, ISO-8601 UTC timestamps, `<Creator><Name>Echenon EX-3</Name></Creator>`, presence of coasting trackpoints, absence of trackpoints during the pause gap, omission of `<HeartRateBpm>` when `hr_valid == 0`, deadline cancellation callback behavior, and valid `.tcx.gz` gzip header/CRC32/decompression round-trip.

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus `test_tcx_generator.cpp`.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
