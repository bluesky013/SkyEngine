> **Status: Frozen (not scheduled).** The current eager propagation is correct; making it lazy would change `ITransformEvent` semantics across ~10 render components for marginal gain, while the framework's role is authoring/script. Kept for reference; revisit only if profiling shows transform propagation as a hotspot.

## Why

`TransformComponent` recomputes the entire descendant subtree on every `SetLocal*` call (`TransformComponent.cpp` `OnTransformChanged`), and `Transform::operator*` composes non-uniform scale by component-wise multiplication, which is not a correct affine composition under rotation. Commercial engines defer world-transform computation behind dirty flags (UE `UpdateComponentToWorld`) and define explicit parent-space scale rules. Repeated writes to the same transform currently cost O(subtree) each.

## What Changes

- Add a dirty flag and defer world-transform computation until it is read (`EnsureGlobalUpdated`).
- On a local/world write, mark the transform and its descendants dirty instead of recomputing them eagerly; coalesce multiple writes between reads.
- Keep the transform-change event, but ensure the world transform is up to date when it is delivered, or switch consumers to query `GetWorldTransform()` (which resolves lazily).
- Define the non-uniform-scale policy for parent/child composition (either a documented restriction to uniform scale under rotation, or an explicit parent-space application rule), and document the limitation.
- Scope: framework `TransformComponent` and its event consumers (render adaptor animation/light/camera components).

## Capabilities

### New Capabilities
- `lazy-transform-update`: dirty-flagged, read-time resolution of world transforms with a defined scale-composition policy.

### Modified Capabilities
<!-- aurora-adaptor/render components consume ITransformEvent; their contract may change and is migrated here. -->

## Impact

- Code: `engine/framework/include/framework/world/TransformComponent.h`, `engine/framework/src/world/TransformComponent.cpp`, `engine/core/include/core/math/Transform.h` (scale policy), and `ITransformEvent` consumers in `engine/render/adaptor` (light/camera/animation/prefab/skeletal).
- Tests: `engine/test/framework/ComponentTest.cpp` (lazy resolution, coalescing), `engine/test/core/TransformTest.cpp` (scale composition).
- Depends on `harden-framework-components` (correct eager propagation is the baseline this change makes lazy).
