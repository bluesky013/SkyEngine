## Why

Navigation already exports renderer-independent triangles, but has no working editor build action or Aurora consumer. The runtime ignores reflected settings, its factory binds an unused editor event, and concrete implementation lives in the engine interface module. Query, streaming and incremental-build defects also prevent reliable use.

## What Changes

- Keep engine navigation contracts; move the world service, assets and authoring into Recast, and the terrain bridge into its provider plugin. Preserve domain include paths.
- Follow Jolt: linkable implementation owners, a small runtime entry, an independent rendering bridge and an editor module. Runtime and offline builds remain usable without Aurora or Sandbox.
- Register deferred Build Navigation and debug visibility actions through the common editor registry.
- Validate/apply settings and retain implementation code for surviving objects and callbacks.
- Correct path completion, tile origins, incremental replacement, streaming ownership and explicit build/load failures; test real Recast generation and queries.
- Run concurrent readers with independent Detour search contexts and a shared mesh read guard; retain exclusive protection for topology changes. Snapshot asynchronous filters and keep cancellation/result/budget accounting consistent until actual completion.
- Bound navigation asset decoding before allocation, back off failed coordinates without starving healthy work, and unregister the terrain provider before releasing its last world reference.
- Provide default LZ4 through Framework's compression interface instead of loading CompressionModule. Keep built-in support configurable, retain acquired provider/code leases and remove the obsolete plugin/host entries.

## Capabilities

### New Capabilities

- `navigation-aurora-integration`: separate editor authoring and Aurora debug visualization consuming navigation contracts.

### Modified Capabilities

- `navigation-runtime`: settings, ownership and module lifecycle.
- `navigation-recast-backend`: complete paths, build failures and tile replacement.
- `navigation-tile-streaming`: owned layered manifests and coordinate budgets.
- `navigation-async-load`: remove asynchronous copies of already resident manifest bytes.
- `navigation-mesh-builder`: plugin ownership and rejection of failed generation.
- `navigation-async-query`: parallel independent search contexts, frozen submissions and real in-flight cancellation accounting.
- `navigation-mesh-asset`: checked encoded lengths/counts before allocation.
- `asset-cook-config`: Framework-owned default LZ4 service and provider lifetime, without a runtime codec module.

## Impact

Navigation/Recast targets, module manifests, cook-host dependencies, editor extension contracts and terrain consumers change together. No new third-party library or Aurora ownership/loading change is required.

This replaces the former plan to repair `IWorldBuilderGather` timing: the editor no longer consumes that event. Project launchers retain ownership of game worlds and enable the declarative navigation subsystem. This does not add another game-world host. The separate `navigation-terrain-build-tests` backlog is not claimed complete by backend-only tests.
