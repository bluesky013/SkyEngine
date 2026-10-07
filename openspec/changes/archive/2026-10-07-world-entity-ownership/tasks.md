## 1. Ownership migration

- [x] 1.1 Store actors as `std::vector<std::unique_ptr<Actor>>`.
- [x] 1.2 `CreateActor`/`GetActorByUuid` return non-owning `Actor*`; `GetActors()` returns the unique_ptr vector.
- [x] 1.3 `AttachToWorld(std::unique_ptr<Actor>) -> Actor*` (ownership in); `DetachFromWorld(Actor*) -> std::unique_ptr<Actor>` (ownership out).
- [x] 1.4 `IWorldEvent::OnActorAttached/Detached(Actor*)` and `Actor::SetParent(Actor*)`.
- [x] 1.5 Remove `ActorPtr`/`ActorWeakPtr`.
- [x] 1.6 Keep the O(1) id → slot index; detach is swap-remove.

## 2. Consumer migration

- [x] 2.1 `engine/test/framework/ComponentTest.cpp` (updated, incl. detach/reattach).
- [x] 2.2 `plugins/bullet/test/BulletBackendTest.cpp` (detach returns ownership; test now registers `World::Reflect`).
- [ ] 2.3 `engine/editor` (Qt) — intentionally **not** migrated (deprecated).
- [x] 2.4 Non-Qt consumers that only iterate/`->`/`.get()` compile unchanged (render adaptor, audio, navigation, recast, network, terrain).

## 3. Generation-safe handles (not planned)

- Not in scope: `ActorHandle` generation safety is dropped from the plan. The design uses non-owning `Actor*`; revisit only if dangling references become a real problem.

## 4. Tests

- [x] 4.1 `DetachReattachKeepsSingleEntry` (ownership transfer across detach/attach).
- [x] 4.2 `ActorLookupConsistentAfterDetach` (index consistency).
- [x] 4.3 `PhysicsComponentTest.AttachDetachAndReattach` passes with the new semantics.
- [ ] 4.4 Stale-handle detection (with 3.1).

## 5. Verification

- [x] 5.1 Built `FrameworkTest`, `BulletPhysicsTest`, `SkyRender`, `Audio`, `Navigation`, `EnetNetwork`, `RecastNavigation`, `Terrain`.
- [ ] 5.2 Editor-enabled build (`SKY_BUILD_EDITOR=ON`) — skipped by decision (Qt editor deprecated).
- [ ] 5.3 clang-format/clang-tidy on changed files.
