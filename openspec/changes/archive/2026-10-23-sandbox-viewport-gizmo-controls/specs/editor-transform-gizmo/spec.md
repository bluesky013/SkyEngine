## ADDED Requirements

### Requirement: Shared selection and transform handles
The editor SHALL expose translate, rotate and scale tools for exactly one selected Actor with a valid Transform in the active edit World, using SelectionService as the sole selection owner. Translation SHALL support axis and plane constraints, rotation SHALL support axis constraints, and scaling SHALL support local axis and uniform constraints. Translation and rotation SHALL support World and Local orientation. Scaling SHALL remain Local and disable the space toggle.

#### Scenario: Selected Actor is edited
- **WHEN** an Actor is selected in the Outliner and the user drags a translation axis
- **THEN** the viewport and Inspector SHALL reflect the same Actor's constrained Transform

#### Scenario: Ineligible selection
- **WHEN** selection is empty, multiple, an asset, detached, or lacks a Transform
- **THEN** transform editing SHALL be disabled without modifying any Actor

#### Scenario: Parent and local orientation
- **WHEN** a child of a rotated and invertibly scaled parent is manipulated
- **THEN** its local Transform SHALL be derived under the existing TRS convention while preserving unrelated components and parentage

### Requirement: Customizable gizmo drawing contract
Gizmo geometry generation SHALL be replaceable through an injected interface whose public data contains no UI toolkit or GPU types. The default provider SHALL implement all supported handles. Drawing and hit testing SHALL use the same owned geometry and stable handle identifiers. Replacing the provider SHALL cancel active manipulation before releasing the old provider.

#### Scenario: Custom appearance
- **WHEN** the host injects a provider that changes a translation handle's geometry
- **THEN** the new shape SHALL be rendered and hit tested at its new location while retaining the same translation semantics

#### Scenario: Hidden or invalid handle
- **WHEN** a provider omits a handle or emits non-finite geometry
- **THEN** omitted geometry SHALL not be hittable and invalid output SHALL return an explicit failure without applying a transform

#### Scenario: Filled contour closure
- **WHEN** a custom provider emits a filled convex polygon without the closed-polyline flag
- **THEN** drawing and hit testing SHALL treat the fill as closed, including tolerance around its final edge

### Requirement: Relative snapping and valid transforms
Snapping SHALL quantize translation, rotation and scale deltas relative to the drag start using configurable positive steps. Scaling SHALL preserve initial component signs and SHALL NOT cross zero. Invalid/non-finite transforms and non-invertible parent transforms SHALL be rejected explicitly. Degenerate drag projections SHALL retain the last valid value.

#### Scenario: Stable snap from an off-grid origin
- **WHEN** a translated Actor starts off-grid and is dragged with snapping enabled
- **THEN** movement SHALL be an integer multiple of the configured step from its starting position, without an initial jump

#### Scenario: Singular parent
- **WHEN** the target's parent has a zero scale component
- **THEN** manipulation SHALL be refused with an explicit reason and both world and local transforms SHALL remain unchanged

### Requirement: Cancelable preview and single-step history
A gesture SHALL preview without adding history or dirtying the World, then commit exactly one before/after local Transform command for a nonzero change. Cancel and no-op gestures SHALL add no command. Undo and redo SHALL restore exact values and mark the document dirty. The implementation SHALL resolve Actor identity without retaining a dangling component pointer.

#### Scenario: Drag then undo and redo
- **WHEN** a gesture produces multiple intermediate previews and is released
- **THEN** exactly one undo step SHALL be created, Undo SHALL restore the initial local Transform, and Redo SHALL restore the final local Transform

#### Scenario: Minimum rotation snap
- **WHEN** a rotation gesture produces the supported minimum snap of 0.1 degrees
- **THEN** release SHALL retain that change as one undoable command rather than classifying it as a no-op

#### Scenario: Cancel preview
- **WHEN** Escape, focus/capture loss, a modal, a document switch, target invalidation, parent change, save, history traversal, or Play interrupts a gesture
- **THEN** the valid target SHALL be restored before the next operation, capture SHALL be released, and no preview command or new dirty state SHALL remain

#### Scenario: Target already released
- **WHEN** cancellation discovers that the target identity is no longer valid
- **THEN** it SHALL release the interaction without accessing old component storage

### Requirement: Unified tool actions
Tool controls SHALL use EditorActionRegistry and shared ActionBarBuilder/ToolBar/MenuBar widgets with SVG icons, themed metrics and shared enabled/checked state. Q/W/E/R SHALL select the pointer/translate/rotate/scale tools, X SHALL switch orientation, G SHALL toggle snapping, and Escape SHALL cancel the active gesture. Numeric settings SHALL use reflected data and ReflectedFormView.

#### Scenario: Consistent entry points
- **WHEN** the user changes tools through a toolbar action or its eligible shortcut
- **THEN** the same controller state and checked indication SHALL update

#### Scenario: Scene persistence
- **WHEN** a committed manipulation is saved and the World is reopened
- **THEN** the Actor SHALL retain the committed local Transform and parent reference
