---
schema_version: 1
wo_id: WO-16.3
pr_id: PR-16
title: Dashboard HR field name mismatch fix
complexity: mechanical
status: COMPLETED
allowed_files:
- web/echenon-dashboard.js
forbidden_files:
- src/**
- include/**
- framework/**
- platformio.ini
- web/ride.js
required_skills: []
verification_command: pio test -e native --verbose
max_attempts: 3
max_structural_failures: 2
spec_hash: null
completion:
  status: COMPLETED
  verification_summary: pio test -e native --verbose passed. data.heart_rate → data.hr fixed in handleLiveMessage()
---

# WO-16.3 — Dashboard HR Field Name Mismatch Fix

> **PR:** PR-16 (WebSocket Client Registration Fix)
> **Complexity:** mechanical
> **Verification:** `pio test -e native --verbose`

---

## 1. Objective

Fix the heart rate field name mismatch in the Echenon dashboard. The LiveFeed
telemetry JSON sends `"hr"` but `handleLiveMessage()` checks `data.heart_rate`.
This causes the HR tile to always show `--` on the dashboard, even though it
works correctly on the `/ride` page (which uses `d.hr`).

---

## 2. Strict Boundary Box

| File | Changes |
|---|---|
| `web/echenon-dashboard.js` | Fix field name in `handleLiveMessage()` |

---

## 3. Implementation Specification

**File:** `web/echenon-dashboard.js`, `handleLiveMessage()`, lines 147-149

**Current:**
```js
    if (hrEl && data.heart_rate !== undefined) {
      hrEl.textContent = data.heart_rate;
    }
```

**Change to:**
```js
    if (hrEl && data.hr !== undefined) {
      hrEl.textContent = data.hr;
    }
```

**Rationale:** The C++ LiveFeed (`EchenonModule.cpp:649`) serialises the field as
`doc["hr"] = liveHr;`. The ride page (`ride.js:13`) correctly reads `d.hr`.
The dashboard used `heart_rate` which never matches, so the HR tile stays at its
default `--` text.

---

## 4. Verification Criteria

1. `pio test -e native --verbose` passes (no C++ changes).
2. `handleLiveMessage()` now reads `data.hr` instead of `data.heart_rate`.
## Completion Report

_No completion report recorded._
