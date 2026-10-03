## Why

`engine/framework/world` is the traditional OOP component authoring layer (`Actor` / `ComponentBase` / `ComponentAdaptor`) that plugins and the editor build on. Review found real defects in it: components removed at runtime skip `OnDetachFromWorld` and leak render-side resources, unknown component types crash the loader, and `TransformComponent` hierarchy is corrupted on load and does not propagate world transforms from parent to child. It also still ships a dead `EntityManager` stub that contradicts the decision to keep framework OOP and use pure ECS only on the render side (`engine/core/ecs`).

## What Changes

- Guarantee component lifecycle symmetry: attach only on successful add, detach on remove, and detach all actors on `World::Reset`; guard `World::AttachToWorld` against re-attaching to the same world.
- Make component lookup/creation null-safe: unknown or non-constructible component types no longer dereference null in `Actor::AddComponent(Uuid)` and `Actor::LoadJson`.
- Fix `TransformComponent` hierarchy: derive world transform from `parent * local`, resolve load-time hierarchy without corrupting serialized local data, propagate parent transform changes to children, reject reparent cycles, and clean up child links on destruction.
- Track and report the previous parent in `Actor::SetParent`; resolve a missing parent transform gracefully on load.
- Make per-actor component tick and serialization order deterministic and stable by storing components in a `std::map` ordered by type id (`Actor::GetComponents()` now returns `std::map<Uuid, ComponentPtr>`; consumers use `auto`).
- Fix `Uuid::operator<` to a strict weak ordering in `engine/core` (prerequisite for deterministic ordering and for `std::set`/`std::map` keyed by `Uuid`).
- Remove the dead `engine/framework/world/Entity.{h,cpp}` `EntityManager` stub; framework stays OOP, render keeps using `engine/core/ecs`.
- Refine the component decoupling rule into three parts: persisted data is logic-only; runtime caches are non-authoritative and reconstructible; render state ownership stays render-side.

## Capabilities

### New Capabilities
- `framework-component`: the traditional OOP component system in `engine/framework` — component registration, actor/component lifecycle, transform hierarchy, deterministic ordering, and the boundary that render-side state uses the separate `core/ecs`.

### Modified Capabilities
<!-- None: no existing spec defines framework component behavior. -->

## Impact

- Code: `engine/core/include/core/util/Uuid.h`; `engine/framework/include/framework/world/{Actor.h,TransformComponent.h,World.h}`, `engine/framework/src/world/{Actor.cpp,World.cpp,TransformComponent.cpp,SimpleRotateComponent.cpp}`, `engine/framework/world/Entity.{h,cpp}` (removed).
- Tests: `engine/test/framework/ComponentTest.cpp` (lifecycle, unknown type, reset detach, re-attach guard, transform derivation/propagation/reparent/cycle, hierarchy save-load, deterministic order); `engine/test/core/UtilTest.cpp` (`Uuid` strict ordering).
- `GetComponents()` now returns `std::map<Uuid, ComponentPtr>` (ordered); editor/guizmo consumers use `auto`/structured bindings and are unaffected.
- No change to `engine/core/ecs` or the render ECS model.
