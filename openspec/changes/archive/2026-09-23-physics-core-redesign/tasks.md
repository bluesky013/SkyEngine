## 1. Engine contract (interface/data/utility only)

- [x] 1.1 Add `physics/PhysicsObjectId.h` (index + generation handle) and its pure resolution helpers
- [x] 1.2 Add `physics/PhysicsShapes.h` with backend-neutral shape descriptors (box, sphere, capsule, heightfield, triangle mesh, convex, compound) and pure validation
- [x] 1.3 Add `physics/PhysicsBody.h` with `BodyKind`, `PhysicsBodyDesc` (shape, mass, damping, material ref, filter, CCD, gravity, initial transform) and pure validation
- [x] 1.4 Add `physics/PhysicsFilter.h` with the group/mask contract, `PhysicsQueryFilter`, named layer registration, and the pure `Accepts` function
- [x] 1.5 Extend `physics/PhysicsMaterial.h` with combine modes and linear/angular damping
- [x] 1.6 Add `physics/PhysicsConstraint.h` with descriptors (fixed, hinge, slider, distance, generic 6-DOF), handles, limits, and drives
- [x] 1.7 Add `physics/PhysicsCharacter.h` with the character descriptor and controller contract (move, grounded, transform, capsule)
- [x] 1.8 Add `physics/PhysicsEvents.h` with the contact/trigger event types and payload (ids, point, normal, impulse)
- [x] 1.9 Add `physics/PhysicsSnapshot.h` with the world-state snapshot types keyed by `PhysicsObjectId`
- [x] 1.10 Add `physics/PhysicsStepping.h` with the step config and the pure fixed-step `PhysicsStepper` (accumulator, max sub-steps, interpolation factor)
- [x] 1.11 Add `physics/PhysicsDeterminism.h` with the `PhysicsMathMode { Fast, Exact }` enum and the mode/determinism reporting helpers (interface only; no math implementation)
- [x] 1.12 Add `physics/IPhysicsBackend.h` with the capability descriptor (includes math mode + deterministic flag), lifecycle, world/material creation, and shape cooking contract
- [x] 1.13 Rework `physics/PhysicsRegistry.h` into a pure single-active-backend factory registry (register/unregister/capabilities/create world, reject unavailable mode)
- [x] 1.14 Replace `physics/PhysicsQuery.h` results with handle-based `PhysicsQueryHit`/`PhysicsQueryOverlap` and add query filter support
- [x] 1.15 Extend `physics/PhysicsDebugGeometry.h` with debug categories and add `physics/PhysicsWorldStats.h`
- [x] 1.16 Add `#pragma once` to every physics header and remove component/implementation declarations from the engine module
- [x] 1.17 Confirm `engine/physics` links only engine interface libraries and contains no plugin includes

## 2. Bullet backend (Fast, first-version feature complete)

- [x] 2.1 Restructure `plugins/bullet` around `IPhysicsBackend` (capabilities reporting `Fast` / non-deterministic, lifecycle, world create/destroy) and register through the new registry
- [x] 2.2 Implement world-owned object storage with stable id assignment and generation-guarded lookup
- [x] 2.3 Implement shape cooking for box, sphere, capsule, heightfield (apply scale/offset/up axis), triangle mesh, and compound, with no render dependency
- [x] 2.4 Set the backend user pointer on every object kind (bodies, characters, triggers) so queries resolve handles
- [x] 2.5 Implement the fixed `Step(dt)` entry point with no wall-clock/frame-delta reads
- [x] 2.6 Implement handle-based `Raycast`/`Sweep`/`Overlap` with skip-and-continue filtering and deterministic ordering
- [x] 2.7 Implement material creation and per-body material/damping application (no hardcoded constants)
- [x] 2.8 Implement constraint descriptors (fixed, hinge, slider, distance, generic 6-DOF) as first-version functionality
- [x] 2.9 Implement continuous collision detection as first-version functionality and advertise `supportsCCD`
- [x] 2.10 Implement the capsule character controller against the new contract
- [x] 2.11 Implement the contact/trigger event stream
- [x] 2.12 Implement snapshot capture/restore over stable ids (dynamic/kinematic default, full option)
- [x] 2.13 Implement categorized debug geometry collection
- [x] 2.14 Destroy all backend objects safely when a world is destroyed (no dangling world pointer in controller/object teardown)
- [x] 2.15 Keep the collision-mesh conversion seam render-agnostic and register it in `plugins/bullet/render`
- [ ] 2.16 Add backend tests: fixed-step repeatability on the same build, query handle resolution + filtering, constraints, CCD, character, events, snapshot restore, teardown

## 3. Physics runtime (inside the backend plugin)

- [x] 3.1 Implement the runtime inside `plugins/bullet` (`PhysicsSystem` sub-system + `PhysicsBodyComponent`); the module registers the sub-system under `PHYSICS_SYSTEM_NAME` and registers the backend and components (no separate plugin)
- [x] 3.2 Add the world sub-system wrapper that attaches an engine physics world to a `sky::World` and owns the stepper; reject `Exact` requests while unavailable
- [x] 3.3 Move `RigidBodyComponent`/`CollisionComponent` (and new character/constraint components) into the plugin using handle-based, detach-safe ownership
- [x] 3.4 Implement step driving: frame delta -> accumulator -> fixed step(s) -> interpolated transforms exposed to components
- [x] 3.5 Implement contact/trigger event dispatch from the world sub-system to consumers
- [x] 3.6 Implement serialization registration and component round-trip with stable field names and versioning
- [x] 3.7 Add mobile budgets: default sleep, active-body stats, triangle mesh restricted to static, opt-in CCD
- [x] 3.8 Add runtime-plugin tests: attach/detach safety, component round-trip, event dispatch, stats, mode rejection

## 4. Consumer migration

- [x] 4.1 Migrate the terrain collision layer to handle-based bodies and the heightfield descriptor
- [x] 4.2 Migrate navigation's collision-mesh consumption to the new triangle-mesh/query surface
- [x] 4.3 Migrate character/animation paths to the character controller contract and handle-based reads
- [x] 4.4 Migrate any raycast/sweep/overlap consumers to typed handle results and query filters
- [x] 4.5 Remove direct includes of backend/concrete physics types from consumers

## 5. Editor integration (PENDING)

Status: paused. The new editor is the Sandbox launched through `Launcher`; `engine/editor` is legacy and MUST NOT be a dependency. The engine-facing contract is ready (`phy::IPhysicsSystem`: `GetWorld`, `SetSimulating`/`IsSimulating`/`SingleStep`, plus `IPhysicsWorld` debug geometry/snapshots); wiring waits on the Sandbox world/subsystem setup.

- [ ] 5.1 Expose play/pause/single-step/replay over the stepper and snapshots (via `phy::IPhysicsSystem`), once Sandbox is launched through `Launcher`
- [ ] 5.2 Expose per-category debug geometry retrieval and route it to the editor/render layer
- [ ] 5.3 Add editor property support (getter/setter) for the migrated physics components
- [ ] 5.4 Verify the editor builds and simulates with physics disabled (feature-absent path; without the legacy `engine/editor` dependency; `Sandbox` launched via `Launcher`)

## 6. Remove old implementation and build config

- [x] 6.1 Delete the engine-side physics implementation (old registry factory, `RigidBody`/`CollisionObject` base classes, components) once consumers are migrated
- [x] 6.2 Update `plugins/plugins.json` and `configs/modules_*.json`: drop the separate runtime module and rename the bridge to `BulletPhysicsRenderModule`
- [x] 6.3 Update plugin CMake and `sky_plugin_apply_target_dependencies` for the new targets
- [x] 6.4 Remove dead code (unregistered `BulletPhysicsConfig` reflection, typo'd/nullable shape serialization)

## 7. Validation

- [x] 7.1 Run `openspec validate physics-core-redesign` and fix any spec issues
- [x] 7.2 Build engine + plugins on the desktop target with no physics/bullet regressions
- [x] 7.3 Run all physics, terrain, navigation, and vegetation tests
- [x] 7.4 Verify `Exact` mode is reported unavailable and rejected on the delivered `Fast` backend
- [x] 7.5 Confirm `engine/physics` has no plugin/render include or link and document the module boundary

## 8. Reserved (future change, not implemented here)

- [ ] 8.1 Add the future `Exact` backend (`plugins/jolt` with `CROSS_PLATFORM_DETERMINISTIC`, same source/defines on all platforms)
- [ ] 8.2 Sort/re-validate Jolt's non-deterministic outputs (broadphase queries, listener order, `GetActiveBodies`)
- [ ] 8.3 Add the cross-platform determinism trace test, including console targets
- [ ] 8.4 Enable rollback re-simulation and lockstep on `Exact` worlds
