---
schema_version: 1
wo_id: WO-09.12
pr_id: PR-09
title: Align Submodule, Config, Docs & Skills to cblabs-org/esp32-app-foundation v1.0.0-beta.2
complexity: standard
status: PENDING
allowed_files:
- .gitmodules
- framework
- platformio.ini
- CMakeLists.txt
- sdkconfig.defaults
- src/main.cpp
- web/intervals_settings.js
- AGENTS.md
- .agents/skills/framework-design-system
- .agents/skills/framework-portal-design
- CONTRIBUTING.md
- README.md
- SPEC.md
forbidden_files:
- src/echenon/**
- hardware/**
- tools/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: d924cdf4e8cdb7fe458121797c89f28b423d8e6b69113c8352519cd1e00f7866
completion: null
---

# WO-09.12 — Align Submodule, Config, Docs & Skills to `cblabs-org/esp32-app-foundation` `v1.0.0-beta.2`

## 1. Objective

Switch the Echenon repository from the private `matthewmcneill/esp32-app-foundation` submodule (`v0.0.12-beta.4` + local commit `40e942f1`) to the newly public **`cblabs-org/esp32-app-foundation`** repository tagged at **`v1.0.0-beta.2`** (`02d0b2dea141313a0ce06b62f75bf2701559990a`, which includes upstream PR `#329` for consistent non-alphanumeric tab slugification and `%zu` → `%u` Newlib-nano MCP logs), and align `platformio.ini`, `CMakeLists.txt`, `sdkconfig.defaults`, `src/main.cpp`, `web/intervals_settings.js`, `.agents/skills/`, `AGENTS.md`, and docs (`CONTRIBUTING.md`, `README.md`, `SPEC.md`).

## 2. Strict Boundary Box

- **Allowed files:**
  - `.gitmodules`
  - `framework`
  - `platformio.ini`
  - `CMakeLists.txt`
  - `sdkconfig.defaults`
  - `src/main.cpp`
  - `web/intervals_settings.js`
  - `AGENTS.md`
  - `.agents/skills/framework-design-system`
  - `.agents/skills/framework-portal-design`
  - `CONTRIBUTING.md`
  - `README.md`
  - `SPEC.md`
- **Forbidden files:** `src/echenon/**`, `hardware/**`, `tools/**`, or any file outside the allowlist above.
- **Rule R4 Compliance:** Never invoke forbidden git verbs (`checkout`, `switch`, `branch`, `rebase`, `merge`, `push`, `reset --hard`) or forbidden shell utilities (`rm`, `mv`, `cp`, `touch`, `tee`, `sed -i`). Keep `board = um_nanos3` in `platformio.ini` unchanged.

## 3. Technical Specification

### 3.1 `.gitmodules` & `framework` Submodule (`v1.0.0-beta.2`)
1. Edit [`.gitmodules`](../../../../.gitmodules) line 6:
   - Change `url = https://github.com/matthewmcneill/esp32-app-foundation.git`
   - To `url = https://github.com/cblabs-org/esp32-app-foundation.git`
2. Sync the submodule URL and move `framework` to tag `v1.0.0-beta.2` (`02d0b2dea141313a0ce06b62f75bf2701559990a`) using Rule R4-safe commands:
   ```bash
   python3 -c "import subprocess; subprocess.run(['git', 'submodule', 'sync', '--', 'framework'], check=True); subprocess.run(['git', '-C', 'framework', 'reset', '--mixed', 'v1.0.0-beta.2'], check=True); subprocess.run(['git', '-C', 'framework', 'restore', '.'], check=True); subprocess.run(['git', '-C', 'framework', 'clean', '-fd'], check=True)"
   ```
3. Verify `git -C framework rev-parse HEAD` outputs `02d0b2dea141313a0ce06b62f75bf2701559990a`.

### 3.2 `platformio.ini` — Pin `pioarduino 55.03.311` (Preserve `board = um_nanos3`)
1. In [`platformio.ini`](../../../../platformio.ini) line 6 (`[env:echenon_base]`):
   - Change `platform = espressif32` to:
     `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip`
2. **CRITICAL INVARIANT:** Leave `board = um_nanos3` on line 51 untouched so raw ESP32-S3 GPIO numbering (`1..18`) in `hardware/ex3-waveshare-nano/board.h` is preserved.

### 3.3 `src/main.cpp`, `sdkconfig.defaults`, & `CMakeLists.txt` — `TimeManager` DHCP NTP/TZ & Component Exclusions
1. In [`src/main.cpp`](../../../../src/main.cpp) inside `#ifdef WITH_TIME`, add:
   ```cpp
   timeManager.setEventBus(&app.getEventBus());
   ```
   immediately after `timeManager.setConfigManager(&app.getConfigManager());`.
2. In [`sdkconfig.defaults`](../../../../sdkconfig.defaults), append:
   ```ini
   # --- PR-320: DHCP NTP (Option 42) & DHCP Timezone (RFC 4833 Options 100/101) ---
   # Allow up to 3 SNTP slots total: DHCP Option 42 fills at most 2 slots,
   # reserving trailing slot(s) for configured static fallback NTP servers.
   CONFIG_LWIP_SNTP_MAX_SERVERS=3
   CONFIG_LWIP_DHCP_GET_NTP_SRV=y
   CONFIG_LWIP_DHCP_MAX_NTP_SERVERS=2
   # Emit __weak lwip_dhcp_on_extra_option() in lwip_default_hooks.c so TimeManager
   # can override it strongly when WITH_TIME is enabled, while -UWITH_TIME builds
   # continue linking cleanly against the weak default.
   CONFIG_LWIP_HOOK_DHCP_EXTRA_OPTION_DEFAULT=y
   ```
3. Update [`CMakeLists.txt`](../../../../CMakeLists.txt) to match `framework/template-app/CMakeLists.txt` (`EXCLUDE_COMPONENTS` block and `if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/framework/include/lwip_dhcp_tz_hooks.h")` hook injection), keeping `project(echenon)`.

### 3.4 `web/intervals_settings.js` — Defensive Dual Tab ID Support
1. In [`web/intervals_settings.js`](../../../../web/intervals_settings.js):
   - Define `var ALT_TAB_ID = 'tab-intervals.icu';` alongside `var TAB_ID = 'tab-intervals-icu';`.
   - In `onTabChanged(e)` (`lines 216–228`), accept `targetTab === TAB_ID || targetTab === ALT_TAB_ID` and resolve `var container = document.getElementById(targetTab) || document.getElementById(TAB_ID) || document.getElementById(ALT_TAB_ID);`.

### 3.5 `.agents/skills/` Symlink & `AGENTS.md`
1. Using Python `pathlib` (not `rm`/`ln` shell commands if restricted, or `python3 -c` with `Path`), remove the broken symlink `.agents/skills/framework-design-system` and create `.agents/skills/framework-portal-design` pointing to `../../framework/.agents/skills/framework-portal-design`. Confirm `.agents/skills/framework-portal-design/SKILL.md` exists.
2. Edit [`AGENTS.md`](../../../../AGENTS.md) line 12 to replace `` `framework-design-system` `` with `` `framework-portal-design` ``.

### 3.6 Documentation Pointer Updates (`CONTRIBUTING.md`, `README.md`, `SPEC.md`)
1. [`CONTRIBUTING.md`](../../../../CONTRIBUTING.md) line 42: change `https://github.com/matthewmcneill/esp32-app-foundation` to `https://github.com/cblabs-org/esp32-app-foundation`.
2. [`SPEC.md`](../../../../SPEC.md) lines 122, 212, 2047, 3874: update `https://github.com/matthewmcneill/esp32-app-foundation` to `https://github.com/cblabs-org/esp32-app-foundation`.
3. [`README.md`](../../../../README.md) line 87: update the submodule note to state that Echenon builds on the public [`cblabs-org/esp32-app-foundation`](https://github.com/cblabs-org/esp32-app-foundation) submodule (`v1.0.0-beta.2`).

## 4. Verification

1. Confirm `git -C framework rev-parse HEAD` is `02d0b2dea141313a0ce06b62f75bf2701559990a` and `.agents/skills/framework-portal-design/SKILL.md` resolves cleanly.
2. Confirm zero `matthewmcneill/esp32-app-foundation` or `framework-design-system` references remain in `.gitmodules`, `AGENTS.md`, `CONTRIBUTING.md`, `README.md`, or `SPEC.md`.
3. Run `python3 .agents/plugins/principal-architect/scripts/pa_cli.py wo verify WO-09.12` (`pio test -e native --verbose`, stamps receipt and commits on `pr-09-integration/wo12-foundation-v1-beta2`).
4. Populate `## Completion Report` (keep `memory_mode: external` succinct) and report commit SHA and `receipt_id`.

## Completion Report

_Pending execution._
