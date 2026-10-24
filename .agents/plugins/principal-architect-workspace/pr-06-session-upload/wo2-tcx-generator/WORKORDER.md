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
spec_hash: b5c711263680aa199c9ef1170f50052aa5f470c934baf61dbc1d9ddf349f2389
completion: null
---

# WO-06.2: Streaming 1 Hz TCX.gz Generator (4 KB Scratch Buffer, Coasting & Pause Gaps)

## 1. Objective & Scope

Implement `TCXGenerator` (`src/echenon/TCXGenerator.h` / `.cpp`) to stream stored `RideRecord` arrays into both uncompressed Training Center Database v2 XML (`.tcx`) and gzip-compressed `.tcx.gz` files using a bounded `4 KB` formatting buffer (`SPEC.md` §9.4–§9.5).

## 2. Detailed Requirements

### 2.1 `TCXGenerator` Contract (`src/echenon/TCXGenerator.h` & `.cpp`)
1. **XML Schema & Structure (`SPEC.md` §9.4–§9.5):**
   - Root `<TrainingCenterDatabase xmlns="http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ns3="http://www.garmin.com/xmlschemas/ActivityExtension/v2">`.
   - `<Activities><Activity Sport="Biking">`.
   - `<Id>YYYY-MM-DDTHH:MM:SSZ</Id>` formatted in UTC from `start_epoch_s`.
   - `<Lap StartTime="YYYY-MM-DDTHH:MM:SSZ">` containing `<TotalTimeSeconds>`, `<DistanceMeters>` (integrated from virtual road speed $v(P)$ over each 1 s record!), `<Calories>` ($\text{total\_work\_kj}$, assuming $\sim 24\%$ human metabolic efficiency where $1\text{ kJ mechanical} \approx 1\text{ kcal}$), `<AverageHeartRateBpm>` / `<MaximumHeartRateBpm>` (omitted if no valid HR records in session), `<Cadence>`, `<Intensity>Active</Intensity>`, `<TriggerMethod>Manual</TriggerMethod>`, `<Track>`, and `<Extensions><ns3:LX><ns3:AvgWatts>`, `<ns3:MaxWatts>`.
   - `<Creator xsi:type="Device_t"><Name>Echenon EX-3</Name>...</Creator>` and `<Author xsi:type="Application_t"><Name>Echenon Firmware</Name>...</Author>`.
2. **1 Hz `<Trackpoint>` Streaming Rules:**
   - Each `RideRecord` emits a `<Trackpoint>` with timestamp `start_epoch_s + record.timestamp_offset_s`:
     - `<Time>YYYY-MM-DDTHH:MM:SSZ</Time>`
     - `<DistanceMeters>...</DistanceMeters>` (cumulative virtual distance in meters)
     - `<HeartRateBpm><Value>...</Value></HeartRateBpm>` **only** if `(record.flags & RIDE_FLAG_HR_VALID)` and `record.heart_rate_bpm > 0`.
     - `<Cadence>...</Cadence>`
     - `<Extensions><ns3:TPX><ns3:Speed>...</ns3:Speed><ns3:Watts>...</ns3:Watts></ns3:TPX></Extensions>` (speed in `m/s`, watts in integer `W`).
   - **Coasting (`cadence == 0` while `ACTIVE`):** `<Trackpoint>` IS emitted with `<Cadence>0</Cadence>` and `<ns3:Watts>0</ns3:Watts>`.
   - **Pause Gaps:** Because `SessionManager` does not record samples while `PAUSED` and increments `timestamp_offset_s` by elapsed real time on `resume()`, `<Trackpoint>` timestamps jump cleanly across the pause gap with zero trackpoints emitted during the paused interval.
3. **Bounded `4 KB` Streaming Output & `.tcx.gz` Compression:**
   - Provide a chunk-sink interface `using WriteChunkFn = std::function<bool(const uint8_t* data, size_t len)>;` so the generator can stream directly to an HTTP response, a LittleFS file, or a streaming gzip compressor (`tdefl_compressor` / `zlib` with standard 10-byte gzip header + CRC32 + ISIZE trailer) using at most a `4096-byte` scratch buffer.

### 2.2 Native Unit Tests (`test/test_native/test_tcx_generator.cpp`)
- Test XML generation for a synthetic ride containing normal pedalling, active coasting (`0 RPM`, `0 W`), a 60-second pause gap, and missing-HR intervals.
- Verify well-formed XML tags, ISO-8601 UTC timestamps, `<Creator><Name>Echenon EX-3</Name></Creator>`, presence of coasting trackpoints, absence of trackpoints during the pause gap, omission of `<HeartRateBpm>` when `hr_valid == 0`, and valid `.tcx.gz` gzip header/CRC32/decompression round-trip.

## 3. Acceptance Criteria
- `pio test -e native` compiles and passes all existing tests plus `test_tcx_generator.cpp`.

## Completion Report
*(To be populated by `pa-engineer` upon completion.)*
