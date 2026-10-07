## Why

The world/actor model mixes ownership: `World` holds `std::vector<std::shared_ptr<Actor>>` while `Actor` holds a raw `World*` and `TransformComponent` holds raw parent/child pointers. Because `GetActors()` and `CreateActor()` expose `shared_ptr`, any external holder can keep an `Actor` alive after it logically left the world, and detach/attach invariants can be bypassed. Entity lookup and removal were also linear (the O(1) index added in `harden-framework-components` fixes that). Commercial engines own entities exclusively and reference them by non-owning pointer.

## What Changes

- **BREAKING**: `World` owns actors exclusively (`std::vector<std::unique_ptr<Actor>>`). The public API returns non-owning `Actor*`; `DetachFromWorld` returns `std::unique_ptr<Actor>` (ownership out) and `AttachToWorld` takes `std::unique_ptr<Actor>` (ownership in). Remove the `ActorPtr`/`ActorWeakPtr` aliases.
- Keep the dense actor array and the O(1) id → slot index; removal is swap-remove.
- Migrate consumers: `engine/test/framework/ComponentTest.cpp` and `plugins/bullet/test/BulletBackendTest.cpp`. Other non-Qt consumers (render adaptor, audio, navigation, sandbox, guizmo) compile unchanged. The Qt `engine/editor` is intentionally **not** migrated (deprecated).
- Not planned: generation-safe entity handles (a stale raw `Actor*` is not detectable). Revisit only if dangling references become a real problem.
- No behavior change to scene semantics; ownership/identity only.

## Capabilities

### New Capabilities
- `world-entity-ownership`: world-exclusive entity ownership with non-owning actor references.

### Modified Capabilities
<!-- network-replication-source maps Actor::GetUuid() to ReplicatedEntityId; that mapping is out of scope here. -->

## Impact

- Code: `engine/framework/include/framework/world/{World.h,Actor.h}`, `engine/framework/src/world/{World.cpp,Actor.cpp}`, `engine/test/framework/ComponentTest.cpp`, `plugins/bullet/test/BulletBackendTest.cpp`.
- Not touched: `engine/editor/**` (Qt, deprecated) — it will not compile against the new API and is left as-is by decision.
- Depends on `harden-framework-components` (the O(1) index).
- No serialized format change (`Actor` uuid stays the persistent identity).
