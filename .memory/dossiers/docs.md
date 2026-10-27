# docs

## Invariants & contracts

### [docseed-125127e0] **Mode 1, relay/active (Intercept)**: the console is connected. The ESP32 takes the knob and the console's motor drive in as signals, emulates the encoder toward the console and drives the motor through an H-bridge ("console as signal source", `docs/mitm-encoder-design-notes.md` §11). The Echelon app runs unchanged.
`invariant · kind: semantic · anchors: docs/mitm-encoder-design-notes.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-3f76c244] [`docs/power-physics-model.md`](../../power-physics-model.md) — First-principles electromechanical derivation (`Physics P3r`), reverse-engineering of Echelon's console algorithm, evaluation across 14 candidate models, and `PowerEngine` implementation spec.
`invariant · kind: semantic · anchors: docs/power-physics-model.md · source_doc: docs/measurements/phase3/README.md · evidence: doc:docs/measurements/phase3/README.md`

### [docseed-53738f4a] [`docs/power-physics-model.md`](power-physics-model.md) — first-principles electromechanical power model (`Physics P3r`) and cam/eddy-current physics derivation
`invariant · kind: semantic · anchors: docs/power-physics-model.md · source_doc: docs/mitm-encoder-design-notes.md · evidence: doc:docs/mitm-encoder-design-notes.md`

### [docseed-5c68dcae] EX-3 internal sensors, looms, connectors and actuators: [`docs/ex3_specification.md`](docs/ex3_specification.md)
`invariant · kind: semantic · anchors: docs/ex3_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-678b3000] The cadence, potentiometer and encoder signals operate at 3.3 V logic (see [`docs/ex3_specification.md` §10](docs/ex3_specification.md#10-voltage-summary)). They reach the ESP32 only through series resistors (§4.4).
`invariant · kind: semantic · anchors: docs/ex3_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-68e8f40f] **Document ID:** `docs/power-physics-model.md`
`invariant · kind: semantic · anchors: docs/power-physics-model.md · source_doc: docs/power-physics-model.md · evidence: doc:docs/power-physics-model.md`

### [docseed-9b04c122] Questions 1, 2, 3, and 5 were empirically resolved during Phase 3 (`C01`–`C07`). Full characterization data, state-machine specifications, and v2 firmware rules are documented in [`docs/digital-hardware-emulation-and-console-dynamics.md`](digital-hardware-emulation-and-console-dynamics.md).
`invariant · kind: semantic · anchors: docs/digital-hardware-emulation-and-console-dynamics.md · source_doc: docs/mitm-encoder-design-notes.md · evidence: doc:docs/mitm-encoder-design-notes.md`

### [docseed-9e31f3f2] Operating scenarios UC1–UC6: [`docs/use_case_analysis.md`](docs/use_case_analysis.md)
`invariant · kind: semantic · anchors: docs/use_case_analysis.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-ac60238f] [`docs/encoder-resistance-analysis.md`](encoder-resistance-analysis.md) — Phase 0 signal quality, calibration, cam model
`invariant · kind: semantic · anchors: docs/encoder-resistance-analysis.md · source_doc: docs/mitm-encoder-design-notes.md · evidence: doc:docs/mitm-encoder-design-notes.md`

### [docseed-b2888efc] Development rationale: [`docs/implementation-strategy.md`](docs/implementation-strategy.md)
`invariant · kind: semantic · anchors: docs/implementation-strategy.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-b54b7cc6] Circuit build guide per phase: [`docs/hardware_specification.md`](docs/hardware_specification.md)
`invariant · kind: semantic · anchors: docs/hardware_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-ffde7036] [`docs/digital-hardware-emulation-and-console-dynamics.md`](digital-hardware-emulation-and-console-dynamics.md) — **definitive Phase 3 reference** for synthetic cadence, quadrature encoder synthesis, console decoder timing/aliasing, and motor settling dynamics
`invariant · kind: semantic · anchors: docs/digital-hardware-emulation-and-console-dynamics.md · source_doc: docs/mitm-encoder-design-notes.md · evidence: doc:docs/mitm-encoder-design-notes.md`

## Procedures / Recipes

### [docseed-ce828c61] [`docs/digital-hardware-emulation-and-console-dynamics.md`](../../digital-hardware-emulation-and-console-dynamics.md) — Complete engineering reference for synthetic cadence emulation (`GPIO 11` NPN open-collector), digital quadrature encoder synthesis (`GPIO 17/18`, `8 edges = 1 level`), console decoder timing & the `75 ms` polling-aliasing anomaly, and bang-bang motor settling dynamics (`C03...
`recipe · kind: procedural · anchors: docs/digital-hardware-emulation-and-console-dynamics.md · source_doc: docs/measurements/phase3/README.md · evidence: doc:docs/measurements/phase3/README.md`
