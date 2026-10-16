# ui-core Specification

## Purpose
TBD - created by archiving change engine-ui-system. Update Purpose after archive.
## Requirements
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

### Requirement: Retained element tree
The core SHALL provide a `UIElement` base node and a `UIContext` that owns a single root element. Elements
SHALL support adding and removing children, parenting, and traversal in paint order.

#### Scenario: Add a child element
- **WHEN** a child element is added to a parent element
- **THEN** the child SHALL report that parent and SHALL appear in the parent's child list

#### Scenario: Remove a child element
- **WHEN** a previously added child is removed
- **THEN** the child SHALL no longer appear in the parent's child list and SHALL have no parent

#### Scenario: Single root
- **WHEN** a `UIContext` is created and multiple top-level elements are added
- **THEN** all of them SHALL be parented to the context's single root element

### Requirement: Visibility and dirty propagation
Elements SHALL support visibility and separate desired-size, arrangement, and paint invalidation. Layout invalidation SHALL propagate toward ancestors rather than recursively dirtying descendants. The explicit incremental layout entries SHALL skip only clean controlled branches with unchanged allocation inputs; volatile branches SHALL remain eligible for layout. Existing external forced layout calls SHALL remain supported. Visibility SHALL continue to suppress hidden elements and their descendants during paint.

#### Scenario: Hidden element is not painted
- **WHEN** an element's visibility is set to false
- **THEN** that element and its descendants SHALL emit no draw data

#### Scenario: Changed leaf invalidates its consumers
- **WHEN** a leaf changes its desired size
- **THEN** the leaf and ancestor allocation consumers SHALL become dirty while unrelated sibling branches SHALL not be recursively marked dirty

#### Scenario: Bounds changes update descendants during arrangement
- **WHEN** a host changes an element's bounds and requests layout
- **THEN** its descendant allocations SHALL update through arrangement while unrelated sibling branches remain eligible for reuse

#### Scenario: Clean controlled subtree skips incremental work
- **WHEN** an incremental pass reaches an unchanged controlled subtree with no pending layout work
- **THEN** that subtree SHALL NOT be re-laid-out solely because a new frame begins

### Requirement: Layout system
The core SHALL provide measure and arrange stages supporting anchor and box-size rules, and SHALL expose each element's resolved bounds after layout. Its non-virtual MeasureDesired entry SHALL enforce normalized constraints through OnMeasure, and MeasureForLayout/Layout SHALL resolve known own-rule axes before measuring. Layout(parentRect) SHALL compute the requested rectangle from the same rule budget and enter Arrange(finalRect). A parent that already owns a child's final rectangle SHALL measure/arrange that allocation without applying the child's anchor/size rules again. Descendants SHALL be arranged before return, and absolute layout-surface coordinates SHALL remain unchanged. Legacy Measure(float&, float&) SHALL remain supported through the one-way bridge.

#### Scenario: Arrange computes bounds
- **WHEN** ordinary layout runs for an element with a fixed size inside a parent
- **THEN** its resolved bounds SHALL equal the requested fixed size, including an explicit size larger than the parent's available extent

#### Scenario: Layout respects parent shrink
- **WHEN** a child with a non-overflowing AUTO/FILL request is laid out inside a fixed parent content box
- **THEN** its resolved allocation SHALL respect that content budget

#### Scenario: Parent allocation is final
- **WHEN** a slot container allocates a child rectangle and calls the child's Arrange
- **THEN** the child's resolved bounds SHALL equal that rectangle without a second anchor or box-size resolution

#### Scenario: Nested container geometry exists before paint
- **WHEN** a container containing another slot container is arranged
- **THEN** the nested container and its descendants SHALL have resolved bounds before any paint call

#### Scenario: Parent resize updates descendants
- **WHEN** an arranged container is arranged again with a different final rectangle
- **THEN** its descendant slot allocations SHALL reflect the new rectangle before paint

#### Scenario: Known width constrains desired height
- **WHEN** an element's width is resolved before measuring width-dependent content
- **THEN** OnMeasure SHALL receive that resolved width budget before producing desired height

### Requirement: Style and theme separation
Widget appearance SHALL be resolved from a `UIStyle`/theme value structure rather than being hard-coded in
widget logic. The theme SHALL be replaceable at runtime.

#### Scenario: Theme change affects paint
- **WHEN** the active theme's panel background color is changed and paint runs
- **THEN** panel draw data SHALL use the new color

### Requirement: Draw data emission
Core paint SHALL emit a render-agnostic UIDrawData containing a batched triangle list with position, UV, and color, indices, and draw commands with index range, clip rectangle, and an opaque texture handle. Vertex positions and draw clip rectangles SHALL be expressed in rendered-surface coordinates. Active layout clips SHALL be transformed to conservative surface AABBs before intersection. Core SHALL NOT emit Aurora or backend types. Each paint call SHALL rebuild complete visible output even when layout and paint flags were already clean; dirty state SHALL NOT be used to omit draw emission without a retained rendering system.

#### Scenario: Draw commands carry clip and texture
- **WHEN** an element with a texture is painted inside a clipping container
- **THEN** its draw command SHALL reference the texture handle and a clip rectangle equal to the intersection of active transformed surface clip AABBs

#### Scenario: No GPU types in draw data
- **WHEN** the UIDrawData type and its members are inspected
- **THEN** no Aurora, RHI, or backend type SHALL appear

#### Scenario: Unchanged frames retain full output
- **WHEN** a visible tree is painted twice with unchanged state and clean dirty flags
- **THEN** both frames SHALL contain complete equivalent draw output rather than an empty second frame

### Requirement: Input event routing and hit-testing
The core SHALL accept input whose pointer positions are in rendered-surface coordinates and SHALL hit-test the element tree in reverse stable paint/z order, using effective transformed surface clip AABBs and inverse accumulated geometry. The topmost visible hit element SHALL receive the event. A non-invertible element and its subtree SHALL NOT be hit or receive pointer dispatch with undefined coordinates.

#### Scenario: Topmost element receives pointer
- **WHEN** two overlapping elements are hit by a pointer event
- **THEN** the element painted last SHALL receive the event

#### Scenario: Clipped element is not hit
- **WHEN** a pointer is outside a container's transformed surface clip AABB but inside a child's unclipped transformed bounds
- **THEN** the child SHALL NOT be hit

#### Scenario: Non-invertible subtree is excluded
- **WHEN** an element or its ancestor has a singular accumulated transform
- **THEN** neither that element nor its descendants SHALL be selected as a pointer hit target

### Requirement: Event consumption result
Pointer and keyboard handlers SHALL return a result indicating whether the event was handled, so the router
can report consumption to the caller. Elements SHALL default to unhandled.

#### Scenario: Handled event reported
- **WHEN** an element handles a pointer event
- **THEN** the router SHALL return a handled result

#### Scenario: Default unhandled
- **WHEN** no element handles an event
- **THEN** the router SHALL return an unhandled result

### Requirement: Focus and pointer capture
The core SHALL track a focused element for keyboard events and a captured element for pointer events, so a
drag continues to target the element that received the press.

#### Scenario: Focus receives keyboard
- **WHEN** an element is focused and a key event is dispatched
- **THEN** the focused element SHALL receive the key event

#### Scenario: Capture holds drag
- **WHEN** an element captures the pointer on press and the pointer moves outside its bounds
- **THEN** subsequent move and release events SHALL still be delivered to that element

### Requirement: Input suppression flag
The UI context SHALL expose whether it wants to consume input this frame, so the framework can gate game
input while a UI surface is active.

#### Scenario: Active UI claims input
- **WHEN** a modal or focused UI surface is active
- **THEN** the context SHALL report that it wants input

#### Scenario: Inactive UI releases input
- **WHEN** no UI surface is active
- **THEN** the context SHALL report that it does not want input

### Requirement: Widget primitives
The core SHALL provide base widget primitives for container/panel, text, image, and button, each deriving
from `UIElement` and emitting draw data through the theme.

#### Scenario: Button reports click
- **WHEN** a button receives a press and release inside its bounds
- **THEN** the button SHALL raise a click event

#### Scenario: Image widget uses texture handle
- **WHEN** an image widget is configured with a texture handle
- **THEN** its draw command SHALL reference that handle

### Requirement: Percent sizing
The layout system SHALL support a percent size mode where an axis resolves to a fraction of the parent
extent, and the document loader SHALL parse `"50%"`-style strings into that mode.

#### Scenario: Percent resolves against parent
- **WHEN** a child with a 50 percent width is laid out inside a 200-wide parent
- **THEN** the child width SHALL be 100

### Requirement: Z-ordered children
Elements SHALL carry a `z` value; painting SHALL draw children in ascending `z` with a stable order for equal
`z`, and hit-testing SHALL consider them in the reverse order.

#### Scenario: Higher z paints later
- **WHEN** two overlapping children differ in `z`
- **THEN** the higher-`z` child SHALL be painted after the lower one and SHALL be hit first

### Requirement: Linear layout containers
The UI system SHALL provide HBox and VBox containers that arrange their children sequentially along the main axis with spacing and padding and size themselves to their content when AUTO. They SHALL query child layout rules with available constraints, reserve consumed Fixed/Auto extents and spacing, and allocate Fill only from the nonnegative remaining main-axis budget. Their final child allocations SHALL NOT overlap because a Fill child receives the original full parent extent again.

#### Scenario: HBox places children in a row
- **WHEN** an HBox with spacing contains two fixed-size children
- **THEN** the second child's left SHALL equal the first child's right plus the spacing

#### Scenario: VBox content height
- **WHEN** a VBox has AUTO height and two stacked children
- **THEN** its measured height SHALL cover both children plus spacing and padding, subject to the caller's hard constraints

#### Scenario: Fill follows an already allocated sibling
- **WHEN** a finite HBox or VBox has a non-Fill child followed by Fill content
- **THEN** the Fill child's allocated extent SHALL be the remaining main-axis share rather than the full parent extent

### Requirement: Pointer enter and leave
The event router SHALL track the hovered element and notify the previous element with a leave and the new
element with an enter when the hovered element changes. Enter/leave SHALL NOT consume the move event.

#### Scenario: Enter on hover
- **WHEN** the pointer moves over an element that was not hovered
- **THEN** that element SHALL receive a pointer enter notification

#### Scenario: Leave on moving away
- **WHEN** the pointer moves from one element to another
- **THEN** the first element SHALL receive a pointer leave before the second receives enter

### Requirement: Unhandled event bubbling
An element that does not handle an event SHALL have the event propagate to its ancestors until one handles it
or the tree is exhausted.

#### Scenario: Parent handles child event
- **WHEN** a child does not handle a pointer event and its parent does
- **THEN** the parent's handler SHALL be invoked

### Requirement: Focus traversal
Elements SHALL carry a focusable flag (default false). The context SHALL provide next/previous focus lookup
over enabled, effectively-visible, focusable elements with wrap-around, in traversal order.

#### Scenario: Next focus
- **WHEN** focus traversal advances from a focusable element
- **THEN** the next focusable element in traversal order SHALL be returned

#### Scenario: Wrap around
- **WHEN** traversal advances from the last focusable element
- **THEN** the first focusable element SHALL be returned

### Requirement: Element transform
Elements SHALL support a 2D transform (translation, rotation, scale, pivot) that is applied to emitted vertex positions, and transforms SHALL compose down the element tree with the context surface transform. Paint, geometry queries, clipping, and hit-testing SHALL use that same composition. GetBounds/Arrange SHALL retain absolute layout-surface rectangles and the current transform/pivot convention; render transforms SHALL NOT implicitly move their origin into parent-local coordinates.

#### Scenario: Translation moves geometry
- **WHEN** an element with a translation transform is painted
- **THEN** its emitted vertices SHALL be offset by the translation

#### Scenario: Parent transform affects children
- **WHEN** a parent with a transform contains a child that emits geometry
- **THEN** the child's vertices SHALL be transformed by the parent transform as well

#### Scenario: Hit geometry matches paint geometry
- **WHEN** a child under nested transforms is painted and hit-tested
- **THEN** both operations SHALL use the same accumulated layout-to-surface matrix

### Requirement: Hierarchical opacity
Elements SHALL carry an opacity in the range 0 to 1 that multiplies into emitted vertex alpha, and a parent's
opacity SHALL apply to its subtree.

#### Scenario: Opacity scales alpha
- **WHEN** an element with opacity 0.5 emits opaque geometry
- **THEN** the emitted vertex alpha SHALL be half

#### Scenario: Parent opacity cascades
- **WHEN** a parent with opacity less than 1 contains a child that emits geometry
- **THEN** the child's vertex alpha SHALL be reduced by the parent's opacity

### Requirement: Per-field style cascade
Theme style resolution SHALL merge style classes field by field, so a class that sets only some fields
overrides those fields while earlier classes keep the rest.

#### Scenario: Overlay overrides one field
- **WHEN** a base class sets the background and an overlay class sets only the border width
- **THEN** the resolved style SHALL have the base background and the overlay border width

#### Scenario: Later class wins per field
- **WHEN** two classes set different fields
- **THEN** the resolved style SHALL contain both

### Requirement: Nine-slice images
An image SHALL support per-edge slice insets and, when set, SHALL render nine quads so corners stay fixed
while edges and center stretch.

#### Scenario: Nine quads emitted
- **WHEN** an image with non-zero slice insets is painted
- **THEN** it SHALL emit nine quads

#### Scenario: No insets means single quad
- **WHEN** an image has zero slice insets
- **THEN** it SHALL emit a single quad

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
