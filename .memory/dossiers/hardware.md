# hardware

## Procedures / Recipes

### [docseed-149da17b] **Hardware-portable**: all hardware-specific configuration (pin map, bike signal set, partition table, SDK settings) lives in one target directory under `hardware/`. A different ESP32 board or bike is a new target directory plus a PlatformIO environment. Firmware uses `#if HAS_*` guards for bike signals and `WITH_*` flags for compiled modules (§12.3).
`recipe · kind: procedural · anchors: hardware · source_doc: SPEC.md · evidence: doc:SPEC.md`
