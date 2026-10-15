# BLE Test Batch Summary: batch-a-prime

## Environment & Configuration
- **Date / Time:** 2026-10-15 19:16:23 BST
- **Batch ID:** `batch-a-prime`
- **Target Peripheral:** `3C:0F:02:DE:72:26`
- **Test Client:** Google Pixel 10 Pro Fold (`ro.build.display.id`: `CP3A.261005.005`, Android 17)
- **Application:** nRF Connect for Android (`no.nordicsemi.android.mcp`)
- **Hold Duration:** 30.0s
- **Attempt Spacing:** 30.0s
- **Bug Reports Captured:** 2 (max 2)

### Bonded Devices Summary
```
Bonded devices: 12
XX:XX:XX:XX:01:04(Public) => XX:XX:XX:XX:01:04(Public) [ DUAL ] [0x760408] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] BMW 31613
[BR/EDR UUIDs]: 00001101-0000-1000-8000-00805f9b34fb 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 00001133-0000-1000-8000-00805f9b34fb 4de17a00-52cb-11e6-bdf4-0800200c9a66
XX:XX:XX:XX:98:9F(Public) => XX:XX:XX:XX:98:9F(Public) [ DUAL ] [0x244404] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] Matt's Pixel Buds Pro 2
[BR/EDR UUIDs]: 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 00001124-0000-1000-8000-00805f9b34fb 25e97ff7-24ce-4c4c-8951-f764a708f7b5 81c2e72a-0591-443e-a1ff-05f988593351 df21fe2c-2515-4fdb-8886-f12c4d67927c e7ab2241-ca64-4a69-ac02-05f5c6fe2d62 f8d1fbe4-7966-4334-8024-ff96c9330e15
[LE UUIDs    ]: 0000180f-0000-1000-8000-00805f9b34fb 00001844-0000-1000-8000-00805f9b34fb 0000184e-0000-1000-8000-00805f9b34fb 0000184f-0000-1000-8000-00805f9b34fb 109b862f-50e3-45cc-8ea1-ac62de4846d1
[Packages    ]: [com.google.android.gms, com.google.android.bluetooth]
XX:XX:XX:XX:75:D8(Public) => XX:XX:XX:XX:75:D8(Public) [ DUAL ] [0x2A0704] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:Y LE:N] [ Encryption status(BR/EDR):EncryptionStatus{keySize=16, algorithm=2] LE:N/A] Google Pixel Watch 2 7DBQ
[BR/EDR UUIDs]: 0000110a-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 0000111f-0000-1000-8000-00805f9b34fb 5e8945b0-9525-11e3-a5e2-0800200c9a66 df21fe2c-2515-4fdb-8886-f12c4d67927c
XX:XX:XX:XX:03:03(Public) => XX:XX:XX:XX:03:03(Public) [BR/EDR] [0x800100] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] (BT)Edge 1030
[BR/EDR UUIDs]: 00000000-deca-fade-deca-deafdecacaff 00001101-0000-1000-8000-00805f9b34fb deab91e4-670f-11e1-9ace-30bc4824019b
XX:XX:XX:XX:81:C6(Public) => XX:XX:XX:XX:81:C6(Public) [ DUAL ] [0x001F00] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] (BLE)Edge 1030
[Packages    ]: [com.garmin.android.apps.connectmobile]
XX:XX:XX:XX:43:29(Public) => XX:XX:XX:XX:43:29(Public) [BR/EDR] [0x240408] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] MVH-X580DAB
[BR/EDR UUIDs]: 00001101-0000-1000-8000-00805f9b34fb 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb
[Packages    ]: [com.google.android.googlequicksearchbox]
XX:XX:XX:XX:B7:79(Public) => XX:XX:XX:XX:B7:79(Public) [BR/EDR] [0x740420] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] My VW 6776
[BR/EDR UUIDs]: 00000000-deca-fade-deca-deafdecacaff 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 00001133-0000-1000-8000-00805f9b34fb 4de17a00-52cb-11e6-bdf4-0800200c9a66
XX:XX:XX:XX:00:8C(Public) => XX:XX:XX:XX:00:8C(Public) [ DUAL ] [0x240418] [Pairing Algorithm BR/EDR:N/A LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] Bose NC 700 HP
[BR/EDR UUIDs]: 00000000-deca-fade-deca-deafdecacaff 00001101-0000-1000-8000-00805f9b34fb 00001108-0000-1000-8000-00805f9b34fb 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 81c2e72a-0591-443e-a1ff-05f988593351 85dbf2f9-73e3-43f5-a129-971b91c72f1e 931c7e8a-540f-4686-b798-e8df0a2ad9f7 9b26d8c0-a8ed-440b-95b0-c4714a518bcc f8d1fbe4-7966-4334-8024-ff96c9330e15
XX:XX:XX:XX:40:59(Public) => XX:XX:XX:XX:40:59(Public) [BR/EDR] [0x240404] [Pairing Algorithm BR/EDR:BREDR_SSP LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] Jabra SPEAK 510 USB
[BR/EDR UUIDs]: 00001108-0000-1000-8000-00805f9b34fb 0000110b-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 0000111f-0000-1000-8000-00805f9b34fb
XX:XX:XX:XX:E1:9E(Public) => XX:XX:XX:XX:E1:9E(Public) [ DUAL ] [0x100680] [Pairing Algorithm BR/EDR:BREDR_SSP LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] D30
[BR/EDR UUIDs]: 00001101-0000-1000-8000-00805f9b34fb
XX:XX:XX:XX:FA:04(Public) => XX:XX:XX:XX:FA:04(Public) [BR/EDR] [0x740420] [Pairing Algorithm BR/EDR:SC LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] My VW 0196
[BR/EDR UUIDs]: 00000000-deca-fade-deca-deafdecacaff 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 0000111e-0000-1000-8000-00805f9b34fb 00001133-0000-1000-8000-00805f9b34fb 4de17a00-52cb-11e6-bdf4-0800200c9a66
XX:XX:XX:XX:11:FE(Public) => XX:XX:XX:XX:11:FE(Public) [ DUAL ] [0x240414] [Pairing Algorithm BR/EDR:BREDR_SSP LE:N/A] [ACL BR/EDR:N LE:N] [ Encryption status(BR/EDR):N/A LE:N/A] JBL Flip 7
[BR/EDR UUIDs]: 0000110b-0000-1000-8000-00805f9b34fb 0000110e-0000-1000-8000-00805f9b34fb 65786365-6c70-6f69-6e74-2e04ff072101 df21fe2c-2515-4fdb-8886-f12c4d67927c
```

## Results
| Metric | Value |
| :--- | :--- |
| **Total Attempts** | 50 |
| **Passes** | 14 (28.0%) |
| **Failures** | 36 (72.0%) |
| **Longest Consecutive Failures** | 9 |
| **Connection Drops (during 30s hold)** | 0 |

## Evidence Files
- Attempts log: [`attempts.csv`](attempts.csv)
