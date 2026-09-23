## Why

The engine modules are being made consistent with one rule: `engine/` holds **interfaces and data only**; features are implemented as **plugins** (a plugin is the module boundary). Physics already follows this (`engine/physics` is interface/data; `plugins/bullet` implements it). The terrain and vegetation work, however, placed the **implementation** (field, sub-system, procedural generation, asset serialization, collision layer, component, builder) inside `engine/terrain` and `engine/vegetation`, and left the `plugins/terrain` implementation as legacy. This is inconsistent: engine modules should not carry feature implementations, and optional features (terrain, vegetation) should be plugins so the engine only depends on their abstractions.

## What Changes

- Extract engine-side **interfaces and data** for terrain: keep `TerrainMeta`/`TerrainTileCoord`/`TerrainTileInfo`/`TerrainHeightFormat`, addressing and LOD-description data, the region sink/listener interfaces, and the terrain asset payload/traits; add `ITerrainField` and `ITerrainSystem` interfaces.
- Move the terrain **implementation** (terrain field, `TerrainSystem`, procedural generator, source reflection, asset serialization/registration, asset builder, collision layer, component, builder module, render, editor) out of `engine/terrain` into `plugins/terrain`.
- Extract engine-side **interfaces and data** for vegetation: keep the surface-provider/listener seam and vegetation data types; add `IVegetationSystem` if needed. Component/asset/placement/system move to a new `plugins/vegetation`.
- Update the terrain bridges (`engine/navigation/terrain`, `engine/vegetation/terrain`) and consumers to depend on the engine **interfaces** (`ITerrainSystem`/`ITerrainField`), never on the plugin implementation.
- Split tests: data/interface tests stay with the engine module; implementation tests move with the plugin.
- Keep include paths (`terrain/...`, `vegetation/...`) stable so file moves do not churn includes.

## Capabilities

### New Capabilities

- `engine-plugin-layering`: the layering contract that `engine/` modules expose interfaces/data only, features are implemented as plugins (modules), and consumers depend on interfaces.

### Modified Capabilities

<!-- No existing main specs cover this boundary. -->

## Impact

- **`engine/terrain`**: shrinks to interfaces/data (types, addressing, LOD description, region sinks, asset payload/traits, `ITerrainField`/`ITerrainSystem`); links `Framework` only.
- **`plugins/terrain`**: gains the terrain implementation (runtime field/system/generator/asset/builder/collision/component) plus render/editor; legacy runtime replaced.
- **`engine/vegetation`**: shrinks to the surface seam + data types + `IVegetationSystem`; links `Framework` only.
- **`plugins/vegetation`**: new plugin implementing the vegetation feature (placement, system, asset, component, render, editor, terrain surface bridge).
- **Bridges/consumers**: navigation/vegetation terrain bridges and components resolve `ITerrainSystem` from the world sub-system by name and depend on engine interfaces only.
- No new third-party dependencies. Build graph temporarily changes while the move lands.
