# ui-core Specification

## Purpose
TBD - created by archiving change engine-ui-system. Update Purpose after archive.
## Requirements
### Requirement: Render-agnostic core module
The UI core SHALL live in `engine/ui/core` and SHALL compile without including any render or Aurora header
(`render/*`, `aurora/*`). It SHALL depend only on `Core` and `Framework`.

#### Scenario: Core compiles without render headers
- **WHEN** the `UI` target is built with render and Aurora include paths absent
- **THEN** compilation SHALL succeed with no missing headers or symbols

#### Scenario: Render dependencies are not leaked
- **WHEN** any public header under `engine/ui/core` is inspected
- **THEN** it SHALL NOT include `aurora/...` or `render/...` headers

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
Elements SHALL support a visibility flag. Changing visibility, bounds, or tree structure SHALL mark the
affected subtree dirty for layout and/or paint, and a clean subtree SHALL NOT be re-laid-out.

#### Scenario: Hidden element is not painted
- **WHEN** an element's visibility is set to false
- **THEN** that element and its descendants SHALL emit no draw data

#### Scenario: Dirty subtree only
- **WHEN** one element's bounds are changed
- **THEN** only that element's subtree SHALL be marked for layout, and sibling subtrees SHALL remain clean

### Requirement: Layout system
The core SHALL provide a two-pass measure/arrange layout system supporting anchor and box-size rules, and
SHALL expose each element's resolved bounds after layout.

#### Scenario: Arrange computes bounds
- **WHEN** layout runs for an element with a fixed size inside a parent
- **THEN** the element's resolved bounds size SHALL equal the requested size

#### Scenario: Layout respects parent shrink
- **WHEN** a child is laid out inside a parent with a fixed content box
- **THEN** the child's bounds SHALL be positioned within the parent's content box

### Requirement: Style and theme separation
Widget appearance SHALL be resolved from a `UIStyle`/theme value structure rather than being hard-coded in
widget logic. The theme SHALL be replaceable at runtime.

#### Scenario: Theme change affects paint
- **WHEN** the active theme's panel background color is changed and paint runs
- **THEN** panel draw data SHALL use the new color

### Requirement: Draw data emission
Core paint SHALL emit a render-agnostic `UIDrawData` containing a batched triangle list (position, uv,
color), indices, and draw commands with index range, clip rect, and an opaque texture handle. Core SHALL NOT
emit Aurora or backend types.

#### Scenario: Draw commands carry clip and texture
- **WHEN** an element with a texture is painted inside a clipping container
- **THEN** the resulting draw command SHALL reference the element's texture handle and a clip rect equal to the intersection of active clips

#### Scenario: No GPU types in draw data
- **WHEN** the `UIDrawData` type and its members are inspected
- **THEN** no Aurora, RHI, or backend type SHALL appear

### Requirement: Input event routing and hit-testing
The core SHALL accept an input state (pointer position, buttons, wheel, keyboard) and route events by
hit-testing the element tree in reverse paint order, clip-aware, so the topmost visible element under the
pointer receives the event.

#### Scenario: Topmost element receives pointer
- **WHEN** two overlapping elements are hit by a pointer event
- **THEN** the element painted last SHALL receive the event

#### Scenario: Clipped element is not hit
- **WHEN** a pointer is outside a container's clip rect but inside a child's unclipped bounds
- **THEN** the child SHALL NOT be hit

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
The UI system SHALL provide `HBox` and `VBox` containers that arrange their children sequentially along the
main axis with spacing, respecting container padding, and that size themselves to their content when AUTO.

#### Scenario: HBox places children in a row
- **WHEN** an HBox with spacing contains two fixed-size children
- **THEN** the second child's left SHALL equal the first child's right plus the spacing

#### Scenario: VBox content height
- **WHEN** a VBox has AUTO height and two stacked children
- **THEN** its measured height SHALL cover both children plus spacing and padding

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
Elements SHALL support a 2D transform (translation, rotation, scale, pivot) that is applied to emitted vertex
positions, and transforms SHALL compose down the element tree (a parent transform affects its subtree).

#### Scenario: Translation moves geometry
- **WHEN** an element with a translation transform is painted
- **THEN** its emitted vertices SHALL be offset by the translation

#### Scenario: Parent transform affects children
- **WHEN** a parent with a transform contains a child that emits geometry
- **THEN** the child's vertices SHALL be transformed by the parent transform as well

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

