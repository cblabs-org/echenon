---
schema_version: 1
wo_id: WO-19.4
pr_id: PR-19
title: Stored Rides Portal Pill
complexity: standard
status: PENDING
allowed_files:
- web/stored_rides.js
- src/echenon/SessionOverlay.cpp
- src/echenon/SessionOverlay.h
- src/echenon/EchenonModule.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native --verbose
max_attempts: 5
max_structural_failures: 2
spec_hash: a40472003ddf289d3a51a469b266207dee8517d4f078fce2935271f827aa3fa6
completion: null
---

# WO-19.4 — Stored Rides Portal Pill

## 1. Role & Objective

Build the **Stored Rides** card in the Echenon dashboard tab, providing users with
ride history browsing, per-sink sync status visualization, TCX download, ride deletion,
and per-sink force-push/retry — all from the portal.

**Key Deliverables:**
- `web/stored_rides.js`: IIFE module fetching `/api/session/history`.
- Per-sink sync badges with status icons (✅ Synced, ⏳ Pending, ❌ Failed).
- One-click `[Download TCX]` via `GET /api/session/download?id=<n>`.
- `[Delete]` action via `DELETE /api/echenon/runs/<id>`.
- `[Force Push / Retry]` per-sink via `POST /api/session/upload`.
- PROGMEM gzip asset embedding and tab registration.

## 2. Strict Boundary Box & Constraints

1. **Design System:** Use framework design tokens (`--card-bg`, `--text-muted`, `--primary`, `--success`, `--warning`, `--error`). Responsive on mobile, desktop, and Lenovo Tab P12.
2. **PROGMEM Embedding:** The JS file must be gzip-compressed and embedded via `xxd` into a `.hpp` header in `include/portal_assets/`. Update the build script if needed.
3. **Tab Registration:** Register as sub-pill under Echenon tab via `registry.addTab("Stored Rides", "Echenon", 30, null, "/stored_rides.js")` in `EchenonModule.cpp`.
4. **Lazy Init:** Follow the `tab-changed` event pattern from `upload_settings.js`.
5. **Date Formatting:** Convert UTC epoch seconds to browser local time via `new Date(epoch_s * 1000).toLocaleString()`.

**Forbidden:** No `UploadQueue.*` or `StravaUploader.*` edits.

## 3. Line-Item Instructions

- [ ] Create `web/stored_rides.js` with IIFE pattern, `TAB_ID = 'tab-stored-rides'`.
- [ ] Build card layout: table/list of recent rides with date, duration, avg power, distance, avg HR.
- [ ] Per-sink sync badges: iterate `ride.sync[]` array from `/api/session/history`, render status dot + service name + status text.
- [ ] Clickable remote links for synced rides (e.g., `https://www.strava.com/activities/<id>`).
- [ ] `[Download TCX]` button: `window.location = '/api/session/download?id=' + ride.session_id`.
- [ ] `[Delete]` button: `fetch('/api/echenon/runs/' + ride.id, {method: 'DELETE'})` with confirmation.
- [ ] `[Retry]` per-sink: `fetch('/api/session/upload', {method: 'POST', body: {session_id, service_id}})`.
- [ ] Empty state: "No rides recorded yet" message.
- [ ] Generate PROGMEM gzip header: add build step or manual `gzip + xxd` for `stored_rides_js.hpp`.
- [ ] Register PROGMEM asset route in `SessionOverlay.cpp`: `server.on("/stored_rides.js", ...)`.
- [ ] Register tab in `EchenonModule.cpp`: `registry.addTab("Stored Rides", "Echenon", 30, nullptr, "/stored_rides.js")`.

## 4. Verification, Cryptographic Receipt & Issue Surfacing

Run `pa wo verify WO-19.4` → `pio test -e native --verbose`.

## Completion Report

_Pending execution._
