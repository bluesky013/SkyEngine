## MODIFIED Requirements

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

### Requirement: Uniform blocks scoped to pass and batch tiers

The current UI renderer SHALL project pixel coordinates on the CPU and bind per-batch textures/samplers in set 3. It SHALL NOT require an unused Global/Pass/Material cbuffer or the removed BatchPackWriter. Future uniform inputs SHALL use the shared declaration and target UniformUploadPlan contract, with Global 0 / Pass 1 / Material 2 / Batch 3 ownership.

#### Scenario: CPU projection
- **WHEN** draw vertices are uploaded for a surface size and backend clip-space convention
- **THEN** positions are transformed consistently without an implicit Global uniform block

#### Scenario: Texture batch
- **WHEN** the texture changes between UI draw commands
- **THEN** the renderer binds the matching Batch set 3 image/sampler after the required pipeline


### Requirement: UI 通过 Aurora encoder 与宿主图集成

UIRenderer SHALL translate UIDrawData through Aurora GraphicsEncoder and ResourceGroup interfaces. Production host composition MAY provide a registered FrameCustomPassEncoder with declared resources; the UI module SHALL NOT depend on the removed PipelinePass, RenderGraph or DrawItem API. Native Vulkan/DX12/Metal commands SHALL remain behind Aurora backends.

#### Scenario: Direct renderer remains usable
- **WHEN** a host supplies a valid graphics encoder and UIDrawData
- **THEN** UIRenderer records the indexed batches without constructing a legacy graph

#### Scenario: Host graph integration
- **WHEN** a host composes UI with scene output in a FrozenFrameGraph
- **THEN** the host owns graph declarations and frame retirement; UIRenderer does not construct a second graph
