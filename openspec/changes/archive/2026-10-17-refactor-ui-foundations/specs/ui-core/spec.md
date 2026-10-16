## MODIFIED Requirements

### Requirement: Render-agnostic core module
The UI implementation core SHALL live in `engine/ui/core` as target `UI` under the explicit UI foundation exception and SHALL compile without including any render or Aurora header (`render/*`, `aurora/*`). It SHALL depend only on `UI.Interface`, `Core`, and the document parser, and SHALL NOT depend on editor, FreeType, Framework, or Aurora implementation targets. The root `UI.Interface` target SHALL expose UI provider contracts, their data, and pure utilities. Existing runtime `ui/...` public include paths SHALL remain valid through the appropriate target.

#### Scenario: Core compiles without render headers
- **WHEN** the `UI` target is built with render and Aurora include paths absent
- **THEN** compilation SHALL succeed with no missing headers or symbols

#### Scenario: Render dependencies are not leaked
- **WHEN** any public header under `engine/ui/core` is inspected
- **THEN** it SHALL NOT include `aurora/...` or `render/...` headers

#### Scenario: Runtime core excludes optional implementation modules
- **WHEN** `UI` is configured without the document editor, FreeType adaptor, or Aurora adaptor being linked by its caller
- **THEN** its dependency graph SHALL NOT require those implementations

#### Scenario: Existing runtime includes remain valid
- **WHEN** a runtime toolkit consumer links `UI` from the foundation's separated runtime target
- **THEN** its existing `ui/...` includes SHALL continue to resolve

### Requirement: Layout system
The core SHALL provide measure and arrange stages supporting the existing anchor and box-size rules, and SHALL expose each element's resolved bounds after layout. `Layout(parentRect)` SHALL resolve the element's requested layout and enter a common `Arrange(finalRect)` path. A parent that has already allocated a child rectangle SHALL pass that final rectangle to `Arrange` without resolving the child's anchor and box-size rules again. The arrangement path SHALL arrange descendant containers before returning. Resolved rectangles SHALL retain the existing absolute surface-coordinate convention; this change SHALL NOT introduce a new Geometry coordinate system. The existing unconstrained `Measure(float &, float &)` API SHALL remain available in this change.

#### Scenario: Arrange computes bounds
- **WHEN** layout runs for an element with a fixed size inside a parent
- **THEN** the element's resolved bounds size SHALL equal the requested size

#### Scenario: Layout respects parent shrink
- **WHEN** a child is laid out inside a parent with a fixed content box
- **THEN** the child's bounds SHALL be positioned within the parent's content box

#### Scenario: Parent allocation is final
- **WHEN** a slot container allocates a child rectangle and calls the child's `Arrange`
- **THEN** the child's resolved bounds SHALL equal that rectangle without a second anchor or box-size resolution

#### Scenario: Nested container geometry exists before paint
- **WHEN** a container containing another slot container is arranged
- **THEN** the nested container and its descendants SHALL have resolved bounds before any paint call

#### Scenario: Parent resize updates descendants
- **WHEN** an arranged container is arranged again with a different final rectangle
- **THEN** its descendant slot allocations SHALL reflect the new rectangle before paint

## ADDED Requirements

### Requirement: Slot containers arrange outside paint
StackPanel, GridPanel, DockPanel, and Overlay SHALL allocate their child rectangles through the common arrangement stage. `UIElement::Paint` and these containers' paint callbacks SHALL consume resolved bounds and SHALL NOT perform layout as a side effect. Migrated Sandbox and material-editor compositions SHALL arrange their slot-container content before reading child bounds for input or rendering.

#### Scenario: First input uses arranged geometry
- **WHEN** a slot-container composition is arranged and receives pointer input before its first paint
- **THEN** child hit-testing SHALL use the rectangles resolved during arrangement

#### Scenario: Paint preserves child bounds
- **WHEN** an arranged slot-container subtree is painted twice without a layout change
- **THEN** both paints SHALL preserve its resolved child bounds and SHALL NOT invoke child arrangement

#### Scenario: Layout helper uses the common path
- **WHEN** a slot container's retained convenience layout helper is invoked
- **THEN** it SHALL complete the same recursive arrangement path used by `UIElement::Arrange`
