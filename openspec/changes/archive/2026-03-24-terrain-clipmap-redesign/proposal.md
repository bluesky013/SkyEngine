## Why

The current terrain plugin is a half-finished implementation with critical gaps: the LOD system (QuadTree) exists but is never connected to rendering, the shader outputs solid black (no lighting/texturing), `TerrainSectorRender::SetMaterial()` is mostly commented out, asset loading blocks synchronously, and the component bypasses the engine's `FeatureProcessor` pattern by directly owning rendering objects. This makes the terrain unusable for any real scenario. The engine targets open-world games (tens of km²), requiring a scalable terrain system with geometry clipmap LOD, streaming, PBR material blending, and a design that supports future GPU-driven rendering. Redesigning now—before other systems (vegetation, physics) build dependencies on the current broken API—avoids compounding technical debt.

## What Changes

- **BREAKING**: Replace the QuadTree-based LOD approach with a Geometry Clipmap system (concentric rings around camera, configurable block size/level count/height precision)
- **BREAKING**: Redesign `TerrainComponent` to hold only POD configuration and `Uuid` asset references—remove all rendering objects (`RDMaterialInstancePtr`, `TerrainRender`) from the component
- **BREAKING**: Redesign `TerrainData` serialization format to support tiled heightmap/splatmap storage with `heightScale`/`heightOffset` parameters (UE5 import-compatible)
- Introduce `TerrainFeatureProcessor` following the engine's standard `IFeatureProcessor` lifecycle (Tick/Render), owning all rendering state
- Introduce `ITerrainRenderer` interface with two implementations: `TerrainRenderer` (CPU-driven, Phase 1) and `TerrainRenderGPU` (Compute Cull + DrawIndexedIndirect, Phase 3 placeholder)
- Implement proper terrain PBR shader: heightmap displacement, GPU normal computation, splatmap-based 4-layer material blending, integration with engine IBL/shadow pipeline
- Add CPU terrain query API (`QueryHeight`, `QueryNormal`, `QuerySplatWeights`, `Raycast`) on `TerrainFeatureProcessor` for vegetation/physics/editor use
- Expose GPU resource handles (`GetHeightmapAtlas`, `GetSplatmapAtlas`, `GetClipmapInfo`) for downstream GPU consumers (vegetation compute shaders)
- Redesign editor tools (Build/Grid/Generate tabs) to work with the new clipmap data model
- Refactor `TerrainFeature` (descriptor pool) to serve the new per-clipmap-level resource binding model

## Capabilities

### New Capabilities
- `terrain-clipmap`: Geometry clipmap LOD system—configurable concentric ring geometry, snap-to-grid camera tracking, trim/filler strips for seamless LOD transitions, shared mesh topology with instancing
- `terrain-rendering`: Terrain rendering pipeline—`TerrainFeatureProcessor` lifecycle, `ITerrainRenderer` abstraction with `TerrainRenderer` (traditional) and `TerrainRenderGPU` (placeholder), depth pre-pass and shadow map participation, PBR shader with heightmap displacement + GPU normals + splatmap blending
- `terrain-data`: Terrain data format and component—`ClipmapConfig` (block size, level count, height format, height scale/offset), tiled heightmap/splatmap storage, layer definitions, UE5-compatible coordinate/precision mapping, `TerrainComponent` as pure logic layer
- `terrain-query`: Terrain query API—CPU height/normal/splatmap queries from tile cache, GPU resource exposure (atlas textures, clipmap info struct) for downstream systems, raycast support

### Modified Capabilities
<!-- No existing specs to modify -->

## Impact

- **plugins/terrain/runtime/**: Complete rewrite of `TerrainComponent`, `TerrainRender`, `TerrainSector`, `TerrainFeature`, `TerrainQuadTree`; new files for `TerrainFeatureProcessor`, `TerrainClipmap`, `ITerrainRenderer`, `TerrainRenderer`, `TerrainRenderGPU`, `TerrainQuery`
- **plugins/terrain/editor/**: Update `TerrainToolWidget`, `TerrainEditorTool`, `TerrainGenerator` to work with new `ClipmapConfig` and `TerrainFeatureProcessor` API
- **assets/shaders/terrain/**: Rewrite `terrain.hlsl` with PBR lighting, splatmap sampling, GPU normal computation; add `terrain_depth.hlsl` and `terrain_shadow.hlsl`
- **assets/techniques/**: New `terrain_clipmap.tech`, `terrain_depth.tech`, `terrain_shadow.tech`; retire `terrain.tech`
- **assets/materials/**: Update `default_terrain.mat` to reference new techniques
- **Downstream systems**: Vegetation, physics, and AI systems gain access to `TerrainFeatureProcessor` query API—single dependency direction (they depend on terrain, terrain does not know about them)
- **Serialization**: `TerrainData` format is breaking—existing terrain scene files will need migration
