## Why

`plugins/terrain` is a half-finished system: `Terrain.Static` links `RenderAdaptor`/`rhi` directly, so terrain logic cannot exist without the render layer; the CPU query API is entirely stubbed (`QueryHeight` returns 0, `Raycast` returns false); tiles are stored as flat `Uuid` lists with no world-space addressing, no streaming and no per-world isolation; the clipmap mixes layout math with GPU buffer creation; the editor "Create Terrain" path is commented out; and `tools/` is dead duplicate code. The engine targets open worlds (tens of km²), and the navigation subsystem already proved the pattern that works: a render-agnostic core (`engine/navigation`) plus a backend/render layer (`plugins/recast`) that owns all GPU concerns. Terrain must follow the same split before vegetation, physics, and AI build on it.

## What Changes

- **BREAKING**: Extract a render-agnostic core into a new `engine/terrain` module (target `Terrain`, links `Framework` only, no `render/` or `rhi/` includes), mirroring `engine/navigation`.
- **BREAKING**: Redesign the terrain data model around a world-space tiled heightfield with explicit tile addressing (tile grid, world↔tile coordinate conversion, tile metadata) instead of flat `Uuid` lists; add a terrain asset type carrying a tile manifest and per-tile multi-level LOD heightmap payloads plus splatmap payloads.
- Implement the CPU sampling/query API for real: `QueryHeight`, `QueryNormal`, `QuerySplatWeights`, `Raycast`, resolved against loaded tile data.
- Add proximity tile streaming/paging around a focus position with load/unload radii (hysteresis), a per-tick apply budget, off-tick async tile prefetch, and per-world isolation — mirroring `navigation-tile-streaming`.
- Move all GPU/rendering concerns into an aurora render layer (`TerrainRender.Aurora` adaptor + module): feature processor owning the GPU heightmap/splatmap atlas, clipmap instanced rendering, material binding, and participation in the aurora scene. Core exposes render-agnostic tile data only. Support an optional **GPU-driven path** (GPU block/tile culling, LOD selection, instance compaction, indirect draws) fed by a core residency delta, with a CPU-driven fallback.
- **BREAKING**: Remove `render/` and `rhi/` dependencies from `TerrainComponent`, the clipmap core, and the terrain asset layer.
- Rebuild editor authoring on the new core/component: wire the disabled "Create Terrain" action, grid add/remove, generator seed/height-format usage, and reserve a seam for future sculpt/paint tools.
- Add a render-free, deterministic procedural generation scheme in `engine/terrain`: a seeded world-space noise stack that produces heights, rule-derived splatmaps, and the per-tile LOD chain, runnable both offline (builder) and on demand (streamed/infinite worlds), with the editor reduced to config/preview/bake.
- Add a `terrain-collision` layer: build one static heightfield collider per streamed terrain tile from core height samples, and add/remove colliders in lockstep with tile streaming.
- Consume the backend-neutral heightfield collision shape from the `physics-backend-abstraction` change so terrain can produce real collision geometry.
- Enable a runtime `PhysicsWorld` attach path and load the physics backend module at game runtime so terrain collision works outside the editor.
- Retire the orphaned `tools/` duplicate generator and the stale `TerrainQuadTreeTest` naming; add core tests for tiling, queries, and streaming.

## Capabilities

### New Capabilities

- `terrain-core`: render-agnostic `engine/terrain` module boundary, terrain system lifecycle (`IWorldSubSystem`), tile addressing/coordinate model, LOD/clipmap description as data, backend/factory seam, and the CPU sampling/query API.
- `terrain-data`: tiled terrain asset format (tile manifest, heightmap/splatmap tile payloads, layer definitions, height scale/offset/format), binary serialization round-trip, source, and the POD `TerrainComponent` data model.
- `terrain-streaming`: focus-driven proximity paging with load/unload hysteresis, per-tick apply budget, off-tick async payload prefetch, pending cancellation, and per-world tile state isolation.
- `terrain-collision`: per-tile static heightfield colliders built from core terrain samples and created/destroyed with tile streaming, plus the runtime physics attach.
- `terrain-generation`: deterministic seeded world-space generation of heights, splatmaps, and per-tile LOD chains, runnable offline (builder) and on demand (runtime streaming), with the algorithm in the render-free core.
- `terrain-render`: split out to the `terrain-aurora-render` change.
- `terrain-editor`: split out to the `terrain-editor-tools` change.
- `terrain-generation`: deterministic seeded world-space generation of heights, splatmaps, and per-tile LOD chains, runnable offline (builder) and on demand (runtime streaming), with the algorithm in the render-free core.
- `terrain-collision`: per-tile static heightfield colliders built from core terrain samples and created/destroyed with tile streaming, plus the runtime physics attach.

### Modified Capabilities

<!-- No terrain capabilities exist under openspec/specs/; the previous terrain specs live only in openspec/changes/archive/2026-03-24-terrain-clipmap-redesign and are superseded. -->

## Impact

- **New**: `engine/terrain` (core module, CMake target `Terrain`, links `Framework`), an in-core procedural generator (seeded noise stack, splatmap rules, LOD-chain downsample), optional offline `engine/terrain/builder` for terrain asset cook, and a terrain collision layer linking `Terrain` + `Physics`.
- **Physics**: depends on the `physics-backend-abstraction` change for the backend-neutral heightfield shape; a runtime `PhysicsWorld` attach is added and the physics backend module is enabled in the game module config.
- **Rewritten**: `plugins/terrain/runtime` → aurora render adaptor + module registration; `plugins/terrain/editor` → tools re-bound to core; `plugins/terrain/tools` retired.
- **Build**: `plugins/terrain/CMakeLists.txt`, `plugin.json`, `plugins/plugins.json`, and runtime module configs (`configs/modules_*.json`) updated for the new target split; legacy `RenderAdaptor` link removed from the core.
- **Assets**: terrain shader/technique/material path updated for the atlas-based tile model under `assets/shaders/terrain/`, `assets/techniques/`, `assets/materials/`.
- **Serialization**: `TerrainData` becomes tile-manifest based — existing terrain scene data is incompatible (current terrain is non-functional, so no production data to migrate).
- **Downstream**: vegetation, physics, and navigation gain a stable, render-free terrain query/streaming dependency direction (consumers → `engine/terrain`); render consumers go through the aurora adaptor.
- **Navigation**: this change provides a terrain core read API (resident LOD0 samples over a world-space bounds) plus a terrain tile-change notification, consumed by the `terrain-navigation-integration` change; navigation builds nav meshes from terrain LOD0 via a geometry provider and never from the physics heightfield.
- **Vegetation**: the same terrain core read API and tile-change notification are consumed by the `vegetation-large-world` terrain surface bridge; the vegetation core itself stays terrain-free through its surface provider seam.
- **Module boundary**: per `terrain-vegetation-plugin-restructure`, `engine/terrain` holds only interfaces/data; the terrain implementation lives in `plugins/terrain`.
- No new third-party dependencies.
