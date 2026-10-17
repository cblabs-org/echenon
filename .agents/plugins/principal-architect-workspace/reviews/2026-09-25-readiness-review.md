# Echenon: Chief Architect Readiness Review (PA hand-off)

| | |
|---|---|
| **Date** | 2026-09-25 |
| **Reviewer role** | Chief Architect (Tier 1), second opinion |
| **Question** | Are the SPEC and the programme ready for a Principal Architect to begin PR-01? |
| **Baseline** | Echenon `main` @ `2992155` (the commit that applied the [2026-09-23 audit](2026-09-23-chief-architect-audit.md)) · framework submodule @ `0268974a` (`v0.0.12-dev.57-19`) · framework `origin/main` @ `1b5f44c4`, tag `v0.0.12-dev.82` |
| **Scope** | `SPEC.md`, `docs/*`, `PROGRAM.md`, `PROGRAM-PLAN.md`, all 9 `pr-*/PROJECT.md`, `memory/*`, archive banners, `docs/measurements/phase0/README.md`. The SPEC was diffed against `2992155^`. Framework claims were checked against the `v0.0.12-dev.82` headers, `platformio.ini`, the reference `board.h` and the BLEManager acceptance record. Upstream #225, #226, #235 and #236 were checked with `gh`. |
| **Status** | Advisory. No SPEC, programme or source files were changed. Applying any of this needs your explicit approval under `.agents/rules/01-plan-before-execute.md`. |

---

## 1. Verdict

**Not ready to hand off yet. A small, docs-only repair (gate G0, §6) comes first. After that, PR-01 is GO.**

The programme is structurally sound. The 9-PR passive-first plan, the DR-1 gate,
parallel tracks and interactive hardware WOs are all in place. The governance
rule `.agents/rules/00-operating-model.md` is identical to the skill's asset. The problem
is **commit `2992155` itself.** It applied the audit mostly as pasted directives
rather than as specification. It deleted about 330 lines of SPEC and left
corruption behind. It added new contradictions, including a pin map that
assigns GPIO 6 and 7 to two jobs at once. It also recorded one upstream gate as met without the evidence for it.
A PA treats the SPEC as its source of truth and gives slices of it to workers, so the
SPEC has to be repaired before hand-off.

Nothing past PR-01 is blocked by G0. §5 lists the items that must be fixed before later PRs start.

### Findings summary

| Severity | Count | Blocks |
|---|---|---|
| 🔴 Critical | 3 | PA hand-off (G0) |
| 🟠 Major | 11 | 5 must be written into the PR-01 mandate before it starts; 6 must be fixed before the named later PR |
| 🟡 Minor | 6 | Hygiene |

---

## 2. Critical: fix before PA hand-off

### 🔴 RR-01. Commit `2992155` damaged the SPEC
**Evidence (all in `SPEC.md` unless noted):**
- **Content deleted.** All of §13 Testing (8 subsections, including §13.1, the Phase 0 desk test) and **Appendix A, the User Journeys (10 journeys)**, were removed. §13 now has two table rows. PR-09's PROJECT.md still cites "§13, Appendix A".
- **A model control token was left in the code.** Line 936 reads `class<ctrl94> : public IBLEClientProvider`. It should be `class HRMClient`.
- **Stale tables came back as orphan fragments.** Lines 508–522 hold an old parts list (it re-adds "R1, R2: 10 kΩ pull-downs" and includes a personal order number). Lines 662–672 hold an old NVS table with `cal_mv_min = 1160` and `motor_dead_zone = 5`, which are exactly the values the audit said to remove. Both fragments start with `---|---|` and render as broken markdown.
- **§12.3 appears twice.** The second copy (lines 1778–1804) sits outside a fence, and line 1804 opens an unclosed fence, so **the rendered §13 disappears into a code block**.
- **Headings are missing.** `## 4. Hardware` and `## 9. Session Recording` are gone.
- **§4.6 wiring table was deleted.** It now reads *"(Omitted full table for brevity, but same physical layout)"* (line 474).
- **Several sections are editing instructions, not specification.** Examples: §4.3 "Unify hardware-config model: Use…", §5.2 "State that the actuator is…", §5.3 "Replaced linear mV-to-level formula…", §5.4 "Replaced the hand-rolled ISR decoder…", §5.5 "Rewrite the controller architecture:", §13 "Add native test requirement…".
- The commit message says "Apply all corrections…". The scorecard in §4 shows the work is partial.

**Fix.** Recover the deleted material with `git show 2992155^:SPEC.md`. Restore §13, rewritten to the 9-PR phase numbering, and restore the User Journeys; rename them Appendix J to end the clash with the Phase 0 register, which is called "Appendix A". Remove the orphan fragments, the duplicate §12.3 and the control token. Restore the two headings. Turn every directive into declarative text. Then run a markdown render check.

### 🔴 RR-02. The pin map disagrees with itself in at least eight places, and GPIO 6/7 are assigned twice
| Location | Motor sense + / − | L9110S IA / IB | Encoder VCC sense |
|---|---|---|---|
| SPEC §3.2 block diagram | GPIO 10 / 17 | 6 / 7 | — |
| SPEC §3.2 routing table | 6 / 7 "with 10 kΩ pull-downs" | — | — |
| SPEC §3.3 data flow | 10 / **11** | 6 / 7 | — |
| SPEC §4.3 `config.h`, §4.4 table, §12.3 (both copies) | 6 / 7 | **6 / 7 ⚠ collision** | GPIO 4 (§4.4) |
| HW §5.3 Phase 0 schematic | 6 / 7 | n/a | **GPIO 18 (ADC2)** |
| HW §5.7 Phase 0 test points (same section!) | **10 / 17** | n/a | GPIO 18 |
| HW §7.3.1, §7.3.4 truth table, §7.9.2 | 6 / 7 | **6 / 7 ⚠** | — |
| HW §7.8 Phase 3 pin map | 6 / 7 | 10 / 17 | — |

**Root cause.** Audit action A1 said *"in Phase 0, GPIO 6 and 7 are free and both ADC1"*. The commit promoted that Phase-0-only advice into the permanent map. There are two further problems:
- HW §5.3 puts Grey (cadence return) on GPIO 4, but SPEC §4.4 puts encoder VCC there.
- GPIO 17 and 18 are ADC2, which the audit (H-04) had flagged as unreliable while WiFi is on. In Phase 0, encoder VCC is still on ADC2.

**Fix. Write one authoritative Phase 0 map, and have every other document refer to it rather than copy it.** The ten tap signals map one-to-one onto GPIO 1–10, which are exactly the ten ADC1 channels:

| GPIO | Header | Signal |
|---|---|---|
| 1 | A0 | Blue: pot wiper |
| 2 | A1 | Yellow: pot outer HI |
| 3 | A2 | Green: pot outer LO |
| 4 | A3 | Grey: cadence return |
| 5 | D2 | Orange: cadence |
| 6 | D3 | Red: motor + (via ÷4 divider) |
| 7 | D4 | Black: motor − (via ÷4 divider) |
| 8 | D5 | Encoder Ch A (Brown) |
| 9 | D6 | Encoder Ch B (Orange) |
| 10 | D7 | Encoder VCC (Red) |

Mark the Phase 3 map `[DR-1]`. HW §7.8 already puts sense on 6/7 (ADC1) and drive on 10/17 (digital), which is coherent, so fix HW §7.3 to match it or leave the choice to DR-1.

> ⚠️ The framework's reference `hardware/esp32s3nano/board.h` assigns **GPIO 9 = display DC, GPIO 10 = display CS** (under `WITH_DISPLAY_U8G2`) and **GPIO 8 = mode button with `INPUT_PULLUP`** (under `WITH_BUTTONS`). All three are Echenon tap pins. See RR-03.

### 🔴 RR-03. The SPEC's build configuration will not build, and one path would drive the bike's encoder loom
- **The `extends` line cannot resolve.** §12.2 uses `extends = env:esp32s3nano`. That environment is defined in `framework/platformio.ini`, not in Echenon's. The framework's own consumer pattern (`framework/template-app/platformio.ini`) re-declares an `[env:base]` with `-I` paths into the framework, `lib_extra_dirs`, `extra_scripts` (portalBuilder, sync_sdkconfig, …) and its own `[env:esp32s3nano]`.
- **The sdkconfig paths are wrong.** `hardware/esp32s3nano/sdkconfig.defaults` and `hardware/_common/sdkconfig.ble.defaults` resolve against the Echenon root. They need to be `framework/hardware/...`, and the second file does not exist at the pinned submodule.
- **Inheriting the framework env can drive GPIO 9 and 10.** `${env:esp32s3nano.build_flags}` carries `-D WITH_DISPLAY_U8G2`. If the Echenon board header is cloned from the reference board (the route the custom-boards guide suggests), the U8G2 driver drives **GPIO 9 (the encoder Ch B tap) and GPIO 10 as SPI outputs into the console's encoder loom.** That breaks Phase 0's passive premise and may upset the console. If the header is not cloned, the build simply fails.
- **Three naming models are still in use** (Appendix B #1 is unresolved):
  - `-DBOARD_HEADER=…/board.h` in §12.2 and the PR-01 mandate;
  - a `config.h` file in §4.3, §12.1 and the strategy doc;
  - two families of flags: `HAS_*` (§4.3) and `WITH_*` (§12.3).
- **Stale repo paths remain.**
  - §12.1 still says `lib/esp32-app-foundation`; the submodule is actually at `framework/` (Appendix B #8).
  - §12.1 still lists `Config.h`, which collides with the framework's `ConfigManager` naming (U-02).
  - §12.1 still lists `Calibration.h`, which now belongs to PR-02 (P-10).

**Fix.**
- **Build pattern.** Replace §12.2 with "follow `framework/template-app/platformio.ini`, with `../` rewritten to `framework/`". An alternative is `extra_configs = framework/platformio.ini`, but check that framework-relative paths still resolve before adopting it.
- **Board header.** Use one file, `hardware/ex3-waveshare-nano/board.h`. Base it on `hardware/_common/board_contract.h` and `_TEMPLATE`, *not* on the reference board.
- **Flags.** Rule that the env **must not** define `WITH_DISPLAY_U8G2` or `WITH_BUTTONS`. Use `HAS_*` for bike signals and `WITH_*` for framework modules, and say so in the SPEC.

---

## 3. Major findings

### 3.1 Add these to the PR-01 mandate before the PA starts

#### 🟠 RR-04. The Phase 0 capture design is unspecified, and its stated requirements do not fit in memory
- The strategy asks for motor sense polled at 1 kHz, the pot at 100 Hz, µs timestamps and a PSRAM ring buffer. Its go/no-go also asks for *"CSV export, at least one full ride captured"*. A 45-minute ride with just the two motor channels at 1 kHz is about **32 MB** raw. PSRAM is 8 MB and LittleFS is 2.75 MiB.
- Polling `analogRead()` at kHz rates across several channels from `tick()` does not fit the 10 ms tick budget. It needs the ESP-IDF **ADC continuous (DMA)** driver. It is also unverified whether a pad can be sampled by the ADC and serve a GPIO interrupt at the same time (cadence on GPIO 5, encoder on 8 and 9).
- Per `ex3_specification.md:191`, encoder Ch B is a *"momentary pulse (~1.5–2 V)"*. That is below V<sub>IH</sub> = 2.475 V, so a digital ISR may never see it, and PCNT/`EncoderModule` (H-06) may not work without conditioning.

**Required in the mandate:** two capture modes.
- **(a) Ride log.** A decimated per-channel min/mean/max at 10–20 Hz, plus an event log with µs timestamps (cadence edges, encoder edges, motor-drive onset and offset from analog threshold crossings). This must fit a full ride.
- **(b) Burst capture.** Triggered 5–30 s windows at 1–5 kHz on all ten ADC1 channels into PSRAM, downloaded as CSV. Triggers: manual, motor-drive onset and power-on.

Open the PR with a **WO0 spike**: ADC-continuous throughput with WiFi on, ADC and ISR sharing a pad, and analog capture of encoder Ch B.

#### 🟠 RR-05. The Phase 0 taps are not invisible, and the buck converter adds risk with no Phase 0 benefit
- HW §5.3 wires Yellow, Blue, Green, Orange, Grey and the encoder lines straight to GPIOs ("all 0–3.3V, safe"). When the ESP32 is unpowered and the bike is on (during the documented buck-disconnect-to-flash step, or during boot), the console's 3.3 V lines **back-power the ESP32 through its ESD diodes**. That loads the pot reference and encoder supply, which are exactly what Appendix A #4, #6, #8 and #10 set out to measure. Any internal pull-up on the ISR pins would also bias the console's lines.
- Keep the logger on the buck in Phase 0 and there is still a trimpot-set rail within 0.3 V of the 3.6 V absolute maximum (H-03). Every flash also needs a rewire. Appendix A #13 already requires USB power.

**Required in the mandate:**
- Put a **10–47 kΩ series resistor on every direct tap**.
- Configure every tap pin with **no internal pulls**.
- **Power the logger from USB-C for all of Phase 0 and 1**, with GND tied to Brown. The buck converter is then first used on the PR-04a board.

#### 🟠 RR-06. Bench-readiness action A4 was never applied
The audit's A4 is missing from PR-01, the PROGRAM gates and HW §5.1. It called for:
- confirming scope access;
- buying the JST pairs early;
- building a back-probe or Y harness rather than clipping alligator clips onto JST-SM contacts.

HW §5.1 still lists "alligator clip fly wires ×12". Appendix A #2 needs a current shunt or clamp, which is in no BOM. #13 needs a scope that can do single-shot triggering.

**Required:** a "bench ready" entry condition for PR-01's first hardware WO covering:
- a scope with at least 2 channels and single-shot triggering;
- a way to measure motor current;
- a 9-pin and 3-pin Y-harness;
- the series resistors from RR-05.

#### 🟠 RR-07. The framework pin and upgrade policy is undecided, and PROGRAM.md claims a gate is met without evidence
- **The pinned submodule is old.** `0268974a` (`dev.57-19`) predates BLEManager and the **#236 fix**. #236 is the WifiManager defect that drops permanently to an *open, unauthenticated AP*. PR-01's portal would ship with it.
- **The gate claim in PROGRAM.md is unsupported.** Its 2026-09-25 log says BLEManager is *"merged and confirmed as PR-03 prereq"*. Status of the three r4 gate conditions:

  | Gate condition | Status | Evidence |
  |---|---|---|
  | (1) PR #235 🔴 1–9 fixed | ✅ | PR thread |
  | (2) RFC §17.2 central-role items on a real HR peripheral | ❌ | See below |
  | (3) #236 fixed | ✅ | Closed 2026-09-25 |

  For (2), the framework's `ACCEPTANCE.md` at `dev.82` marks **"Central Scan — N/A (by design)"**. The only acceptance run was at `dev.67`, before WO-8's central use-after-free fix. The PR's own final comment lists central acceptance as still to do.
- **Tag note.** `dev.82` tags the PR branch tip (`61f7f754`). No tag contains the merge commit `1b5f44c4`. The content is the same, but a tag on `main` would be better.
- **OTA boot check is still open.** #226 (OTA `checkOnBoot`) is open, so the SPEC's `autoCheck=false` still has no point in the code that enforces it (P-07).

**Recommend:**
- PR-01 WO1 bumps the submodule to `v0.0.12-dev.82`, with BLE left off, to pick up #236 and avoid a cross-cutting bump in the middle of the programme.
- Correct the PROGRAM.md log.
- Keep PR-03's entry gated on central-role acceptance evidence.

#### 🟠 RR-08. The PR-01 exit criteria differ across four documents
- **The signal count differs:**
  - "ALL 12 wires" in PR-01 and PROGRAM-PLAN;
  - "9+2 signals" in the strategy's go/no-go;
  - **10** tap signals in HW §5.3 and §5.7. White and Brown are power, not taps.
- **"Appendix A exit checklist" is now ambiguous.** Link it explicitly to `docs/measurements/phase0/README.md`. The strategy's go/no-go leaves out register items #2, #6 and #9–#13.
- **The gate expects one outcome.** PR-01 says the scope *"must confirm 3.3V DC (not 9V PWM)"*, which is the pattern the audit warned about (P-02). The ÷4 divider is safe up to about 13 V, so either result lets logging go ahead; the result feeds DR-1. HW §5.2 uses only a 10 ms/div timebase, which cannot resolve kHz PWM, so add a µs/div pass.
- **The register cannot close DR-1 yet.** The Phase 0 README has no "Evidence file" or "Result" columns.
- **DR-1 has no artefact.** There is no decision-record template listing the outputs defined in audit §8.3.

### 3.2 Fix before the named later PR (does not block PR-01)

#### 🟠 RR-09. The implementation strategy was not restructured *(before PR-02)*
`docs/implementation-strategy.md` still has Phase 3 = Motor Interception, 4 = FTMS, 5 = ERG + Cloud, 6 = validation. The programme is 3 = Passive/FTMS, 4 = Session, 5 = MITM, 6 = ERG, 7 = Integration. The passive tap board appears in neither the strategy nor HW §1. PR-05's own header gives three different phase numbers. The strategy also names a framework `RotaryEncoder` module; the actual module is `EncoderModule`.

**Fix.** Re-sequence the strategy, or demote it to rationale and make PROGRAM-PLAN the roadmap.

#### 🟠 RR-10. PR-02 implements what DR-1 decides, and its calibration needs a motor it doesn't have *(before PR-02)*
- PR-02 builds level mapping, PowerEngine and calibration, which is SPEC §5–§6. DR-1 is chartered to rewrite exactly those sections, but PR-02 depends only on PR-01.
- SPEC §5.3 and §6.3 calibrate by *"sweeping through a full revolution"*, which needs the ESP32 to drive the motor. That only arrives in PR-07.
- PR-02 itself still says "guided min/max pot calibration" and "levels match 1–32", which is the linear model.

**Fix.**
- Gate PR-02 on DR-1. This costs almost nothing, because DR-1 follows PR-01 directly.
- Define a **passive calibration**: the rider steps the console through its 32 levels while the logger records wiper and angle at each level, so the console's level acts as the resistance proxy.
- Motor-driven calibration moves to PR-07.

#### 🟠 RR-11. Hidden dependencies and ownership overlaps *(before PR-02/03, PR-05, PR-06)*
- **PR-06 depends on PR-03 in practice.** It uses BLEManager's `ScanInhibit` (in its mandate and SPEC §8.5) but depends only on PR-02. Either add the edge or make the inhibit conditional on `WITH_BLE`.
- **PR-05's hardware line is unclear.** It needs the "Passive Tap Board (from PR-04a)" but has no edge to PR-04a. Either drop the line (the fly-wire rig is enough) or add the edge.
- **The overlap statements disagree.** PR-02 and PR-03 PROJECT.md say "no file overlap", while PROGRAM-PLAN names the overlap in `EchenonModule` and `platformio.ini`. Make them consistent, as Core Directive 4 requires.
- **Two PRs claim the Control Point.** Both PR-05 and PR-08 claim Control Point handling. Suggested split: PR-05 exposes 0x2AD9, handles 0x00 Request Control, and answers target opcodes with 0x05 or 0x02. PR-08 implements the targets.

#### 🟠 RR-12. OTA still arrives after the board is sealed *(before PR-04a)*
PR-04a is the permanent install in the console housing, but OTA appears only in PR-07. `autoCheck=false` has no enforcement point in any PR: the framework default is `true`, and factory reset sets it back to `true`. P-07 was not applied.

**Fix.** Move "manual OTA enabled and tested, `autoCheck` forced to false after config load and after factory reset" into PR-04a.

#### 🟠 RR-13. SPEC contradictions left from items marked "applied" *(before PR-03, PR-05, PR-06, PR-08)*
- **B-03 reconnect policy.**
  - §8.4 now says "indefinite slow retry at 15 s", but the §8.5 reconnect table still says *"max 5 attempts … give up"* (lines 1166–1169).
  - The recovery criteria disagree: 35 s at line 1228 versus ≤ 20 s at line 1117.
  - `slowRetryMs` is a persisted `BLEConfig` field (default 60 000). `getReconnectPolicy()` returns a *copy*, so `setSlowRetryMs(15000)` as written has no effect.
- **U-01 upload pipeline.**
  - §9.4 says *"Both phases run inside the same `fetchUpdate()`"*, while §10.4 says "split into separate fetches".
  - The §8.5 pass criterion still reads "upload completes within DataManager deadline (15 s)".
- **U-02 upload execution model.** §10.2 still shows `UploadManager` (an `ITickable`) calling a synchronous `upload()` in a loop, followed by a sentence saying the opposite. §3.4 is unchanged.
- **S-01 linear level model.** It survives in several places:
  - §3.3 (linear mV→level at 20 Hz);
  - §6.3 (levels "1–32 / 1–100");
  - Principle 7 ("faster, more precise … 100 levels");
  - §4.5 ("works up to 9V", Appendix B #16).
- **B-02 ERG override.** The rider-override latch semantics, the Status opcodes to send, and the web-command endpoint are still undefined.
- **U-04 Strava OAuth.** The `state` parameter, the callback-domain setup and persistence of rotated refresh tokens are still unspecified.
- **HR event path.** The note in §3.4 says HRMClient publishes events, but the §8.2 code calls `_hrmRepeater.publishHr()` three times (Appendix B #11).
- **The §3.5 sample does not match the framework.**
  - It uses `static AppFoundation app; app.begin()`, but the real signature is `Application::begin(const FrameworkConfig&)`.
  - It registers `ftmsServer`, `hrmRepeater` and `hrmClient`, which main.cpp never declares, while §3.4 says `EchenonModule` owns them. The SPEC needs to state the ownership pattern.
- **§1.1 and Principles 1 and 6 still describe the MITM board as the product.** Principle 6 is false for PR-04a.
- **Other stale text.** The device name is `Echenon` in PR-03 and PR-05 but `Echenon-XXXX` elsewhere (Appendix B #6). Coexistence verification is still labelled "PR-07" when it is now PR-09.

#### 🟠 RR-14. Hardware spec §7 still hard-wires what H-02 said must be switchable *(before DR-1 closes)*
- In §7.4.2 the Green line still runs to *"GND (direct connection)"*.
- The §7.5 table says "Always connected" three times.
- §7.9.1 shows Yellow "powered via R4" and Green "tied to GND".

A one-line note says the opposite of all of these. §3.4 is also unchanged: the motor's 800 mA is unmeasured, and the MP1584EN is described as "3 A continuous". Until DR-1, put a **"PROVISIONAL — superseded at DR-1, do not build"** banner on all of §7 and §8, and list a passive-tap-board section as a DR-1 output.

---

## 4. Scorecard: how the 2026-09-23 audit was applied

✅ applied · 🟨 partial or contradicted elsewhere · ❌ not applied or regressed · ⏸ correctly deferred to DR-1

| ID | Status | Note |
|---|---|---|
| A1 pin map / ADC1 | ❌ | Regressed (RR-02) |
| A2 motor sense analog | ✅ | |
| A3 one config model | 🟨 | RR-03 |
| A4 bench readiness | ❌ | RR-06 |
| A5 evidence convention | ✅ | README created; needs result columns (RR-08) |
| A6 `echenon.local` | ✅ | In PR-01 and §11.1 |
| H-01 | ✅ / ⏸ | |
| H-02 | 🟨 | RR-14 |
| H-03 | ⏸ | D3 recorded; hardware spec unchanged |
| H-04 | ❌ | RR-02 |
| H-05 | ⏸ | Scoped to PR-07 |
| H-06 | ✅ | Encoder Ch B concern added (RR-04) |
| S-01 | 🟨 | Directives in §5; §3.3 and §6.3 unchanged |
| M-01 | ✅ | §5.5 |
| M-02 | 🟨 | Values `[DR-1]`; §3.3 still says 20 Hz |
| S-02 | 🟨 | Placeholder flagged; NVS and §6.3 still indexed by level |
| P-01 | ❌ | Gate recorded as met without central-role evidence (RR-07) |
| B-01 | ✅ | Bit 2, 0x2AD6/0x2AD8, Request Control, mode-dependent flags |
| B-02 | 🟨 | State names only |
| B-03 | 🟨 | §8.4 fixed; §8.5 contradicts (RR-13) |
| B-04 | ✅ | PR-03 entry condition |
| U-01 | 🟨 | gz and sizing ✅; pipeline contradicted |
| U-02 | 🟨 | Sentence added; code and tables unchanged |
| U-03 | ✅ | §9.2 |
| U-04 | 🟨 | BYOA named; `state`, callback, token rotation missing |
| W-01 | ✅ | §11.5 |
| W-02 | ✅ | |
| P-02 | ✅ | DR-1 inserted; no artefact template (RR-08) |
| P-03 | ✅ | 9-PR graph |
| P-04 | ✅ | |
| P-05 | ✅ | |
| P-06 | ✅ / 🟨 | Progressive in PR-03 and PR-05; SPEC text still says PR-07 |
| P-07 | ❌ | RR-12 |
| P-08 | 🟨 | Native-test line added; §13 deleted (RR-01); ERG acceptance rephrased ✅ |
| P-09 | ✅ | |
| P-10 | 🟨 | Calibration moved to PR-02; `Calibration.h` and overlap wording remain |
| P-11 | 🟨 | PROGRAM.md links and env name fixed; PROGRAM-PLAN link, banners and untagged submodule remain |
| Appendix B | 🟨 | Fixed: #7, #13 (part), #14, #17. Not fixed: #1, #2, #3, #6, #8, #9, #11, #16 |

---

## 5. Minor

- 🟡 **RR-15. Broken links.** PROGRAM-PLAN.md links `../../../../docs/…`, one `..` too many (P-11 was not fixed).
- 🟡 **RR-16. Stale PA memory.** `memory/MEMORY.md` is stale: it says "PR-01/7", calls PR-04 the highest-risk PR, and was last updated 2026-09-20. This is the first file a PA reads (Core Directive 2).
- 🟡 **RR-17. Archive banners.** They don't name the SPEC sections that supersede them. Archived WO1, still listed as PR-01 prior art, prescribes `[env:echenon]` and `hardware/echenon/board.h`.
- 🟡 **RR-18. PR-07 verification.** It asks for "Override 100-level control", which contradicts SPEC §5.3 ("a goal to validate").
- 🟡 **RR-19. Unclear pointers in the register.** Phase 0 README row #13 cites "§3.5" without naming the document. The H-/S-codes don't link to the audit file.
- 🟡 **RR-20. Remove personal data.** The duplicated parts-list fragment contains a personal order number; remove it when deduplicating (RR-01).

---

## 6. Gate G0: repair checklist before PA hand-off (docs-only, about half a day)

| # | Action | Fixes |
|---|---|---|
| G0-1 | Restore §13 and the User Journeys from `2992155^`, renumbered to the 9-PR phases. Remove the orphan tables, the duplicate §12.3, the unclosed fence and `<ctrl94>`. Restore the §4 and §9 headings and the §4.6 wiring table. Turn directives into specification text. | RR-01 |
| G0-2 | Publish one Phase 0 pin map (GPIO 1–10, all ADC1) in SPEC §4.4. Point HW §5.3, §5.5 and §5.7 at it. Mark the Phase 3 map `[DR-1]` and fix the collisions in HW §7.3 and SPEC §3.2, §3.3 and §12.3. | RR-02 |
| G0-3 | Rewrite §12.2 and §12.3 using the template-app pattern with `framework/` paths. Adopt one `board.h` built from `board_contract.h`, with no `WITH_DISPLAY_U8G2` or `WITH_BUTTONS`. Write down the `HAS_*` / `WITH_*` split. | RR-03 |
| G0-4 | Amend `pr-01-signal-logger/PROJECT.md` with the following. | RR-04 to RR-08 |
| | • Two-mode capture design and the WO0 spike. | RR-04 |
| | • Series-resistor taps, no pulls, USB power in Phase 0/1. | RR-05 |
| | • Bench-ready entry condition. | RR-06 |
| | • Framework bump to `dev.82` in WO1 with BLE off. | RR-07 |
| | • 10 tap signals; exit = `docs/measurements/phase0/README.md` with Evidence and Result columns. | RR-08 |
| | • Scope gate reworded as "characterise"; add a µs/div pass. | RR-08 |
| G0-5 | Correct the PROGRAM.md 2026-09-25 log entry on BLEManager, and fix the PROGRAM-PLAN link depth. Refresh `memory/MEMORY.md`. Add a skeleton `reviews/DR-1-design-freeze.md` listing inputs (register rows → evidence) and outputs (audit §8.3). | RR-07, RR-08, RR-15, RR-16 |

After G0, **PR-01 is GO.** Before PR-02 or PR-03 starts, also clear RR-09, RR-10, RR-11 and the B-03 part of RR-13. The rest of RR-13 and RR-12 are due before the PR each item names. RR-14 must be cleared before DR-1 closes.

---

*End of review. Advisory only.*

---

## 7. Remediation authorization

**2026-09-25 — the human owner, in chat:** *"please spin up one or more sub-agents to remediate all these issues and correct the situation. Do not fix them yourself to help preserve your context."*

Scope: every finding in this review (RR-01 to RR-20). The work is **docs-only**: `SPEC.md`, `docs/*`, and the programme workspace. It follows the shared decisions in `tmp/remediation-brief.md`. No source code, branches, commits or submodule changes are made; the Chief Architect reviews the result and commits separately.

DR-1 decision record (created by this remediation): [DR-1-design-freeze.md](DR-1-design-freeze.md).
