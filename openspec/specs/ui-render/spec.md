# ui-render Specification

## Purpose
TBD - created by archiving change engine-ui-system. Update Purpose after archive.
## Requirements
### Requirement: Aurora-backed UI render module
The UI render implementation SHALL live in `engine/ui/render`, SHALL retain the `UIRender` target and `ui/render/...` public include paths, and SHALL use the `sky::aurora` namespace for Aurora types. It SHALL consume `UIDrawData` and `IUITextureRegistry` through `UI.Interface` and SHALL link `UI.Interface`, `Aurora.RHI.Static`, and `Aurora.ShaderCompiler.Static`, without requiring the `UI` widget runtime or `Aurora.Pipeline.Static`. Neither `UI` nor `UI.Interface` SHALL link or include the Aurora UI renderer implementation. This restructuring SHALL preserve the existing Aurora graphics-encoder integration; the direct graphics-encoder integration SHALL remain available independently of frame graph features.

#### Scenario: Render module links Aurora
- **WHEN** the `UIRender` target is built
- **THEN** it SHALL link `UI.Interface` and the Aurora RHI/shader-compiler targets without a `UI` runtime or Aurora pipeline-module dependency

#### Scenario: Core does not depend on render module
- **WHEN** the dependency direction between `UI` and `UIRender` is inspected
- **THEN** `UI` SHALL NOT link or include `UIRender`

#### Scenario: Engine contract excludes the renderer
- **WHEN** `UI.Interface` is configured or its public headers are inspected
- **THEN** it SHALL NOT link Aurora UI implementation targets or expose renderer implementation headers

#### Scenario: Existing render integration remains available
- **WHEN** a caller links `UIRender` and includes `ui/render/UIRenderer.h` after the move
- **THEN** the existing renderer integration SHALL remain available from the adaptor target

#### Scenario: Draw data is independent of widget construction
- **WHEN** a caller supplies `UIDrawData` directly to the renderer without constructing a widget tree
- **THEN** it SHALL be able to link `UIRender` without linking the `UI` widget runtime

### Requirement: UI 通过 Aurora encoder 与宿主图集成

UIRenderer SHALL translate UIDrawData through Aurora GraphicsEncoder and ResourceGroup interfaces. Production host composition MAY provide a registered FrameCustomPassEncoder with declared resources; the UI module SHALL NOT depend on the removed PipelinePass, RenderGraph or DrawItem API. Native Vulkan/DX12/Metal commands SHALL remain behind Aurora backends.

#### Scenario: Direct renderer remains usable
- **WHEN** a host supplies a valid graphics encoder and UIDrawData
- **THEN** UIRenderer records the indexed batches without constructing a legacy graph

#### Scenario: Host graph integration
- **WHEN** a host composes UI with scene output in a FrozenFrameGraph
- **THEN** the host owns graph declarations and frame retirement; UIRenderer does not construct a second graph

### Requirement: Draw-data translation
The render module SHALL translate core `UIDrawData` into GPU work by uploading vertex/index data and issuing
one draw per contiguous batch, switching pipeline/descriptor state when the referenced texture changes.

#### Scenario: Single texture batch
- **WHEN** all draw commands in a frame reference one atlas texture
- **THEN** the renderer SHALL be able to issue a single indexed draw for that batch

#### Scenario: Texture switch splits batches
- **WHEN** consecutive draw commands reference different texture handles
- **THEN** the renderer SHALL issue separate draws with the corresponding texture bound

### Requirement: Clipping
The render module SHALL apply each draw command's clip rect as a scissor rectangle for axis-aligned clips.

#### Scenario: Clip rect applied
- **WHEN** a draw command carries a clip rect smaller than the viewport
- **THEN** the scissor rectangle for that draw SHALL equal the clip rect

### Requirement: Uniform blocks scoped to pass and batch tiers

The current UI renderer SHALL project pixel coordinates on the CPU and bind per-batch textures/samplers in set 3. It SHALL NOT require an unused Global/Pass/Material cbuffer or the removed BatchPackWriter. Future uniform inputs SHALL use the shared declaration and target UniformUploadPlan contract, with Global 0 / Pass 1 / Material 2 / Batch 3 ownership.

#### Scenario: CPU projection
- **WHEN** draw vertices are uploaded for a surface size and backend clip-space convention
- **THEN** positions are transformed consistently without an implicit Global uniform block

#### Scenario: Texture batch
- **WHEN** the texture changes between UI draw commands
- **THEN** the renderer binds the matching Batch set 3 image/sampler after the required pipeline

### Requirement: Textures and samplers via resource groups
The render module SHALL bind UI textures and samplers through Aurora resource groups and descriptor writes,
not through uniform blocks.

#### Scenario: Atlas texture bound
- **WHEN** a draw command references an atlas texture handle
- **THEN** the corresponding Aurora image and sampler SHALL be bound for that draw

### Requirement: Backend-agnostic with Vulkan first

The UI render pass SHALL be written against Aurora interfaces only and SHALL compile its shaders to the target of
the active backend (SPIR-V for Vulkan, DXIL for DX12, MSL for Metal), so it runs on any backend whose Aurora
capabilities have landed.

#### Scenario: Runs on Vulkan
- **WHEN** the engine runs with the Vulkan backend and a UI tree is painted
- **THEN** the UI pass SHALL execute with SPIR-V bytecode and produce visible output

#### Scenario: Runs on DX12
- **WHEN** the engine runs with the DX12 backend and a UI tree is painted
- **THEN** the UI shader SHALL be compiled to DXIL and the UI pipeline state SHALL be created successfully

#### Scenario: Upright on every backend
- **WHEN** the UI is painted on a backend whose clip-space Y axis differs from another's
- **THEN** the UI SHALL be upright (pixel y=0 at the framebuffer top) on both, with the projection's Y sign taken
  from the device's clip-space Y axis

#### Scenario: Unsupported backend degrades gracefully
- **WHEN** the active backend lacks a required Aurora capability
- **THEN** the UI pass SHALL report the limitation without crashing

### Requirement: Frame data lifetime
The render module SHALL double- or triple-buffer per-frame UI GPU data so the render thread can consume one
frame while the main thread produces the next, consistent with `DeviceFrameContext` in-flight frame semantics.

#### Scenario: No CPU/GPU hazard across frames
- **WHEN** the main thread writes the next frame's UI data while the previous frame's GPU work is in flight
- **THEN** the in-flight frame's buffers SHALL NOT be overwritten

### Requirement: Rounded shapes evaluate layout-space distance
The Aurora UI renderer's rounded-shape path SHALL evaluate its signed-distance function using interpolated original layout coordinates carried in the shape quad's UV, together with the authored layout-space center, half-size, and radius, rather than comparing framebuffer position with layout-space shape data. Transformed shape vertices SHALL retain consistent shape-local distance calculations. Ordinary textured quads SHALL retain their existing texture UV semantics. The actual shader source used by runtime compilation SHALL also be consumable by offline compilation validation without a separately copied test shader.

#### Scenario: Transformed rounded shape uses coherent coordinates
- **WHEN** a rounded shape is translated, rotated, or scaled through UI geometry
- **THEN** its shader SHALL evaluate the shape distance using the corresponding interpolated layout position and authored layout-space radius/extent

#### Scenario: Texture UVs remain texture coordinates
- **WHEN** an ordinary textured quad is emitted
- **THEN** its UV values SHALL continue to sample the requested texture region rather than be interpreted as a shape-layout position

#### Scenario: Offline validation compiles runtime source
- **WHEN** the available shader compiler validates the UI shader offline
- **THEN** it SHALL compile the actual source artifact consumed by the runtime renderer, including the rounded-shape entry point
