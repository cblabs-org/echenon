---
schema_version: 1
wo_id: WO-15.4
pr_id: PR-15
title: SPEC & Design Doc Updates
complexity: standard
status: PENDING
allowed_files:
- SPEC.md
- docs/implementation-strategy.md
- docs/measurements/phase4/wo15-portal-ride/**
forbidden_files:
- src/**
- web/**
- include/**
- platformio.ini
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: d0a96ed095bace613ae48d105410d1e53a995d7ff6856cdf79963b80a598bb49
completion: null
---

# WO-15.4: SPEC & Design Doc Updates

> **Parent PR:** [PR-15 PROJECT.md](../PROJECT.md)
> **Depends on:** WO-15.1, WO-15.2, WO-15.3 (all code changes complete first)
> **Branch:** `pr-15-portal-ride/wo4-spec-updates` (base: `pr-15-portal-ride/wo3-ride-page`)

---

## 1. Objective

Update SPEC.md to replace all `/overlay` references with `/ride`, restructure §11 to document the two-surface model (portal + ride page), document the portal tab rework (6-tab layout, Setup pills), and update `docs/implementation-strategy.md` if it references the overlay. After this WO, `grep -ri 'overlay' SPEC.md` must return **zero hits**.

## 2. Strict Boundary Box

### Allowed Files
| File | Operations |
|---|---|
| `SPEC.md` | Major edits across 12 sections |
| `docs/implementation-strategy.md` | Update overlay references if any |
| `docs/measurements/phase4/wo15-portal-ride/**` | Create directory for HW verification evidence (WO-15.5) |

### Forbidden
- Do NOT modify any source code files
- Do NOT modify any web files
- Do NOT modify platformio.ini or include files

## 3. Detailed Task Checklist

### 3.1 §1.1 Purpose
- [ ] Replace "session overlay" → "ride page"
- [ ] Drop "floating training-computer" framing
- [ ] Update to reflect the two-surface model (portal for config, ride page for daily use)

### 3.2 §3.1 Three Output Paths
- [ ] Replace `📊 OVERLAY` → `📊 RIDE PAGE`
- [ ] Update description to match ride page purpose

### 3.3 §3.2 Block Diagram
- [ ] Replace `Floating overlay` → `Ride page (echenon.local/ride)`
- [ ] Update any ASCII art or mermaid diagrams

### 3.4 §3.3 Data Flow
- [ ] Replace "the overlay receives" → "the ride page receives"
- [ ] Update WebSocket endpoint references if needed

### 3.5 §3.4 Module Decomposition
- [ ] Update `SessionOverlay` module description
- [ ] Note that SessionOverlay now serves the standalone `/ride` page
- [ ] Mention the two-surface architecture

### 3.6 §3.5 Foundation Integration
- [ ] Replace "WebSocket push to the overlay" → "to the ride page"
- [ ] Update any route references

### 3.7 §4.2 Target Deployment
- [ ] Keep split-screen concept
- [ ] Update route: `/overlay` → `/ride`
- [ ] Mention PWA "Add to Home Screen" capability

### 3.8 §11 Session Overlay → Session UI (MAJOR REWRITE)
- [ ] **Rename section** from "Session Overlay" to "Session UI — Ride Page & Portal Tabs"
- [ ] **§11.1 Ride Page** (new subsection):
  - Purpose: daily-use rider control surface
  - Two primary contexts: split-screen (280–430px) and full phone screen
  - URL: `echenon.local/ride`
  - Features: live metrics (WS 2Hz), session controls, ride history, upload status, TCX download
  - PWA manifest (`display: standalone`, dark theme)
  - ⚙️ gear icon → portal
  - Responsive layout specification
- [ ] **§11.2 Portal Tabs** (new subsection):
  - 6-tab bottom navigation layout with order numbers
  - Tab inventory: WiFi (10), Bluetooth (15), Echenon (50), Setup (60), Security, System
  - Echenon tab: device status summary + "Open Ride Page →" button
  - Setup tab: PillBar with Apps, Level Mapping, Power Tuning pills
  - What was removed: App tab (disabled), signal-logger content (behind WITH_SIGNAL_LOGGER)
- [ ] Remove all `/overlay` references from this section

### 3.9 §12.1 Project Structure
- [ ] Replace `overlay.*` → `ride.*` in file tree listing
- [ ] Add `web/ride.html`, `web/ride.js`, `web/manifest.json` to file tree
- [ ] Update `include/portal_assets/` listing with new gzipped assets

### 3.10 §13 Acceptance Tests
- [ ] Replace "overlay" → "ride page" in all test descriptions
- [ ] Add **P12 Split-Screen Acceptance Test**:
  - Open `/ride` on Lenovo Tab P12 in Android split-screen with Echelon app
  - Verify metrics update at 2 Hz
  - Verify session start/stop from ride page
  - Verify PWA install
- [ ] Update existing acceptance test references

### 3.11 Appendix J User Journeys
- [ ] Replace all `echenon.local/overlay` → `echenon.local/ride`
- [ ] Update user journey flows to reflect new navigation:
  - Daily ride: go to `/ride` (PWA or browser)
  - Setup/config: go to portal `/` → navigate tabs
  - From ride page, ⚙️ → portal for settings

### 3.12 docs/implementation-strategy.md
- [ ] Search for "overlay" references and replace with "ride"
- [ ] Update Phase 4 description if it mentions overlay

### 3.13 Create Evidence Directory
- [ ] Create `docs/measurements/phase4/wo15-portal-ride/` directory (for HW verification screenshots in WO-15.5)
- [ ] Add a placeholder `README.md` noting this is for WO-15.5 hardware verification evidence

## 4. Verification

- [ ] `grep -ri 'overlay' SPEC.md` returns **zero hits**
- [ ] All file paths in SPEC §12.1 match actual project structure
- [ ] §11 is restructured into §11.1 (Ride Page) and §11.2 (Portal Tabs)
- [ ] `pio test -e native` still passes (no code changes, but verify nothing broke)
- [ ] `docs/measurements/phase4/wo15-portal-ride/` directory exists

---

## Completion Report

*(Populated by `pa-engineer` after `pa wo verify WO-15.4`)*
