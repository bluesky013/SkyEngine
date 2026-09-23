## Why

The physics subsystem grew organically and now blocks the engine's direction: `engine/physics` carries feature *implementation* (components, sub-system wiring, asset/mesh coupling) instead of interfaces and data, query results cannot resolve rigid bodies or characters, world teardown can use-after-free, and simulation is a variable-delta, single-threaded step with no snapshot or editor-simulation story. The engine targets open worlds, aurora RHI, and cross-platform runtime (PC / Android / iOS / consoles), so physics must become a backend-swappable subsystem with a stable contract. Redesigning the abstraction now — before more gameplay, character, and networking code depends on it — is cheaper than retrofitting it later.

## What Changes

- **Re-establish the module boundary.** `engine/physics` becomes interface-, descriptor-, and pure-utility-only; all runtime implementation moves to a physics plugin. Bullet is the first backend behind a capability-advertising backend contract; further backends stay addable without touching consumers. **BREAKING**: current `engine/physics/src/components`, `PhysicsRegistry` factory shape, and `RigidBody`/`CollisionObject` ownership change.
- **Stable handle model with explicit ownership.** A world owns its physics objects; components and consumers reference them by `PhysicsObjectId`, not raw pointers, removing the component↔backend lifetime coupling that currently dangles on detach and on teardown.
- **Fixed-step simulation.** Simulation advances on a fixed timestep with an accumulator and transform interpolation, decoupled from frame delta and wall clock; the engine owns the stepper and calls the backend's fixed `Step(dt)`.
- **Selectable math mode, `Exact` reserved.** Expose `PhysicsMathMode { Fast, Exact }` and backend capability reporting. This change delivers `Fast` (Bullet, used unmodified); `Exact` (cross-platform bit-exact) is **interface-only** here and is planned as a later change using a third-party deterministic backend (Jolt with `CROSS_PLATFORM_DETERMINISTIC`). No in-house solver and no fixed-point rewrite; Bullet is not modified.
- **Job-friendly step seam.** The step exposes defined read/write phases with a single-threaded fallback and backend capability flags, keeping the door open for deterministic parallel reduction in a future `Exact` backend.
- **Snapshot/restore.** World state can be captured and restored over stable ids for editor replay and rollback-by-restore (re-simulation is reserved for the future `Exact` mode).
- **Engine-side gameplay contracts, implemented in the first version.** Constraint/joint descriptors (fixed, hinge, slider, distance, generic 6-DOF), a capsule character controller, continuous collision detection, and a contact/trigger event stream are defined in the engine and implemented by the backend in the first version.
- **Corrected, typed queries.** `Raycast`/`Sweep`/`Overlap` return typed handles that resolve any object kind, filters skip-and-continue to the next candidate, and result ordering is deterministic.
- **Extended materials and serialization.** Material gains combine modes and damping; body/component data serializes with stable field names and round-trips, replacing the current typo'd and partially-serialized shape data.
- **Editor simulation contract.** Play/pause/single-step/replay plus render-free debug geometry exposed by category; the collision-mesh conversion seam stays render-agnostic and lives in the backend plugin's render bridge submodule.

## Capabilities

### New Capabilities

- `physics-core`: interface/data-only engine module, world/scene ownership, stable `PhysicsObjectId` handles, object lifecycle, and consumer resolution through the world sub-system.
- `physics-bodies`: body kinds (static/dynamic/kinematic), backend-neutral shape descriptors, mass/inertia/damping, sleep, and CCD configuration.
- `physics-stepping`: fixed-step advancement, accumulator/sub-step policy, transform interpolation, and the job-friendly step seam with a single-threaded fallback.
- `physics-snapshot`: capture and restore of world state over stable ids for editor replay and rollback-by-restore.
- `physics-constraints`: backend-neutral joint/constraint descriptors and their creation/removal contract, implemented in the first version.
- `physics-character`: capsule character-controller contract (step height, slope limit, grounded, move semantics) with a backend-provided implementation in the first version.
- `physics-events`: contact/trigger event stream (begin/stay/end) delivered after the step without render dependencies.
- `physics-serialization`: serialization and round-trip of body descriptors, materials, filters, and physics components with stable field names.
- `physics-determinism`: the selectable `PhysicsMathMode` interface (`Fast` delivered, `Exact` reserved), per-mode backend capability reporting, and the reserved cross-platform determinism contract to be satisfied by a future third-party backend.

### Modified Capabilities

- `physics-backend`: backend contract gains capability descriptors (math mode/determinism, job support, supported shape/constraint set), world-owned object lifecycle, and updated registration semantics. The shipped `Fast` backend reports determinism unavailable; `Exact` is reserved.
- `physics-query`: results resolve typed handles for every object kind, filters skip-and-continue, and ordering is deterministic.
- `physics-filter`: adds explicit query-filter semantics on top of the group/mask contract, with documented backend mapping.
- `physics-material`: adds friction/restitution combine modes and damping to the material data.
- `physics-debug-draw`: adds per-category debug geometry and the editor consumption contract while remaining render-free.

## Impact

- **Engine**: `engine/physics` is reduced to `physics/` interfaces + descriptors + pure utilities (no components, no registry factory implementation); new headers for handles, bodies, stepping, snapshot, constraints, character, events, serialization, and `PhysicsMathMode`.
- **Plugins**: `plugins/bullet` is the first physics implementation plugin (unmodified Bullet): it provides the `IPhysicsBackend`, its own world sub-system registered under the engine `PHYSICS_SYSTEM_NAME`, the body/character/constraint components, serialization, and the collision-mesh conversion seam in `plugins/bullet/render`. A future `plugins/physx` / `plugins/jolt` is a peer plugin registering the same sub-system name; there is no separate backend-agnostic runtime plugin.
- **Consumers**: `RigidBodyComponent`/`CollisionComponent` (and their `"Physics"` component registration) move to the physics plugin and use handle-based, snapshot-aware ownership; terrain collision layer, navigation, and character/animation paths migrate to the typed query and event APIs.
- **Build/config**: `plugins/plugins.json`, plugin CMake targets, and plugin dependencies are updated for the new layout; plugins depend only on `Launcher` (the legacy Qt `engine/editor` target is retired). The Sandbox editor is launched through `Launcher` and consumes physics via `phy::IPhysicsSystem`.
- **Compatibility**: runtime scene data carrying the old physics component fields is incompatible and must be re-authored or migrated (no production physics scene data is expected).
