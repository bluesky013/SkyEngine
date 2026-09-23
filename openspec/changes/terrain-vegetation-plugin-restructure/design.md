## Context

The repository is converging on a layering rule: `engine/<feature>` modules expose **interfaces and data** (render-agnostic, no implementation), and the feature itself is implemented as a **plugin** (the module boundary). Physics is the reference: `engine/physics` defines shapes/queries/material/interfaces and `plugins/bullet` implements them (with an aurora bridge submodule in the plugin). Navigation is similar: `engine/navigation` provides core interfaces and `plugins/recast` implements them.

The terrain and vegetation work violated this by putting implementation (field, world sub-system, procedural generation, asset serialization/registration, collision layer, component, offline builder) in `engine/terrain` / `engine/vegetation`. `plugins/terrain` still contains the legacy render-coupled implementation, and no `plugins/vegetation` exists.

Constraints: consumers (`engine/navigation/terrain`, `engine/vegetation/terrain` bridges; terrain components) must not depend on a plugin's concrete implementation; include paths are namespace-style (`terrain/...`, `vegetation/...`) and should stay stable to avoid include churn; the build must return to green after the move.

## Goals / Non-Goals

**Goals:**

- `engine/terrain` and `engine/vegetation` expose interfaces/data only and link `Framework` only.
- Implementation lives in `plugins/terrain` and `plugins/vegetation` (plugins = modules).
- Consumers depend on engine interfaces, resolved at runtime by sub-system name.
- Keep include paths stable; split tests with their implementation/data.

**Non-Goals:**

- Changing the terrain/vegetation behavior, formats, or algorithms (pure move + interface extraction).
- Implementing the aurora render layer / editor here (they remain in the feature plugin, tracked by their own changes).
- A generic dependency-injection framework; resolving by world sub-system name is sufficient.

## Decisions

### D1: engine = interfaces/data; plugin = implementation

**Decision**: An `engine/<feature>` module SHALL contain only interface and data definitions (and pure functions over that data). Feature behavior is implemented in `plugins/<feature>`; consumers depend on the engine interface and resolve the implementation at runtime (world sub-system by name).

**Rationale**: Matches the physics/navigation precedent; lets the engine depend on abstractions and keeps features optional/pluggable. Plugins are modules, so this also matches "module = plugin".

**Alternatives considered**: Keep core logic in engine (the previous approach; inconsistent with physics and makes features non-optional). Rendering-only plugin with logic in engine (leaves logic coupled to the engine build).

### D2: Terrain split

**Decision**: `engine/terrain` keeps only what the engine interfaces reference: `TerrainTypes` (coord/meta/tileinfo/format), `TerrainAddress` (pure conversions), `TerrainRegion` (region sink/change listener interfaces), and `ITerrainField` / `ITerrainSystem`. Everything else — field implementation, `TerrainSystem`, generator, source, asset payload/traits + serialization/registration, LOD description, asset builder, collision layer, component, builder module — moves to `plugins/terrain`.

**Rationale**: Consumers need only the query/system interface and data; the queryable field and the world sub-system are implementation.

**Alternatives considered**: Keep the asset payload implementation in engine (serialization/registration is implementation; the payload struct/traits are data and stay). Keep the component in engine (it wires the sub-system and asset loading, so it is implementation).

### D3: Vegetation split

**Decision**: `engine/vegetation` keeps the surface provider/listener seam, the data types referenced by the system interface, and `IVegetationSystem`; placement, `VegetationSystem`, asset payload/traits + serialization/registration, component, the terrain surface bridge, render, and editor move to a new `plugins/vegetation`.

**Rationale**: Same rule as D1/D2; vegetation is an optional feature.

**Alternatives considered**: Keep vegetation core in engine (inconsistent). Merge vegetation into the terrain plugin (couples two features; separate plugins are cleaner).

### D4: Consumers depend on interfaces

**Decision**: `engine/navigation/terrain` and `engine/vegetation/terrain` bridges and terrain components use `ITerrainSystem`/`ITerrainField` (from `engine/terrain`) and resolve the concrete sub-system from the world by name at runtime.

**Rationale**: Keeps engine bridges free of plugin dependencies; the plugin provides the implementation at runtime.

**Alternatives considered**: Move the bridges into the plugins (spreads integration code and duplicates bridge ownership). Have engine depend on plugin symbols (forbidden).

### D5: Tests move with their subject

**Decision**: Data/interface tests (addressing, LOD description, types) stay in `engine/terrain/test` / `engine/vegetation/test`; implementation tests (field, system, generator, asset, collision, surface bridge) move under the plugins.

**Rationale**: Tests should live with the code they exercise and keep engine test targets interface/data-only.

## Risks / Trade-offs

- [Large mechanical move; temporary build break] -> Land terrain first, then vegetation; keep include paths stable so most includes do not change; fix CMake and target links, then rebuild.
- [Interface breadth] -> Expose exactly what consumers use (`GetMeta`, `GetField`->`ITerrainField&`, region sampling, change listeners, streaming/generation setters); avoid leaking implementation details.
- [Runtime resolution] -> Consumers resolve the sub-system by name; a world without the plugin simply has no terrain system (handled as "unavailable").
- [Two plugins for terrain/vegetation] -> Keep them separate; a shared bridge is not needed because each plugin links only the engine interfaces it uses.
- [Legacy `plugins/terrain`] -> The move replaces the legacy runtime; delete superseded legacy files as part of the move.

## Migration Plan

1. Add `ITerrainField`/`ITerrainSystem` interfaces + keep data in `engine/terrain`; make the existing implementation satisfy them (still in engine) so the build stays green.
2. Move terrain implementation files to `plugins/terrain`; point CMake targets at the new locations; update bridges/consumers to interfaces; move tests; build.
3. Repeat for vegetation (`engine/vegetation` interface/data; `plugins/vegetation` implementation).
4. Rollback: each feature move is independent; reverting the move restores the previous layout.

## Open Questions

- Final names/locations for the interfaces (`ITerrainSystem`, `ITerrainField`, `IVegetationSystem`).
- Whether the terrain asset *payload/serialization* is engine data or plugin implementation (payload/traits stay; serialization/registration move).
- Whether the offline builder module lives in `plugins/terrain` or a tool plugin.
- Whether the nav/vegetation terrain bridges stay in `engine/*` (interface-only) or move to the feature plugin.
