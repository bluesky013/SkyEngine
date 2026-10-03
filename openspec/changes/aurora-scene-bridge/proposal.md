## Why

Framework components are the scene authoring model, while aurora renders from a data-oriented scene ECS. Today the two coexist without a defined transfer path. This is an addition to the existing **aurora adaptor** capability (not a new capability): the adaptor already bridges components/assets; this adds the one-way scene→ECS hand-off.

## What Changes

- Add a `RenderSceneBridge` to the aurora adaptor that subscribes to actor/component lifecycle and transform updates and writes framework component data into the aurora scene ECS pools.
- Keep the transfer strictly one-way: framework → aurora; render state lives only in aurora.
- Keep components render-free at the persistence layer: the bridge reads only `Uuid` + POD and the transform world matrix.
- Scope to the new aurora path; the legacy `engine/render` renderer is not involved.

## Capabilities

### Modified Capabilities
- `aurora-adaptor`: adds one-way scene-to-ECS hand-off (lifecycle-driven create/remove, transform transfer) alongside the existing component/asset bridge.

## Impact

- Code: new bridge in `engine/aurora/adaptor`; subscriptions to `Actor`/`Component` lifecycle and `TransformComponent`; writes to `engine/aurora/core` scene ECS pools.
- Depends on `harden-framework-components` (deterministic order, correct transform propagation).
- No change to framework public component types or aurora RHI.
