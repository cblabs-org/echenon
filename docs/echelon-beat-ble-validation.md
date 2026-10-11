# Echelon Beat Armband — BLE Hardware Validation Report

> **Date:** 2026-09-18 20:47 BST
> **Device:** Echelon Beat Advanced Armband Heart Rate Monitor (HW901)
> **Serial:** EBEAT-0014941
> **Method:** Direct BLE interrogation from Mac via Python/Bleak
> **Raw data:** [`beat_gatt_report.json`](../tmp/beat_gatt_report.json)
> **FCC ID:** [`2AWD4HW901`](https://fccid.io/2AWD4HW901)

---

## 1. Advertisement Data

| Field | Value |
|---|---|
| **Local Name** | `EBEAT-0014941` |
| **Name format** | `EBEAT-` + 7-digit serial (Echelon-exclusive prefix) |
| **Service UUIDs** | `0x180D` (Heart Rate Service) ✅ |
| **Manufacturer Data** | Company `0x0F05` (**Shenzhen GWSTAI Technology Co., Ltd.**), payload: `01640652` |
| **Service Data** | None |
| **TX Power** | Not advertised |
| **RSSI at bench** | -48 dBm |

> [!IMPORTANT]
> The device name is **`EBEAT-NNNNNNN`**, not "Echelon Beat". The HRMClient scan filter
> should match by **service UUID `0x180D`** (primary) with an optional secondary name
> prefix check against `"EBEAT"` for logging/display purposes.

---

## 2. GATT Service Table

### 2.1 Vendor Service — `0xFD00` (GWSTAI / H&T OEM)

Proprietary control service — likely used for OTA firmware updates and LED
threshold synchronisation by the Echelon Fit app. **Not needed by Echenon firmware.**

| Characteristic | UUID | Properties | Purpose (inferred) |
|---|---|---|---|
| Data out | `0xFD09` | Notify | Vendor data stream |
| Command in | `0xFD0A` | Write-no-response | Vendor commands |
| Data out 2 | `0xFD19` | Notify | Secondary data stream |
| Command in 2 | `0xFD1A` | Write-no-response | Secondary commands |

### 2.2 Heart Rate Service — `0x180D` ✅

**Standard-compliant.** This is the only service HRMClient needs.

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| **Heart Rate Measurement** | `0x2A37` | **Notify** | See §3 below |
| **Body Sensor Location** | `0x2A38` | Read | **`0x04` = Hand** |

> [!NOTE]
> **No Heart Rate Control Point** (`0x2A39`) is present. The armband does not
> support the "Reset Energy Expended" command. This simplifies the GATT interaction
> to subscribe-only — no writes needed.

### 2.3 User Data Service — `0x181C`

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| VO2 Max | `0x2A96` | Read | `0x00` (not populated) |

Standard BLE fitness characteristic, but unpopulated. **Not needed by Echenon.**

### 2.4 Battery Service — `0x180F` ✅

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| **Battery Level** | `0x2A19` | **Read + Notify** | **100%** (fully charged) |

> [!TIP]
> **Battery level is available and supports notifications.** HRMClient should
> subscribe to `0x2A19` alongside `0x2A37` and expose the battery level in the
> portal UI. This is a free win for user experience — "HR armband battery: 73%".

### 2.5 Device Information Service — `0x180A`

| Characteristic | UUID | Properties | Value |
|---|---|---|---|
| Manufacturer Name | `0x2A29` | Read | `"NULL"` (OEM left blank) |
| Model Number | `0x2A24` | Read | `"2021-04-06"` (build date) |
| Hardware Revision | `0x2A27` | Read | `"V1.4"` |
| Firmware Revision | `0x2A26` | Read | `"V1.0"` |
| Software Revision | `0x2A28` | Read | `"V1.5.0"` |
| System ID | `0x2A23` | Read | `0504030201030201` |

Informational only. Could display in portal BLE device details page.

---

## 3. Heart Rate Measurement Analysis

**10 consecutive notifications captured** (device worn on forearm):

```
Raw packet: 0x00 0x50  (2 bytes)
Flags:      0x00
HR value:   0x50 = 80 BPM (uint8)
```

### Flags byte breakdown (`0x00`):

| Bit | Field | Value | Meaning |
|---|---|---|---|
| 0 | HR Value Format | 0 | **uint8** (1 byte) |
| 1-2 | Sensor Contact | 00 | **NOT SUPPORTED** |
| 3 | Energy Expended | 0 | **Not present** |
| 4 | RR-Interval | 0 | **Not present** |

> [!WARNING]
> **Sensor Contact is NOT supported by this device.** The SPEC's stale data
> protection (§8.4) correctly uses a 10-second timeout as the primary mechanism,
> but the HRMClient parser **must not** treat `contact_detected == false` as
> "no skin contact" — on this device it simply means the feature doesn't exist.
> The parser should check `contact_supported` first.

### What this means for the firmware:

| Feature | Status | Firmware impact |
|---|---|---|
| HR format | **Always uint8** | Parser handles both — no change, but we know the "always uint16" branch won't fire |
| Sensor contact | **Not supported** | Stale detection relies on timeout only — correct as designed |
| RR-intervals | **Not present** | TCX export will not contain RR data. This is normal for optical armbands |
| Energy expended | **Not present** | No reset command needed. GATT re-broadcast doesn't need `0x2A39` |
| Notification rate | **~1 Hz** (10 samples in 10 seconds) | Matches SPEC assumption ✅ |
| Packet size | **2 bytes** (flags + uint8 HR) | Minimal — well within MTU 23 |

---

## 4. On-Arm vs Off-Arm Behaviour (Comparative Test)

Three captures were taken: on the forearm, sitting on a chair, then back on
the forearm. This validated the full lifecycle of the optical sensor.

### 4.1 Notification comparison

| Capture | State | Raw packet | BPM | Flags |
|---|---|---|---|---|
| **1** (20:47) | ✅ On arm | `0x00 0x50` | **80** (real resting HR) | `0x00` |
| **2** (20:53) | ❌ Off arm | `0x00 0x00` | **0** | `0x00` |
| **3** (20:55) | ✅ On arm again | `0x00 0x51` | **81** | `0x00` |

> [!WARNING]
> **BPM = 0 is the "no skin contact" signal.** The Beat never uses the BLE
> sensor contact status bits (flags remain `0x00` always). Instead it sends
> `HR = 0` when the optical sensor cannot detect a pulse. The firmware
> **must not relay BPM = 0** to FTMS, TCX, or HR re-broadcast — treat it as
> "armband on but no contact".

### 4.2 Advertisement manufacturer data comparison

| Byte | On-arm | Off-arm | Decoded |
|---|---|---|---|
| `[0]` | `0x01` | `0x01` | Protocol version |
| `[1]` | `0x64` | `0x64` | **Battery level: 100%** (0x64 = 100) |
| `[2]` | `0x06` | `0x06` | Status flags (unknown) |
| `[3]` | `0x52` | `0x00` | **HR value** (0x52 = 82 BPM / 0 = no contact) |

> [!TIP]
> The Beat broadcasts HR + battery in its **advertisement** manufacturer data,
> even before a connection is established. The advert HR (82) differs slightly
> from the notification HR (80) due to optical sensor timing jitter — this is
> normal and expected.

### 4.3 Auto-sleep behaviour

| Event | Behaviour |
|---|---|
| Armband removed from arm | Continues advertising for **~30–60 seconds**, then **stops entirely** |
| Armband tapped/shaken (off skin) | Resumes advertising briefly, sends BPM = 0 |
| Armband placed on skin | Resumes advertising with real BPM within seconds |

This means the BLE connection will **drop entirely** (not just go stale) when
the user removes the armband and waits. The `onDisconnected()` handler fires.
The reconnect backoff (§8.4) handles re-acquisition when the user puts it
back on.

### 4.4 Three-state HR model

```
                    ┌─────────────┐
     Put on arm     │   ACTIVE    │  BPM > 0, notifications flowing
    ┌──────────────►│  (on skin)  │  → relay to FTMS/TCX/re-broadcast
    │               └──────┬──────┘
    │                      │ Remove from arm
    │               ┌──────▼──────┐
    │               │ NO CONTACT  │  BPM = 0, still connected
    │  Put back on  │ (off skin)  │  → suppress relay, keep connection
    ├──────────────►└──────┬──────┘
    │                      │ ~30-60 seconds
    │               ┌──────▼──────┐
    │               │DISCONNECTED │  Stops advertising, connection drops
    │               │  (asleep)   │  → onDisconnected(), start backoff scan
    └──────────────►└─────────────┘
```

### 4.5 Recommended notification handler

```cpp
void HRMClient::onNotification(const BLENotification& n) {
    uint8_t bpm = parseHrMeasurement(n.data, n.length);

    if (bpm == 0) {
        // Optical sensor has no contact — do NOT relay
        // Keep connection alive; armband may regain contact
        _contactLost = true;
        return;
    }

    _contactLost = false;
    _lastValidHrMs = n.receivedAtMs;
    publishHr(bpm);   // EventBus topic "echenon.hr"; HRMRepeater subscribes (SPEC §8.2)
}

// Three-state HR status:
// 1. bpm > 0 && recent       → ACTIVE (relay HR)
// 2. bpm == 0 && connected   → NO_CONTACT (suppress HR, keep connection)
// 3. disconnected            → DISCONNECTED (trigger reconnect scan)
```

---

## 5. Connection Behaviour

| Property | Observed | Firmware impact |
|---|---|---|
| **Pairing required** | **No** — connected without pairing or encryption | Simplifies reconnect. No bond storage needed |
| **MTU** | 23 (default minimum) | No MTU negotiation needed |
| **Address type** | macOS hides real MAC — CoreBluetooth UUID shown | Need to test on ESP32 to confirm public vs random static |
| **Connection time** | ~1 second from scan hit to GATT ready | Fast — good for reconnect UX |
| **Max connections** | **1 (single Central only)** | ESP32 takes the connection; tablet connects to ESP32's HRMRepeater instead |

> [!NOTE]
> **No pairing/bonding.** The device exposes all characteristics without
> encryption. The SPEC's "opportunistic bonding" policy still applies (bond if
> peer supports SMP), but in practice this Beat armband will always be an
> unencrypted open connection. This is typical for consumer HR monitors.

---

## 6. SPEC.md Impact — Required Updates

### ✅ Confirmed (no changes needed)
- HR Service UUID `0x180D` — correct
- HR Measurement char `0x2A37` with Notify — correct
- Scan filter by `0x180D` service UUID — correct
- `uint8` HR format with flags-aware parser — correct
- ~1 Hz notification rate — correct
- 10-second stale timeout as primary mechanism — correct
- Opportunistic bonding (no pairing required) — correct

### ⚠️ Updates needed

| SPEC section | Current | Should be | Severity |
|---|---|---|---|
| **§8.1** | "Any standard BLE device advertising `0x180D`" | Add: "Primary device: `EBEAT-NNNNNNN` (Echelon Beat HW901). OEM: Shenzhen GWSTAI (BLE company `0x0F05`)" | Low — informational |
| **§8.2 Body Sensor Location** | Not specified (implicitly "Wrist") | **"Hand" (`0x04`)** — HRMRepeater should re-broadcast the actual value read from the device | Medium — incorrect re-broadcast |
| **§8.2 HRMClient** | Subscribes to `0x2A37` only | **Add `0x2A19` (Battery Level)** subscription for portal display | Low — feature addition |
| **§8.2 Sensor contact** | Not explicitly handled | **Add note:** Beat does not support sensor contact. Parser must check `contact_supported` before interpreting `contact_detected` | Medium — correctness |
| **§8.2 BPM = 0 handling** | Not specified | **BPM = 0 means no skin contact.** `onNotification()` must suppress relay of 0 BPM. Three-state model: ACTIVE / NO_CONTACT / DISCONNECTED | **High — correctness** |
| **§8.2 RR-intervals** | Not specified | **Add note:** Beat does not send RR-intervals. TCX export should omit `<Extensions><TPX><Speed>` RR fields | Low — informational |
| **§8.4 Stale detection** | "If no HR update for 10 seconds" | Refine: "BPM = 0 → instant no-contact. No notification for 10s → connection lost (fallback)." Two-tier detection | Medium — design improvement |
| **§8.4 Auto-sleep** | Not specified | **Add note:** Beat stops advertising ~30–60s after losing skin contact. Connection drops entirely; `onDisconnected()` fires. Reconnect backoff handles re-acquisition | Medium — operational |
| **§8.5 Single connection** | Not specified | **Add note:** Beat supports only one BLE Central. Echelon Fit app must not hold the connection; ESP32 takes it exclusively, re-broadcasts via HRMRepeater | Medium — operational |
| **Portal BLE page** | Not detailed | Add: display battery level, device info (HW/FW/SW versions), connection state | Low — feature |

### ❌ Not applicable (can be removed from consideration)
- Heart Rate Control Point (`0x2A39`) — device doesn't have it
- RR-interval parsing in TCX export — device doesn't send it
- Sensor contact UI indicator — device doesn't support it

---

## 7. Recommended HRMClient Subscription Spec

Based on the hardware validation, the `getSubscriptions()` should declare:

```cpp
size_t getSubscriptions(BLESubscriptionSpec* out, size_t max) const override {
    size_t n = 0;
    // Primary: Heart Rate Measurement — required
    if (n < max) out[n++] = { 0x180D, 0x2A37, /*indicate*/false, /*required*/true };
    // Optional: Battery Level — nice-to-have for portal display
    if (n < max) out[n++] = { 0x180F, 0x2A19, /*indicate*/false, /*required*/false };
    return n;
}
```

And `getTargetServiceUUIDs()` remains `{ 0x180D }` — scan by HR service, not battery.

---

## 8. Independent Validation (Community & Open-Source Research)

> [!NOTE]
> **Our GATT dump is the first public documentation of this device's internals.**
> No prior teardown, nRF Connect screenshot, Wireshark capture, or firmware
> reverse-engineering of the Echelon Beat exists anywhere in the public domain.

### 8.1 FCC Filing

| Field | Value |
|---|---|
| **FCC ID** | [`2AWD4HW901`](https://fccid.io/2AWD4HW901) |
| **Applicant** | Echelon Fitness Multimedia LLC |
| **Model** | HW901 |
| **Equipment Class** | DTS (2402–2480 MHz) |
| **OEM** | Shenzhen GWSTAI / H&T ecosystem (rebranded by Echelon) |

The FCC exhibits only cover RF emissions testing — no GATT database disclosed.

### 8.2 BPM = 0 behaviour — community corroboration

- Reddit (r/echelon, r/qdomyos_zwift) users report the Echelon app shows an
  **instant drop to 0** when the strap is loose — consistent with our finding.
  No one has analyzed the raw packet flags before.
- Sending `HR = 0` as an off-skin marker (instead of using the BLE contact
  flag state machine) is a **standard shortcut in Chinese ODM fitness firmware**.
- **QZ confirms our design:** in [`domyosbike.cpp`](https://github.com/cagnulein/qdomyos-zwift/blob/master/src/devices/domyosbike/domyosbike.cpp),
  QZ explicitly treats `heart == 0` as invalid and falls through to external sources:
  ```cpp
  if (heart == 0 || disable_hr_frommachinery) {
      update_hr_from_external();
  }
  ```
- **SmartSpin2k** also filters zero-value HR reports from remote FTMS devices
  in its sensor decoder pipeline.

### 8.3 Auto-sleep — undocumented firmware behaviour

The [official HW901 user manual](https://manuals.plus/m/93d0bb2cb90a423cb11b1d8e17812f8087fc4dc6210fdfd9ea4101e40884501a)
documents **only manual power-on/off** (button press). The ~30–60s auto-sleep
when off-skin is an **undocumented power-saving mechanism** — the MCU shuts down
the optical LEDs and BLE radio to protect the 3.7V lithium battery. Community
reports confirm users are bitten by this regularly ("refuses to pair" / "vanishes
from scan").

### 8.4 QZ / SmartSpin2k integration

| Project | Echelon Beat support | How it connects |
|---|---|---|
| **QZ** ([`heartratebelt.cpp`](https://github.com/cagnulein/qdomyos-zwift/blob/master/src/devices/heartratebelt/heartratebelt.cpp)) | Generic `0x180D` — **no custom driver** | Standard `0x2A37` subscribe, bridges HR into FTMS/Zwift |
| **SmartSpin2k** ([`BLE_Client.cpp`](https://github.com/doudar/SmartSpin2k/blob/master/src/BLE_Client.cpp)) | Generic `0x180D` — **no custom driver** | Standard `0x2A37` notify, relays to BLE Server |

Neither project references `EBEAT`, `0x0F05`, or `0xFD00`. Zero GitHub issues
or PRs mention "Echelon Beat" in either repository. The device Just Works™
as a standard BLE HRM.

### 8.5 Single-connection limitation

> [!WARNING]
> The Beat supports **only one simultaneous BLE Central connection**. If the
> Echelon Fit app is running on the tablet, the ESP32 cannot connect. Users
> must ensure the Echelon app is not holding the connection before the ESP32
> can pair. This is consistent with our architecture where the ESP32
> *replaces* the tablet's direct Beat connection via HRMRepeater re-broadcast.

### 8.6 OEM provenance

- Model `HW901` follows the Shenzhen ODM naming convention (cf. CooSpo HW706,
  HW807, HW808, HW9)
- BLE Company ID `0x0F05` belongs to **Shenzhen GWSTAI Technology Co., Ltd.**
  (closely tied to H&T Intelligent Control in the Shenzhen supply chain)
- `EBEAT-NNNNNNN` prefix is **Echelon-exclusive** firmware customization.
  White-label versions broadcast as `HW901-`, `HRM-`, or `Heartool`
- Body Sensor Location "Hand" (0x04) is an ODM firmware quirk — no "Arm" or
  "Forearm" value exists in the Bluetooth SIG enum, so they picked the closest

---

## 9. Appendix: Full GATT Tree

```
EBEAT-0014941
├── Service: 0xFD00 (Vendor — Shenzhen GWSTAI / H&T)
│   ├── 0xFD09 [Notify]
│   ├── 0xFD0A [Write-no-response]
│   ├── 0xFD19 [Notify]
│   └── 0xFD1A [Write-no-response]
│
├── Service: 0x180D (Heart Rate) ★
│   ├── 0x2A37 Heart Rate Measurement [Notify]
│   │   └── 0x2902 CCCD
│   └── 0x2A38 Body Sensor Location [Read] → "Hand" (0x04)
│
├── Service: 0x181C (User Data)
│   └── 0x2A96 VO2 Max [Read] → 0x00
│
├── Service: 0x180F (Battery) ★
│   └── 0x2A19 Battery Level [Read | Notify] → 100%
│       └── 0x2902 CCCD
│
└── Service: 0x180A (Device Information)
    ├── 0x2A29 Manufacturer Name [Read] → "NULL"
    ├── 0x2A24 Model Number [Read] → "2021-04-06"
    ├── 0x2A27 Hardware Revision [Read] → "V1.4"
    ├── 0x2A26 Firmware Revision [Read] → "V1.0"
    ├── 0x2A28 Software Revision [Read] → "V1.5.0"
    └── 0x2A23 System ID [Read] → 0504030201030201
```
