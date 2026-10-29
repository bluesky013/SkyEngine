## Why

`RendererRuntime.cpp` obscures the existing ECS preparation pipeline by mixing packet effects, resource synchronization, output snapshots, graph construction and GPU progress in long callback bodies. The runtime must expose its place in the coordinator lifecycle without taking over ECS system execution or weakening frozen-access contracts.

## What Changes

- Keep the existing coordinator callbacks as a short lifecycle outline; extract cohesive packet, resource and GPU stages into private methods in the same implementation file.
- Make the boundary between frozen ECS preparation and the queued, owned prepared render frame explicit.
- Preserve resource subscription updates for changed scenes even when all their viewports are suspended.
- Preserve output acquisition, graph borrower cleanup, submission and retirement ordering; stop attempting to discard the invalid batch ID returned when no upload work is admitted.
- Document the actual ECS call chain and validate existing CPU and native GPU regressions.

## Capabilities

### New Capabilities
- `renderer-runtime-orchestration`: Runtime orchestration around the existing ECS preparation and GPU submission lifecycles.

### Modified Capabilities

None.

## Impact

- Implementation remains in `engine/aurora/core/src/renderer/RendererRuntime.cpp`, owned by `Aurora.Core.Static`.
- No public API, module ownership, target, directory or ECS scheduling changes.
- Existing Party runtime-source, shadow and viewport fixes remain a separate implementation change and review.
