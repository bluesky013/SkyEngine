## 1. Terrain interface extraction

- [x] 1.1 Add `ITerrainField` (query interface: height/normal/splat/raycast, tile heights, loaded count, meta) to `engine/terrain`
- [x] 1.2 Add `ITerrainSystem` (meta/field access, region sampling, change listeners, streaming/generation setters, resident set) to `engine/terrain`
- [x] 1.3 Keep `engine/terrain` to interfaces + interface-referenced data only (`TerrainTypes`, `TerrainAddress`, `TerrainRegion`, `ITerrainField`/`ITerrainSystem`); asset payload/schema, source, and LOD description live in the plugin
- [x] 1.4 Confirm the engine terrain target links `Framework` only and compiles without the implementation

## 2. Move terrain implementation to `plugins/terrain`

- [x] 2.1 Move `TerrainField`, `TerrainSystem` (implementing the interfaces), procedural generator, source reflection, asset serialization/registration, asset builder, and component into `plugins/terrain`
- [x] 2.2 Move the terrain collision layer and the offline builder module into `plugins/terrain`
- [x] 2.3 Replace the legacy `plugins/terrain` runtime; update `plugin.json`/`plugins.json`/CMake targets for the new layout
- [x] 2.4 Keep include paths (`terrain/...`) stable across the move

## 3. Move vegetation implementation to `plugins/vegetation`

- [x] 3.1 Keep `engine/vegetation` to the surface seam + data types + system interface
- [x] 3.2 Create `plugins/vegetation` implementing placement, sub-system, asset serialization/registration, component, terrain surface bridge, render, and editor
- [x] 3.3 Register the plugin (`plugin.json`, `plugins.json`, runtime module configs)

## 4. Consumers, bridges, and CMake

- [x] 4.1 Point `engine/navigation/terrain` and `engine/vegetation/terrain` bridges at `ITerrainSystem`/`ITerrainField` (resolved by world sub-system name)
- [x] 4.2 Update engine/plugin CMake so engine modules link `Framework` (+ other engine interfaces) only, and plugins link the engine modules they implement
- [x] 4.3 Move implementation tests under `plugins/terrain` / `plugins/vegetation`; keep data/interface tests with the engine modules

## 5. Validation

- [x] 5.1 Build the engine and plugins with no unresolved symbols
- [x] 5.2 Run terrain, vegetation, navigation, and physics tests and confirm they pass
- [x] 5.3 Confirm engine modules do not link or include plugin implementations
- [x] 5.4 Update the terrain/vegetation change docs to reference the plugin boundary
