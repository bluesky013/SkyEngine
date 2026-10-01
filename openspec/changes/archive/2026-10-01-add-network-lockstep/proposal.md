## Why

State replication is the wrong shape for RTS and fighting genres: thousands of units and a shared, exactly-reproducible simulation are better served by exchanging only inputs while every peer runs an identical deterministic simulation. Neither `add-network-replication` nor `add-network-prediction` provides determinism, and the engine's physics `Exact` math mode is currently reserved. This change defines deterministic lockstep and rollback replay, and the determinism requirements they depend on.

## What Changes

- **Input lockstep.** Peers exchange input frames on a reliable-ordered channel and advance a fixed, deterministic simulation tick; no entity state is transmitted in steady state.
- **Input delay and prediction policy.** A configurable input delay, and for latency-sensitive genres an optional predict-remote-input mode, are defined.
- **Rollback replay.** Using deterministic state capture/restore, the simulation can be rolled back to a prior tick, corrected inputs applied, and frames resimulated.
- **Determinism contract.** Fixed-step only, deterministic iteration order, deterministic RNG, no wall-clock or frame-rate dependence, and reliance on a deterministic backend math mode (`Exact`, currently reserved in `engine/physics`).
- **Desync detection and recovery.** Peers exchange periodic simulation hashes; a mismatch triggers a resynchronization from an authoritative snapshot.
- **Session resync.** Reconnecting peers resynchronize the deterministic state using the core session identity.
- **Non-goals**: the state-replication path (unchanged), client authority, and a deterministic physics backend (tracked separately with `physics-determinism`); this change consumes it.

## Capabilities

### New Capabilities

- `network-lockstep`: input frame exchange, fixed deterministic tick advancement, input delay, and reconnect resync.
- `network-determinism`: the constraints and verification that make a simulation reproducible across peers (fixed step, deterministic ordering/RNG/time, deterministic math mode).
- `network-rollback`: deterministic state capture/restore, rollback-to-tick, correction, and resimulation.

### Modified Capabilities

<!-- None: this adds a deterministic simulation path alongside replication; no existing requirements change. -->

## Impact

- **Engine**: adds `NetworkLockstep` (`engine/network/lockstep/`, links `NetworkReplication` + `Physics`) and a determinism surface over the ECS/world; `engine/network` core is unchanged.
- **Physics**: depends on a deterministic math mode (`PhysicsMathMode::Exact`, reserved), delivered by introducing **Jolt Physics as a second complete physics engine** behind `IPhysicsBackend` (with `CROSS_PLATFORM_DETERMINISTIC`), and reuses `IPhysicsWorld::CaptureState`/`RestoreState` for rollback.
- **World/ECS**: requires deterministic component iteration and RNG; state capture/restore over stable ids; gameplay-layer float/order determinism is a documented contract.
- **Consumers**: games select lockstep vs replication per mode; the lockstep path transmits inputs only.
- **Compatibility**: additive; depends on `add-network-core`, `add-network-replication`, and the `Exact` physics backend (`plugins/jolt` + Jolt third-party packaging), which does not exist yet.
