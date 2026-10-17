# docs

## Invariants & contracts

### [docseed-125127e0] **Mode 1, relay/active (Intercept)**: the console is connected. The ESP32 takes the knob and the console's motor drive in as signals, emulates the encoder toward the console and drives the motor through an H-bridge ("console as signal source", `docs/mitm-encoder-design-notes.md` §11). The Echelon app runs unchanged.
`invariant · kind: semantic · anchors: docs/mitm-encoder-design-notes.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-5c68dcae] EX-3 internal sensors, looms, connectors and actuators: [`docs/ex3_specification.md`](docs/ex3_specification.md)
`invariant · kind: semantic · anchors: docs/ex3_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-678b3000] The cadence, potentiometer and encoder signals operate at 3.3 V logic (see [`docs/ex3_specification.md` §10](docs/ex3_specification.md#10-voltage-summary)). They reach the ESP32 only through series resistors (§4.4).
`invariant · kind: semantic · anchors: docs/ex3_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-9e31f3f2] Operating scenarios UC1–UC6: [`docs/use_case_analysis.md`](docs/use_case_analysis.md)
`invariant · kind: semantic · anchors: docs/use_case_analysis.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-b2888efc] Development rationale: [`docs/implementation-strategy.md`](docs/implementation-strategy.md)
`invariant · kind: semantic · anchors: docs/implementation-strategy.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-b54b7cc6] Circuit build guide per phase: [`docs/hardware_specification.md`](docs/hardware_specification.md)
`invariant · kind: semantic · anchors: docs/hardware_specification.md · source_doc: SPEC.md · evidence: doc:SPEC.md`

### [docseed-e3c9185d] [`docs/encoder-resistance-analysis.md`](encoder-resistance-analysis.md) — signal quality, calibration, cam model
`invariant · kind: semantic · anchors: docs/encoder-resistance-analysis.md · source_doc: docs/mitm-encoder-design-notes.md · evidence: doc:docs/mitm-encoder-design-notes.md`
