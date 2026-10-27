---
schema_version: 1
wo_id: WO-06.8
pr_id: PR-06
title: File Headers, std::function Deviation Comments & CSS Design Tokens
complexity: standard
status: PENDING
allowed_files:
- src/echenon/RideStore.cpp
- src/echenon/SessionManager.cpp
- src/echenon/SessionManager.h
- src/echenon/TCXGenerator.cpp
- src/echenon/UploadQueue.cpp
- src/echenon/UploadQueue.h
- src/echenon/StravaUploader.cpp
- src/echenon/StravaUploader.h
- src/echenon/SessionOverlay.cpp
- src/echenon/IUploader.h
- test/test_native/test_session_manager.cpp
- test/test_native/test_tcx_generator.cpp
- test/test_native/test_upload_queue.cpp
- test/test_native/test_strava_uploader.cpp
- web/session.js
- web/upload_settings.js
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-06.8: File Headers, std::function Deviation Comments & CSS Design Tokens

> **Parent:** [`PROJECT.md`](../PROJECT.md)
> **Branch:** `feat/pr-06-session-upload` (continue on HEAD, after WO-06.7)
> **Stacked on:** `WO-06.7`
> **Source:** Release audit findings D1 (advisory), D2 (advisory), D6 (advisory)

---

## Context

The framework release audit identified three advisory (non-blocking) style findings
in PR-06 code. This WO remediates all three in a single cosmetic-only commit.

## Requirements

### R1: `.cpp` File Headers (D1 — House Style)

Add a structured file header comment block to the **top** of each PR-06 `.cpp` file
(before the first `#include`). The `.h` files already have `@file` + `@brief` Doxygen
— this brings the `.cpp` files into alignment.

**Files (10):**
- `src/echenon/RideStore.cpp`
- `src/echenon/SessionManager.cpp`
- `src/echenon/TCXGenerator.cpp`
- `src/echenon/UploadQueue.cpp`
- `src/echenon/StravaUploader.cpp`
- `src/echenon/SessionOverlay.cpp`
- `test/test_native/test_session_manager.cpp`
- `test/test_native/test_tcx_generator.cpp`
- `test/test_native/test_upload_queue.cpp`
- `test/test_native/test_strava_uploader.cpp`

**Format** (match the existing `.h` Doxygen style):
```cpp
/// @file <filename>.cpp
/// @brief <One-line description matching the .h @brief or describing the test suite>.
```

Keep it minimal — do NOT add license blocks, author fields, or creation dates.
This is purely for navigation and documentation tooling.

### R2: `std::function` Deviation Comments (D2 — GRAVE-05)

The framework's GRAVE-05 rule discourages stored `std::function` class members due
to heap fragmentation risk. PR-06 has 7 stored `std::function` members that are
dependency-injection callbacks set **once at boot** and never reassigned. These are
acceptable but need documented rationale.

Add a brief comment above each `std::function` member declaration explaining the
deviation:

**Files and locations:**
- `src/echenon/SessionManager.h` — 1 member (`_getElapsedMs` or similar)
- `src/echenon/StravaUploader.h` — 3 members (lines ~293-295)
- `src/echenon/UploadQueue.h` — 3 members (lines ~241-243)

**Comment format:**
```cpp
    // GRAVE-05 deviation: DI callback set once at boot, never reallocated.
    // Framework's own DataManager uses the same pattern for IDataSource injection.
    std::function<...> _callbackName;
```

### R3: CSS Design Tokens (D6 — Portal House Style)

Replace hardcoded hex colour values in `web/session.js` and `web/upload_settings.js`
with CSS `var()` custom property references. The framework's portal design system
defines these tokens in the portal shell CSS.

**Reference pattern** (from `web/echenon-dashboard.js`):
```javascript
// ✅ Correct: uses CSS custom property
label.style.color = 'var(--success)';

// ❌ Wrong: hardcoded hex
label.style.color = '#22c55e';
```

**Specific replacements:**
- `#22c55e` → `var(--success)` (green / active / connected)
- `#f59e0b` → `var(--warning)` (amber / paused / pending)
- `#94a3b8` → `var(--text-muted)` (grey / idle / muted text)
- `#3b82f6` → `var(--primary)` (blue / ended / info)
- `#0f172a` → `var(--bg-input)` (dark background)
- `#334155` → `var(--border)` (border colour)

**Rules:**
- Where a `var(--token, #fallback)` pattern already exists, **remove the fallback** —
  the portal shell always defines these tokens, so fallbacks are unnecessary noise.
  Change `var(--text-muted, #94a3b8)` to `var(--text-muted)`.
- Where a bare hex like `color: '#22c55e'` appears in JS, replace with the
  corresponding `var()` reference.
- In CSS string literals within JS (the `style` array), apply the same rule.
- `rgba(...)` values in the badge classes (`.ech-sess-badge-idle` etc.) — replace
  the hardcoded `rgba` with `color-mix()` using the design token, OR leave as-is
  with a `/* TODO: badge alpha needs color-mix() */` comment if `color-mix()` is
  not supported by the portal's target browsers.

**Do NOT touch:**
- Files outside the allowed list above.
- `#fff` / `#000` (pure white/black are universal, not design tokens).
- The `web/echenon-dashboard.js` or `web/calibration-wizard.js` files (out of scope —
  pre-existing pattern, not a PR-06 regression).

## Verification

- `pio test -e native` — all tests pass (no behavioural changes).
- Visual inspection: no functional code changes, only comments and string constants.
- Both firmware builds compile cleanly.

## Commit Message

```
style(pr-06): file headers, GRAVE-05 comments, CSS design tokens

D1: Add @file/@brief headers to 10 PR-06 .cpp files matching .h convention.
D2: Document GRAVE-05 std::function deviation rationale on 7 DI callbacks
    (set once at boot, never reallocated; same pattern as framework DataManager).
D6: Replace hardcoded hex colours with CSS var() design tokens in session.js
    and upload_settings.js to match portal design system.

Release audit remediation — cosmetic only; no behavioural changes.
```
## Completion Report

_No completion report recorded._
