## 1. Remove the framework ECS stub

- [x] 1.1 Delete `engine/framework/include/framework/world/Entity.h` and `engine/framework/src/world/Entity.cpp` (`EntityManager`, `EntityValues`, framework `EntityId`).
- [x] 1.2 Remove the `#include <framework/world/Entity.h>` from `engine/framework/include/framework/world/World.h`.
- [x] 1.3 Confirm no other translation unit references `framework/world/Entity.h` and that the framework target still builds.

## 2. Actor/World lifecycle and null-safety

- [x] 2.1 `RemoveComponent(typeId)` calls `OnDetachFromWorld()` before erasing when `world != nullptr`.
- [x] 2.2 Guard `AddComponent(const Uuid&)` against a null `FindTypeById` result and a null `info->newFunc`.
- [x] 2.3 Guard `LoadJson` component construction against a null type node and null `newFunc` (and reuse the looked-up node).
- [x] 2.4 `EmplaceComponent` calls `OnAttachToWorld()` only when the insertion succeeded.
- [x] 2.5 `World::Reset()` detaches every actor before clearing.
- [x] 2.6 `World::AttachToWorld` returns early when the actor already belongs to this world.

## 3. Transform hierarchy correctness

- [x] 3.1 Fix `UpdateGlobal` to derive `global = (parent ? parent->global : identity) * local`.
- [x] 3.2 Add `LinkParent`; `SetParent` preserves world, `SetParentPreserveLocal` preserves local.
- [x] 3.3 `OnTransformChanged` recomputes each child's `global` before recursing.
- [x] 3.4 `World::LoadJson` links the parent with `SetParentPreserveLocal` and skips gracefully when the parent transform is missing.
- [x] 3.5 `Actor::SetParent` reports the real previous parent to `OnParentChanged`.
- [x] 3.6 `LinkParent` rejects self/duplicate links and cycles.
- [x] 3.7 Destructor reparents surviving children to the root via `LinkParent(nullptr)` on a copy of the child list.
- [x] 3.8 `OnSerialized` derives `global` from `local` (parent resolved afterward on load).

## 4. Deterministic ordering

- [x] 4.1 Store components in `std::map<Uuid, ComponentPtr>` ordered by type id; `GetComponents()` returns it.
- [x] 4.2 `Actor::Tick` and `Actor::SaveJson` iterate the deterministic order.
- [x] 4.3 Fix `Uuid::operator<` to a strict weak ordering in `engine/core` (prerequisite).

## 5. Component dependency guard

- [x] 5.1 `SimpleRotateComponent::Tick` returns early when the actor has no `TransformComponent`.

## 6. Tests

- [x] 6.1 `RemoveComponentDetaches` (detach hook runs on remove).
- [x] 6.2 `WorldResetDetaches` and `AttachToSameWorldDoesNotDuplicate`.
- [x] 6.3 `UnknownComponentTypeIsSafe` (no crash on unregistered type id).
- [x] 6.4 `LocalSetDoesNotCompoundGlobal` (world transform derivation).
- [x] 6.5 `ParentMovePropagatesToChild` (child update on parent change).
- [x] 6.6 `ReparentPreservesWorld` (keep-world reparent).
- [x] 6.7 `CycleIsRejected`.
- [x] 6.8 `SaveLoadPreservesHierarchy` (translated parent round-trip).
- [x] 6.9 `DeterministicSerializationOrder` (stable serialized output).
- [x] 6.10 Core `UtilTest.UuidStrictOrdering`.

## 7. Build and verification

- [x] 7.1 Build `Framework` and `FrameworkTest` (also `CoreTest`) in Debug.
- [x] 7.2 Run `ComponentTest.*` (12/12) and `UtilTest.*` (7/7); full `CoreTest` (269/269). The pre-existing `AssetManagerTest.BuilderTest` failure is unrelated (reproduces with the original `Uuid.h`).
- [ ] 7.3 Run clang-format/clang-tidy on changed files.
