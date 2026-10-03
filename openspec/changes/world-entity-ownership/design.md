## Context

Current ownership (after `harden-framework-components`):

- `World` stores `std::vector<ActorPtr> actors` where `ActorPtr = std::shared_ptr<Actor>`, plus `std::unordered_map<Uuid, size_t> actorIndex` for O(1) lookup.
- `Actor` stores a raw `World* world`; `TransformComponent` stores raw `parent`/`children`.
- Public API (`CreateActor`, `GetActorByUuid`, `GetActors`) exposes `ActorPtr`.
- Consumers: `engine/editor` (Qt `WorldTreeView`, `InspectorWidget`), `engine/render/editor`, `engine/sandbox` tests, `plugins/bullet` tests, `plugins/guizmo`.

Sharing ownership through `shared_ptr` lets an `Actor` outlive its logical world membership, and raw cross-references can dangle on destruction.

## Goals / Non-Goals

**Goals:**

- Single, exclusive ownership: the world owns actors.
- Non-owning references for cross-object links and consumers.
- Preserve O(1) lookup/removal and dense iteration.

**Non-Goals:**

- Changing scene semantics, serialization, or component behavior.
- Adding prefab/instancing or networking identity (mapped separately).
- Changing `engine/core/ecs`.

## Decisions

### D1: World owns actors with `std::unique_ptr<Actor>`

`std::vector<std::unique_ptr<Actor>> actors` plus `std::unordered_map<Uuid, size_t> actorIndex`. Rationale: removes shared ownership and the aliasing escape; keeps the dense array. `GetActors()` returns `const std::vector<std::unique_ptr<Actor>>&`.

### D2: Non-owning `Actor*` (generation handles not planned)

The public API returns a non-owning `Actor*`, and `DetachFromWorld` returns `std::unique_ptr<Actor>` so a caller can retain or move the actor (this is what makes detach→reattach safe without shared ownership — the pattern used by `PhysicsComponentTest.AttachDetachAndReattach`).

Generation-safe handles (`index` + `generation`, resolved through the world) are **not planned**: the immediate structural flaw is shared ownership (fixed here). Revisit only if dangling references become a real problem; the design uses non-owning `Actor*`.

Alternative: introduce handles in this change. Deferred to keep the change contained and verifiable.

### D3: Remove `ActorPtr`/`ActorWeakPtr`

Delete the `shared_ptr`/`weak_ptr` aliases and migrate signatures:
- `CreateActor(...) -> Actor*`
- `GetActorByUuid(...) -> Actor*`
- `AttachToWorld(std::unique_ptr<Actor>) -> Actor*`; `DetachFromWorld(Actor*) -> std::unique_ptr<Actor>`
- `IWorldEvent::OnActorAttached/Detached(Actor*)`
- `Actor::SetParent(Actor*)`; `TransformComponent` keeps raw parent pointers internally (same-world, non-owning, cleared on destroy).
- `GetActors()` returns `const std::vector<std::unique_ptr<Actor>>&`.

### D4: Migration of consumers

- The Qt editor is not migrated (deprecated); its `WorldActorItem` still uses the old `ActorPtr` and is left as-is.
- Plugins/tests that iterate `GetActors()` use `actor.get()` for the pointer.
- The Qt editor is not in the default build config; verify with an editor-enabled build before landing.

## Risks / Trade-offs

- [Raw `Actor*` references can dangle if an actor is destroyed while a caller holds one] → The world owns actors and clears cross-links on destruction; callers re-resolve from the world. Accepted for now (generation handles are not planned).
- [Qt editor is not built in the default config] → It is being deprecated and was intentionally not migrated; it will not compile against the new API. Accepted.

## Migration Plan

1. `World` stores `std::vector<std::unique_ptr<Actor>>` + `Uuid -> slot` index.
2. `CreateActor`/`GetActorByUuid` return non-owning `Actor*`; `GetActors()` returns the unique_ptr vector.
3. `AttachToWorld(std::unique_ptr<Actor>) -> Actor*` (ownership in); `DetachFromWorld(Actor*) -> std::unique_ptr<Actor>` (ownership out).
4. Remove `ActorPtr`/`ActorWeakPtr`; update `IWorldEvent`/`Actor::SetParent` to `Actor*`.
5. Migrate non-Qt consumers (framework tests, bullet test); Qt `engine/editor` intentionally skipped.
6. Verify: default build (framework/plugins).

## Open Questions

- None. The end state uses non-owning `Actor*`; generation handles are not planned.
