# tools/calibration-rig

## Invariants & contracts

### [docseed-6df6e875] [`tools/calibration-rig/calibration_sweep.py:133-134`](../tools/calibration-rig/calibration_sweep.py#L133-L134) surfaces an actionable hint when `MCPServer` returns JSON-RPC `-32000` (`read-only` mode).
`invariant · kind: semantic · anchors: tools/calibration-rig/calibration_sweep.py:133-134, tools/calibration-rig/calibration_sweep.py · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`

## Procedures / Recipes

### [docseed-966ead29] Legacy unauthenticated `/api/echenon/rig/*` HTTP endpoints were retired in `WO-09.1` and replaced by `IMCPContributor` tools `echenon_rig_status` and `echenon_rig_command` (`/api/mcp`), with Python rig scripts in `tools/calibration-rig/` updated in lockstep.
`recipe · kind: procedural · anchors: tools/calibration-rig · source_doc: docs/release-audit-v0.3.0-beta.1.md · evidence: doc:docs/release-audit-v0.3.0-beta.1.md`
