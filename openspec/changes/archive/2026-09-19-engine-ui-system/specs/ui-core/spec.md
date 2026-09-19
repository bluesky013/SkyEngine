## ADDED Requirements

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
