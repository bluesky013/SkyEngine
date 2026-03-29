## 1. Data Layer Foundation

- [x] 1.1 Define `ClipmapConfig` POD struct in `TerrainBase.h` (blockSize, numLevels, resolution, heightScale, heightOffset, heightFormat) with serialization reflection
- [x] 1.2 Define `LayerInfo` struct (name, albedo Uuid, normal Uuid, roughness Uuid) with serialization reflection
- [x] 1.3 Rewrite `TerrainData` to contain `ClipmapConfig`, material Uuid, tileCountX/Y, heightmapTiles vector, splatmapTiles vector, layers vector; register all in `TerrainComponent::Reflect()`
- [x] 1.4 Remove old data types (`TerrainSectionData`, `TerrainSectionSize`, `TerrainQuad`, `TerrainBuildConfig`) and update any references

## 2. Logic-Render Separation

- [x] 2.1 Strip render includes from `TerrainComponent.h` — remove `#include <terrain/TerrainRender.h>` and all `render/adaptor/assets/*` headers; remove `RDMaterialInstancePtr` and `std::unique_ptr<TerrainRender>` members
- [x] 2.2 Rewrite `TerrainComponent` lifecycle: `OnAttachToWorld()` obtains `TerrainFeatureProcessor` via `GetFeatureProcessor<>()` and calls `SetConfig`/`SetMaterial`/`SetTileData`; `OnDetachFromWorld()` calls FP `Reset()`
- [x] 2.3 Remove `LoadMaterial()`, `OnRebuildTerrain()`, `ResetRender()` from component — all asset loading moves to feature processor
- [x] 2.4 Verify `TerrainComponent.h` compiles without any transitive render/rhi includes

## 3. TerrainFeatureProcessor

- [x] 3.1 Create `TerrainFeatureProcessor.h/.cpp` implementing `IFeatureProcessor` with `Tick(float)` and `Render(rdg::RenderGraph&)`
- [x] 3.2 Implement `SetConfig(ClipmapConfig)`, `SetMaterial(Uuid)`, `SetTileData(...)` — FP internally loads material asset and creates `RDMaterialInstancePtr`
- [x] 3.3 Implement `Reset()` to destroy all rendering state
- [x] 3.4 Register `TerrainFeatureProcessor` via `FeatureProcessorBuilder<TerrainFeatureProcessor>` in `TerrainModule::Init()`
- [x] 3.5 Implement heightmap tile loading in FP: async load via `AssetManager`, create GPU textures, store in tile map

## 4. Clipmap Geometry

- [x] 4.1 Create `TerrainClipmap.h/.cpp` — manages N clipmap levels, each with a list of block positions
- [x] 4.2 Implement shared vertex buffer generation: NxN grid of `TerrainVertex` (uint8 x, y pairs) for one block
- [x] 4.3 Implement shared index buffer generation for one block
- [x] 4.4 Implement clipmap ring block layout: compute which blocks form the ring at each level (exclude inner area covered by finer level)
- [x] 4.5 Implement trim/filler strip geometry between adjacent levels
- [x] 4.6 Implement `UpdateSnapPositions(Vector3 cameraPos)`: snap each level's center to integer multiples of `resolution × 2^level`
- [x] 4.7 Implement per-level instance data generation: for each visible block, compute world offset, level scale, heightmap UV mapping

## 5. ITerrainRenderer and TerrainRenderer

- [x] 5.1 Define `ITerrainRenderer` interface: `Init`, `SetMaterial`, `UpdateClipmap`, `Tick`, `Render`, `Shutdown`
- [x] 5.2 Implement `TerrainRenderer` (CPU-driven): holds `RenderGeometry` (shared VB/IB), per-level instance buffers, per-level `RenderPrimitive` list
- [x] 5.3 Implement CPU per-block frustum culling in `TerrainRenderer::UpdateClipmap()` using block AABB vs camera frustum
- [x] 5.4 Implement instance buffer upload: write visible block instance data to GPU dynamic buffer each frame
- [x] 5.5 Implement `TerrainRenderer::Render()`: for each level, issue `DrawIndexedInstanced` with visible block count
- [x] 5.6 Implement `TerrainRenderer` attach/detach to `RenderScene` (add/remove primitives)
- [x] 5.7 Create `TerrainRenderGPU` stub: implements `ITerrainRenderer`, all methods log warning and return (placeholder for Phase 3)
- [x] 5.8 Implement renderer selection in FP: check `device.features.multiDrawIndirect` and config flag; create `TerrainRenderer` or `TerrainRenderGPU`

## 6. Terrain Shaders and Techniques

- [x] 6.1 Rewrite `terrain.hlsl` vertex shader: sample heightmap atlas, apply heightScale/heightOffset, compute world position from clipmap instance data (offset + level scale)
- [x] 6.2 Implement GPU normal computation in shader: sample 4 neighboring heightmap texels, compute tangent/bitangent, cross product for normal
- [x] 6.3 Implement fragment shader: sample splatmap for layer weights, sample texture arrays (albedo, normal, roughness) per layer, blend by weights
- [x] 6.4 Integrate PBR lighting: sample ShadowMap, IrradianceMap, PrefilteredMap, BRDFLut from pass bindings (space0); compute standard PBR output
- [x] 6.5 Create `terrain_clipmap.tech` technique file (graphics, VS/FS, ForwardColor pass tag, depth test/write enabled)
- [x] 6.6 Create `terrain_depth.tech` (depth-only variant for depth pre-pass)
- [x] 6.7 Create `terrain_shadow.tech` (shadow map variant)
- [x] 6.8 Update `default_terrain.mat` to reference new technique files
- [x] 6.9 Define terrain vertex format in vertex library (`vertex_library.vtxlib`) if not using existing format

## 7. TerrainFeature Resource Management

- [x] 7.1 Refactor `TerrainFeature` descriptor pool to support per-clipmap-level resource groups (heightmap binding per level, splatmap binding, material texture arrays)
- [x] 7.2 Implement resource group allocation for each clipmap level's instance set
- [x] 7.3 Bind heightmap tiles as per-instance textures (or atlas with indirection, preparing for Phase 2)

## 8. Terrain Query API

- [x] 8.1 Implement `QueryHeight(float x, float z)`: locate tile from world coords, bilinear interpolate heightmap texels from in-memory tile data
- [x] 8.2 Implement `QueryNormal(float x, float z)`: finite-difference from 4 neighboring QueryHeight samples
- [x] 8.3 Implement `QuerySplatWeights(float x, float z)`: bilinear sample splatmap tile, return normalized Vector4
- [x] 8.4 Implement `Raycast(Ray, maxDist, &HitResult)`: ray march along heightfield with adaptive step size
- [x] 8.5 Implement `GetHeightmapAtlas()` / `GetSplatmapAtlas()` / `GetClipmapInfo()` stubs (return nullptr / current clipmap info)

## 9. Editor Tool Updates

- [x] 9.1 Update `TerrainBuildTool` widget to expose `ClipmapConfig` fields (blockSize, numLevels, resolution, heightScale, heightOffset, heightFormat) via `TObjectWidget`
- [x] 9.2 Update `TerrainGridTool` to work with tile grid (tileCountX × tileCountY) instead of old section coords
- [x] 9.3 Update `TerrainGenerateTool` to generate tiled heightmaps matching new format (R16_UNORM tiles with configured dimensions)
- [x] 9.4 Update `TerrainHelper` visualization to draw clipmap-aligned grid rather than old section grid
- [x] 9.5 Update `TerrainEditorTool` and `TerrainEditorModule` to use `TerrainFeatureProcessor` instead of direct component manipulation

## 10. Cleanup and Verification

- [x] 10.1 Remove `TerrainQuadTree.h/.cpp` (replaced by clipmap)
- [x] 10.2 Remove `TerrainSector.h/.cpp` and `TerrainSectorRender` (replaced by clipmap blocks)
- [x] 10.3 Remove old `TerrainRender.h/.cpp` (replaced by `ITerrainRenderer` / `TerrainRenderer`)
- [x] 10.4 Remove `TerrainUtils.h/.cpp` if no longer needed
- [x] 10.5 Update `CMakeLists.txt` for terrain plugin: add new source files, remove deleted files
- [x] 10.6 Update `TerrainQuadTreeTest.cpp` or replace with clipmap geometry tests
- [x] 10.7 Build and verify terrain plugin compiles cleanly
- [x] 10.8 Verify terrain renders correctly with PBR lighting and splatmap blending in a test scene
