# PR-07: Integration & Polish

> **Programme:** Echenon Firmware
> **PR target base:** PR-05 + PR-06 (merge both before starting)
> **Spec sections:** §3.4 (coupling audit), §3.5 (Foundation integration), §13

## Mandate

Final integration pass — no new features. This PR:

1. **Wire all 11 sub-components** in `EchenonModule` with correct initialization
   order and tick sequence.
2. **Coupling audit** against the Service Module Pattern table (§3.4).
3. **ADR compliance verification:** ADR-16 (static scope), ADR-17 (tick budget
   <10ms), ADR-39 (transient service locator — no stored `Application*`).
4. **Secret registration:** verify all OAuth tokens use `Redactor::instance()`.
5. **Build validation:** zero errors, zero warnings, flash fits 16MB partition,
   free DRAM >40KB for WiFi+BLE stack.
6. **Radio coexistence test (SPEC §8.5) — max configuration:**
   - **3 Peripheral connections** (FTMS notifying 2 Hz on ≥1, plus tablet + nRF
     Connect)
   - **1 Central connection** (HR strap relaying ~1 Hz)
   - Portal `/overlay` WebSocket streaming at 2 Hz
   - HTTP upload (Strava or simulated)
   - **5 minutes continuous**
   - Pass criteria:
     - FTMS gaps <600ms
     - HR steady-state gaps <2.5s
     - HR dropout recovery <35s
     - WebSocket survives
     - Upload completes in 15s
   - HR relay latency: p99 <50ms (measured via `BLENotification.receivedAtMs`)
7. **README.md:** comprehensive build/flash/setup instructions, wiring reference,
   architecture overview, license badge.
8. **Known limitations:** document per §13.4 (power accuracy, synthetic speed,
   no ERG mode, single HR device).

After this PR merges, the firmware is release-candidate quality.

## Prior Art

- [wo10-integration/WORKORDER.md](../archive-pa-decomposition/wo10-integration/WORKORDER.md)

## Work Order Ledger
<!-- PA fills this in during planning -->

## Session Log
<!-- PA enriches this progressively -->

## Status
**Plan Status:** NOT STARTED
