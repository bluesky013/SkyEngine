## 1. Backend lifecycle

- [x] 1.1 Add `Init`/`Shutdown` hooks to `PhysicsRegistry::Impl` and invoke them from `PhysicsRegistry::Register`/`UnRegister`
- [x] 1.2 Implement no-op lifecycle hooks in the Bullet backend
- [x] 1.3 Guard shape/world creation against an uninitialized or absent backend
- [x] 1.4 Add a test for register -> init -> create -> unregister -> shutdown ordering

## 2. Extended shape set

- [x] 2.1 Add `HeightFieldShape` (grid width/height, sample format, height scale/offset, up axis) to `physics/PhysicsBase.h`
- [x] 2.2 Add `CapsuleShape` (radius, height, pivot) to `physics/PhysicsBase.h`
- [x] 2.3 Extend `PhysicsRegistry::Impl` and `PhysicsShape` with `CreateHeightField` and `CreateCapsule`
- [x] 2.4 Register serialization for the new shape descriptions
- [x] 2.5 Implement `btHeightfieldTerrainShape` in `BulletShapes` with sample conversion, up-axis, and quad-edge handling
- [x] 2.6 Implement `btCapsuleShape` in `BulletShapes`
- [x] 2.7 Add tests for heightfield and capsule shape creation

## 3. Query API

- [x] 3.1 Define backend-neutral query result types (raycast hit, sweep result, overlap result)
- [x] 3.2 Add `Raycast`, `Sweep`, and `Overlap` to `PhysicsWorld` with a filter/mask parameter
- [x] 3.3 Implement the query methods in `BulletPhysicsWorld` using `btCollisionWorld`
- [x] 3.4 Add tests for raycast hit/miss and filtered queries

## 4. Material abstraction

- [x] 4.1 Populate `physics/PhysicsMaterial.h` with `PhysicsMaterialData` (static friction, dynamic friction, restitution)
- [x] 4.2 Add `CreateMaterial` to `PhysicsRegistry::Impl` and the Bullet backend
- [x] 4.3 Let `CollisionObject`/`RigidBody` accept a material and apply it
- [x] 4.4 Remove the hardcoded restitution/friction from `BulletCollisionObject` and `BulletRigidBody`
- [x] 4.5 Define and document the default material
- [x] 4.6 Add a test asserting material values reach the backend object

## 5. Filter contract

- [x] 5.1 Document the `CollisionFilterBit` group/mask contract (how it is interpreted and must be mapped)
- [x] 5.2 Verify the Bullet group/mask mapping matches the documented contract and add a test
- [x] 5.3 Record the equivalent boolean filter-function form used by a filter-shader backend
- [x] 5.4 Note the >32-bit escalation path in the contract documentation

## 6. Render-agnostic debug geometry

- [x] 6.1 Replace the `RD*`-typed debug interface with a plain `PhysicsDebugGeometry` type (positions, colors, primitives)
- [x] 6.2 Remove `render/` includes from all `engine/physics` headers
- [x] 6.3 Update the Bullet debug draw to produce plain geometry
- [x] 6.4 Drop the `RenderAdaptor` link from the Bullet plugin if no longer required
- [x] 6.5 Confirm the physics core builds with no render include or library dependency

## 7. Character controller contract

- [x] 7.1 Define a minimal `CharacterController` contract (position, move, gravity/slope/step parameters)
- [x] 7.2 Implement it in the Bullet backend with `btKinematicCharacterController`
- [x] 7.3 Add `CharacterController` creation wiring in `PhysicsWorld`
- [x] 7.4 Add a test for basic character movement and ground contact

## 8. Documentation and validation

- [x] 8.1 Record the PhysX backend mapping (lifecycle, cooking, `PxRigidStatic`/`PxRigidDynamic`, `PxMaterial`, filter shader, `PxScene` queries, `PxControllerManager`) as the readiness checklist
- [x] 8.2 Run the physics tests and confirm they pass
- [x] 8.3 Build the engine and confirm no physics or bullet compile/link regressions
- [x] 8.4 Confirm `plugins/recast` still resolves `GetTriangleMesh` and triangle-mesh colliders unchanged
