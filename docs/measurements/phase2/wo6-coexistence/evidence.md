# WO6 — P12 v1 Topology Coexistence: Evidence

> **Test:** SPEC §13.5 test 2.7 — Radio coexistence (§8.5)
> **Date:** 2026-10-17 22:58–23:04 BST
> **Firmware:** `v0.1.0-dev.15` (build serial `Oct 17 2026 18:48:19`)
> **Framework:** submodule at `1e90ec68` (PR #296 — MAC byte-order fix)
> **Branch:** `hr-relay/wo6-coexistence` at `728fcfb`
> **Tester:** Owner (interactive session with agent)

## Result: PASS

**No HR gap > 2.5 s.** Maximum gap was **1,200 ms** across 326 notifications
over 325.3 s (5 min 25 s). The v1 BLE topology is proven on the P12.

## Topology Under Test

```
Beat HW901 ──BLE central──▶ ESP32-S3 ──BLE peripheral──▶ P12 Echelon Fit
(E7:A7:DC:1C:4C:AD)         (Echenon-7226)               (58:65:DF:78:F7:84)
                                │
                            WiFi STA (<REDACTED_SSID>)
                                │
                    ┌───────────┴───────────┐
                    │                       │
              Portal (WebSocket          3 × HTTP stress
               open, ~2 Hz WS)          bursts (10 req each)
```

## Equipment

| Item | Details |
|---|---|
| **MCU** | ESP32-S3 Waveshare Nano (2 cores, rev 2, 16 MB flash) |
| **Beat armband** | Echelon Beat HW901, `E7:A7:DC:1C:4C:AD` (RandomStatic) |
| **Tablet** | Lenovo Tab P12, Echelon Fit app |
| **P12 BLE params** | Interval 48.75 ms, latency 0, supervision 5000 ms, MTU 185 |
| **WiFi** | SSID `<REDACTED_SSID>`, IP `192.168.1.100`, RSSI -54 to -60 dBm |

## Timeline

| Time (BST) | Uptime (s) | Event |
|---|---|---|
| 22:52 | ~0 | Device powered on (or prior) |
| 22:57 | ~280 | Beat HW901 connected (already paired from prior session) |
| 22:58:18 | ~372 | P12 connected via Echelon Fit; portal opened in browser |
| 22:58:25 | ~378 | **Topology verified:** central + peripheral + portal all active |
| 22:58:25 | ~378 | **Pre-test baseline recorded** |
| 22:58:39 | ~393 | **Stress burst 1:** 10/10 HTTP 200 OK (22:58:39–22:58:41) |
| 23:00:26 | ~500 | **Stress burst 2:** 10/10 HTTP 200 OK (23:00:26–23:00:28) |
| 23:02:26 | ~620 | **Stress burst 3:** 10/10 HTTP 200 OK (23:02:26–23:02:27) |
| 23:03:47 | ~699 | **Post-test snapshot recorded** |

## HR Gap Analysis (Pass Criterion: no gap > 2.5 s)

**Source:** Serial log `att_handle=16` notifications (GATT notify to P12).
**Test window:** Uptime 372,000 ms → 699,000 ms (327 s, >5 min).

| Metric | Value |
|---|---|
| Total notifications in window | 326 |
| Duration covered | 325.3 s |
| **Maximum gap** | **1,200 ms** |
| Minimum gap | 825 ms |
| Average gap | 1,001 ms |
| Gaps > 2,500 ms | **0** |

### Gap Distribution

| Bucket | Count |
|---|---|
| < 900 ms | 4 |
| 900–1,100 ms | 295 |
| 1,100–1,500 ms | 26 |
| 1,500–2,000 ms | 0 |
| 2,000–2,500 ms | 0 |
| > 2,500 ms | **0** |

90.8% of gaps fall within ±100 ms of the expected 1 Hz cadence. The 26 gaps
in the 1,100–1,500 ms bucket and 4 gaps < 900 ms are normal BLE scheduling
jitter — well within the 2.5 s criterion.

## Heap Trajectory

| Metric | Pre-test (uptime 372 s) | Post-test (uptime 699 s) | Delta |
|---|---|---|---|
| `heap_free` | 124,511 B | 123,799 B | **-712 B** |
| `heap_min_free` | 86,580 B | 81,080 B | **-5,500 B** |
| `heap_max_alloc` | 49,152 B | 49,152 B | 0 |
| `psram_free` | 8,385,692 B | 8,385,692 B | 0 |

- `heap_free` delta of 712 B is negligible (within normal fluctuation).
- `heap_min_free` dropped 5,500 B — this reflects the transient DRAM demand
  during the 3 HTTP stress bursts (each burst is 10 concurrent-ish HTTP
  requests competing with BLE). The final `heap_min_free` of 81,080 B is
  still well above the 20,480 B DRAM safety threshold (RFC 6).
- **No memory leak detected.** `heap_free` is essentially flat; the
  `heap_min_free` drop is from transient HTTP allocation peaks, not
  accumulating leaks.

## Connection Stability

| Connection | Survived full test? |
|---|---|
| Beat HW901 (central, `hr_client`) | ✅ Yes — connected throughout |
| P12 Echelon Fit (peripheral, handle 1) | ✅ Yes — connected throughout |
| Portal WebSocket | ✅ Yes — kept alive with periodic interaction |
| WiFi STA (`<REDACTED_SSID>`) | ✅ Yes — all 30 HTTP requests succeeded |

## BLE Counters (Post-test)

| Counter | Value |
|---|---|
| `droppedEvents` | **0** |
| `scanInhibitCount` | **0** |
| BLE faults | **none** |

## HTTP Stress Results

| Burst | Time | Requests | Result |
|---|---|---|---|
| 1 | 22:58:39–22:58:41 | 10 | 10/10 OK |
| 2 | 23:00:26–23:00:28 | 10 | 10/10 OK |
| 3 | 23:02:26–23:02:27 | 10 | 10/10 OK |
| **Total** | — | **30** | **30/30 OK** |

## Additional Stress: WiFi Network Scan

The owner also triggered a **WiFi network scan** from the portal during the
final section of the test window. A WiFi scan forces the radio to hop across
all 2.4 GHz channels, which is one of the most demanding coexistence
scenarios for BLE — the ESP-IDF coexistence arbiter must interleave BLE
connection events with active WiFi channel probing. The HR relay continued
uninterrupted through this additional stress.

## Gate Decision

### **PASS** — PR-03 programme gate cleared.

The v1 BLE topology (Beat HW901 central + P12 peripheral + WiFi portal +
HTTP stress) sustained for >5 minutes with:

- **Zero HR gaps > 2.5 s** (max 1.2 s, criterion ≤ 2.5 s)
- **Zero dropped events**
- **Zero connection drops** (all 3 paths survived)
- **Stable heap** (no leak, min 81 KB >> 20 KB threshold)
- **30/30 HTTP requests successful** during BLE coexistence

## Observations

- The `secureConnection: failed rc=1285` error during Beat connection setup
  is expected — the Beat HW901 does not support bonding/encryption. No
  impact on HR notifications.
- The P12's connection parameters (48.75 ms interval, 5000 ms supervision
  timeout) are identical to WO3 and WO5 — consistent behavior.
- PSRAM usage is flat at 2,916 B used — no PSRAM pressure under coexistence load.
