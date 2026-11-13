---
schema_version: 1
wo_id: WO-09.7
pr_id: PR-09
title: Evidence Redaction (SSID/BSSID/URL) & .gitignore Hardening
complexity: mechanical
status: PENDING
allowed_files:
- .gitignore
- docs/echelon-teardown/conversation.md
- docs/measurements/phase0/B01-20261003-sensor-bringup-report.md
- docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md
- docs/measurements/phase2/wo5-rerun-2.5-2.6-evidence.md
- docs/measurements/phase2/wo5-strap-acceptance/evidence.md
- docs/measurements/phase2/wo6-coexistence/evidence.md
- docs/measurements/phase3/evidence/pr19-dual-sink-serial.log
- docs/measurements/phase3/evidence/pr19-hardware-verification.md
- docs/measurements/phase3/evidence/pr21-wahoo-serial.log
- docs/measurements/phase3/evidence/pr21-wahoo-verification.md
forbidden_files:
- framework/**
- src/**
- web/**
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: c3c4686f8b68e5290d11ca19ae969a4d31dc3a573fab951f8846723589648109
completion: null
---

# WO-09.7 — Evidence Redaction (SSID/BSSID/URL) & .gitignore Hardening

## 1. Objective

Scrub all personal/private network identifiers and personal links from git-tracked documentation and verification evidence logs prior to making `cblabs-org/echenon` public, and harden `.gitignore` against accidental credential/secret commits.

## 2. Strict Boundary Box

- **Allowed files:** ONLY `.gitignore` and the 10 tracked docs/logs enumerated below:
  1. `docs/echelon-teardown/conversation.md`
  2. `docs/measurements/phase0/B01-20261003-sensor-bringup-report.md`
  3. `docs/measurements/phase2/BLE-0x3e-debug-2026-10-13.md`
  4. `docs/measurements/phase2/wo5-rerun-2.5-2.6-evidence.md`
  5. `docs/measurements/phase2/wo5-strap-acceptance/evidence.md`
  6. `docs/measurements/phase2/wo6-coexistence/evidence.md`
  7. `docs/measurements/phase3/evidence/pr19-dual-sink-serial.log`
  8. `docs/measurements/phase3/evidence/pr19-hardware-verification.md`
  9. `docs/measurements/phase3/evidence/pr21-wahoo-serial.log`
  10. `docs/measurements/phase3/evidence/pr21-wahoo-verification.md`
- **Forbidden files:** `framework/**`, `src/**`, `web/**`, or any file not listed above.
- **Preserve log timestamps, structure, and engineering metrics:** Only replace the sensitive tokens (`<REDACTED_SSID>`, `XX:XX:XX:..:..:..` MAC/BSSID strings, and `<redacted-session-link>` personal link) while keeping all timing, heap, RSSI, latency, and test data completely intact.
- **Shell hygiene:** Never use bash `$VAR` loops, `/tmp`, or raw `pio` CLI in `run_command`.

## 3. Technical Specification

### 3.1 Redactions Across the 10 Tracked Files
Inspect matches with `git grep -n -E "<REDACTED_SSID>|XX:XX:XX|<redacted-session-link>"` and redact cleanly:
1. Every case-sensitive/insensitive occurrence of `<REDACTED_SSID>` (e.g., `Connecting to <REDACTED_SSID>`, `'<REDACTED_SSID>'`, `"ssid": "<REDACTED_SSID>"`) -> `<REDACTED_SSID>`.
2. Every router BSSID / MAC matching `XX:XX:XX:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}` (or partial `XX:XX:XX` prefix) -> `XX:XX:XX:XX:XX:XX`.
   - Double-check whether any other router BSSIDs appear on those same `[wifi]` / `WiFi` log lines and anonymize if tied to the home AP (leaving device BLE addresses documented in BLE protocol specs untouched unless clearly home AP BSSID).
3. Personal Gemini session link `<redacted-session-link>` -> `<redacted-session-link>`.

### 3.2 `.gitignore` Hardening
Read `.gitignore` first, then append a clearly commented `# Secrets & local environment files` block ignoring:
- `.env`
- `.env.*`
- `*.pem`
- `*.key`
- `*credentials*.json`
- `*secrets*.h`
Ensure existing rules (including `tmp/`, `.pio/`, etc.) remain intact and do not accidentally ignore any currently tracked files (`git ls-files -i -c --exclude-standard` should remain empty!).

## 4. Verification

1. Run `git grep -n -E "<REDACTED_SSID>|XX:XX:XX|<redacted-session-link>" || echo "CLEAN_ZERO_MATCHES"` and verify **zero** matches remain across the entire tracked tree.
2. Check `git ls-files -i -c --exclude-standard` is empty and `git status --short` shows only allowed files.
3. Run `~/.local/bin/pa wo verify WO-09.7` (executes `pio test -e native --verbose`, stamps receipt, commits on `pr-09-integration/wo7-public-repo-hygiene`).
4. Post completion summary to `@coordinator` via `post_message`.

## Completion Report

_Pending execution._
