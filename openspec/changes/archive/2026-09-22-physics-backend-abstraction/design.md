## Context

`engine/physics` is a render-free leaf module: `Physics` links only `Framework`, and its public surface is abstract (`PhysicsWorld`, `CollisionObject`, `RigidBody`, `CharacterController`, `IShapeImpl`) with a single backend factory, `PhysicsRegistry::Impl` (`PhysicsRegistry.h:30-43`), registered by `plugins/bullet` in `BulletModule.cpp`. This is the same "leaf interface + backend plugin" pattern as navigation/recast, and it already decouples the engine from Bullet in principle.

Concrete gaps found in the current code:

- `PhysicsRegistry::Impl` has no `Init`/`Shutdown`; a backend with global SDK state (PhysX) has nowhere to bootstrap.
- `PhysicsWorld` (`PhysicsWorld.h`) exposes only add/remove and gravity/debug toggles; there is no raycast/sweep/overlap anywhere in `engine/physics`.
- `PhysicsMaterial.h` is empty; `BulletCollisionObject` sets restitution `1.0` and `BulletRigidBody` sets `0.5` as hardcoded constants.
- Filtering is `CollisionFilterBit` (32-bit) plus `group`/`mask` on `CollisionObject`/`RigidBody`; the backend contract is undocumented and Bullet-specific.
- Shapes are `SphereShape`, `BoxShape`, `TriangleMeshShape` (`PhysicsBase.h`); `CompoundShape` is defined but unused; no heightfield or capsule.
- `PhysicsDebugDraw.h` includes `render/resource/Technique.h` and takes `RDGfxTechPtr`, and `plugins/bullet` links `RenderAdaptor` for `BulletDebugDraw`.
- `CharacterController` is an empty class.

Constraints: engine positions are 32-bit `float`; `plugins/recast` consumes `PhysicsShape::GetTriangleMesh()` and triangle-mesh colliders, so that contract must not break; `plugins/terrain` collision will consume a heightfield shape from this change; PhysX is a future backend, not implemented here.

## Goals / Non-Goals

**Goals:**

- Make `engine/physics` a complete backend-neutral abstraction so Bullet is swappable and PhysX can be added without touching core consumers.
- Add backend lifecycle, backend-neutral shape descriptions (heightfield, capsule), query API, material, and a documented filter contract.
- Remove the render-type leak from the physics core (debug draw becomes plain geometry).
- Keep the existing Bullet behavior working and implement every new contract in the Bullet backend.
- Provide a minimal character controller contract so terrain collision is actually usable.

**Non-Goals:**

- Implementing a PhysX backend (only making the abstraction ready and documenting the mapping).
- Multi-backend simultaneous registration (one active backend at a time, as today).
- Full compound shapes, convex hulls, soft bodies, vehicles, or cloth.
- Changing the triangle-mesh (`GetTriangleMesh`) contract relied on by navigation.
- Broad gameplay/character tuning; the character controller is minimal.

## Decisions

### D1: Lifecycle on the backend factory, single active backend

**Decision**: Add `Impl::Init()` / `Impl::Shutdown()` (or a backend-neutral `PhysicsBackend` init contract) invoked via `PhysicsRegistry` when a backend is registered/unregistered; keep one active backend at a time. Bullet's implementation is a no-op; PhysX would create/destroy `PxFoundation`/`PxPhysics`/`PxCooking`/dispatcher.

**Rationale**: PhysX requires strict global init/teardown; without it, a PhysX backend cannot be constructed at all. Keeping a single active backend matches `Singleton` usage today and avoids per-world SDK duplication.

**Alternatives considered**: Multiple registered backends selected per world (unneeded complexity). Lazy init on first `CreatePhysicsWorld` (fragile teardown, ordering issues).

### D2: Backend-neutral shape descriptions, cooking delegated to the backend

**Decision**: Add `HeightFieldShape` (grid width/height, sample bytes + sample format, height scale/offset, up axis) and `CapsuleShape` (radius, height, pivot) to `PhysicsBase.h`, and extend `PhysicsRegistry::Impl` with `CreateHeightField`/`CreateCapsule`. The shape description carries plain data only; any cooking (PhysX) happens inside the backend. Bullet builds `btHeightfieldTerrainShape` / `btCapsuleShape` directly.

**Rationale**: A backend-neutral description lets terrain produce collision data without knowing the backend, and keeps cook/timing backend-owned. Documenting the cooking contract now (sync vs async, caching) avoids a rework when PhysX arrives.

**Alternatives considered**: Heightfield as a triangle mesh (heavier and needs cooking anyway). Backend-specific shape factories exposed to terrain (reintroduces coupling).

### D3: Backend-neutral query API on `PhysicsWorld`

**Decision**: Add `Raycast`, `Sweep`, and `Overlap` to `PhysicsWorld`, returning backend-neutral result structures (`RaycastHit { object, position, normal, distance }`, `SweepResult`, `OverlapResult`), with a filter/mask parameter. Implement with `btCollisionWorld::rayTest` / `convexSweepTest`; PhysX maps to `PxScene::raycast/sweep/overlap`.

**Rationale**: Character controllers, terrain picking, and gameplay need queries; without an abstraction each backend would expose its own API and leak. Closest-hit semantics and a max-distance/filter input are the common denominator across Bullet and PhysX.

**Alternatives considered**: Expose raw backend queries through the backend pointer (couples consumers). Query only via terrain's own `Raycast` (does not cover generic physics queries).

### D4: Material abstraction

**Decision**: Define `PhysicsMaterialData` (static friction, dynamic friction, restitution) and `Impl::CreateMaterial`, and have `CollisionObject`/`RigidBody` accept a material; remove the hardcoded restitution/friction from the Bullet objects. Bullet maps material values onto `setFriction`/`setRestitution`; PhysX maps to `PxMaterial`.

**Rationale**: Friction/restitution must be data, not constants baked into the backend, for both correctness and backend parity. Materials are the shared vocabulary between backends.

**Alternatives considered**: Per-collider material fields on `CollisionData` (spreads authority); keeping backend constants (non-portable).

### D5: Documented 32-bit filter contract

**Decision**: Keep `CollisionFilterBit` (32-bit) + `group`/`mask` as the engine-level filter model and document the backend mapping: Bullet `addRigidBody(body, group, mask)`; PhysX `PxFilterData.word0 = group, word1 = mask` with a `PxSimulationFilterShader` implementing `(a.group & b.mask) && (b.group & a.mask)`. Note the escalation path (>32 bits -> backend filter callback) as a future option.

**Rationale**: The existing bitmask is sufficient for current needs and maps to both backends; making the contract explicit prevents each backend from inventing semantics.

**Alternatives considered**: Move to a callback/category model now (over-engineering); leave semantics implicit (blocks PhysX parity).

### D6: Render-agnostic debug geometry

**Decision**: Replace the `RD*`-typed `PhysicsDebugDraw` with a plain debug-geometry output (`PhysicsDebugGeometry`: positions, colors, line/triangle lists), analogous to `NaviDebugGeometry`. The backend converts its internal debug data into that form; a render layer (aurora or legacy) consumes it. Remove `render/` includes from `engine/physics` and, if possible, drop the `RenderAdaptor` link from `plugins/bullet`.

**Rationale**: The current header leaks a render type into the physics core and forces a render link on the backend, contradicting the leaf-module goal and blocking a render-optional PhysX backend.

**Alternatives considered**: Keep a render debug adapter separate from the interface (still leaves the include). Leave as-is (only fix when aurora lands).

### D7: Minimal character controller contract

**Decision**: Give `CharacterController` a minimal contract (position, move/displace, gravity/slope/step parameters, enable) and implement it in the Bullet backend with `btKinematicCharacterController` (or a sweep-based controller). PhysX would map to `PxControllerManager`.

**Rationale**: The empty base class means terrain collision cannot be exercised by a player; a minimal contract unblocks terrain collision and keeps backend parity.

**Alternatives considered**: Implement character control only in the Bullet plugin with no base contract (backend-specific, non-portable). Full character feature set (out of scope).

### D8: PhysX readiness is documented, not implemented

**Decision**: Record the concrete PhysX mapping (lifecycle, geometry cooking, `PxRigidStatic`/`PxRigidDynamic`, `PxMaterial`, filter shader, `PxScene` queries, `PxControllerManager`) in this design as the acceptance criterion for the abstraction. Implementing `plugins/physx` is a separate change.

**Rationale**: The point of the abstraction work is to be provably sufficient for a second backend; writing the mapping down makes the readiness testable and prevents over/under-abstracting.

### D9: Triangle-mesh collision data is render-agnostic

**Decision**: Physics triangle-mesh collision data is render-agnostic and carried as data. `TriangleMeshShape` holds `CounterPtr<TriangleMesh>` (runtime data); `MeshPhysicsConfig` keeps a serialized `Uuid mesh` reference to a mesh asset, and the core defines an interface seam (`SetPhysicsMeshProvider` / `CreatePhysicsMesh`) that resolves it into `TriangleMesh` data. The implementation/registration lives in a **render bridge submodule of the bullet plugin** (`plugins/bullet/aurora`, target `BulletPhysicsAuroraModule`), which converts aurora's render-agnostic CPU mesh payload (`aurora::MeshAssetData` vertex/index bytes) into physics `TriangleMesh` data. `engine/physics` stays interface/data only; no physics target links legacy render (neither the core nor `BulletPhysicsModule`); the `BulletPhysicsModule` backend links only `Physics` + `bullet3`.

**Rationale**: The legacy path loaded a render `Mesh` asset (`CreateTriangleMesh(MeshAsset)` in legacy `RenderAdaptor`) inside the backend, coupling physics to legacy render and forcing a legacy-render link. The principle is that core logic and data are decoupled from render: the core owns an interface/data seam; the render-specific conversion (aurora mesh asset -> physics mesh) lives in a render bridge submodule of the backend plugin, where render dependencies belong.

**Alternatives considered**: Load render meshes at runtime inside the core/backend (rejected: couples data/logic to render). Bridge that links legacy `RenderAdaptor` (rejected: legacy render, not aurora). Conversion directly in `engine/physics` (rejected: the core must not depend on a renderer; the bullet plugin's aurora submodule owns it).

## Risks / Trade-offs

- [Interface changes ripple into components] -> Keep new members virtual with safe defaults and preserve existing behavior; update `CollisionComponent`/`RigidBodyComponent` in the same change.
- [Triangle-mesh cooking/timing contract] -> Triangle-mesh collision data is render-agnostic and produced offline (D9); for PhysX, cooking of that data is asynchronous plus cached (the unused `IMeshConfigNotify` seam is a candidate hook).
- [Bullet heightfield conventions] -> Centered local space with unit grid spacing; apply shape local scaling to `resolution` or bake world-space samples, and test the collider against `QueryHeight`.
- [Render debug consumer missing] -> Plain debug geometry has no consumer until a render layer draws it; keep it optional and inert by default, and let the aurora layer consume it later.
- [32-bit filter limit] -> Acceptable for now; document the callback escalation path so a PhysX filter callback does not require a core redesign.
- [Character controller quality differs per backend] -> Keep the contract minimal and behavior-based; document Bullet/PhysX differences rather than promising identical motion.
- [Over-abstracting] -> Only abstract what two backends actually need (lifecycle, shapes, queries, material, filter, debug, character); do not add speculative interfaces.

## Migration Plan

1. Add backend lifecycle and the new shape descriptions/factory methods; implement no-ops and Bullet shapes. Additive, existing behavior unchanged.
2. Add the query API and material abstraction; migrate Bullet objects off hardcoded constants.
3. Replace the debug-draw interface with plain geometry; remove render includes/links from the physics core and Bullet where possible.
4. Add the character controller contract and the Bullet implementation.
5. Rollback strategy: phases are independent; each is additive until the debug-draw swap, which can revert to the old interface without affecting shapes/queries/materials.

## Open Questions

- Should `PhysicsWorld` queries return the `CollisionObject`/`RigidBody` handle, or a neutral shape/user-data identifier?
- Is material per-collider, per-body, or both (and what is the default when unset)?
- Should `PlaneShape`/`ConvexShape` be added now for completeness, or deferred until a consumer needs them?
- Where should physics debug geometry be produced (backend fills on demand, or world collects each frame)?
- Does any backend need multiple simultaneous worlds sharing one SDK instance (affects lifecycle, not current scope)?
