## ADDED Requirements

### Requirement: Scene techniques match RenderQueue Tags

ScenePass SHALL declare RenderQueues with accepted technique Tags and select each eligible item's matching material technique before queue sorting/execution. Draws SHALL retain the selected technique's actual program, render state and binding inputs. Selection SHALL NOT use the first material technique unconditionally, silently substitute SceneForward.slang, or replace shadow techniques with a universal shader. Material programs SHALL consume compatible exported Global and target-Pass interfaces while retaining their own shading implementation.

#### Scenario: Different material programs in one queue
- **WHEN** Unlit, PBR and user-defined materials each provide a technique matching an OpaquePass queue Tag
- **THEN** their draws enter that queue with their own programs and data while obeying the common Pass resource/output contract

#### Scenario: Opaque and shadow techniques
- **WHEN** a material provides opaque and shadow Tags with different programs
- **THEN** the corresponding queues select their matching programs, preserving material-specific deformation or alpha testing

#### Scenario: Missing or ambiguous Tag
- **WHEN** no technique matches a queue, or multiple techniques match without an explicit selection policy
- **THEN** the non-matching item is excluded, while ambiguous selection reports an explicit error rather than selecting by array order

#### Scenario: Invalid matched technique
- **WHEN** a matched technique has a missing program or incompatible execution/output interface
- **THEN** validation reports an explicit error rather than choosing another technique or an implicit scene shader

#### Scenario: Unused shared declarations
- **WHEN** a material includes a Pass interface without using some declared lighting resources
- **THEN** binding validates actual reflected use without requiring values or algorithms for unused declarations

### Requirement: Fullscreen and compute select techniques explicitly

FullScreenPass and ComputePass SHALL explicitly select their techniques without requiring a scene, RenderQueue Tag match or dummy material. Program descriptions SHALL resolve explicit stage/entry names; fullscreen execution SHALL validate graphics compatibility and compute execution SHALL validate compute-only compatibility. Dispatch dimensions SHALL remain invocation inputs.

#### Scenario: Fullscreen technique without a scene
- **WHEN** a FullScreenPass selects a graphics technique with custom entry names
- **THEN** it renders using those entries and Pass resources without material queue matching or mandatory Batch data

#### Scenario: Compute technique without a material
- **WHEN** a ComputePass selects a compute technique and supplies dispatch dimensions plus Pass inputs/outputs
- **THEN** it dispatches without a scene material, queue Tag or mandatory Batch while retaining existing compute resource/access validation

#### Scenario: Technique stage mismatch
- **WHEN** a selected technique's stages do not satisfy the Pass execution kind
- **THEN** validation rejects it before GPU execution without inferring replacement entries

### Requirement: View shader data belongs to Pass

Resource tiers SHALL remain Global set 0, Pass set 1 and Batch set 2. Optional View/camera/projection/viewport shader parameters SHALL belong to the consuming Pass invocation, with no separate View tier. Global values SHALL be common across that pipeline's views for the frame. CPU View organization and scheduling SHALL remain independent of this shader binding choice.

#### Scenario: Several views in one frame
- **WHEN** main, secondary and shadow camera invocations coexist
- **THEN** each uses its own immutable Pass values/resource versions without overwriting another invocation or varying a shared Global camera block

#### Scenario: Pass without View inputs
- **WHEN** a fullscreen, compute or scene Pass does not declare active camera inputs
- **THEN** no camera block or dummy View binding is required

#### Scenario: Feature supplies Pass-wide data
- **WHEN** a Feature produces lighting, a shadow map or IBL used across a Pass
- **THEN** the consumer binds it through the Pass contract rather than classifying it as Batch solely because a Feature produced it

### Requirement: Batch composes material and Feature inputs

ScenePass Batch SHALL support material-authored properties/resources and Feature-supplied draw data within set 2. The selected interface SHALL explicitly assign each binding to one provider, or to one assembler for an intentionally combined block. Binding composition SHALL reject conflicting ownership, missing active inputs and incompatible layouts before atomically replacing draw bindings. Generic rendering SHALL NOT require a fixed SceneBatchParams payload or equate material CPU-property offsets with reflected uniform offsets.

#### Scenario: Material and Feature provide different Batch bindings
- **WHEN** a selected technique requires a material parameter/texture binding and Feature transform or instance bindings
- **THEN** both contributions compose into its Batch according to declared slots and validated shader layouts

#### Scenario: Competing providers
- **WHEN** two providers claim the same Batch binding
- **THEN** composition reports a conflict and preserves the prior draw bindings rather than overwriting by call order

#### Scenario: Combined block has one assembler
- **WHEN** a concrete interface places material and Feature fields in one uniform block
- **THEN** one declared assembler packs the fields according to that layout and validates the result without overlapping independent byte writes

#### Scenario: Missing input or layout mismatch
- **WHEN** an active Batch resource lacks its provider or a supplied property has an incompatible type/layout
- **THEN** validation fails explicitly before draw publication or GPU execution

#### Scenario: Optional Batch
- **WHEN** a FullScreenPass or ComputePass technique has no active set-2 resources
- **THEN** execution succeeds using its required Global/Pass resources without allocating dummy Batch bindings

#### Scenario: Explicit technique requires Batch
- **WHEN** a fullscreen or compute technique declares active set-2 resources
- **THEN** its caller must supply compatible Batch values and cannot ignore them based on the Pass kind

### Requirement: Custom pipelines consume foundation binding mechanisms

Aurora SHALL expose generic binding validation through existing foundation interfaces/implementation targets without forcing consumers to link built-in pipeline implementations. Plugin Pass implementations and declarations SHALL remain plugin-owned. Validation SHALL preserve supported backend/resource boundaries, explicit static/dynamic policy, actual reflection checks and atomic binding replacement. PreparedRaster SHALL retain its geometry/resource-version/snapshot role without owning material layouts or queue-selection policy.

#### Scenario: Standalone custom pipeline target
- **WHEN** a user pipeline consumes shared declarations, bindings and FrameGraph feature interfaces
- **THEN** it builds without linking the built-in Pipeline library or shader compiler to obtain C++ types, and no engine target depends on the plugin

#### Scenario: Unsupported binding capability
- **WHEN** a custom interface requests a resource shape unsupported by its execution path
- **THEN** validation returns an explicit unsupported error without treating declaration export as backend support

### Requirement: Host explicitly assembles pipeline owners

The host SHALL assemble selected built-in or user features through existing RegisterRenderFeature/IFrameGraphFeature commands and real module leases. Built-in Scene registration SHALL NOT be a prerequisite or fallback for custom pipelines. Plugins SHALL share the existing producer, Renderer/Device, graph composition and retirement protocols.

#### Scenario: User pipeline replaces built-in scene
- **WHEN** the host selects custom features without registering built-in Scene
- **THEN** those features produce output without constructing built-in Passes or enforcing their layouts

#### Scenario: Owner retires with outstanding work
- **WHEN** an owner retires with accepted compilation, frame or GPU/presentation borrowers
- **THEN** new work stops and existing material/Feature/source/resource/module snapshots remain valid until users complete or drain
