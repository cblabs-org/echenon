---
schema_version: 1
pr_id: PR-09
github_pr: 20
audit_round: 5
head_sha: 9b861b6ef8a0cfaab0427b9d297a745cc11fb5c8
auditor_harness: antigravity
verdict: APPROVED
findings:
- id: NON-BLOCKING-1
  axis: spec_alignment
  file_path: docs/release-audit-v0.3.0-beta.1.md
  line_range: L206-L228
  summary: Section 4.2/4.3 (L206, L212, L216, L228) cites .pio/build/ex3-waveshare-nano/firmware.elf
    (xtensa-esp32s3-elf-size -A) yet records the earlier build_project (12:25:21 UTC)
    ex3-waveshare-nano sizes (Flash 1,798,083 B [28.6%], Total Image Size 1,798,340
    B, .flash.text 1,287,632 B) rather than the +284 B re-linked ELF flashed during
    upload_firmware (12:32:55 UTC) and verified on hardware in monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log:44
    (Flash 1,798,367 B [28.6%], Total image size 1,798,624 B, .flash.text 1,287,916
    B). In addition, the -diag (12:28:36) and -rig (12:30:45) ELF app_desc strings
    retain v0.3.0-dev.50 because CMake PROJECT_VER was not reconfigured after tagging
    v0.3.0-dev.51.
  remediation: Optionally update the ex3-waveshare-nano Flash/Total/.flash.text cells
    in docs/release-audit-v0.3.0-beta.1.md (L206, L216, L228) to 1,798,367 B (28.6%),
    1,798,624 B, and 1,287,916 B (or refresh all three environments during the owner-gated
    Step 4.2 clean rebuild after tagging v0.3.0-beta.1).
---

<!-- pa:audit pr_id=PR-09 round=5 head_sha=9b861b6ef8a0cfaab0427b9d297a745cc11fb5c8 verdict=APPROVED blocking=0 major=1 minor=0 -->

**Audit Verdict:** APPROVED (0 blocking, 1 non-blocking)

## Summary

Cold-firewall Round 5 verification of **PR-09** (`feat/pr-09-integration`, GitHub PR #20, `head_sha: 9b861b6ef8a0cfaab0427b9d297a745cc11fb5c8`) against `main`, `SPEC.md`, the locked `PROJECT.md` Part I Charter (`WO-09.1` through `WO-09.15`), and prior audit rounds (`AUDIT-round-1.md` through `AUDIT-round-4.md`) is **APPROVED** with **0 blocking and 1 non-blocking finding** (`NON-BLOCKING-1`).

Round 5 specifically verifies `WO-09.14` (`802768575082b2e324637d542d7470e9d3a20e9d`, receipt `rcpt_1ea7627b8a9e`, tag `v0.3.0-dev.51`) and `WO-09.15` (`86ab97cbd3d6b18acb93c98fec628684579b3a1a`, receipt `rcpt_662b7ae30cbf`) alongside the full 15-work-order PR diff (`125` changed files vs `main`, `0` charter violations):
- **Newlib-Nano `%zu` / `%llu` Specifier Elimination (`WO-09.14`):**
  - Repository-wide scan across `src/`, `hardware/`, `framework/src/`, `framework/modules/`, and `framework/include/` confirms `0` active `%zu`, `%llu`, `%z*`, `%ll*`, or `PRI*64` format specifiers (`src/echenon/SignalLogic.h:494` is an explanatory comment only).
  - `src/echenon/RidePersistence.cpp` replaces all three `%zu` occurrences (`L195-196`, `L326-327`, `L356-357`) with `%u` and `static_cast<unsigned>(...)`.
  - `src/echenon/SignalLogic.h:492-510` (`formatBurstCsvHeader`) formats `uint64_t burstStartUs` via a stack-only `char startUsBuf[21]` base-10 conversion (zero heap allocation) and `%s`, and `test/test_native/test_signal_logger.cpp:307-320` (`BurstCsvFormatHeaderAbove32BitTimestamp`) verifies `burstStartUs = 5000000000ULL` (`> 2^32`, `overflowCount = 7`, `perChanRate = 10416`).
- **Post-`DeviceCrypto` Uploader Boot Ordering & `_athleteId` Guard (`WO-09.14`):**
  - `src/main.cpp` removes `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` from pre-`app.begin(config)` setup (`L161-173`) and defers them until after `app.begin(config)` alongside `stravaUploader.begin()` immediately before `uploadQueue.begin()` (`L238-243`), keeping all pre-`app.begin(config)` wiring (`setKeyManager`, `setSecretRegistrar`, `setMetadataSaveCallback`, `uploadQueue.registerSink`, `app.registerModule`, `sessionOverlay.set*` at `L107-190`) intact so `ConfigManager` module registration and `applyConfig()` ordering are preserved.
  - `src/echenon/IntervalsIcuUploader.cpp:250-267` (`loadCredentials()`) guards `_athleteId` with `if (!storedAthleteId.empty())` (`L257-260`) so an absent `KEY_ATHLETE_ID` entry in `ISecretStore` does not clobber `_athleteId` populated earlier by `applyConfig()`, verified by `test/test_native/test_intervals_uploader.cpp:430-452` (`ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId`).
- **Release Audit Synchronization (`WO-09.15`):**
  - `docs/release-audit-v0.3.0-beta.1.md` updates evaluated commit/tag metadata to `v0.3.0-dev.51` (`802768575082b2e324637d542d7470e9d3a20e9d`), records `271 / 271` native unit tests, documents the `WO-09.14` fixes and live `/dev/cu.usbmodem101` `v0.3.0-dev.51` UART verification (`monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log`: `App version: v0.3.0-dev.51`, `0` panics/WDT resets, `0` `KeyManager: DeviceCrypto not initialized.` errors, `I (1194) RidePersistence: Scanned 0 persisted rides (0 bytes total)`, boot free heap `8,599,311 B` / min-free `8,558,752 B`, `uptime=30s`/`60s` `free_heap=8,376,307 B`, `dram=108,775 B`, `dram_min=75,880 B`, `DataManager` HWM `10,732 B free (of 12,288)`), preserves the `v0.3.0-dev.50` SoftAP/`#329` check and `v0.3.0-dev.49` `4,944 s` STA soak, and leaves Launch Gate Step `2.1` `[x]` with Steps `3`-`5` `[ ]`.
  - One non-blocking documentation delta (`NON-BLOCKING-1`, `+284 B`) exists in `docs/release-audit-v0.3.0-beta.1.md:206,216,228` between the `12:25:21` `build_project` output and the `12:32:55` `upload_firmware` re-linked ELF on disk.



## Five-Axis Evaluation

### 1. `spec_alignment` — PASS (`NON-BLOCKING-1`)
- **Format Safety & Boot Ordering (`WO-09.14`):** `RidePersistence.cpp:195-196, 326-327, 356-357`, `SignalLogic.h:492-510`, `IntervalsIcuUploader.cpp:255-260`, and `src/main.cpp:161-173, 238-243` match `WO-09.14` requirements and `SPEC.md` upload/diagnostics behavior.
- **Release Audit & Launch Gate (`WO-09.15`):** `docs/release-audit-v0.3.0-beta.1.md` accurately cites `WO-09.14`, `v0.3.0-dev.51` live UART logs, `271/271` tests, Launch Gate Step `2.1` `[x]`, and owner-gated Steps `3`-`5` `[ ]`. See `NON-BLOCKING-1` for the `+284 B` ELF size delta between the pre-upload `build_project` run (`12:25:21`) and the flashed `upload_firmware` ELF (`12:32:55`).

### 2. `invariants` — PASS
- **Newlib-Nano & Encapsulation:** `0` `%zu`/`%llu`/`%z`/`%ll` format specifiers remain in `src/`, `hardware/`, `framework/src/`, `framework/modules/`, or `framework/include/`; `0` `extern Application` in `src/`; `1` `#ifdef UNIT_TEST`-guarded `extern uint32_t millis();` (`src/echenon/FTMSServer.cpp:37`); `13` `// GRAVE-05:` commented `std::function` uses; `63/63` C++ files in `src/` carry Apache-2.0 SPDX headers; `23` `AuthManager::checkPortalAuth` call sites; `0` `/api/echenon/rig/` routes; `0` bare hex colors in `web/*.js`; `; -D WITH_OTA` disabled in `platformio.ini:67`.
- **Module Lifecycle & Keystore Ordering:** Pre-`app.begin(config)` module registration (`src/main.cpp:107-190`) and post-`DeviceCrypto` uploader initialization (`src/main.cpp:238-243`) uphold the framework lock contract and ensure `KeyManager` / `Redactor` readiness (`0` `DeviceCrypto not initialized.` UART errors in `v0.3.0-dev.51`).
- **Charter & Work Order Integrity:** `schemas.verify_charter_hash(PROJECT.md)` is `True`, and `schemas.verify_spec_hash` returns `True` for all 15 Work Orders (`WO-09.1` through `WO-09.15`).
- **Rule 03 Versioning Governance:** Only `dev` tags (`v0.3.0-dev.51`) exist; `v0.3.0-beta.1` tagging, merging to `main`, and `deploy_release.py` remain strictly owner-gated.

### 3. `edge_cases` — PASS
- **64-Bit Timestamp Boundary (`SignalLogic.h:496-503`):** Stack base-10 conversion uses `char startUsBuf[21]` (sufficient for max 20 decimal digits of `UINT64_MAX` plus NUL) with `do { ... } while (tmp != 0u)` so `0ULL`, values `> 2^32` (`5000000000ULL`), and `UINT64_MAX` format safely with zero heap allocation and no 32-bit `va_list` corruption.
- **Credential Merge Preservation (`IntervalsIcuUploader.cpp:255-260`):** Preserves `_athleteId` populated from `/config/mod/echenon_intervals.json` (`applyConfig()`) when `KEY_ATHLETE_ID` is absent from `KeyManager`, while still honoring non-empty keystore values and `setApiCredentials()`. `StravaUploader::loadCredentials()` and `WahooUploader::loadCredentials()` do not overwrite config-backed fields.

### 4. `test_quality` — PASS
- **Native Verification Floor (`platformio-mcp_platformio` `run_tests`):** Executed on `native` environment (`20` test files in `test/test_native/`): `271` test cases, `271` passed, `0` failed, `0` skipped in `00:00:00.592`, including `SignalLoggerCsvFormatTest.BurstCsvFormatHeaderAbove32BitTimestamp` and `IntervalsIcuUploaderTest.ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId`.
- **ELF & Serial Verification:** `xtensa-esp32s3-elf-size -A` and `xtensa-esp32s3-elf-objdump -t` confirm exact matches for `ex3-waveshare-nano-diag` (`59,836 B` RAM, `DIRAM 161,947 B`, `.flash.text 1,300,964 B`, `.flash.rodata 375,288 B`), `ex3-waveshare-nano-rig` (`59,604 B` RAM, `DIRAM 161,395 B`, `.flash.text 1,292,852 B`, `.flash.rodata 372,824 B`), `ex3-waveshare-nano` static DRAM (`59,516 B` = `.dram0.data 20,428 B` + `.dram0.bss 39,088 B`, `DIRAM 161,307 B`, `IRAM 16,384 B`, `.iram0.text 117,147 B`, `.flash.rodata 371,848 B`), and all top-5 `.dram0` symbols (`g_cnxMgr 3,880 B`, `app 3,344 B`, `Redactor::_instance 3,172 B`, `port_IntStack 3,072 B`, `echenonModule 2,872 B`, plus `mcpServer 104 B`). Live UART log `monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log` confirms every metric cited in `docs/release-audit-v0.3.0-beta.1.md` (with `+284 B` `.flash.text` tracked in `NON-BLOCKING-1`).

### 5. `scope_hygiene` — PASS
- **Charter Boundary & Submodule Cleanliness:** All `125` files in `git diff --name-status main..9b861b6ef8a0cfaab0427b9d297a745cc11fb5c8` reside strictly within locked `charter.allowed_modules` (plus `.agents/plugins/principal-architect-workspace/pr-09-integration/`). `git -C framework status --porcelain` shows `0` dirty files, and `framework` resolves to `02d0b2dea141313a0ce06b62f75bf2701559990a` (`v1.0.0-beta.2`).
- **Privacy & Secret Hygiene:** `git grep` across the repository outside `.agents/` and `framework/` confirms `0` matches for redacted home WiFi SSIDs, BSSID OUIs, or personal session URLs. Release docs (`docs/release-audit-v0.3.0-beta.1.md`, `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `SPEC.md`, `docs/beta-build-guide.md`) contain `0` non-SoftAP private LAN IPs or MAC addresses.



## Findings

### `NON-BLOCKING-1` — Pre-upload vs flashed `ex3-waveshare-nano` Flash size delta (`+284 B`) in `docs/release-audit-v0.3.0-beta.1.md`
- **Axis:** `spec_alignment`
- **Location:** `docs/release-audit-v0.3.0-beta.1.md:206-228`
- **Summary:** Section 4.2 and 4.3 (`L206`, `L212`, `L216`, `L228`) cite `.pio/build/ex3-waveshare-nano/firmware.elf` (`xtensa-esp32s3-elf-size -A`) yet report the earlier `build_project` (`12:25:21 UTC`) `ex3-waveshare-nano` figures (`Flash: 1,798,083 B` [`28.6%`], `Total Image Size: 1,798,340 B`, `.flash.text: 1,287,632 B`) rather than the re-linked ELF produced and flashed during `upload_firmware` (`12:32:55 UTC`, `monitor-2026-11-17T12-33-56-554Z-9d90ddd7.log:44`): `Flash: 1,798,367 B` (`28.6%`), `Total image size: 1,798,624 B`, `.flash.text: 1,287,916 B` (`+284 B`, unchanged `28.6%` flash percentage and identical `59,516 B` static DRAM). Additionally, `-diag` (`12:28:36`) and `-rig` (`12:30:45`) ELFs retain `v0.3.0-dev.50` in `esp_app_desc` due to CMake `PROJECT_VER` caching after tagging `v0.3.0-dev.51`.
- **Remediation:** Optionally update the `ex3-waveshare-nano` Flash, Total Image Size, and `.flash.text` entries in `docs/release-audit-v0.3.0-beta.1.md` (`L206`, `L216`, `L228`) to `1,798,367 B` (`28.6%`), `1,798,624 B`, and `1,287,916 B`, or refresh all three build environments during the owner-gated Step 4.2 clean rebuild after cutting `v0.3.0-beta.1`.
