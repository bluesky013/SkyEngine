## ADDED Requirements

### Requirement: Registered asset generators in editable mesh documents
The editor SHALL provide a cross-DLL generator registry with stable IDs, input capabilities, reflected settings and owned cancellable requests. Mesh documents SHALL discover generators through this contract without including concrete generator implementations.

#### Scenario: Plugin registers a deformation binding generator
- **WHEN** the editor plugin registers a compatible generator
- **THEN** the Mesh document exposes it through standard editor actions and reflected settings without a generator-specific host branch

#### Scenario: Preview a high-resolution source mesh
- **WHEN** a detailed source mesh is opened with wire or weight overlays
- **THEN** the preview displays the complete visible surface within the UI snapshot budget, resolves occlusion across primitives and interpolates the selected bone's weights across its triangles
- **AND** generation and saved bindings retain the complete source geometry

#### Scenario: Inspect the material and then a bone weight
- **WHEN** an artist opens a cooked mesh with a saved binding
- **THEN** Aurora shows the mesh with its material slots, control points and bones, and the editor remains interactive while shaders are prepared
- **WHEN** the artist enables or disables bone weights
- **THEN** the preview keeps its camera and ground, changing only the representation of the subject

#### Scenario: Reuse the asset preview stage
- **WHEN** a mesh or character is previewed using the shared session
- **THEN** the default three-quarter view frames the subject, the preview ground sits beneath it, and orbit, pan, zoom and frame use the same controls
- **AND** the subject bounds used for framing exclude the ground, and preview objects do not become actors in the supplied World
- **AND** Alt with left, middle or right drag orbits, pans or dollies around the preview subject, the wheel zooms and F frames the subject
- **AND** resizing maintains the horizontal field of view and plain left drag remains available for control editing

### Requirement: Constrained binding generation is one undoable edit
The Mesh document SHALL preserve authored control IDs and constraint modes, generate from an immutable source revision, and apply a validated result as one coherent undoable edit. Failed, cancelled or stale requests SHALL preserve the prior valid binding.

#### Scenario: Source changes while generating
- **WHEN** generation completes after its source or control-point revision changed
- **THEN** the document rejects the stale result and keeps its current state

#### Scenario: Undo generated binding
- **WHEN** the user undoes an applied generation result
- **THEN** the previous complete cage, skeleton, skin and control settings are restored together

### Requirement: Configurable control point budget
The generator SHALL expose and persist a maximum control point budget through reflected settings. The seal example SHALL default to 30 points without imposing 30 as a hardcoded maximum. Authored controls SHALL count toward that budget, while the identity root bone SHALL be accounted for separately against the compiler's bone capacity. Every render vertex SHALL retain at most four influences regardless of the selected point budget.

#### Scenario: Change generation budget
- **WHEN** the user changes the budget to another supported value and regenerates
- **THEN** the generator honors that budget, retains authored controls, reports the actual count and quality diagnostics, and saves the selected value in the recipe
