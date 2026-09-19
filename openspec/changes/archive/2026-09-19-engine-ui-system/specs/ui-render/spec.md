## ADDED Requirements

### Requirement: Aurora-backed UI render module
The UI render module SHALL live in `engine/ui/render`, SHALL use the `sky::aurora` namespace for Aurora
types, and SHALL depend on `UI` plus `Aurora.Pipeline` / `Aurora.RHI` / `Aurora.Shader`.

#### Scenario: Render module links Aurora
- **WHEN** the `UIRender` target is built
- **THEN** it SHALL link the UI core target and Aurora pipeline/RHI targets

#### Scenario: Core does not depend on render module
- **WHEN** the dependency direction between `UI` and `UIRender` is inspected
- **THEN** `UI` SHALL NOT link or include `UIRender`

### Requirement: UI pass builds through the Aurora pipeline
The render module SHALL expose a UI pass derived from `sky::aurora::PipelinePass` and SHALL record its work
through the Aurora render graph, using a raster pass and `DrawItem`s rather than hand-written backend calls.

#### Scenario: Pass records draw items
- **WHEN** the UI pass builds its render graph for a frame with draw commands
- **THEN** it SHALL push a `DrawItem` for each batch into a UI queue

#### Scenario: No direct backend calls
- **WHEN** the UI pass source is inspected
- **THEN** it SHALL NOT call Vulkan, DX12, or Metal APIs directly

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
The UI render module SHALL declare its uniform data through `RgBlockDesc` as the single source of truth,
mapping screen-space/pass data to set 1 and per-draw data to the batch tier (set 2) written via
`BatchPackWriter` and consumed as `DrawItem::batchDynamicOffset`. The UI system SHALL NOT require a Global
(set 0) block; whether the UI pass binds a global resource group is defined by a separate UI pass integration
design.

#### Scenario: UI block set declares no global block
- **WHEN** the UI render module's uniform blocks are inspected
- **THEN** all declared blocks SHALL use set 1 or set 2, and none SHALL use set 0

#### Scenario: Batch data uses dynamic offset
- **WHEN** multiple UI draws are recorded in one frame
- **THEN** per-draw constants SHALL be written into the batch tier and referenced by dynamic offset rather than distinct descriptor sets

#### Scenario: Layout and HLSL stay in sync
- **WHEN** a field is added to a UI uniform block
- **THEN** the RHI resource-group layout and the generated HLSL header SHALL both derive from the same `RgBlockDesc`

### Requirement: Textures and samplers via resource groups
The render module SHALL bind UI textures and samplers through Aurora resource groups and descriptor writes,
not through uniform blocks.

#### Scenario: Atlas texture bound
- **WHEN** a draw command references an atlas texture handle
- **THEN** the corresponding Aurora image and sampler SHALL be bound for that draw

### Requirement: Backend-agnostic with Vulkan first
The UI render pass SHALL be written against Aurora interfaces only and SHALL run on the Vulkan backend first,
with DX12 and Metal enabled as their Aurora capabilities land.

#### Scenario: Runs on Vulkan
- **WHEN** the engine runs with the Vulkan backend and a UI tree is painted
- **THEN** the UI pass SHALL execute and produce visible output

#### Scenario: Unsupported backend degrades gracefully
- **WHEN** the active backend lacks a required Aurora capability
- **THEN** the UI pass SHALL report the limitation without crashing

### Requirement: Frame data lifetime
The render module SHALL double- or triple-buffer per-frame UI GPU data so the render thread can consume one
frame while the main thread produces the next, consistent with `DeviceFrameContext` in-flight frame semantics.

#### Scenario: No CPU/GPU hazard across frames
- **WHEN** the main thread writes the next frame's UI data while the previous frame's GPU work is in flight
- **THEN** the in-flight frame's buffers SHALL NOT be overwritten
