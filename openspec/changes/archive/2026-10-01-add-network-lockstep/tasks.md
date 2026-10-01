## 1. Determinism foundation

- [x] 1.1 Define the deterministic simulation mode and reject non-deterministic math mode configurations
- [x] 1.2 Provide a deterministic RNG seeded from the tick and expose it to simulation code
- [x] 1.3 Audit and enforce deterministic entity/component iteration order in the simulation path
- [x] 1.4 Forbid wall-clock and frame-rate dependence in the simulation path
- [x] 1.5 Add tests asserting identical results for identical inputs across two simulated peers

## 2. Lockstep input exchange

- [x] 2.1 Define the input frame payload (tick + per-player inputs)
- [x] 2.2 Exchange input frames on the reliable-ordered channel and block a tick until all frames arrive
- [x] 2.3 Implement configurable input delay
- [x] 2.4 Implement reconnect resynchronization from authoritative state before rejoining lockstep
- [x] 2.5 Add tests for ordering, delay, and resync

## 3. Rollback replay

- [x] 3.1 Capture/restore deterministic simulation state using the physics snapshot contract and stable ids
- [x] 3.2 Implement rollback-to-tick, corrected input application, and resimulation to the present
- [x] 3.3 Bound rollback depth and fall back to resync beyond the limit
- [x] 3.4 Gate rollback to deterministic mode only
- [x] 3.5 Add tests for late-input correction and bounded rollback

## 4. Desync detection and recovery

- [x] 4.1 Exchange simulation hashes at a configured cadence
- [x] 4.2 Report mismatches and initiate resynchronization from authoritative state
- [x] 4.3 Add a test that a forced divergence is detected and recovered

## 5. Validation

- [x] 5.1 Build and run lockstep/rollback tests (using a simplified deterministic sim until `Exact` physics exists)
- [x] 5.2 Confirm `engine/network` core and the replication path are unchanged
- [x] 5.3 Document the dependency on a deterministic physics backend (`physics-determinism`) and deferred items

## 6. Notes (added during implementation)

- Determinism is provided by the `ILockstepSimulation` contract (fixed step via `ReplicationTick`, deterministic
  iteration order, deterministic RNG, deterministic math mode). The engine's shipped physics is still `Fast`;
  a deterministic `Exact` backend (Jolt) remains a separate dependency tracked by the physics module.
- `NetworkWorld` (Framework `Actor`/`ComponentBase`) integration is deferred with `add-network-replication`;
  the data-oriented path is the reference.
