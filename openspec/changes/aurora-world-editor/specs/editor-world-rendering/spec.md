## ADDED Requirements

### Requirement: Editable World reaches the renderer through one producer
The editor SHALL extract the active World through the owned scene bridge after simulation tick and before the host seals its packet. The editor SHALL retain the same packet when the renderer reports Busy.

#### Scenario: Transform edit updates the viewport
- **WHEN** a user edits a scene component's transform through the reflected inspector
- **THEN** the next accepted host packet contains the updated world transform and the viewport renders that transform

#### Scenario: Scene orientation across output backends
- **WHEN** a scene object is translated along the camera's positive Y axis
- **THEN** it appears above the image center in Vulkan and DX12 with HDR both enabled and disabled, using the scene Pass's canonical Y-up projection and the existing logical output transform

### Requirement: Scene editing preserves logical data and ownership
Scene create/delete actions SHALL use the shared action registry and undo service. Persisted data SHALL contain logical asset UUIDs and component values, and SHALL NOT contain renderer or native GPU pointers.

#### Scenario: Save and reopen a primitive
- **WHEN** a user creates a Cube, edits its transform, saves the World and reopens it
- **THEN** the same logical mesh UUID and edited transform are restored and rendered through the production resource source

#### Scenario: Delete and undo
- **WHEN** a user deletes a scene object and then undoes the action
- **THEN** the editor retains the actor's authored data for undo and the bridge observes a new renderer lifetime without reusing an unretired generation

### Requirement: Inspector bindings remain valid across document changes
The editor SHALL invalidate reflected bindings and history before releasing the previous document's component storage. Both const and mutable getters SHALL capture previous values for undo.

#### Scenario: Switch documents
- **WHEN** the active document is replaced or closed
- **THEN** selection and undo history no longer reference the previous document's components

### Requirement: Scene hierarchy and component inspection remain separate
The Outliner SHALL display Actors with their Transform parent/child hierarchy and SHALL NOT display components as selectable scene nodes. Selecting an Actor SHALL display its inspectable components as grouped sections in the existing reflected Inspector, with Transform first. Field edits, defaults and expansion state SHALL remain independent across component sections, including identically named fields. Scene deletion SHALL target an Actor selection.

#### Scenario: Inspect an Actor with several components
- **WHEN** an Actor with Transform and StaticMesh components is selected in the Outliner
- **THEN** the Inspector displays both component sections, and editing or undoing a StaticMesh property affects only that Actor's StaticMesh

#### Scenario: Child appears before its parent in storage
- **WHEN** a child Actor precedes its parent in World storage
- **THEN** the Outliner displays the parent followed by the indented child using their real Actor IDs

#### Scenario: Selected Actor is removed
- **WHEN** the selected Actor is detached or its World is unbound
- **THEN** all Inspector component bindings are cleared before the previous storage is released

### Requirement: Targeted editor module builds refresh runtime dependencies
The build SHALL use the existing editor module configuration and dependency helpers to order enabled runtime module targets. Building an editor module SHALL rebuild its declared runtime dependencies without linking those DLLs into the consumer or requiring the host executable target to be selected.

#### Scenario: Shader reflection layout changes
- **WHEN** shader reflection types change and SandboxModule is built directly
- **THEN** its configured Aurora.ShaderCompiler dependency is rebuilt before SandboxModule so the running editor consumes matching reflection layouts
