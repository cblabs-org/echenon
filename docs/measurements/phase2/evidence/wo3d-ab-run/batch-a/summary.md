# BLE Test Batch Summary: batch-a

## Environment & Configuration
- **Date / Time:** 2026-10-15 13:00–17:33 BST (run in two halves: 29 + 21 attempts)
- **Batch ID:** `batch-a`
- **Firmware:** Echenon (with WiFi active)
- **Target Peripheral:** `3C:0F:02:DE:72:26`
- **Test Client:** Google Pixel 10 Pro Fold (`ro.build.display.id`: `CP3A.261005.005`, Android 17)
- **Application:** nRF Connect for Android (`no.nordicsemi.android.mcp`)
- **Hold Duration:** 30.0s
- **Attempt Spacing:** 30.0s
- **Bug Reports Captured:** 4 (2 per half-run, max 2 each)

## Results
| Metric | Value |
| :--- | :--- |
| **Total Attempts** | 50 |
| **Passes** | 13 (26.0%) |
| **Failures** | 37 (74.0%) |
| **Longest Consecutive Failures** | 15 |
| **Connection Drops (during 30s hold)** | 0 |
| **PASS latency** | min=622ms avg=961ms max=1488ms |

## Notes
- Run was split due to user needing phone access. First half (29 attempts) and second half (21 attempts) were merged with renumbered `attempt_num`. All 50 attempts use the same firmware, same phone, same protocol.
- All failures were `android_status=147` (HCI error 0x93 / Connection Failed to be Established).

## Evidence Files
- Attempts log: [`attempts.csv`](attempts.csv)
- ESP32 serial (first half): [`esp32_serial_batch-a.log`](esp32_serial_batch-a.log)
- ESP32 serial (second half): [`esp32_serial_batch-a-cont.log`](esp32_serial_batch-a-cont.log)
- ESP32 events (first half): [`esp32_events_batch-a.csv`](esp32_events_batch-a.csv)
- ESP32 events (second half): [`esp32_events_batch-a-cont.csv`](esp32_events_batch-a-cont.csv)
