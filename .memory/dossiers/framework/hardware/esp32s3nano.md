# framework/hardware/esp32s3nano

## Invariants & contracts

### [docseed-4ffa8947] **One header.** `board.h` is the single source of truth for pins and bike signals. It follows the framework's board contract (`framework/hardware/_common/board_contract.h`) and is authored from the framework's `framework/hardware/_TEMPLATE/`, not cloned from the reference `framework/hardware/esp32s3nano/board.h`, whose display and button pins collide with Echenon's tap pins (§12.3). No other p...
`contract · kind: semantic · anchors: framework/hardware/_common/board_contract.h, framework/hardware/_TEMPLATE, framework/hardware/esp32s3nano/board.h · source_doc: SPEC.md · evidence: doc:SPEC.md`
