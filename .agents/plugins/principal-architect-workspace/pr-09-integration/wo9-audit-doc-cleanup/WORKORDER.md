---
schema_version: 1
wo_id: WO-09.9
pr_id: PR-09
title: Mask Privacy Grep Literals & Align SignalLogger Routes in Release Audit Doc
complexity: mechanical
status: COMPLETED
allowed_files:
- docs/release-audit-v0.3.0-beta.1.md
forbidden_files:
- framework/**
- src/**
- web/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: 1577c8cb2c8ea9b876a40ff8c18000353c40225538c0d7585ee50844793b0442
completion:
  status: COMPLETED
  attempt_num: 1
  receipt_id: rcpt_70001776b36b
  commit_sha: 6d785f89008abbecb0f07d5d27167b49594e9d4b
  files_modified:
  - docs/release-audit-v0.3.0-beta.1.md
  verification_summary: 'pio test -e native --verbose: ================ 269 test cases:
    269 succeeded in 00:00:00.653 ================'
  out_of_scope_findings: []
  escalation_request: null
  decisions_made: []
  dead_ends: []
---

# WO-09.9 — Mask Privacy Grep Literals & Align SignalLogger Routes in Release Audit Doc

## 1. Objective

Close both non-blocking documentation findings from `AUDIT-round-1.md` (`NON-BLOCKING-1` and `NON-BLOCKING-2`) inside [docs/release-audit-v0.3.0-beta.1.md](docs/release-audit-v0.3.0-beta.1.md) before merging `PR-09` and flipping the repository public.

## 2. Strict Boundary Box

- **Allowed files:** `docs/release-audit-v0.3.0-beta.1.md` only.
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, or any file outside `docs/release-audit-v0.3.0-beta.1.md`.
- **Privacy rule:** Do NOT leave any literal home WiFi SSID or router BSSID OUI substrings anywhere inside `docs/release-audit-v0.3.0-beta.1.md`.
- **Shell hygiene:** Never use bash `$VAR` loops, `/tmp`, or raw `pio` CLI in `run_command`.

## 3. Technical Specification

Read lines `100–120` and `205–220` of `docs/release-audit-v0.3.0-beta.1.md` and `src/echenon/SignalLogger.cpp` (`360–510`), then apply these exact 3 fixes:

1. **Line 106 (`NON-BLOCKING-2`):** Replace the legacy route names (`/api/echenon/CaptureConfig` and `/api/echenon/SignalLog/*`) with the exact 5 routes registered in `SignalLogger::registerRoutes()` (`src/echenon/SignalLogger.cpp:363-505`):
   ``- [`src/echenon/SignalLogger.cpp`](../src/echenon/SignalLogger.cpp#L363-L505) — 5 routes (`WITH_SIGNAL_LOGGER` diagnostic builds): `/api/echenon/status` (`GET`), `/api/echenon/burst/start` (`POST`), `/api/echenon/ride-log.csv` (`GET`), `/api/echenon/events.csv` (`GET`), and `/api/echenon/burst.csv` (`GET`).``
2. **Line 114 (`NON-BLOCKING-1`):** Mask the literal home WiFi SSID and router BSSID OUI tokens inside the quoted `git grep` command string using `<REDACTED_SSID>` and `<REDACTED_OUI>` (and `<REDACTED_SESSION_URL>` or redacted domain pattern so zero real SSID/OUI literals remain in `docs/`).
3. **Line 214 (`NON-BLOCKING-1`):** Similarly mask the literal SSID and BSSID OUI tokens inside the static audit table's quoted `git grep` command with `<REDACTED_SSID>` and `<REDACTED_OUI>` while preserving the `0 matches` / `PASS` verdict.

## 4. Verification

1. Confirm `docs/release-audit-v0.3.0-beta.1.md` has zero remaining literal SSID/OUI strings (`git grep -n -E "<REDACTED_check>"` — confirm lines `106`, `114`, and `214` look clean and accurate).
2. Keep `.agents/` clean beforehand, then run `~/.local/bin/pa wo verify WO-09.9` (`pio test -e native --verbose`, stamps receipt, commits on `pr-09-integration/wo9-audit-doc-cleanup`).
3. Post the resulting commit SHA and `receipt_id` to `@coordinator` using `post_message`.

## Completion Report

_Pending execution._
