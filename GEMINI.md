# Echelon — Antigravity Overrides

The canonical agent bootstrap lives in `AGENTS.md`. Read it first.

## Mandatory rules — reliability backstop

As your **very first action** in any session, actively read every file in
`.agents/rules/` in filename order using file-read tools — do not assume
they were auto-loaded.

<!-- repo-memory:begin v2 — managed by repo-memory; edits inside are overwritten -->
@[memory atlas](.memory/ATLAS.md)

## Repository Memory (`repo-memory`)
- **Pushed context (`[repo-memory ...]`):** Hooks automatically inject verified invariants, gotchas, failure signatures, and ruled-out approaches when you start a task or touch anchored files. Treat them as repository notes verified against the code; do not repeat or echo them as new discoveries.
- **Pull for depth (MCP):** Use `memory_brief` (module dossier), `memory_why` (entry/file/line rationale), `memory_history` (evolution), and `memory_search` (FTS5 search) when exploring unfamiliar subsystems.
- **Distillation:** End-of-turn capture happens automatically via the `Stop` hook on milestone turns (no action required). Use `memory_note` mid-turn only when you want to record a critical discovery or dead end immediately.
<!-- repo-memory:end -->
