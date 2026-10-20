# PR-01 Progress Review (WO0 to WO4a): Chief Architect

| | |
|---|---|
| **Date** | 2026-10-04 |
| **Reviewer role** | Chief Architect (Tier 1) |
| **Baseline** | PR-01 stack, `signal-logger/wo0-adc-spike` to `fix/adc-gpio-conflict` (`da5ffee`), 35 commits ahead of `main` (`5c04505`). Framework at `v0.0.12-dev.82`. |
| **Inputs** | Firmware (`SignalLogger`, `EchenonModule`, `main.cpp`, `board.h`, `platformio.ini`); PR-01 PROJECT.md session log; bench evidence B01–B04 and A08; `docs/encoder-resistance-analysis.md`; `docs/mitm-encoder-design-notes.md`; the 2026-10-04 bench wrap-up |
| **Status** | Advisory. The logger fixes go to the PR-01 PA. The programme items are folded into [PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md) and the [DR-1 record](DR-1-design-freeze.md). |

---

## 1. Verdict

The bench work found more than Phase 0 was expected to. Three findings change the design:

- **The pot reference is the other way round from what the SPEC assumes (E-01).** Yellow measures 0 V and Green about 1.67 V. The provisional standalone design would have fought the console's own reference.
- **The stock console misses clicks (B02).** It drops 28% of knob clicks, and 42% of single clicks.
- **PCNT fixes the click problem completely (B04).** This makes an **encoder-only MITM** a realistic, lower-risk route to a better ride and to ERG (P-B).

**The capture firmware is not fit for the WO5 register campaign as it stands.** Burst exports put samples under the wrong channels, motor events flood the event log, timestamps are per DMA read rather than per sample, and encoder edges are no longer logged. These problems hit exactly the rows WO5 must close (#1, #3 and #13). **A new WO4d (logger fixes) must come before WO5.**

---

## 2. Logger defects (PR-01: fix in WO4d, before WO5)

### 🔴 L-01. Burst CSV puts samples under the wrong channels
`_burstBuffer` stores raw samples in DMA order with no channel tag. `/burst.csv` then reshapes them into rows of `kNumChannels` = 10. Since WO4a only **8** channels are scanned (`{0–6, 9}`), and nothing guarantees a frame starts on channel 0 or that no samples were dropped. **Every burst column is misattributed.**

The header also gives no sample rate. That rate is now 83.3 kS/s ÷ 8 ≈ 10.4 kS/s per channel. A full buffer of 1.5 M samples is about 18 s, not the 30 s maximum.

**Fix:** de-interleave using `type2.channel`. Export one column per channel that is actually scanned, with real signal names. Put the per-channel rate, the start time and the dropped-sample count in the header.

### 🔴 L-02. Motor on/off events chatter and flood the event log
`handleSoftwareThresholds()` uses one `_motorOn` flag for both channel 5 (motor+) and channel 6 (motor−). While the motor runs forward, every motor+ sample sets the flag and every motor− sample at 0 V clears it. At about 10 kS/s per channel, the 4,000-entry event ring is overwritten in well under a second, taking the cadence events with it.

**Fix:** keep state per channel, record the drive direction, and add hysteresis or a minimum-duration filter.

### 🟠 L-03. Timestamps are per DMA read, not per sample
`ts_us = esp_timer_get_time()` is taken once per `adc_continuous_read()`. It is then stamped on every sample in that read, which covers about 3 ms at 83 kS/s. If the main loop stalls, the 4,000-byte driver pool (about 12 ms of data) backs up, and timestamps arrive late by up to that amount. The claimed µs resolution is not real. The overflow callback is not registered either, so lost samples go uncounted.

**Fix:** derive each sample's time from its index × the sample period, anchored to a counted frame. Register `on_pool_ovf` and export the overflow count.

### 🟠 L-04. Encoder edges are no longer logged
WO4a moved GPIO 8/9 to PCNT. The channel-8 Ch B threshold path is now dead code, and nothing writes encoder events to the log. The encoder → motor → pot cascade timing is therefore unmeasurable: that is the strategy's Phase 0 go/no-go and register row #3.

**Fix:** subscribe to `EncoderModule`'s EventBus events and log them with timestamps (type `encoder`, with direction).

### 🟠 L-05. The ride log wraps at 30 minutes and exports dead columns
36,000 rows × 50 ms is 30 minutes, after which the log silently overwrites the start of the ride. The CSV still has `ch7`/`ch8` columns, which are always 0. B02 uses named headers while B03 uses `chN`, so the evidence formats disagree.

**Fix:**
- Use 8-channel rows (124 → 100 B per row) and a configurable 10 Hz window, so 60 minutes fits.
- Add a `ride_log_wrapped` status flag.
- Use named headers everywhere.

### 🟡 L-06. Exports read the ring buffers while capture writes to them
The CSV handlers run in the httpd task with no lock or snapshot, so rows can tear mid-export.

**Fix:** pause capture during export, or copy indices under the spinlock.

### 🟡 L-07. Leftover debris in `SignalLogger.cpp`
- A function-static `_adc_configured` flag means `stop()` followed by `begin()` never reconfigures the ADC.
- `fail_count` is a no-op.
- `printf` and `ESP_LOGI` both fire every second.
- Stream-of-consciousness comments remain in `begin()`.
- The WO2 note about `PULLUP_ENABLE` on ISR pins is moot now that the ISRs are gone; close it.

### 🟡 L-08. SignalLogger takes about 7.5 of the 8 MB of PSRAM
The ride log (4.5 MB), burst buffer (3 MB) and event log (40 KB) together leave almost nothing for later PRs (RideStore and others).

**Programme rule:** after PR-01, `SignalLogger` is compiled only under `WITH_SIGNAL_LOGGER` (diagnostic builds). Production targets do not allocate it.

---

## 3. Evidence-quality findings (PR-01 and DR-1)

### 🔴 E-01. Pot polarity and reference are not what the SPEC assumes
B01 shows **Yellow ("POT_REF_HI") = 0 V and Green ("POT_REF_LO") ≈ 1.67 V** (raw ≈ 2,084 counts). The wiper spans about 97 to 1,348 counts across levels 1–32 (B03). The console therefore references the pot at about 1.67 V on Green, with Yellow at ground.

**Consequences:**
- The SPEC and hardware spec signal names are wrong.
- The provisional Phase 3 standalone network is wrong: R4 drives 3.3 V into Yellow, and the Green-to-GND link would short the console's reference.
- The switches added in remediation (SW1–SW4, open by default) prevent harm, but **DR-1 §2.2/§2.9 must redesign** this network.
- Row #6 still needs the source-impedance (load) test.

**Doc-sync:** record the polarity as a measured fact now. Leave the circuit redesign to DR-1.

### 🟠 E-02. Absolute ADC readings through 22 kΩ are suspect
B01 found the cadence line unreadable through 22 kΩ and fixed it with 4.7 kΩ in series plus 100 nF. The root cause is general. A multiplexed SAR converter sampling at 83 kS/s cannot settle its sample capacitor through 22 kΩ. The pot taps are still on 22 kΩ, so their **absolute** values may be biased. B03's linearity and step size are relative and should hold, but this has not been verified.

**Fix:**
- Move every ADC tap to 4.7 kΩ plus a local 10–100 nF capacitor, or cross-check each tap against a DMM and record the offset (WO5).
- Confirm that 4.7 kΩ + 100 nF (τ ≈ 0.47 ms) does not distort the console's reed edge (row #7).

### 🟠 E-03. The WO0 / A08 "Ch B sub-threshold" conclusion is superseded
WO0 measured "Ch B" peaking at 1.45 V and concluded it needed analog thresholding. After the wire-colour correction (Black = A, Red = B, Orange = VCC), PCNT decodes both channels with 0% bounce and 100% detection (B04). WO0 almost certainly probed the wrong wire.

**Fix:**
- Mark A08 *superseded* in the register.
- Remove "Ch B conditioning" from SPEC §5.4 and DR-1 §2.5.
- Drop the teardown's "1.5–2 V Ch B pulse" claim.

### 🟡 E-04. Two different level formulas
B03 gives `wiper = 39.1 × level + 97`. Design notes §4 give `level = (wiper − 125) / 37.7 + 1`. Adopt B03's fit as the single reference and cite it.

### 🟡 E-05. Evidence naming drifted
The B01–B04 reports use a `B` prefix, while the register's convention is `A<row>-…`.

**Fix:** keep the B-series as "bench reports". Have the register cite them per row:
- B01 → #6, #7, #8 (partial)
- B02 and B04 → #8
- B03 → #4

### 🟡 E-06. The scope gate was skipped
The motor-sense taps (WO4 item F) were connected before the motor-drive scope characterisation (item C). The ÷4 divider makes this safe, so there was no hazard. Record it in the session log. Rows #1, #2 and #11 are still open.

---

## 4. Programme findings

### P-A. Owner decisions taken during the bench sessions (recorded, not re-opened)
1. Use the framework `EncoderModule` (PCNT) for the knob. GPIO 8/9 leave the ADC scan; encoder VCC stays on GPIO 10.
2. Cadence tap: 4.7 kΩ in series plus 100 nF.
3. Encoder loom colours are Black = A, Red = B, Orange = VCC (measured).
4. **256 internal resistance levels** (`uint8_t`) are the canonical scale. Echelon 1–32 and other scales are mappings. The mapping must define its offset. The notes' `internal × 32 / 256` gives 0–31, not 1–32; SPEC §5.3 resolves this at DR-1.

### P-B. New architecture option: encoder-only MITM (DR-1 decision 2.10)
B02 shows the **stock** console misses 28% of clicks and 42% of single clicks. Intercepting **only the 3-pin encoder loom**, decoding with PCNT and resynthesising clean quadrature to the console:
- fixes a defect the rider can feel, with no motor split and no pot simulation;
- **is the audit's H-05 option (c) "Path B" (encoder injection)**, so an ERG mode that synthesises clicks gives 32-level ERG with the console still driving the motor. That could deliver UC4/UC5 without PR-07's motor split.

The design notes' spectrum (Monitor → Encoder MITM → Full MITM → Console bypass) is the option set for DR-1. The "console as signal source" model (ESP32 PID with the console drive as a hint) is the refined form of PR-07.

**Prerequisites before DR-1 can choose:**
- the safe subset of the console fault-tolerance tests in design notes §12: #1, #2, #3, #7 and #9, plus the open questions on minimum pulse width and mid-seek behaviour;
- a **normally-closed fail-safe** on the encoder path, so the knob still works when the ESP32 is unpowered.

Tests #4 and #6 (pot or motor disconnected while seeking) need an explicit owner go-ahead.

**Programme impact if adopted** (proposal for the DR-1 decision):
- PR-04a's board includes the J3/J4 encoder intercept with NC bypass.
- A new firmware PR adds encoder relay, resynthesis and clean-click.
- PR-08 targets encoder-injection ERG first.
- PR-07 (motor MITM) becomes the high-fidelity or v1.1 path.

### P-C. The cam model simplifies audit finding S-01
- Within the console's working range, the pot is **linear in shaft angle** (R² = 0.9989, about 151° over 32 levels).
- The cam turns that angle into magnet displacement `d(θ) = e·(1 − cos θ)`, with e ≈ 8 mm and θ = (level − 1) × 4.87°. Braking resistance is a further non-linear function of the magnet gap, `R = g(d(θ))`. So **level is a linear position index but not a linear resistance scale**: steps near level 1 barely change resistance, and steps around level 19 (θ ≈ 90°, the steepest part of the cosine) change it most.
- The sweep stays below 180°, so `d(θ)` is monotonic over the console's range. Each level maps to exactly one resistance; there is no mirror ambiguity unless the motor is driven past the working range (the PR-07 case).
- In console-driven modes (passive and encoder MITM), level comes straight from a linear wiper fit, so no full-revolution tracking is needed. Angle tracking, click-off detection and homing matter only when the ESP32 drives the motor (PR-07).
- **Power model:** `g` is unknown and changes with each bike's magnet set, so a curve-fit polynomial in level or angle is not good enough. Power must be indexed by level (or internal 256-level position) using PR-02's **empirical per-level table** (power against cadence at each level). That table captures `g∘d` directly.
- **ERG and FTMS:** target power → level must go through that table, never a linear interpolation across levels. Resistance values reported to apps (0x2AD6 range, Indoor Bike Data) should be linearised from the same table, which is design-notes §6 option B or C and DR-1 §2.11.

The stock console overshoots on 98% of mid/high moves (B03). That behaviour is the baseline any motor-control option must beat.

### P-D. Process
WO4 drifted from "scope gate" into encoder, calibration and architecture work. The output was valuable and the decisions were the owner's. But:
- the work landed on an off-stack branch;
- the gate order was skipped;
- the evidence naming drifted;
- the design notes went unreviewed.

**Rule going forward:** an interactive WO may declare an *exploration* boundary. Findings are committed as bench reports, and design conclusions go to the Chief Architect or DR-1 as proposals. The wrap-up report currently lives outside the repo; **commit it** to `pr-01-signal-logger/` for provenance.

### P-E. Framework
Upstream is at `v0.0.12-beta.2`. The PR-03 central-role acceptance gate is still unverified. Check it before PR-03. No action is needed for PR-01.

---

## 5. Actions

| # | Action | Owner | When |
|---|---|---|---|
| 1 | **WO4d logger fixes**: L-01 to L-05 and L-07, plus native tests for the de-interleave, motor state machine and timestamp maths (these can fold into WO6) | PR-01 PA | Before WO5 |
| 2 | WO5 adds: E-02 tap cross-check / 4.7 kΩ + C rework; the row #6 load test; the safe console fault-tolerance tests (#1, #2, #3, #7, #9) and the open questions from P-B | PR-01 PA (interactive) | WO5 |
| 3 | WO7 doc-sync adds: E-01 measured polarity; E-03 A08 superseded; E-04 single formula; E-05 register mapping of B-reports; commit the wrap-up into the PR folder | PR-01 PA | WO7 |
| 4 | `WITH_SIGNAL_LOGGER` guard (L-08) | PR-01 PA (WO4d) | WO4d |
| 5 | Folded into this commit: DR-1 inputs and decisions 2.10–2.11; PROGRAM status and log; PROGRAM-PLAN candidate restructure | Chief Architect | Done |
