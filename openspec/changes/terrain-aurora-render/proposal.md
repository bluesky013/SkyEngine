## Why

`terrain-large-world-core` delivered the render-agnostic core (tiled data, LOD chains, streaming, collision, procedural generation, component). Its aurora render layer was split out because it needs the aurora renderer and the GPU-driven pipeline. This change implements that render layer on aurora.

## What Changes

- Create the aurora terrain render adaptor target and module registration.
- Implement the aurora terrain feature processor that pulls core render data (metadata, visible (tile, LOD) set, payloads).
- Move clipmap block mesh, per-level layout, and snap-to-grid camera tracking into the render layer, mapping each ring to a tile LOD.
- Implement per-LOD GPU heightmap/splatmap atlases (upload on tile-LOD load, release/reuse on unload) and instanced clipmap block rendering (sampling the ring's LOD atlas) through the aurora scene/RDG.
- Implement LOD seam stitching (skirt by default, optional geomorphing) for clipmap rings and adjacent tiles at different LOD.
- Bind terrain material and color/depth/shadow techniques; update `assets/shaders|techniques|materials/terrain`.
- Implement the GPU-driven path (resident tile buffer + per-LOD atlas slots from the core residency delta, GPU compute culling/LOD selection, instance compaction, indirect draws) with a CPU-driven fallback.

## Capabilities

### New Capabilities

- `terrain-render`: aurora render layer for terrain (feature processor, atlas, clipmap, stitching, material, GPU-driven path + CPU fallback).

### Modified Capabilities

<!-- none -->

## Impact

- `plugins/terrain` render layer (aurora adaptor + module) and terrain shader/technique/material assets.
- Depends on `engine/terrain` interfaces (`ITerrainSystem`/`ITerrainField`, region sampling, residency delta) and aurora.
- No new third-party dependencies.
