## Context

SkyEngine2's terrain plugin (`plugins/terrain/`) is a partially implemented system. The quadtree LOD infrastructure exists but is never connected to rendering—all sectors render at LOD 0. `TerrainSectorRender::SetMaterial()` is mostly commented out. The shader outputs `float4(0,0,0,0)`. The `TerrainComponent` directly owns `TerrainRender` and GPU resources (`RDMaterialInstancePtr`), bypassing the engine's `IFeatureProcessor` pattern used by `StaticMeshComponent`, `LightComponent`, and `SkyBoxComponent`. Asset loading blocks synchronously via `BlockUntilLoaded()`.

The engine already supports Task/Mesh Shaders with HZB occlusion culling (in `standard_pbr.hlsl`), `DrawIndexedIndirect` across all RHI backends (Vulkan/Metal/DX12/GLES), a render graph with `ComputePass` framework (execution path not yet completed), and a hierarchical depth pyramid (`HizGenerator`). The standard rendering pipeline follows: `Component` → `FeatureProcessor` → `Renderer` → `RenderPrimitive` → `RenderScene`.

Target use case: open-world terrain (tens of km²) with streaming, PBR materials, and future GPU-driven rendering. Data format must support lossless import from UE5 Landscape exports (.r16 heightmap, per-layer .png weightmaps).

## Goals / Non-Goals

**Goals:**
- Replace QuadTree LOD with Geometry Clipmap (constant vertex count, predictable performance)
- Separate logic layer (TerrainComponent) from render layer (TerrainFeatureProcessor) following engine patterns
- Implement working PBR terrain rendering (height displacement, normals, splatmap blending, shadows, IBL)
- Define a tiled data format compatible with UE5 Landscape export dimensions and precision
- Expose terrain query API (CPU + GPU) for downstream systems (vegetation, physics, editor)
- Provide `ITerrainRenderer` abstraction with `TerrainRenderer` (traditional) and `TerrainRenderGPU` (placeholder)
- Configurable parameters: block size (64/128/256), level count (1-12), height format (R16_UNORM/R32_SFLOAT), height scale/offset

**Non-Goals:**
- GPU-driven rendering implementation (`TerrainRenderGPU` is interface-only in this change; requires ComputePass execution to be completed first)
- Tile-based streaming (Phase 2—data format supports it, runtime loads all tiles at init)
- Virtual Texturing for materials
- Terrain sculpting/painting editor tools
- Vegetation system (terrain exposes query API but does not implement consumers)
- Physics collision mesh generation
- Landscape spline / road / river decals
- UE5 import tool implementation (data format is designed for it; the tool is a separate change)

## Decisions

### D1: Geometry Clipmap over QuadTree

**Decision**: Use geometry clipmap (concentric rings centered on camera) instead of completing the existing QuadTree LOD.

**Rationale**: Clipmap provides constant vertex count regardless of world size (~800K vertices for 8 levels × 256²), predictable draw call count (one instanced draw per level), seamless LOD transitions without T-junction stitching, and natural alignment with GPU instancing. QuadTree requires complex split/merge logic, produces irregular draw patterns unfriendly to GPU batching, and needs explicit T-junction repair at LOD boundaries.

**Alternatives considered**:
- Complete existing QuadTree: Higher complexity, irregular GPU workload, T-junction issues
- CDLOD (C. Strugar): Hybrid quadtree-clipmap—more complex than pure clipmap with marginal benefit for our scale

### D2: FeatureProcessor ownership model

**Decision**: `TerrainFeatureProcessor : IFeatureProcessor` owns `ITerrainRenderer` and all GPU resources. `TerrainComponent` holds only `ClipmapConfig` (POD) and `Uuid` references. Component communicates to FP via `SetConfig()`, `SetMaterial(Uuid)`, `SetHeightmapSource(Uuid)`.

**Rationale**: Aligns with `MeshFeatureProcessor`/`MeshRenderer` and `LightFeatureProcessor`/`MainDirectLight` patterns. Enables independent testing of logic layer. Material/texture creation stays in the render layer where it belongs. FP participates in engine Tick/Render lifecycle.

**Alternatives considered**:
- SkyBox pattern (Component owns unique_ptr<Renderer>): Simpler but still leaks render types into component header; doesn't support FP-level Tick/Render orchestration needed for clipmap updates

### D3: ITerrainRenderer with TerrainRenderer / TerrainRenderGPU

**Decision**: Abstract `ITerrainRenderer` interface with two concrete implementations. `TerrainRenderer` (Phase 1): CPU frustum cull + `DrawIndexedInstanced`. `TerrainRenderGPU` (Phase 3): Compute cull + `DrawIndexedIndirect`. Selection at runtime based on device capabilities and config flag.

**Rationale**: Decouples clipmap geometry management from the draw submission strategy. Allows GPU-driven path to be developed independently without touching clipmap or FP code. Fallback path ensures all hardware is supported.

**Alternatives considered**:
- Task/Mesh Shader path: Engine supports it for meshes, but terrain's regular grid structure benefits more from Compute + Indirect pattern; mesh shaders are overkill for uniform topology
- Single renderer with feature flags: Harder to test and maintain; clean separation is worth the interface cost

### D4: Tiled heightmap with height scale/offset

**Decision**: Store heightmaps as tiled `R16_UNORM` (default) or `R32_SFLOAT` textures. World height = `texelValue * heightScale + heightOffset`. Tile sizes aligned to 64/128/256 vertices.

**Rationale**: R16_UNORM maps directly to UE5's uint16 height encoding. The scale/offset parameterization absorbs UE5's `Scale.Z` and the 32768 zero-point convention. Tile size 64/128/256 matches UE5's `ComponentSizeQuads+1` of 64/128/256 exactly. GPU atlas for streaming is a future addition on top of the same tile format.

**Alternatives considered**:
- R32_SFLOAT only: 2× memory/bandwidth for heights; unnecessary for most terrains (UE5 gets ±256m from 16-bit at default scale)
- Raw world-space heights without scale/offset: Incompatible with UE5 import without lossy conversion

### D5: Splatmap format—RGBA8 per 4 layers

**Decision**: Store material layer weights as RGBA8 textures, 4 layers packed per texture (matching UE5 weightmap format). Same tile grid as heightmap. Total layers = `ceil(numLayers / 4)` textures per tile.

**Rationale**: Direct 1:1 mapping from UE5 weightmap export. 8 bits per layer is industry standard. Texture array in shader for clean indexing.

### D6: Clipmap ring geometry—shared mesh + instancing

**Decision**: All clipmap blocks share a single `N×N` vertex/index buffer. Each block is positioned via per-instance data (offset, level scale, heightmap atlas coordinates). One `DrawIndexedInstanced` call per clipmap level renders all visible blocks in that level.

**Rationale**: Minimizes GPU memory (one VB/IB total). Maximizes instancing efficiency. Trim/filler strips between levels use a second shared mesh to eliminate seams.

### D7: Terrain query API on FeatureProcessor

**Decision**: `TerrainFeatureProcessor` exposes:
- CPU queries: `QueryHeight(x,z)→float`, `QueryNormal(x,z)→Vec3`, `QuerySplatWeights(x,z)→Vec4`, `Raycast(Ray, maxDist, &HitResult)→bool`
- GPU resource handles: `GetHeightmapAtlas()→RDTexture2DPtr`, `GetSplatmapAtlas()→RDTexture2DPtr`, `GetClipmapInfo()→ClipmapInfo`

CPU queries read from loaded tile data in memory. GPU handles initially return nullptr (until Phase 2's atlas is built). Dependency direction: consumers → terrain (one-way).

**Rationale**: Vegetation compute shaders need GPU heightmap/splatmap access. Physics/AI/editor need CPU height queries. Placing the API on FP keeps render resources encapsulated.

## Risks / Trade-offs

- **[Clipmap snap artifacts]** → Snap-to-grid quantization can cause subtle vertex swimming at level boundaries. Mitigation: snap offset to exact integer multiples of level resolution; use morphing in vertex shader near level boundaries (can be added incrementally).

- **[No streaming in Phase 1]** → All heightmap/splatmap tiles loaded at init; large terrains may exceed GPU memory. Mitigation: Phase 1 targets moderate terrain sizes (e.g., 8×8 tiles × 128² = 1024² heightmap ≈ 2MB R16). Phase 2 adds tile streaming with LRU cache. Data format already supports streaming without changes.

- **[ComputePass not ready for GPU-driven]** → Engine's `ComputePass` execution path is incomplete. `TerrainRenderGPU` cannot work until this is fixed. Mitigation: `TerrainRenderGPU` is a placeholder with the interface defined. `TerrainRenderer` (traditional) is the functional implementation. GPU-driven path is a separate future change.

- **[Breaking serialization format]** → Existing `TerrainData` scenes will not load. Mitigation: current terrain is non-functional (shader outputs black), so no production data exists to migrate. Document the format change.

- **[Editor tool rewrite scope]** → Build/Grid/Generate tabs need updates for new data model. Mitigation: Reuse existing widget framework (`TObjectWidget`, `ToolWidget`); only data bindings change, not UI structure.

- **[CPU query performance]** → `QueryHeight` reads from tile cache in memory; for high-frequency queries (physics tick at many points), bilinear interpolation across tile boundaries needs care. Mitigation: tile overlap by 1 row/column (standard practice) to simplify boundary sampling.
