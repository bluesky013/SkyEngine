## Context

The engine has a fixed-step simulation path (`engine/physics` accumulator) and a snapshot/restore contract (`IPhysicsWorld::CaptureState`/`RestoreState`), but its shipped math mode is `Fast` and `Exact` (cross-platform deterministic) is explicitly reserved and planned with a third-party backend (Jolt Physics). `add-network-core` provides reliable-ordered channels and sessions; `add-network-replication` provides authoritative state for resync. This change defines the deterministic path those pieces enable: input lockstep for RTS and rollback replay for fighting genres.

Determinism here is a three-tier property, not a single switch: (1) physics determinism from the `Exact` backend, (2) engine-simulation determinism (ECS iteration order, RNG, time), and (3) gameplay determinism (float math and ordering in game code). `Exact` physics is necessary but not sufficient.

## Goals / Non-Goals

**Goals:**

- Reproducible simulation across peers from identical inputs, advancing on a fixed deterministic tick.
- Bounded rollback replay for latency-sensitive genres, reusing deterministic state capture/restore.
- Detect and recover from divergence.

**Non-Goals:**

- Defining or implementing the deterministic physics backend itself (the `plugins/jolt` adapter and its third-party packaging are owned by the physics module or a dedicated change; this change consumes `PhysicsMathMode::Exact`).
- Changing the state-replication path.
- Client authority or cheating prevention beyond input validation.

## Decisions

### D1. Input lockstep over the reliable-ordered channel

Peers exchange input frames on a reliable-ordered channel and advance a fixed simulation tick; steady-state entity state is not transmitted.

**Why:** RTS/fighting need reproducible shared state, not per-entity bandwidth. Reliable-ordered guarantees every peer sees the same input set for a tick. **Alternative:** state replication for RTS — rejected (bandwidth and consistency cost).

### D2. Determinism is a hard prerequisite, not best-effort

The simulation SHALL run on a fixed step with deterministic iteration order, deterministic RNG seeded from the tick, no wall-clock reads, and a deterministic math mode (`PhysicsMathMode::Exact`). Iteration order SHALL be stabilized by stable entity id rather than dense-array layout, because `SparseSet` swap-remove reorders storage; the same layout-independent ordering is required by the replication source seam (`add-network-replication` D9).

**Why:** a single nondeterministic source (float mode, unordered container iteration, `rand()`, `steady_clock`, storage-layout-dependent traversal) causes divergence that compounds every tick. **Trade-off:** constrains gameplay code and depends on a physics backend that is still reserved.

### D3. Input delay by default; prediction is opt-in

A configurable input delay ensures inputs arrive before their tick; genres that cannot tolerate it enable prediction of remote inputs plus rollback.

**Why:** RTS tolerates 1–3 frames of input delay well; fighting games need prediction + rollback. **Alternative:** always predict — rejected, it adds rollback cost where it is unnecessary.

### D4. Rollback uses deterministic capture/restore and resimulates frames

Rollback captures simulation state at a tick, restores it, applies corrected inputs, and resimulates subsequent frames.

**Why:** reuses the physics `CaptureState`/`RestoreState` contract and stable-id world capture rather than inventing a second mechanism. **Trade-off:** resimulation cost bounds how far back rollback can go.

### D5. Divergence is detected by periodic hashes

Peers exchange a simulation hash at a configured cadence; a mismatch triggers resynchronization from an authoritative snapshot.

**Why:** silent divergence is worse than a visible resync; per-tick hashes are too costly. **Alternative:** hash every tick — rejected.

### D6. Reconnect resynchronizes the deterministic state

A reconnecting peer obtains authoritative state through the core session and replication path before rejoining the input frame.

**Why:** a peer that missed inputs cannot deterministically catch up without a state transfer.

### D7. Lockstep and replication coexist as selectable modes

The deterministic path and the state-replication path are separate modes; a game selects one per session. The deterministic path is delivered as the `NetworkLockstep` sub-target (`engine/network/lockstep/`, links `NetworkReplication` + `Physics`), so games that do not use determinism do not link it.

**Why:** the two have incompatible state-ownership models and must not be mixed within a simulation.

### D8. The deterministic backend is a second complete physics engine (Jolt), not a Bullet patch

`Exact` mode is delivered by adding **Jolt Physics** as a peer backend behind `IPhysicsBackend` with `CROSS_PLATFORM_DETERMINISTIC`, not by modifying Bullet or writing an in-house fixed-point solver. Jolt is a complete rigid-body and collision engine (rigid bodies, full shape set, 12+ constraint types with motors, character controllers, vehicles, ragdolls, soft bodies, sensors, CCD, optional double precision) with a thin adapter — the same shape as the existing `plugins/bullet`.

**Commercial precedent:** Jolt ships in Decima (Horizon Forbidden West), Kojima Productions (Death Stranding 2), Dagor Engine (War Thunder), and Egosoft's engine (X4 Foundations); it is integrated by Godot 4.4 as an official module and by Source (`VPhysics-Jolt`) and Unreal plugins. This makes it a low-risk `Exact` choice rather than an experimental one.

**Industry precedent for the math approach:** classic deterministic engines used in-house integer fixed-point (Age of Empires, StarCraft/WarCraft) or in-house deterministic physics (Halo, Rocket League); engines that deliberately avoid determinism use state replication instead (Unreal Chaos, Source/Quake). Jolt's `CROSS_PLATFORM_DETERMINISTIC` (~+8% cost) is the modern "library" route that fits the engine's no-in-house-solver direction.

**Why:** avoids a large fixed-point rewrite and avoids modifying Bullet, while reusing the backend-swap seam already established by physics.

The three determinism tiers (see Context) map to distinct obligations:

1. **Physics** — Jolt with the flag, identical source/defines on every platform, plus engine-side normalization of non-deterministic output order (broadphase queries, contact/activation listener order, `GetActiveBodies`) and a deterministic narrow-phase/collector path for queries.
2. **Engine simulation** — deterministic ECS/component iteration, tick-seeded RNG, no wall-clock or frame-rate dependence, bounded fixed-step catch-up.
3. **Gameplay** — the engine-facing `float`/`Transform` boundary, math functions, sorting and container traversal must be deterministic; this is a documented contract and a validation obligation, not something the physics backend can provide.

**Trade-off:** console determinism is unproven (Jolt's tested platform matrix excludes consoles) and needs separate validation; and Jolt must be added to the third-party packaging (`cmake/thirdparty.json` currently has no `jolt`).

## Risks / Trade-offs

- **Deterministic physics backend not available** -> the feature is blocked behind `physics-determinism`; keep the interface ready and test with a simplified deterministic sim.
- **Jolt not yet packaged** -> `cmake/thirdparty.json` has no `jolt`; adding it (per-platform build + identical defines) is a prerequisite tracked by the physics side.
- **Console determinism unproven** -> Jolt's tested matrix excludes consoles; treat console `Exact` as a separate validation task.
- **Platform float differences** -> require the deterministic math mode; forbid fast-math and unordered iteration in the simulation path.
- **Gameplay-layer nondeterminism** -> `Exact` physics alone is insufficient; document forbidden patterns and add determinism checks in the simulation path.
- **Rollback cost with large worlds** -> bound rollback to a few frames and restrict rollback to genres with small/fast state.
- **Desync from gameplay code** -> provide determinism checks; document forbidden patterns in the simulation path.
- **Catch-up spiral** -> fixed per-frame tick budget, same policy as physics and replication.

## Follow-up Changes

- `plugins/jolt` and its third-party packaging providing `PhysicsMathMode::Exact` (owned by the physics side; this change consumes it).
- A cross-platform determinism trace test (reserved with the `Exact` backend).
- Dedicated-server target.

## Open Questions

- Default input delay and rollback window per genre.
- Simulation hash cadence and hash function.
- Whether lockstep is peer-to-peer or host-authoritative in the first version.
- How the deterministic RNG is exposed to gameplay code.
