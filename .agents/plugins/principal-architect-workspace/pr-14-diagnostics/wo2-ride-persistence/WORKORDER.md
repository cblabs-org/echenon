---
schema_version: 1
wo_id: WO-14.2
pr_id: PR-14
title: Run-Data Persistence (LittleFS + Gzip)
complexity: standard
status: COMPLETED
allowed_files:
- src/echenon/RideStore.h
- src/echenon/RideStore.cpp
- src/echenon/RidePersistence.h
- src/echenon/RidePersistence.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 3638d4e67dd3ffebc5906daf394551d65ec7947082602b883ad5ebbe4fa73483
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_d652d68e3680
  commit_sha: a37b3d2af5722b5a3f09c37fc7c54b43eb9ff204
  files_modified:
  - src/echenon/RidePersistence.cpp
  - src/echenon/RidePersistence.h
  - src/echenon/RideStore.cpp
  - src/echenon/RideStore.h
  verification_summary: 'pio test -e native --verbose: ================ 199 test cases:
    199 succeeded in 00:00:00.466 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-14.2 — Run-Data Persistence (LittleFS + Gzip)

## 1. Role & Objective

You are an expert senior embedded C++ engineer. Implement run-data persistence
that saves completed rides as `.ride.gz` (gzip-compressed binary) files to
LittleFS via the existing `IRideFileSystem` abstraction, with a configurable
storage cap and oldest-first eviction.

Read the `framework-expert` skill at
`/Users/mcneillm/Documents/Projects/Echenon/.agents/skills/framework-expert/SKILL.md`.
Relevant references:
- `interfaces/` for `IConfigurable` contract
- `systems/config/` for ConfigManager patterns
- `embedded-safety/` for flash wear rules
- `performance/` for PSRAM/memory considerations

## 2. Strict Boundary Box & Constraints

- **You may ONLY create/edit** the files listed in `allowed_files`.
- **DO NOT** touch `EchenonModule`, `SessionManager`, `main.cpp`, web files,
  test files, portal code, or `DiagnosticsModule`.
- **No heap `std::function`** (GRAVE-05).
- **Flash wear rules**:
  - Block-append writes only (no rewrite-in-place of existing data)
  - Checkpoint interval ≥ 30 s (reuse `RideStore::CHECKPOINT_INTERVAL_RECORDS`)
  - Oldest-first eviction (never circular rewrite of existing files)
- **Binary `.ride.gz` format**: `RideFileHeader` + `RideRecord[]` compressed
  with gzip (miniz). CSV conversion is NOT in this WO — it happens on-the-fly
  in WO-14.3.
- **PSRAM budget**: No new PSRAM allocation. Gzip compression uses stack/heap
  scratch buffers ≤ 4 KB.

## 3. Line-Item Instructions

### 3.1 RidePersistence Class

- [ ] Create `src/echenon/RidePersistence.h` / `.cpp` — a new helper class
  that manages persistent ride storage:

  ```cpp
  namespace echenon {
  class RidePersistence {
  public:
      static constexpr uint32_t DEFAULT_CAP_KB = 512;
      static constexpr uint32_t MIN_CAP_KB     = 64;
      static constexpr uint32_t MAX_CAP_KB     = 2048;
      static constexpr const char* PERSIST_DIR  = "/rides/persist";

      explicit RidePersistence(IRideFileSystem& fs);

      // --- Configuration ---
      void setEnabled(bool enabled);
      bool isEnabled() const;
      void setCapKb(uint32_t cap_kb);
      uint32_t getCapKb() const;

      // --- Persistence Operations ---
      bool saveRide(uint32_t session_id, const RideFileHeader& header,
                    const RideRecord* records, uint16_t record_count);
      bool deleteRide(uint32_t session_id);
      bool loadRide(uint32_t session_id,
                    RideFileHeader& out_header,
                    std::vector<RideRecord>& out_records) const;

      // --- Stored Ride Management ---
      struct StoredRideInfo {
          uint32_t session_id;
          uint32_t start_epoch_s;
          uint32_t file_size_bytes;
          uint16_t record_count;
      };
      const std::vector<StoredRideInfo>& getStoredRides() const;
      size_t getStoredCount() const;
      uint32_t getTotalSizeBytes() const;

      // --- Lifecycle ---
      void begin();   // Scans PERSIST_DIR for existing .ride.gz files
      void clear();   // Deletes all stored rides

  private:
      bool compressAndWrite(const char* path, const uint8_t* data, size_t len);
      bool readAndDecompress(const char* path,
                             std::vector<uint8_t>& out) const;
      void evictOldest();
      void scanStoredRides();
      static std::string ridePath(uint32_t session_id);

      IRideFileSystem& _fs;
      bool             _enabled = false;
      uint32_t         _capKb   = DEFAULT_CAP_KB;
      std::vector<StoredRideInfo> _storedRides;
  };
  } // namespace echenon
  ```

### 3.2 Gzip Compression

- [ ] Use `miniz` (already available in ESP-IDF / PlatformIO native) for gzip
  compression:
  - `saveRide()` serializes `RideFileHeader` + `RideRecord[]` into a
    contiguous byte buffer, then compresses with `mz_compress2()` or streaming
    `mz_deflateInit2()` with `MZ_DEFAULT_WINDOW_BITS | 16` (gzip wrapper).
  - Compression scratch buffer: ≤ 4 KB stack allocation for the deflate
    output chunk. Use streaming deflation to avoid allocating the full
    uncompressed buffer.
  - `loadRide()` reads the `.ride.gz` file, decompresses with
    `mz_uncompress()` or streaming `mz_inflateInit2()` with gzip flag, and
    parses `RideFileHeader` + `RideRecord[]`.

- [ ] For native tests (`UNIT_TEST`), provide a miniz stub or use the real
  miniz header-only library. The `#include "miniz.h"` path should work for
  both ESP32 and native builds. If miniz is not available in native, use
  conditional compilation:
  - Under `UNIT_TEST`: store uncompressed (just write raw bytes to the
    in-memory filesystem) and document the deviation.
  - Under ESP32: use proper gzip compression.

### 3.3 Oldest-First Eviction

- [ ] Before writing a new `.ride.gz` file, check if
  `getTotalSizeBytes() + estimated_new_size > _capKb * 1024`. If so, delete
  the oldest stored ride (by `start_epoch_s`) until sufficient space is
  available.
- [ ] The eviction log should be traceable: `ESP_LOGW` (or equivalent) when
  a ride is evicted.

### 3.4 File Naming & Directory Structure

- [ ] Stored ride files: `/rides/persist/ride_<session_id>.ride.gz`
- [ ] On `begin()`, scan the persist directory and rebuild the
  `_storedRides` index from file headers (read first 16 bytes of each
  decompressed file to extract `RideFileHeader`).
- [ ] Ensure `PERSIST_DIR` is created via `_fs.mkdir()` if it doesn't exist.

### 3.5 Integration Hook (No Wiring Yet)

- [ ] The `RidePersistence` class is standalone. It does NOT hook into
  `SessionManager` or `EchenonModule` in this WO. The wiring happens in
  WO-14.4 / WO-14.3 when the REST API and integration code are built.
- [ ] The class uses `IRideFileSystem&` as its filesystem abstraction, making
  it testable with `InMemoryRideFileSystem`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

- Run `pio test -e native --verbose` — all existing tests must pass.
- After verification, run `pa wo verify WO-14.2`.
- Surface any concerns in the Completion Report.

## Completion Report
<!-- Written by the Tier 3 Worker at task completion -->

**Status:** COMPLETED
**Receipt ID:** rcpt_d652d68e3680
**Commit SHA:** 099d86bee7c4
**Tree Hash:** d652d68e3680f862b2bad8bfbc70efbef9ef1dca08147e0f4cc331ffb5c710e0

### Files Modified
- `src/echenon/RidePersistence.h` — New header (class declaration per WO spec)
- `src/echenon/RidePersistence.cpp` — Full implementation (compress/decompress, save/load/delete, eviction, scanning)
- `src/echenon/RideStore.h` — Added `listDir()` to `IRideFileSystem` and `InMemoryRideFileSystem`
- `src/echenon/RideStore.cpp` — Implemented `listDir()` for `InMemoryRideFileSystem` and `LittleFSRideFileSystem`

### Decisions Made
- **Added `listDir()` to `IRideFileSystem`**: The WO requires `begin()` to scan `PERSIST_DIR` for existing `.ride.gz` files. The existing interface had no directory listing capability. Added a minimal `listDir(dirPath, out)` method that returns filenames (not full paths) of direct children. Implemented for both `InMemoryRideFileSystem` (prefix-matching on internal `std::map` keys) and `LittleFSRideFileSystem` (Arduino-style `openNextFile()` iteration).
- **Conditional compression**: ESP32 builds use `miniz` (`mz_compress2`/`mz_uncompress`). Native/UNIT_TEST builds store raw uncompressed bytes. The `.ride.gz` extension is used in both cases for API consistency. This follows the WO § 3.2 conditional compilation directive.
- **No `std::function` used**: All callbacks avoided per GRAVE-05. Lambdas used only as arguments to standard algorithms (`std::sort`, `std::find_if`) which decay to stateless inline code.

### Verification Summary
- `pio test -e native --verbose` → 199/199 tests passed (0 failures)
- No existing test was modified or broken
- Class is fully standalone — no wiring into SessionManager, EchenonModule, or any other module
