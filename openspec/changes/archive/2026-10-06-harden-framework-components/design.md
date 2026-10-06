## Context

`engine/framework/world` implements the traditional OOP component model:

- `Actor` owns components in `std::unordered_map<Uuid, std::unique_ptr<ComponentBase>>` (`Actor.h:106`).
- `ComponentBase` exposes `Tick` / `OnAttachToWorld` / `OnDetachFromWorld` / `OnSerialized` (`Component.h:18`).
- `ComponentAdaptor<Data>` provides reflected data + `SaveJson`/`LoadJson` for free.
- Render/plugin components override `OnAttachToWorld` / `OnDetachFromWorld` to acquire and release render-side objects (for example `StaticMeshComponent::OnDetachFromWorld` removes its renderer, `StaticMeshComponent.cpp:61`).

Separately, the render side uses a data-oriented ECS (`engine/core/ecs`, `core-ecs` spec) and the aurora scene types (`engine/aurora/core/scene/SceneTypes.h`). The decision for this change is to keep framework OOP and treat the ECS as a render-side concern only.

The review of this layer found lifecycle asymmetry, null dereferences on unregistered component types, and incorrect parent/child transform propagation — all currently masked because the hierarchy test is commented out (`engine/test/framework/ComponentTest.cpp:128`).

## Positioning

Scope for this and the follow-on changes: framework scene/script, `engine/network`, and the new `engine/aurora` render path. The legacy `engine/render` renderer is explicitly out of scope — no compatibility or migration work for it.

The framework component system serves exactly three roles:

- **Scene interface** — `World`/`Actor`/component data + serialization + transform hierarchy; the object model the editor edits.
- **Script interface** — reflection members (`getter`/`setter`) plus lifecycle events, so a script runtime can read/write scene state.
- **Decoupling contract** — the component's data and reflected metadata are the only thing render/network/editor depend on. This is the component system's other major use: it keeps those layers off the concrete scene model.

```
                编辑器 / 脚本 (sandbox editor, python runtime)
                             │ 统一反射元数据(唯一契约)
                             ▼
     ┌────────────────────────────────────────────────────────┐
     │        engine/framework   (场景接口 + 脚本接口 + 契约)   │
     │  World ─ Actor ─ ComponentBase / ComponentAdaptor<Data> │
     └──────┬────────────────────────────────┬─────────────────┘
            │ 只读 POD + Uuid + engine 接口   │ IReplicationSource 契约
            ▼                                ▼
  ┌───────────────────────┐       ┌──────────────────────────────┐
  │  aurora hand-off 适配  │       │  engine/network (复制算法)    │
  │  → aurora scene (ECS)  │       │  不依赖 Framework             │
  │  渲染态归 aurora        │       │  字段增量 / 基线 / 权威        │
  └───────────────────────┘       └──────────────────────────────┘
```

Dependency direction is one-way (`plugin`/`adaptor` → `engine`); `engine/network` and `engine/aurora/core` are peers that consume framework contracts without being depended on by framework.

## Boundary Responsibilities

- Scene data / hierarchy / serialization / lifecycle → framework.
- Script property access and events → framework reflection.
- Unified metadata flags → framework reflection (read by editor/script/network).
- Render state / batch submission / GPU → aurora.
- Replication algorithm / baselines / authority → `engine/network`, via `IReplicationSource`.
- Structural changes (spawn/despawn) → host-owned, applied outside `World::Tick` (no framework API).

## Goals / Non-Goals

**Goals:**

- Symmetric component lifecycle: attach on add, detach on remove/destroy, for actors that are in a world.
- Null-safe component creation/loading for unknown or non-constructible types.
- Correct `TransformComponent` hierarchy: authoritative `local`, derived `global = parent.global * local`, load-time hierarchy resolution that does not corrupt `local`, and parent-change propagation to descendants.
- Deterministic, stable per-actor component ordering for `Tick` and serialization.
- Remove the dead `EntityManager` stub from framework.

**Non-Goals:**

- Migrating framework `Actor`/`ComponentBase` to ECS.
- Changing `engine/core/ecs`, aurora scene ECS, or the render data model.
- Changing the serialized component layout on disk.
- Changing the reflection registration mechanism (`SerializationContext` / `ComponentFactory`).
- Adding the framework→aurora-scene bridge (separate change).

## Decisions

### D1: Framework stays OOP; delete the parallel `EntityManager`

Delete `engine/framework/world/Entity.{h,cpp}` and its include in `World.h`. Rationale: framework is intentionally the traditional component authoring layer, while `engine/core/ecs` is the single ECS (`core-ecs` spec). Keeping a second, non-functional `EntityManager` with a conflicting `EntityId = uint64_t` is a competing abstraction. Alternative (migrate framework to `core/ecs`) is rejected by the stated architecture direction.

### D2: Lifecycle symmetry in `Actor` and `World`

- `EmplaceComponent` sets `component->actor`, and calls `OnAttachToWorld()` **only when the insertion succeeded** (`world != nullptr && res.second`). Previously a failed (duplicate) insert still attached a component that was then destroyed.
- `RemoveComponent(typeId)` calls `OnDetachFromWorld()` before erasing when `world != nullptr`.
- `World::Reset()` detaches every actor before clearing (previously `actors.clear()` destroyed attached components without detach).
- `World::AttachToWorld` returns early when the actor already belongs to this world (previously it appended a duplicate).
- `Actor` does not own world teardown: the world detaches actors in its destructor and in `Reset`. A virtual `OnDestroy` hook from the destructor was rejected — calling virtuals during destruction is fragile, and `OnDetachFromWorld` already means "leave the world".

### D3: `TransformComponent` invariant and derivation

Adopt an explicit source-of-truth rule:

- `SetLocal*` sets `local`, then derives `global = (parent ? parent->global : identity) * local`. `UpdateGlobal` is fixed to drop the erroneous trailing `* data.global` that compounded the previous world transform.
- `SetWorld*` sets `global`, then derives `local = (parent ? parent->global.GetInverse() : identity) * global` (`UpdateLocal` unchanged).
- A private `LinkParent` performs the link change (unlink old, set `data.parent`, link new) and rejects self/duplicate links and cycles (a new parent that is already a descendant).
- Two public reparent operations wrap `LinkParent`:
  - `SetParent(parent)` — reparent preserving **world** (derive `local`); keeps existing runtime/editor behavior.
  - `SetParentPreserveLocal(parent)` — reparent preserving **local** (derive `global`); used by load, where `local` is the serialized authority.
- `Actor::SetParent` uses `SetParent` (world preserving) and reports the previous parent, derived from the transform's current parent before re-linking, to `OnParentChanged`.
- Load path: `Actor::LoadJson` restores `local` + `data.parent`; `World::LoadJson` resolves the parent link with `SetParentPreserveLocal`, so serialized `local` is never divided by the parent transform.

Alternative considered: a single `SetParent` with `UpdateLocal`. Rejected — it cannot serve load (local authoritative) and runtime reparent (world preserving) with one behavior.

The destructor reparents surviving children to the root through `LinkParent(nullptr)` (on a copy of the child list) so children neither keep a dangling parent pointer nor a stale serialized `data.parent`.

### D4: Parent-change propagation

`OnTransformChanged` recomputes each child's `global = this->global * child->local` before recursing into that child, instead of re-broadcasting stale child globals. Cost is O(subtree) per transform mutation, which is acceptable for scene-sized hierarchies and matches the existing recursive broadcast.

### D5: Deterministic component ordering

Store components in `std::map<Uuid, ComponentPtr>` ordered by type id, so `Tick` and `SaveJson` iterate deterministically with no extra state. `GetComponents()` returns the map; editor/guizmo consumers read it through `auto`/structured bindings and are unaffected.

Rationale: the requirement is deterministic, stable order — not a semantic one — and a correct `Uuid::operator<` (D8) is exactly the comparator `std::map` needs. Lookup becomes O(log n) instead of O(1), negligible for per-actor component counts.

Alternatives considered: `unordered_map` plus a sorted order cache (redundant state once we sort by type id anyway — the earlier draft, dropped); an insertion-ordered `std::vector` (changes the container type without giving a more meaningful order than type-id).

### D6: Null-safe type lookup

`Actor::AddComponent(const Uuid&)` and `Actor::LoadJson` check `FindTypeById(...) != nullptr` and `info->newFunc != nullptr` before constructing; otherwise log with the `Actor`/`World` tag and skip. No crash on unknown/abstract component types.

### D7: `SimpleRotateComponent` dependency guard

`Tick` fetches `TransformComponent` and returns early if absent, instead of dereferencing null.

### D8: `Uuid::operator<` is a strict weak ordering (core prerequisite)

Fix `Uuid::operator<` in `engine/core` to a lexicographic comparison: `(word[0] < v.word[0]) || (word[0] == v.word[0] && word[1] < v.word[1])`. The previous componentwise-dominance form was not a strict weak ordering, making `std::set<Uuid>` / `std::map<Uuid, ...>` undefined behavior and making any Uuid sort untrustworthy. Deterministic component ordering (D5) sorts by `Uuid`, so this fix is a prerequisite. The fix lives in `engine/core` (not framework) and is verified by a core test.

### D9: Robustness fixes found during review

- `World::LoadJson` resolves a parent only when the parent transform exists; it no longer asserts/aborts on a malformed hierarchy.
- `Actor::SetParent` reports the real previous parent (was always `nullptr`).
- `TransformComponent::LinkParent` rejects cycles, preventing infinite recursion in `OnTransformChanged`/destruction.
- Tick and serialization iterate a deterministic type-id order; structural mutation during `Tick` remains unsupported. The network side resolves structural changes at a defined post-tick apply point (see `actor-replication-source`), not during iteration.

## Risks / Trade-offs

- [Changing transform derivation affects existing scenes/editor] → Keep `local` as the serialized authority and default reparent semantics; re-enable the hierarchy test and add round-trip tests with a translated parent; verify editor gizmo behavior manually.
- [Deterministic ordering depends on `Uuid::operator<`] → Fixed in core (D8) and covered by a core test; sorting by type id is stable for a fixed component set.
- [`OnDetachFromWorld` on remove may surprise components that only expected world teardown] → Audit render/audio/physics components; all currently implement detach as the inverse of attach, so removal semantics are correct.
- [Recursive propagation cost on deep hierarchies] → O(subtree) per mutation; unchanged from the existing recursive notification pattern, which already visited every child.

## Architecture Invariants

These hold for this change and the follow-on changes:

- Persisted component data SHALL hold only POD, `Uuid`, and engine value types; non-serialized runtime caches MAY hold asset/engine handles but must be reconstructible and non-authoritative; components SHALL NOT own render/GPU state.
- A framework-layer component SHALL include only `framework`, its own module's types, and `core`; an adaptor-layer component MAY include its own adaptor module but not another layer's implementation headers.
- The unified metadata lives once, on the reflected component member node; editor/script/network read the same table.
- Network identity uses a stable type id, never runtime reflection.
- Lifecycle is symmetric: anything entering a world is attached, anything leaving is detached.
- Tick and serialization order are deterministic.
- Structural changes are host-owned and applied outside `World::Tick`; the actor/component containers are not mutated during iteration.

## Migration Plan

- Internal C++ API change only; no serialized format change (`TransformData` still stores `local` + `parent`).
- Land code + re-enabled/extended tests together; re-run `engine/test/framework` and downstream plugin builds.
- Rollback: revert the commit; no persisted data migration required.

## Open Questions

- None blocking. `OnParentChanged` derives the previous parent from the transform's current parent rather than storing an `Actor*`; `Actor::SetParent` keeps its world-preserving behavior, and load uses `SetParentPreserveLocal`.
