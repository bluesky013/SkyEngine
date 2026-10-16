## MODIFIED Requirements

### Requirement: Aurora-backed UI render module
The UI render implementation SHALL live in `engine/ui/render`, SHALL retain the `UIRender` target and `ui/render/...` public include paths, and SHALL use the `sky::aurora` namespace for Aurora types. It SHALL consume `UIDrawData` and `IUITextureRegistry` through `UI.Interface` and SHALL link `UI.Interface`, `Aurora.RHI.Static`, and `Aurora.ShaderCompiler.Static`, without requiring the `UI` widget runtime or `Aurora.Pipeline.Static`. Neither `UI` nor `UI.Interface` SHALL link or include the Aurora UI renderer implementation. This restructuring SHALL preserve the existing Aurora graphics-encoder integration; implementing a PipelinePass or RDG integration SHALL NOT be required by this change.

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
