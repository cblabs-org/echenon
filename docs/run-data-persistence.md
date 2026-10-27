# Run-Data Persistence — Developer Guide

> **SPEC reference:** [SPEC.md §14](../SPEC.md) (Run-Data Persistence)
>
> **Source files:**
> [`RidePersistence.h`](../src/echenon/RidePersistence.h),
> [`RidePersistence.cpp`](../src/echenon/RidePersistence.cpp),
> [`RideStore.h`](../src/echenon/RideStore.h) (shared types)

---

## Overview

`RidePersistence` provides optional persistent storage of completed rides to
LittleFS. When enabled via configuration, it saves each completed ride as a
gzip-compressed binary file and manages storage within a configurable capacity
budget with oldest-first eviction.

This is distinct from `RideStore`, which manages the **active session** PSRAM
buffer, crash-recovery checkpoints, and a 5-ride completed history ring.
`RidePersistence` is a long-term archive that survives reboots and power
cycles.

---

## How to Enable Ride Persistence

### Via Configuration

Set `save_rides_to_flash` to `true` in the `echenon` configuration namespace:

```json
{
  "echenon": {
    "save_rides_to_flash": true,
    "diag_ride_cap_kb": 512
  }
}
```

### Via the Portal

Navigate to the portal and enable the ride persistence toggle in the
diagnostics settings. The storage cap can be adjusted between 64 KB and
2048 KB (2 MB).

### Via the RidePersistence API

```cpp
RidePersistence persistence(fs);
persistence.setEnabled(true);
persistence.setCapKb(512);   // 512 KB budget (default)
persistence.begin();         // scans /rides/persist/ for existing files
```

---

## Storage Budget and Eviction Behaviour

### Budget Calculation

At 1 Hz sampling (8 bytes per `RideRecord`), typical ride sizes are:

| Ride Duration | Records | Uncompressed | Compressed (~3:1) |
|---|---|---|---|
| 30 min | 1,800 | 14.4 KB | ~5 KB |
| 60 min | 3,600 | 28.8 KB | ~10 KB |
| 90 min | 5,400 | 43.2 KB | ~15 KB |
| 120 min | 7,200 | 57.6 KB | ~20 KB |

With the default 512 KB cap, approximately 25–50 rides of typical length
can be stored.

### Eviction Policy

When `saveRide()` would exceed `diag_ride_cap_kb`:

1. The oldest stored ride (by `start_epoch_s`) is deleted.
2. The check repeats until sufficient space is available.
3. The new ride is then written.

This is a simple FIFO policy that prioritises recent rides. Rides can also
be deleted explicitly via the REST API (`DELETE /api/echenon/runs/<id>`) or
programmatically via `persistence.deleteRide(session_id)`.

### Configuration Constraints

| Setting | Type | Default | Min | Max |
|---|---|---|---|---|
| `save_rides_to_flash` | `bool` | `false` | — | — |
| `diag_ride_cap_kb` | `uint32_t` | `512` | `64` | `2048` |

Setting `diag_ride_cap_kb` below `MIN_CAP_KB` (64) or above `MAX_CAP_KB`
(2048) is clamped to the nearest valid value.

---

## How to Download Rides via API

All endpoints require authentication (SPEC §11.4).

### List Stored Rides

```bash
curl -u admin:password http://echenon.local/api/echenon/runs
```

**Response:**

```json
{
  "runs": [
    {
      "session_id": 12345,
      "start_epoch": 1698400000,
      "file_size_bytes": 4096,
      "record_count": 512
    },
    {
      "session_id": 12346,
      "start_epoch": 1698486400,
      "file_size_bytes": 8192,
      "record_count": 1024
    }
  ],
  "count": 2,
  "total_size_kb": 12
}
```

### Download a Ride as CSV

```bash
curl -u admin:password \
  http://echenon.local/api/echenon/runs/12345.csv.gz \
  -o ride_12345.csv
```

The response is streamed as CSV (chunked transfer encoding, ≤ 4 KB chunks)
with a `Content-Disposition` header for automatic download.

### Delete a Ride

```bash
curl -u admin:password -X DELETE \
  http://echenon.local/api/echenon/runs/12345
```

**Response:** `{"ok": true}` on success; `404` if not found.

---

## CSV Format Specification

The CSV download contains one row per `RideRecord` (1 Hz sampling):

```csv
elapsed_s,power_w,cadence_rpm,hr_bpm,resistance,flags
0,0,0,0,127,2
1,45,62,0,127,2
2,85,68,72,128,3
3,120,75,76,130,3
...
```

### Column Reference

| Column | Type | Description |
|---|---|---|
| `elapsed_s` | `uint16` | Seconds since session start (0–65535) |
| `power_w` | `uint16` | 1 Hz averaged power in watts (0–2000) |
| `cadence_rpm` | `uint8` | 1 Hz averaged cadence in RPM (0–255) |
| `hr_bpm` | `uint8` | Heart rate in BPM (0 if unavailable/stale) |
| `resistance` | `uint8` | Canonical internal resistance `i ∈ 0..254` (`round(254 × p̃)`); `255` = uncalibrated |
| `flags` | `uint8` | Bit 0: `hr_valid`, Bit 1: `resistance_calibrated`, Bit 2: `paused_marker` |

### Reconstructing External Scales from `resistance`

The `resistance` column stores the canonical internal resistance value `i`.
To reconstruct external scales:

```
Normalized position:   p̃ = i / 254.0          (0.0 to 1.0)
Echelon level:         L = round(i × 31 / 254) + 1   (1 to 32)
```

For the Peloton 0–100 scale, use the power-equivalence mapping from SPEC §5.3.

---

## Binary File Format

The `.ride.gz` file format is a simple concatenation of a fixed-size header
followed by packed records:

```
┌─────────────────────────────────────────────┐
│  RideFileHeader (16 bytes)                  │
│  ┌──────────────┬──────────┬──────────────┐ │
│  │ magic (4B)   │ ver (2B) │ flags (2B)   │ │
│  │ 0x45434852   │ 0x0001   │ NTP|FINAL    │ │
│  ├──────────────┼──────────┴──────────────┤ │
│  │ session_id   │ start_epoch_s           │ │
│  │ (4B)         │ (4B)                    │ │
│  └──────────────┴─────────────────────────┘ │
├─────────────────────────────────────────────┤
│  RideRecord[0] (8 bytes)                    │
│  RideRecord[1] (8 bytes)                    │
│  ...                                        │
│  RideRecord[N-1] (8 bytes)                  │
└─────────────────────────────────────────────┘
```

### Header Fields

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 4 | `magic` | `0x45434852` ("ECHR") |
| 4 | 2 | `version` | File format version (`1`) |
| 6 | 2 | `flags` | Bit 0: NTP valid; Bit 1: finalized |
| 8 | 4 | `session_id` | Unique session identifier |
| 12 | 4 | `start_epoch_s` | UTC epoch of session start |

### Compression

- **ESP32:** Files are gzip-compressed before writing to LittleFS.
- **Native tests:** Files are stored uncompressed (gzip is not available in
  the `UNIT_TEST` environment).

---

## Integration with the Session Pipeline

`RidePersistence` is a standalone class that receives completed ride data
after session finalization. The integration flow is:

```
SessionManager → session ends → RideStore.finalizeSession()
                                      │
                                      ├── RideStore saves to /rides/ (FIFO history)
                                      │
                                      └── If save_rides_to_flash:
                                          RidePersistence.saveRide(
                                              session_id,
                                              header,
                                              records,
                                              record_count
                                          )
                                          │
                                          ├── Evict oldest if over cap
                                          └── Write to /rides/persist/ride_<id>.ride.gz
```

### File System Separation

| Path | Owner | Purpose |
|---|---|---|
| `/ride_active.bin` | `RideStore` | Active session crash-recovery checkpoint |
| `/rides/` | `RideStore` | Completed ride FIFO (5-ride history) |
| `/rides/index.bin` | `RideStore` | Stored ride index |
| `/rides/persist/` | `RidePersistence` | Long-term compressed ride archive |

The two subsystems use different directories and do not interfere with each
other.

---

## Flash Wear Considerations

LittleFS has a built-in wear-levelling layer, but write patterns still matter
for longevity:

- **Write-once pattern.** Ride files are written in a single sequential pass
  (`compressAndWrite()`), never updated in place.
- **No frequent small writes.** Unlike the active checkpoint (which writes
  every 30 s during a ride), persistence writes happen only once per
  completed session.
- **Eviction is rare.** With the default 512 KB cap and typical 5–10 KB
  rides, eviction happens only after dozens of rides.
- **Conservative estimate.** A 16 MB flash chip rated for 100,000 erase
  cycles with wear levelling can sustain millions of persistence writes
  before wear becomes a concern.

---

## Testing

`RidePersistence` is tested via the `InMemoryRideFileSystem` abstraction in
the native `env:native` googletest suite. The test file covers:

- Save and load round-trip (header + records)
- Delete by session ID
- Cap enforcement with oldest-first eviction
- `begin()` directory scan to rebuild the inventory
- Edge cases: empty persistence, single ride, cap exactly reached
- Total size tracking across multiple rides
- Gzip compression round-trip (ESP32 target only)
