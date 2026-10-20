---
schema_version: 1
charter:
  pr_id: "PR-11"
  slug: "pr-11-hr-zone-control"
  title: "HR-Zone Auto-Resistance, portal-native"
  branch: "feat/pr-11-hr-zone-control"
  base_branch: "main"
  allowed_modules:
    - "src/echenon/"
    - "docs/"
    - "framework/"
    - "platformio.ini"
    - "web/"
  charter_locked: false
status: PLANNED
---

## 2. Scope

_See legacy content below._


# PR-11: HR-Zone Auto-Resistance, portal-native — v2

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-08 merged
> **Dev Phase:** Phase 6 — HR-zone auto-resistance
> **Depends on:** PR-08. Unblocks PR-13
> **Use case:** [`docs/use_case_analysis.md`](../../../../docs/use_case_analysis.md) UC5 (Approach B)

## 1. Mandate

Implement **portal-native HR-zone auto-resistance** (`HRZoneController`): HR →
zone → target resistance, driven through the PR-08 control path without an
external app. Rider sets zones and limits in the portal; the knob overrides, as
in ERG.

## 3. Work Order Ledger
<!-- PA fills this in after DR-2 -->

## 4. Session Log
<!-- PA enriches this progressively -->

### 2026-10-12: Stub created (Chief Architect)
- Created when the owner restructured the programme into v1 Passive / v2 Active
  releases (2026-10-12, in chat; recorded in [PROGRAM.md](../PROGRAM.md)). This
  stub is **not** an approved decomposition.

## Status
**Plan Status:** PROVISIONAL — decomposed after DR-2
