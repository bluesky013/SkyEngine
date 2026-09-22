## Why

`engine/physics` already uses the right shape: a render-free leaf module (`Physics` links only `Framework`) exposing abstract interfaces (`PhysicsWorld`, `CollisionObject`, `RigidBody`, `CharacterController`, `IShapeImpl`) plus a backend factory (`PhysicsRegistry::Impl`), with `plugins/bullet` as the concrete backend. That direction is correct and a second backend is possible in principle. However, the abstraction has gaps that block a clean second backend (PhysX) and even weaken the current Bullet path:

- The backend factory has no lifecycle hook, so a backend needing global init/teardown (PhysX `PxFoundation`/`PxPhysics`/`PxCooking`) has nowhere to do it.
- There is no query API at all (no raycast/sweep/overlap), which character controllers, terrain picking, and gameplay need.
- There is no material abstraction (`PhysicsMaterial.h` is empty) and friction/restitution are hardcoded in the Bullet backend (`BulletCollisionObject` restitution 1.0, `BulletRigidBody` 0.5).
- Collision filtering is only a 32-bit bitmask/group-and-mask convention with no documented backend contract (PhysX uses a filter shader plus `PxFilterData`).
- The shape set is limited to box/sphere/triangle-mesh; heightfield and capsule — needed for terrain collision and character controllers — are missing, and triangle-mesh creation has no defined cooking/timing contract.
- `PhysicsDebugDraw.h` includes `render/resource/Technique.h` and takes `RDGfxTechPtr`, leaking a render type into the physics core, and the Bullet backend links `RenderAdaptor` because of it.
- `CharacterController` is an empty base class.

This change completes the abstraction so backends stay swappable, and wires the missing behavior into the Bullet backend. It is a prerequisite for terrain collision (heightfield) and for any future PhysX backend.

## What Changes

- Add backend lifecycle to `PhysicsRegistry::Impl` (`Init`/`Shutdown`) and route it through the registry; the Bullet backend keeps no-op behavior.
- Add backend-neutral shape descriptions: `HeightFieldShape` (grid dimensions, sample buffer, height scale/offset, up axis) and `CapsuleShape`, and extend the factory with `CreateHeightField`/`CreateCapsule`; implement both in the Bullet backend (`btHeightfieldTerrainShape`, `btCapsuleShape`).
- Add a backend-neutral query API on `PhysicsWorld`: raycast, sweep, and overlap, returning backend-neutral result structures; implement it in the Bullet backend.
- Introduce `PhysicsMaterialData` (static/dynamic friction, restitution) plus `CreateMaterial`; make `CollisionObject`/`RigidBody` consume materials instead of hardcoded constants.
- Define and document the collision-filter contract (how `CollisionFilterBit` group/mask maps onto each backend) and implement the Bullet mapping; keep it PhysX-filter-shader-ready.
- Make physics debug output render-agnostic: replace the `RD*`-typed debug interface with plain debug geometry (position/color triangle/line data), like navigation's `NaviDebugGeometry`, and remove `render/` includes from `engine/physics`.
- Give `CharacterController` a minimal contract and implement it in the Bullet backend (kinematic character controller).
- Update `engine/physics` and `plugins/bullet` CMake/link so the physics core has no render dependency.
- Make triangle-mesh collision data render-agnostic: `TriangleMeshShape` carries `CounterPtr<TriangleMesh>`, `MeshPhysicsConfig` references a mesh asset resolved through an interface seam (`CreatePhysicsMesh`), and the aurora implementation lives in a bullet-plugin submodule (`BulletPhysicsAuroraModule`); physics never loads legacy render meshes and no physics target links legacy render.

## Capabilities

### New Capabilities

- `physics-backend`: backend factory lifecycle, backend-neutral shape descriptions (including heightfield and capsule), shape-creation contract, and the Bullet backend alignment.
- `physics-query`: raycast, sweep, and overlap queries on the physics world with backend-neutral result types and a Bullet implementation.
- `physics-material`: material data (friction/restitution) and material creation, consumed by collision objects and rigid bodies instead of hardcoded constants.
- `physics-filter`: documented, backend-neutral collision filtering contract over `CollisionFilterBit`, implemented for Bullet and ready for PhysX filter shaders.
- `physics-debug-draw`: render-agnostic physics debug geometry output that removes render types from the physics core.

### Modified Capabilities

<!-- No physics capabilities exist under openspec/specs/. -->

## Impact

- **`engine/physics`**: `PhysicsBase.h` (new shapes; `TriangleMeshShape` now carries `CounterPtr<TriangleMesh>` and `MeshPhysicsConfig` references a collision mesh asset), `PhysicsShape.h`, `PhysicsRegistry.h/.cpp` (lifecycle + shape creation), `PhysicsWorld.h` (queries + material), `CollisionObject.h`, `RigidBody.h`, `PhysicsMaterial.h`, `CharacterController.h`, `PhysicsDebugDraw.h` (render-free), `components/*` (collision mesh reference), CMake.
- **`plugins/bullet`**: implements lifecycle (no-op), heightfield/capsule shapes, queries, material, filter mapping, and render-free debug draw; links only `Physics` + `bullet3` (no legacy render). Its aurora render bridge submodule (`BulletPhysicsAuroraModule`) converts aurora mesh assets into physics mesh data and registers the seam.
- **`plugins/recast`**: unaffected; it consumes `PhysicsShape::GetTriangleMesh()` and triangle-mesh colliders, whose contract is preserved.
- **`plugins/terrain`**: the terrain collision layer (in `terrain-large-world-core`) depends on `HeightFieldShape` from this change; this change owns the shape, terrain owns the per-tile collider and streaming lifecycle.
- **PhysX readiness**: a future `plugins/physx` backend implements `PhysicsRegistry::Impl` (lifecycle, shapes with cooking, `PxScene` queries, `PxMaterial`, filter shader, `PxControllerManager`) with no changes to `engine/physics` consumers. Implementing PhysX is out of scope here.
- No new third-party dependencies.
