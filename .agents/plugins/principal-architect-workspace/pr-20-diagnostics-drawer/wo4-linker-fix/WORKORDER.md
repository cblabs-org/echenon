---
wo_id: WO-20.4
pr_id: PR-20
programme_id: prog-default
slug: wo4-linker-fix
title: Remove duplicate LiveFeed tick() overloads (linker fix)
complexity: mechanical
required_skills: []
allowed_files:
  - src/echenon/LiveFeed.cpp
forbidden_files:
  - src/echenon/LiveFeed.h
  - src/echenon/SessionOverlay.cpp
  - src/echenon/EchenonModule.cpp
  - web/**
verification_command: pio test -e native --verbose
max_attempts: 3
status: COMPLETED
spec_hash: null
---

# WO-20.4 — Remove Duplicate LiveFeed tick() Overloads (Linker Fix)

> **PR:** PR-20 (Echelon Tab Live Diagnostics Drawer)
> **Depends on:** WO-20.1 (caused the issue)
> **Branch:** `pr-20-diagnostics-drawer/wo3-hw-verification` (commit directly here)

---

## 1. Objective

Fix a multiple-definition linker error caused by WO-20.1 implementing two
`LiveFeed::tick()` overloads in `LiveFeed.cpp` that were already implemented
in `SessionOverlay.cpp`.

---

## 2. Strict Boundary Box

### 2.1 Allowed Files (read-write)

| File | Scope of change |
|------|----------------|
| `src/echenon/LiveFeed.cpp` | Remove the two duplicate overload implementations |

### 2.2 Read-only context

- `src/echenon/SessionOverlay.cpp` lines 88-167 — already provides the session/HR overload and reordered-params delegate

### 2.3 Forbidden

- Do NOT modify any other files

---

## 3. Technical Specification

### The problem

`LiveFeed.cpp` now has these overloads:
1. **Lines 21-65:** Short 11-field `tick()` — ✅ KEEP (original, no conflict)
2. **Lines 67-117:** Session/HR 14-field `tick()` — ❌ REMOVE (duplicate of SessionOverlay.cpp:89-136)
3. **Lines 119-136:** Reordered-params delegate — ❌ REMOVE (duplicate of SessionOverlay.cpp:138-166)
4. **Lines 138-206:** Diagnostic 23-field `tick()` — ✅ KEEP (new, unique)

### The fix

Delete lines 67-136 from `src/echenon/LiveFeed.cpp`. This removes the two
duplicate implementations. The session/HR overload and reordered-params delegate
remain implemented in `SessionOverlay.cpp`.

The diagnostic overload (lines 138-206) remains in `LiveFeed.cpp` and is unique.

---

## 4. Verification

- `pio test -e native --verbose` must pass
- Full hardware build (`ex3-waveshare-nano` environment) must link without errors

---

## Completion Report

- **status:** COMPLETED
- **commit_sha:** 54f6c5e
- **files_modified:** `src/echenon/LiveFeed.cpp`
- **verification_summary:** 233/233 native tests passed (0.54s). Removed 71 lines (two duplicate `tick()` overloads: session/HR 14-field and reordered-params delegate) that conflicted with implementations in `SessionOverlay.cpp`.

