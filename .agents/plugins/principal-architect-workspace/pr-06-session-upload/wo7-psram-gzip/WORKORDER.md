---
schema_version: 1
wo_id: WO-06.7
pr_id: PR-06
title: Move StreamingGzipEncoder to PSRAM Heap Allocation + EventBus Payload Safety
complexity: standard
status: PENDING
allowed_files:
- src/echenon/TCXGenerator.h
- src/echenon/TCXGenerator.cpp
- src/echenon/SessionManager.h
- src/echenon/SessionManager.cpp
- test/test_native/test_tcx_generator.cpp
- test/test_native/test_session_manager.cpp
forbidden_files: []
required_skills:
- framework-expert
verification_command: pio test -e native
max_attempts: 5
max_structural_failures: 2
spec_hash: null
completion: null
---

# WO-06.7: Move StreamingGzipEncoder to PSRAM Heap Allocation + EventBus Payload Safety

> **Parent:** [`PROJECT.md`](../PROJECT.md)
> **Branch:** `feat/pr-06-session-upload` (continue on HEAD)
> **Stacked on:** `WO-06.6` (`c5d85a0`)
> **Source:** Release audit findings D4 (blocking) + D3 (advisory)

---

## Context

The framework release audit identified two memory-related concerns in PR-06:

1. **D4 (PSRAM preference):** `StreamingGzipEncoder` (defined at `TCXGenerator.cpp:172`)
   is instantiated as a **stack-local object** at line 822. Its member data:
   - `_outBuf[4096]` (4,096 bytes) — output scratch buffer
   - `_hashHead[512]` (1,024 bytes) — LZ77 hash table (`int16_t[HASH_SIZE]`)
   - Scalars (~30 bytes)
   - **Total: ~5,150 bytes on stack**

   With `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` and the full call chain
   (`UploadQueue::stepQueue()` → `DataManager::fetchUpdate()` → `generateTcxGzToFile()`
   → lambdas), this leaves limited headroom. The ESP32-S3-N16R8 has **8 MB PSRAM** —
   the encoder should be heap-allocated there.

2. **D3 (EventBus payload lifetime):** `SessionManager::publishStateChange()` at
   line 172–177 constructs a `SessionStateChangeEvent` on the stack and passes its
   address to `_eventBus->publish()`. This is safe under synchronous dispatch but
   fragile if EventBus ever becomes async. `UploadQueue` already uses the correct
   pattern: a persistent `UploadEvent _eventRing[4]` class member.

## Requirements

### R1: PSRAM Heap Allocation for StreamingGzipEncoder

1. Change the `StreamingGzipEncoder` instantiation in `TCXGenerator::generateTcxGz()`
   (and `generateTcxGzToFile()` if it creates one too) from a **stack-local** object
   to a **heap-allocated** `std::unique_ptr<StreamingGzipEncoder>`.

2. On ESP32 (`ESP_PLATFORM`), allocate via `heap_caps_malloc(sizeof(StreamingGzipEncoder), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` with placement-new. On native (host unit tests), use regular `new`.

3. **Alternative (simpler):** Add a static factory method or use a custom allocator.
   The simplest correct approach: use `std::unique_ptr` with `new` and rely on the
   ESP-IDF `CONFIG_SPIRAM_USE_MALLOC=y` + `malloc()` routing, OR explicitly use
   `ps_malloc()` / `heap_caps_malloc()` with a custom deleter. Choose whichever is
   cleanest.

4. Ensure the destructor runs correctly (no raw `free()` mismatches).

5. Add a null-check on allocation failure — return `false` from `generateTcxGz()`.

### R2: SessionManager EventBus Payload — Class Member

1. Add a `SessionStateChangeEvent _lastEvent;` member to `SessionManager` (in the
   private section of the `.h`).

2. In `publishStateChange()`, write to `_lastEvent` and pass `&_lastEvent` to
   `_eventBus->publish()` instead of a stack-local temporary. This matches the
   `UploadQueue::_eventRing` pattern.

3. This is a safety improvement only — no behavioural change. Existing tests must
   continue to pass without modification.

## Verification

- `pio test -e native` — all 199 tests pass (including gzip round-trip tests).
- No new test files required — this is a refactor within existing coverage.
- Both firmware builds (`ex3-waveshare-nano`, `ex3-waveshare-nano-diag`) compile
  cleanly.

## Commit Message

```
fix(pr-06): move StreamingGzipEncoder to PSRAM heap + EventBus payload safety

D4: StreamingGzipEncoder (~5 KB) moved from stack to PSRAM heap allocation
    via heap_caps_malloc(MALLOC_CAP_SPIRAM) with std::unique_ptr. Frees
    main task stack headroom (CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384).

D3: SessionManager::publishStateChange() EventBus payload migrated from
    stack-local temporary to persistent class member _lastEvent, matching
    UploadQueue's _eventRing pattern for async-safety.

Release audit remediation — no behavioural change; all 199 tests pass.
```
## Completion Report

_No completion report recorded._
