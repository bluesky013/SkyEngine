> **Status: Frozen (not scheduled).** See the proposal banner.

## Context

`TransformComponent` (`engine/framework/src/world/TransformComponent.cpp`) currently:

- `SetLocal*` sets `local`, calls `UpdateGlobal` (`global = parent.global * local`), then `OnTransformChanged`, which recurses over `children` recomputing each child's global — an eager full-subtree pass per write.
- `SetWorld*` sets `global`, derives `local`, then the same eager recursion.
- `OnTransformChanged` broadcasts `ITransformEvent::OnTransformChanged(global, local)` to subtree listeners.

Consumers in `engine/render/adaptor` (`CameraComponent`, `LightComponent`, `PrefabComponent`, animation components) implement `ITransformEvent` and read either the passed `global` or `GetWorldTransform()`.

`Transform::operator*` (`engine/core/include/core/math/Transform.h`) composes `scale = scale * rhs.scale`, which is not valid for non-uniform scale under rotation.

## Goals / Non-Goals

**Goals:**

- Coalesce multiple transform writes into one world-transform resolution.
- Resolve world transforms lazily when read, or once per frame.
- Define and document the non-uniform-scale policy.

**Non-Goals:**

- Changing the transform hierarchy model (parent/child links, `SetParent*`).
- Changing serialization (`TransformData` still stores `local` + `parent`).
- Adding sockets or a separate scene-graph service.

## Decisions

### D1: Dirty flag with read-time resolution

Each `TransformComponent` holds a dirty flag. Writes mark self + descendants dirty (an O(descendants) flag walk, no matrix math). `GetWorldTransform`/`GetWorldMatrix` call `EnsureGlobalUpdated()`, which resolves the parent chain first (parent must be clean) and recomputes `global = parent.global * local` only when dirty.

Alternative considered: full frame-boundary resolve pass in `World`. Deferred — read-time resolution fixes the repeated-write cost with less structural change; a world pass can be layered later if profiling needs it.

### D2: Event delivery contract

Writes mark dirty and mark descendants dirty, then notify. To keep consumers correct, the event is delivered **after** the emitter's `EnsureGlobalUpdated()` so the passed `global` is current for the emitter. Descendants are notified via the existing recursion, but each descendant resolves on demand; consumers that need a descendant's world transform SHOULD call `GetWorldTransform()` (which ensures) rather than relying on a passed value.

Rationale: preserves the hierarchy notification semantics while making computation lazy. Alternative (defer all notifications to a frame boundary) is more invasive and changes event timing for all consumers; deferred until a world resolve phase is introduced.

### D3: Non-uniform scale policy

Document the limitation: component-wise scale composition is only correct when a rotated node does not inherit non-uniform scale from an ancestor. Two acceptable implementations:

- Restrict + warn: detect a rotated child under a non-uniformly-scaled ancestor and log/warn; behavior falls back to the current component-wise result.
- Correct composition: carry a parent-space matrix chain and compose with `Matrix4` (heavier; changes `Transform` composition).

Leaning restrict + warn for this change; correct composition tracked separately if content needs it.

## Risks / Trade-offs

- [Consumers relying on synchronously-passed descendant globals] → Migrate them to query `GetWorldTransform()` (which ensures) and audit each `ITransformEvent` implementation in `engine/render/adaptor`.
- [Dirty-marking walk is still O(descendants) per write] → It is flag-only (no matrix math); coalescing wins come from avoiding recompute on repeated writes and repeated reads.
- [Scale policy] → Document explicitly; add a test/assert so unsupported combinations do not silently produce wrong matrices.

## Migration Plan

1. Add the dirty flag and `EnsureGlobalUpdated`; make readers resolve.
2. Change writes to mark dirty + notify instead of eager recompute.
3. Migrate `ITransformEvent` consumers to query world transforms.
4. Add tests (lazy resolution, coalescing, scale policy).
5. Verify default build + run framework/render tests.

## Open Questions

- Should the resolve point be purely read-time, or also a `World` pre-render pass? (Leaning read-time now, optional pass later.)
- Is a warning sufficient for the scale case, or should the engine forbid non-uniform scale on rotated hierarchies at edit time?
