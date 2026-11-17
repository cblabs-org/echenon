---
schema_version: 1
wo_id: WO-09.14
pr_id: PR-09
title: Fix Newlib-Nano %zu/%llu Format Bugs and Post-DeviceCrypto Uploader Boot Ordering
complexity: standard
status: PENDING
allowed_files:
- src/echenon/RidePersistence.cpp
- src/echenon/SignalLogic.h
- src/echenon/IntervalsIcuUploader.cpp
- src/main.cpp
- test/test_native/test_signal_logger.cpp
- test/test_native/test_intervals_uploader.cpp
forbidden_files:
- framework/**
- hardware/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 971e609d0a46b80d17174491d46699c379f300d9bc83fe07728dcf935360f117
completion: null
---

# WO-09.14: Fix Newlib-Nano `%zu`/`%llu` Format Bugs and Post-DeviceCrypto Uploader Boot Ordering

## Part I — Specification & Strict Boundary Box

### 1. Objective
Clean up the remaining 3 newlib-nano / boot-keystore technical debt bugs surfaced during the `v1.0.0-beta.2` alignment and live hardware serial verification:
1. **`src/echenon/RidePersistence.cpp` (`L195`, `L325`, `L355`):** Replace unsupported C99 `%zu` (`size_t`) format specifiers under `CONFIG_NEWLIB_NANO_FORMAT=y` with `%u` + `static_cast<unsigned>(...)`.
2. **`src/echenon/SignalLogic.h` (`formatBurstCsvHeader`, `L492-500`) + `test/test_native/test_signal_logger.cpp`:** Replace unsafe `%llu` (`uint64_t burstStartUs`, which corrupts `va_list` under 32-bit `newlib-nano` and risks a `LoadProhibited` crash on `%s` when `burstStartUs >= 2^32` after `71.5 min` uptime) with a zero-heap stack base-10 decimal buffer (`char startUsBuf[21]`) formatted via `%s`, plus add a `> 32-bit` (`5000000000ULL`) unit test.
3. **`src/main.cpp` (`L163`, `L172`, `L240-242`), `src/echenon/IntervalsIcuUploader.cpp` (`L255-262`), & `test/test_native/test_intervals_uploader.cpp`:** Defer `echenonModule.intervalsUploader().begin()` and `echenonModule.wahooUploader().begin()` until after `app.begin(config)` (right alongside `stravaUploader.begin()` so `DeviceCrypto` / `KeyManager` are initialized — eliminating the 8 `E (911) KeyManager: DeviceCrypto not initialized.` boot errors and allowing saved Intervals/Wahoo secrets to load & register with `Redactor`), and guard `_athleteId` in `IntervalsIcuUploader::loadCredentials()` so an absent `KEY_ATHLETE_ID` secret does not overwrite `_athleteId` previously loaded via `applyConfig()`.

### 2. Required Code Changes

#### A. `src/echenon/RidePersistence.cpp`
- Line `195`: replace `(%zu bytes)` with `(%u bytes)` and wrap `decompressed.size()` in `static_cast<unsigned>(decompressed.size())`.
- Line `325`: replace `(%zu bytes)` with `(%u bytes)` and wrap `decompressed.size()` in `static_cast<unsigned>(decompressed.size())`.
- Line `355`: replace `Scanned %zu persisted rides` with `Scanned %u persisted rides` and wrap `_storedRides.size()` in `static_cast<unsigned>(_storedRides.size())`.

#### B. `src/echenon/SignalLogic.h` & `test/test_native/test_signal_logger.cpp`
- In `formatBurstCsvHeader(uint32_t perChanRate, uint64_t burstStartUs, uint32_t overflowCount, char* buf, size_t bufSize)` (`src/echenon/SignalLogic.h:492-500`):
  - Format `burstStartUs` into a stack buffer `char startUsBuf[21];` (`uint64_t` max is 20 digits + NUL) without heap allocation:
    ```cpp
    char startUsBuf[21];
    char* p = startUsBuf + sizeof(startUsBuf) - 1;
    *p = '\0';
    uint64_t tmp = burstStartUs;
    do {
        *--p = static_cast<char>('0' + (tmp % 10u));
        tmp /= 10u;
    } while (tmp != 0u);
    ```
  - Pass `p` to `"# sample_rate_per_channel_hz=%u,start_time_us=%s,overflow_count=%u\n%s"` in `snprintf`.
- In `test/test_native/test_signal_logger.cpp`:
  - Keep existing `BurstCsvFormat` test intact and add a unit test (or assertion) covering `burstStartUs = 5000000000ULL` (`> 2^32`, `overflowCount = 7`, `perChanRate = 10416`) verifying exact output `# sample_rate_per_channel_hz=10416,start_time_us=5000000000,overflow_count=7\n` followed by `kBurstCsvColumns`.

#### C. `src/main.cpp`, `src/echenon/IntervalsIcuUploader.cpp`, & `test/test_native/test_intervals_uploader.cpp`
- In `src/main.cpp`:
  - Remove `echenonModule.intervalsUploader().begin();` (`L163`) and `echenonModule.wahooUploader().begin();` (`L172`) from pre-`app.begin(config)` module wiring (keep `setKeyManager`, `setSecretRegistrar`, `setMetadataSaveCallback`, `uploadQueue.registerSink`, `app.registerModule`, `sessionOverlay.set*` pre-`app.begin(config)` so `ConfigManager` still registers module slots `4`, `5`, `6` cleanly!).
  - Call `echenonModule.intervalsUploader().begin();` and `echenonModule.wahooUploader().begin();` inside `#ifdef WITH_SESSION` right next to `stravaUploader.begin();` (after `app.begin(config)`, where `KeyManager` / `DeviceCrypto` and `/config` `applyConfig()` have already run). Note: place them just before `uploadQueue.begin();` alongside `stravaUploader.begin();` (`L240-243`).
- In `src/echenon/IntervalsIcuUploader.cpp` (`loadCredentials()`, `L250-263`):
  - Replace unconditional `_athleteId = _secretStore->getSecret(KEY_ATHLETE_ID);` with:
    ```cpp
    const std::string storedAthleteId = _secretStore->getSecret(KEY_ATHLETE_ID);
    if (!storedAthleteId.empty()) {
        _athleteId = storedAthleteId;
    }
    ```
    so an empty keystore lookup does not clobber `_athleteId` populated earlier by `applyConfig()`.
- In `test/test_native/test_intervals_uploader.cpp`:
  - Add a new `TEST(IntervalsIcuUploaderTest, ApplyConfigPreservesAthleteIdWhenKeystoreLacksAthleteId)` test verifying that calling `applyConfig()` (setting `athlete_id = "i99999"`, `enabled = true`) followed by `begin()` / `loadCredentials()` when `ISecretStore` only holds `KEY_API_KEY` (no `KEY_ATHLETE_ID`) preserves `_athleteId == "i99999"` and `isConfigured() == true`.

### 3. Boundaries & Verification
- Edit ONLY the 6 `allowed_files` listed in frontmatter.
- Preserve existing Apache-2.0 headers and code comments unrelated to the fix.
- Verify `git diff --stat` shows your expected edits BEFORE calling `wo verify`.
- Verify via `python3 .agents/plugins/principal-architect/scripts/pa_cli.py wo verify WO-09.14` (`271 / 271` native tests passing).

## Completion Report
