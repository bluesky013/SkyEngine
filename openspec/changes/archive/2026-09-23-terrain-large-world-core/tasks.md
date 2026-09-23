## 1. Core module scaffold (`engine/terrain`)

- [x] 1.1 Create `engine/terrain/CMakeLists.txt` with target `Terrain` as STATIC linking only `Framework`, plus `engine/terrain/include`/`src` layout
- [x] 1.2 Add the `Terrain` target to the engine build graph and confirm it has no `render/`, `rhi/`, or `RenderAdaptor` dependency
- [x] 1.3 Add a `TerrainTest` test target (googletest) alongside `NavigationTest` conventions
- [x] 1.4 Verify the core target compiles and links standalone without the aurora/legacy render layers

## 2. Data model and tile addressing

- [x] 2.1 Define `TerrainTileCoord`, `TerrainHeightFormat`, and `TerrainMeta` in `engine/terrain`
- [x] 2.2 Define `TerrainTileInfo` (coordinate, world bounds, min/max height, LOD availability) and the tile manifest container
- [x] 2.3 Implement `WorldToTile`, `TileToWorld`, and world-to-local-texel conversion with deterministic round-trip
- [x] 2.4 Define the clipmap/LOD level description as plain data with no GPU types
- [x] 2.5 Add unit tests for addressing round-trip and LOD description
- [x] 2.6 Add per-LOD vertex-size/sample-count helpers to `TerrainMeta` and cover them with tests

## 3. Terrain asset format and procedural generation

- [x] 3.1 Define the terrain asset payload: metadata + tile manifest + per-tile multi-level LOD height payloads in the declared height format
- [x] 3.2 Add splatmap tile payloads grouped four layers per RGBA tile on the near/high-detail LODs
- [x] 3.3 Implement binary save/load for the terrain asset (LOD-aware) and register the asset type/handler
- [x] 3.4 Add a serialization round-trip test covering metadata, manifest, and per-LOD tile payloads
- [x] 3.5 Record per-tile LOD availability in the manifest and expose addressable access by (coordinate, LOD)
- [x] 3.6 Define the terrain source data type (scene-independent tile configuration, including LOD count) with reflection
- [x] 3.7 Add an offline terrain asset builder producing a per-tile LOD chain from source
- [x] 3.8 Define a reflected `TerrainGenerateConfig`
- [x] 3.9 Implement deterministic, world-space layered-noise height generation
- [x] 3.10 Implement rule-derived splatmap generation from slope and height
- [x] 3.11 Implement per-tile LOD-chain downsampling (average height, renormalize splat) from LOD0
- [x] 3.12 Implement an off-tick tile-generation task producing a tile + LOD payload
- [x] 3.13 Add the on-demand generation path so streaming generates a tile LOD that is missing from the cooked asset
- [x] 3.14 Add generation tests for determinism, order independence, seamless borders, splat normalization, and LOD-chain consistency

## 4. Runtime field and CPU query API

- [x] 4.1 Implement the render-agnostic runtime field that stores highest-detail (LOD0) tile height/splat samples keyed by tile coordinate
- [x] 4.2 Implement `QueryHeight` with bilinear interpolation and height scale/offset; return a "no data" signal for unloaded tiles
- [x] 4.3 Implement `QueryNormal` via finite differences from neighboring height samples
- [x] 4.4 Implement `QuerySplatWeights` by bilinear sampling the splatmap tile
- [x] 4.5 Implement `Raycast` marching loaded tiles with bounds early-out
- [x] 4.6 Adopt a one-texel tile overlap convention and cover border sampling in tests
- [x] 4.7 Add unit tests for height/normal/splat queries and raycast hit/miss/unloaded cases

## 5. Terrain sub-system and streaming

- [x] 5.1 Implement `TerrainSystem` as `IWorldSubSystem` owning the field and manifest per world
- [x] 5.2 Implement focus, per-LOD load/unload radii (annuli), a per-tick budget, and paging state keyed by (tile coordinate, LOD)
- [x] 5.3 Implement `UpdateStreaming`: per-LOD annulus unload pass, in-annulus load scan, and main-thread apply within the per-tick budget
- [x] 5.4 Implement the off-tick async tile-LOD prefetch task and pending-load cancellation
- [x] 5.5 Implement per-LOD hysteresis and tolerate missing/stale payloads
- [x] 5.6 Implement invalidation of the loaded set when the terrain asset/manifest changes
- [x] 5.7 Expose loaded (coordinate, LOD) entries and their payloads to render and non-render consumers
- [x] 5.8 Add tests for per-LOD paging, hysteresis, budget application, cancellation, LOD switching, and per-world isolation
- [x] 5.9 Derive the per-LOD annulus radii and the level -> tile LOD mapping and cover them with tests
- [x] 5.10 Expose a residency change delta (added/removed (coordinate, LOD) entries) for incremental GPU updates
- [x] 5.11 Record hole/no-data tiles in the manifest and never load them in streaming

## 6. Terrain collision layer

Prerequisite: the backend-neutral heightfield shape from the `physics-backend-abstraction` change.

- [x] 6.1 Create a terrain collision layer linking `Terrain` + `Physics` that consumes core tile data
- [x] 6.2 Build a static collision object per loaded tile from highest-detail (LOD0) height samples
- [x] 6.3 Create and destroy colliders in lockstep with `TerrainSystem` LOD0 tile add/remove, per world
- [x] 6.4 Confirm collision uses highest-detail (LOD0) data and never the render clipmap LOD
- [x] 6.5 Add a runtime `PhysicsWorld` attach path and enable the physics backend module in the game module config
- [x] 6.6 Add tests for per-tile collider construction, removal on unload, and per-world isolation

## 7. Component and module wiring

- [x] 7.1 Rewrite `TerrainComponent` to hold only plain data (metadata, terrain asset/source id, material id, layers, streaming params)
- [x] 7.2 Update component reflection/serialization and register it in the component factory
- [x] 7.3 Wire `OnAttachToWorld`/`OnDetachFromWorld` to push/release state through `TerrainSystem`
- [x] 7.4 Update `plugin.json`, `plugins/plugins.json`, and plugin CMake for the core/render/collision target split
- [x] 7.5 Add terrain entries to the runtime module configs and confirm the module loads in game/editor startup

## 8. Cleanup and validation

- [x] 8.1 Delete the orphaned `plugins/terrain/tools` target and sources
- [x] 8.2 Replace the stale `TerrainQuadTreeTest` with core-focused tests and remove the quadtree naming
- [x] 8.3 Remove `render/`/`rhi/` includes and `RenderAdaptor` links from all terrain core files
- [x] 8.4 Run the terrain and navigation-style tests and confirm they pass
- [x] 8.5 Build the engine (desktop) and confirm no terrain compile/link regressions
- [x] 8.6 Update README/skill references that mention the terrain plugin layout if affected

> The aurora render layer was split into `terrain-aurora-render`; editor tooling (aurora sandbox, non-Qt) into `terrain-editor-tools`.
