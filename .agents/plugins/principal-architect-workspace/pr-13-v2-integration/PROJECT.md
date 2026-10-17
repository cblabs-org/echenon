---
schema_version: 1
charter:
  pr_id: "PR-13"
  slug: "pr-13-v2-integration"
  title: "v2 Integration and Release"
  branch: "feat/pr-13-v2-integration"
  base_branch: "main"
  allowed_modules:
    - "src/echenon/"
    - "docs/"
    - "hardware/"
    - "framework/"
    - "platformio.ini"
    - "web/"
  charter_locked: false
status: PLANNED
---

## 2. Scope

_See legacy content below._


# PR-13: v2 Integration & Release — v2

> **Release:** **v2 "Active"** (provisional). See [PROGRAM-PLAN.md "Releases"](../PROGRAM-PLAN.md#releases).

> **Programme:** Echenon Firmware ([PROGRAM.md](../PROGRAM.md), [PROGRAM-PLAN.md](../PROGRAM-PLAN.md))
> **PR target base:** `main` after PR-07, PR-10, PR-08, PR-11 and PR-12 merged
> **Dev Phase:** Phase 7 — v2 Integration / RC
> **Depends on:** all v2 PRs (PR-07, PR-10, PR-08, PR-11, PR-12)
> **Spec sections:** [SPEC](../../../../SPEC.md) §13 (v2 tests), Appendix J (v2 journeys)

## 1. Mandate

Integrate and release v2: UC4–UC6 and HR-zone auto-resistance validated end to
end; mode switching (mode 1 ↔ mode 2 ↔ stock bypass); v2 coexistence test;
fail-safe regression; release-candidate firmware. A beta or stable tag needs the
owner (rule 03).

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
