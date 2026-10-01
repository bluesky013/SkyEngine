## 1. Determinism foundation

- [ ] 1.1 Define the deterministic simulation mode and reject non-deterministic math mode configurations
- [ ] 1.2 Provide a deterministic RNG seeded from the tick and expose it to simulation code
- [ ] 1.3 Audit and enforce deterministic entity/component iteration order in the simulation path
- [ ] 1.4 Forbid wall-clock and frame-rate dependence in the simulation path
- [ ] 1.5 Add tests asserting identical results for identical inputs across two simulated peers

## 2. Lockstep input exchange

- [ ] 2.1 Define the input frame payload (tick + per-player inputs)
- [ ] 2.2 Exchange input frames on the reliable-ordered channel and block a tick until all frames arrive
- [ ] 2.3 Implement configurable input delay
- [ ] 2.4 Implement reconnect resynchronization from authoritative state before rejoining lockstep
- [ ] 2.5 Add tests for ordering, delay, and resync

## 3. Rollback replay

- [ ] 3.1 Capture/restore deterministic simulation state using the physics snapshot contract and stable ids
- [ ] 3.2 Implement rollback-to-tick, corrected input application, and resimulation to the present
- [ ] 3.3 Bound rollback depth and fall back to resync beyond the limit
- [ ] 3.4 Gate rollback to deterministic mode only
- [ ] 3.5 Add tests for late-input correction and bounded rollback

## 4. Desync detection and recovery

- [ ] 4.1 Exchange simulation hashes at a configured cadence
- [ ] 4.2 Report mismatches and initiate resynchronization from authoritative state
- [ ] 4.3 Add a test that a forced divergence is detected and recovered

## 5. Validation

- [ ] 5.1 Build and run lockstep/rollback tests (using a simplified deterministic sim until `Exact` physics exists)
- [ ] 5.2 Confirm `engine/network` core and the replication path are unchanged
- [ ] 5.3 Document the dependency on a deterministic physics backend (`physics-determinism`) and deferred items
