---
schema_version: 1
wo_id: WO-09.8
pr_id: PR-09
title: Multi-Env Firmware Builds, Native Tests & Beta Release Audit Report
complexity: interactive
status: COMPLETED
allowed_files:
- docs/release-audit-v0.3.0-beta.1.md
- test/**
forbidden_files:
- framework/**
- src/**
- web/**
required_skills:
- framework-expert
- pio-manager
- hardware-in-the-loop-test
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 5543541990d44ddec1e3eab3687db0eb2b87011d915fa08f4d44ce28594f22c4
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_30f66aaa6250
  commit_sha: 4ba5c82eb287daff0f9ae6507f2a2a6b7bf54d5e
  files_modified:
  - docs/release-audit-v0.3.0-beta.1.md
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.585 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-09.8 — Multi-Env Firmware Builds, Native Tests & Beta Release Audit Report

## 1. Objective

Run the full **`Beta` channel 10-Dimension Release Quality Gate** from [framework-expert](.agents/skills/framework-expert/SKILL.md) across the integrated codebase (after `WO-09.1`–`WO-09.7`), execute native unit tests + all 3 firmware environment builds + ELF static DRAM/Flash inspection + hardware device check, close any native test coverage gaps for `WO-09.1`, and publish the formal audit record at [docs/release-audit-v0.3.0-beta.1.md](docs/release-audit-v0.3.0-beta.1.md).

## 2. Strict Boundary Box

- **Allowed files:** `docs/release-audit-v0.3.0-beta.1.md`, `test/**`.
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, or any source/doc file outside `docs/release-audit-v0.3.0-beta.1.md` and `test/`.
- **PlatformIO MCP rule:** Execute ALL PlatformIO builds, tests, size reports, and device checks exclusively via `call_mcp_tool` (`ServerName: "platformio-mcp_platformio"` — `run_tests`, `build_project`, `size_report`, `list_devices`, `start_monitor` / `agent_flash_monitor_verify` only if device attached and user-safe). Never invoke `pio` or `esptool` directly in `run_command`.
- **Truthfulness rule:** Report ONLY directly observed test counts, Flash/RAM byte sizes, grep counts, and device status. Never extrapolate or fabricate hardware soak telemetry if no board is connected; clearly distinguish verified checks from any pre-tag owner bench gate.
- **Shell hygiene:** Never use bash `$VAR` loops or `/tmp` paths in `run_command`.

## 3. Technical Specification

### 3.1 Read Framework Expert Release Audit Guidance
1. Read `.agents/skills/framework-expert/SKILL.md` and its release quality gate / compliance audit reference doc(s) so all 10 Dimensions for the **Beta** channel are evaluated accurately.

### 3.2 Native Unit Test Coverage Check (`test/test_native/**`)
1. Inspect existing tests in `test/` (`ls -1 test/` or `test/test_native/`) and run `run_tests` (`environment: "native"`).
2. If practical without modifying `src/` (only touching `test/**`), add/extend unit tests covering key `WO-09.1` calibration reset/persistence logic or MCP handler contracts if testable natively; otherwise ensure the full existing `native` test suite runs cleanly after final edits.

### 3.3 Full Verification Matrix (via `platformio-mcp_platformio`)
1. **`run_tests` (`environment: "native"`):** Record exact total tests, passed, failed, duration.
2. **`build_project` across all 3 ESP32-S3 environments:**
   - `ex3-waveshare-nano`
   - `ex3-waveshare-nano-diag`
   - `ex3-waveshare-nano-rig`
   Record exact pass/fail and compiler warnings for each.
3. **`size_report` (`environment: "ex3-waveshare-nano"`):**
   - Record Flash usage/limit and static RAM usage/limit.
   - Inspect `.pio/build/ex3-waveshare-nano/firmware.elf` `.bss`/`.data` symbols (using PlatformIO's xtensa `size` / `nm` binary inside `~/.platformio/packages/`) to confirm static BSS/DATA overhead and generous free DRAM margin (`> 40 KB`).
4. **Hardware Presence & Bench Verification (`list_devices`):**
   - Call `list_devices` via `platformio-mcp_platformio`.
   - **If an ESP32-S3-Nano is attached on USB-C** (and user hardware is connected cleanly without VIN active): inspect non-destructive serial health / boot log if safe, or flash/verify carefully; record exact serial evidence.
   - **If NO hardware device is attached:** record that no USB device is attached in this session, cite existing committed Phase 2 / Phase 3 HIL verification evidence (`docs/measurements/phase3/evidence/pr19-hardware-verification.md`, `pr21-wahoo-verification.md`), and explicitly include a **mandatory Pre-Tag 60 s Live Bench Soak Gate** in the Owner Launch Checklist (boot marker, heap `> 40 KB`, portal auth + `/api/mcp` smoke check, FTMS advertising, clean reset) before running `git tag v0.3.0-beta.1`.
5. **Static Hygiene & Security Audits (`run_command`):**
   - `grep -rn "extern Application" src/` -> confirm **0 matches**.
   - `grep -rn "std::function" src/` -> verify ONLY documented/reviewed cases (`GRAVE-05` justification comments where unavoidable in BLE/callbacks, zero in MCP tools / hot paths).
   - `git grep -n -E "<REDACTED_SSID>|XX:XX:XX|<redacted-session-link>" || true` -> confirm **0 matches**.
   - `grep -rn "/api/echenon/rig/" src/ web/ tools/` -> confirm **0 matches** (HTTP rig endpoints completely removed).

### 3.4 Write `docs/release-audit-v0.3.0-beta.1.md`
Include:
- **Release Metadata:** Version `v0.3.0-beta.1` (target tag, owner-gated per Rule 03), Hardware target (`v1 Passive Tap Board / Waveshare ESP32-S3-Nano`), Channel (`Beta`), `WITH_OTA` status (`Disabled for beta — USB-C updates with SW0 VIN interlock OFF`).
- **10-Dimension Release Audit Matrix:** Status (`PASS` / `PASS WITH BETA WAIVER` / `OWNER GATE`), exact evidence file/check for each of the 10 framework dimensions.
- **Verification Output Summary:** Exact tables for `native` tests, 3-env firmware builds, Flash & RAM budgets, ELF symbol summary, privacy & encapsulation greps, and HIL status.
- **Owner Launch Gate Checklist (Explicit Execution Order for Human):**
  1. Connect ESP32-S3-Nano (`SW0` isolating `VIN`) and run 60 s live bench soak on `v0.3.0-beta.1` candidate (`ex3-waveshare-nano`).
  2. Confirm `matthewmcneill/esp32-app-foundation` submodule repository is flipped public (parallel owner process).
  3. Review Step 6A Audit report, merge `PR-09` (`feat/pr-09-integration`) to `main`.
  4. Tag `v0.3.0-beta.1` (after checking latest `git tag` and owner sign-off per `.agents/rules/03-versioning.md`).
  5. Flip `cblabs-org/echenon` repository public and publish GitHub Beta Release with pre-built firmware binaries / release notes.

## 4. Verification

1. Re-run `run_tests` (`environment: "native"`) after any final edits to `test/**`.
2. Run `~/.local/bin/pa wo verify WO-09.8` (executes `pio test -e native --verbose`, stamps receipt, commits on `pr-09-integration/wo8-release-audit-verify`).
3. Post completion summary (metrics + audit verdict + any open owner gate items) to `@coordinator` via `post_message`.

## Completion Report

_Pending execution._
