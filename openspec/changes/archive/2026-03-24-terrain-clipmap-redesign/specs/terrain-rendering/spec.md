## ADDED Requirements

### Requirement: TerrainFeatureProcessor lifecycle
The system SHALL provide a `TerrainFeatureProcessor` that implements `IFeatureProcessor` and is registered with `RenderScene`. The FP SHALL own all terrain rendering state and participate in the engine's Tick/Render lifecycle.

#### Scenario: Feature processor registration
- **WHEN** the terrain module initializes
- **THEN** `TerrainFeatureProcessor` is registered as a feature processor builder, and any `RenderScene` can instantiate it

#### Scenario: Tick updates clipmap
- **WHEN** `TerrainFeatureProcessor::Tick(dt)` is called each frame
- **THEN** the FP reads the active camera position from the scene view and updates clipmap level snap offsets

#### Scenario: Render submits terrain draw calls
- **WHEN** `TerrainFeatureProcessor::Render(rdg)` is called
- **THEN** the FP delegates to the active `ITerrainRenderer` to submit draw calls to the render graph

### Requirement: ITerrainRenderer abstraction
The system SHALL define an `ITerrainRenderer` interface with methods: `Init(RenderScene*, ClipmapConfig)`, `SetMaterial(RDMaterialInstancePtr)`, `UpdateClipmap(Vector3 cameraPos)`, `Tick(float dt)`, `Render(rdg::RenderGraph&)`, and `Shutdown()`. Two implementations SHALL exist: `TerrainRenderer` and `TerrainRenderGPU`.

#### Scenario: Traditional renderer selection
- **WHEN** `TerrainRenderGPU` is not available (device lacks multiDrawIndirect or config disables GPU-driven)
- **THEN** `TerrainFeatureProcessor` creates a `TerrainRenderer` instance

#### Scenario: GPU-driven renderer selection
- **WHEN** device supports `multiDrawIndirect` and config enables GPU-driven rendering
- **THEN** `TerrainFeatureProcessor` creates a `TerrainRenderGPU` instance

### Requirement: TerrainRenderer CPU-driven path
`TerrainRenderer` SHALL perform per-block CPU frustum culling, build a visible block list, upload instance data to GPU, and issue one `DrawIndexedInstanced` call per clipmap level.

#### Scenario: Blocks outside frustum are culled
- **WHEN** a clipmap block's AABB does not intersect the camera frustum
- **THEN** that block is excluded from the instance buffer and not rendered

#### Scenario: Visible blocks rendered with instancing
- **WHEN** 5 of 12 blocks in a level pass frustum testing
- **THEN** the system uploads 5 instance entries and issues `DrawIndexedInstanced` with instanceCount=5

### Requirement: TerrainRenderGPU placeholder
`TerrainRenderGPU` SHALL implement the `ITerrainRenderer` interface. In this phase, all methods SHALL be no-ops or log a warning that GPU-driven terrain requires ComputePass support.

#### Scenario: GPU renderer instantiated without compute support
- **WHEN** `TerrainRenderGPU::Init()` is called but ComputePass execution is not available
- **THEN** the renderer logs a warning and all rendering methods return without submitting draw calls

### Requirement: PBR terrain shader
The terrain fragment shader SHALL sample a splatmap to determine per-texel material layer weights, blend up to 4 albedo/normal/roughness layers from a texture array, compute world-space normals from the heightmap using finite differences, and evaluate PBR lighting using the engine's existing shadow map, IBL irradiance, IBL prefiltered, and BRDF LUT resources.

#### Scenario: Terrain receives shadows and IBL
- **WHEN** a terrain surface is rendered in the ForwardColor pass
- **THEN** the fragment shader samples `ShadowMap`, `IrradianceMap`, `PrefilteredMap`, and `BRDFLut` from pass-level bindings (space0) and applies standard PBR shading

#### Scenario: Splatmap 4-layer blending
- **WHEN** a splatmap texel has weights (R=0.5, G=0.3, B=0.2, A=0.0)
- **THEN** layer 0 (albedo/normal/roughness) contributes 50%, layer 1 contributes 30%, layer 2 contributes 20%, layer 3 contributes 0%

### Requirement: GPU normal computation
The vertex or fragment shader SHALL compute terrain normals by sampling adjacent heightmap texels and computing the cross product of finite-difference tangent vectors, rather than relying on precomputed normal maps.

#### Scenario: Normal computed from heightmap
- **WHEN** the shader evaluates a terrain surface point
- **THEN** it samples heightmap at (u±texelSize, v) and (u, v±texelSize), computes tangent and bitangent vectors from height differences, and derives the normal via cross product

### Requirement: Depth pre-pass and shadow map participation
The terrain system SHALL provide technique variants for depth-only rendering (depth pre-pass) and shadow map rendering, so terrain contributes to the HZB pyramid and casts shadows.

#### Scenario: Terrain in depth pre-pass
- **WHEN** the depth pre-pass executes
- **THEN** terrain geometry is rendered with a depth-only shader, contributing to the depth buffer used by `HizGenerator`

#### Scenario: Terrain casts shadows
- **WHEN** the shadow map pass executes
- **THEN** terrain geometry is rendered into the shadow map from the light's perspective

### Requirement: Logic-render layer separation
`TerrainComponent` SHALL NOT include any render-layer headers (`TerrainRender.h`, `render/adaptor/assets/*`). It SHALL NOT hold `RDMaterialInstancePtr`, `RDTexture2DPtr`, or any `rhi::*` types. All communication from component to feature processor SHALL use POD structures and `Uuid` asset references.

#### Scenario: Component header has no render includes
- **WHEN** `TerrainComponent.h` is compiled
- **THEN** it does not transitively include any header from `render/` or `rhi/` namespaces

#### Scenario: Component notifies FP via POD
- **WHEN** the user changes terrain configuration in the editor
- **THEN** `TerrainComponent` calls `TerrainFeatureProcessor::SetConfig(ClipmapConfig)` passing a POD struct, and the FP internally handles material/texture creation
