# ui Specification

## Purpose

Define retained UI data, layout, geometry, input, text, animation, rendering and editor integration.

## Scope notes

### ui-editor

Define the headless UI document editing model: mutating the element tree, serializing to the UI document
format, and undo/redo. A visual shell is a separate concern that depends on the render pass.

### ui-geometry

Keep UI paint, clipping, hit testing and geometry queries consistent through shared layout-to-surface transforms.

### ui-invalidation

Support explicit incremental measurement and arrangement while preserving complete visible draw output on every paint.

### ui-measurement

Define normalized UI size constraints and consistent bounded measurement across containers, text and editor compositions.

## Requirements

### Requirement: Easing functions

The animation system SHALL provide easing functions that map a normalized time to an eased value in the range
0 to 1.

#### Scenario: Ease out quad
- **WHEN** EASE_OUT_QUAD is evaluated at 0.5
- **THEN** the result SHALL be 0.75

### Requirement: Keyframe track

A float track SHALL store time/value keys and linearly interpolate between them, clamped at the ends.

#### Scenario: Midpoint interpolation
- **WHEN** a track with keys (0, 0) and (1, 10) is evaluated at 0.5
- **THEN** it SHALL return 5

### Requirement: Time-driven tween

A `UIAnimation` SHALL call its update callback with the eased normalized progress and SHALL report completion;
the context SHALL advance animations with `Tick(delta)` and remove completed ones.

#### Scenario: Advance updates progress
- **WHEN** an animation of duration 1 is advanced by 0.5 with linear easing
- **THEN** the update callback SHALL receive 0.5

#### Scenario: Completion
- **WHEN** an animation is advanced past its duration
- **THEN** its completion callback SHALL run and the context SHALL remove it

#### Scenario: Loop
- **WHEN** a looping animation is advanced past its duration
- **THEN** its time SHALL wrap and it SHALL remain active

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

### Requirement: UI document format

The UI system SHALL define a declarative UI document format describing a tree of elements. Each node SHALL
carry a `type`, an optional `name`, `props` (grouped layout and visual/content values), optional `bindings`, and
`children`. Documents SHALL reference style/theme classes rather than raw visual constants.

#### Scenario: Document describes a tree
- **WHEN** a document declaring a panel with a text and an image child is loaded
- **THEN** the resulting element tree SHALL contain the panel with the text and image as children in declaration order

#### Scenario: Unknown type diagnosed
- **WHEN** a document node declares a `type` not present in the element-type registry
- **THEN** the loader SHALL report a diagnostic identifying the unknown type and SHALL NOT crash

### Requirement: Element-type registry

The UI system SHALL provide an extensible element-type registry mapping a document `type` string to an element
factory, so new widget types can be registered without modifying the loader.

#### Scenario: Register a custom type
- **WHEN** a new element type is registered with the registry
- **THEN** documents using that type SHALL instantiate it through the registered factory

### Requirement: Runtime document loader

The UI system SHALL provide a runtime loader that parses a UI document and builds a `UIElement` tree, exposing
loaded elements by name for lookup.

#### Scenario: Load and look up by name
- **WHEN** a document with a named element is loaded
- **THEN** the loader SHALL return a tree from which that element can be retrieved by its name

#### Scenario: Missing referenced asset
- **WHEN** a document references an asset that cannot be resolved
- **THEN** the loader SHALL report a diagnostic and SHALL continue loading the remainder of the document

### Requirement: Style and theme resources

Styles and themes SHALL be loadable as data. Documents SHALL resolve appearance through style classes so that
replacing the active theme re-skins loaded documents without editing them.

#### Scenario: Theme swap re-skins document
- **WHEN** the active theme is replaced after a document is loaded
- **THEN** elements referencing style classes SHALL use the new theme's style values

### Requirement: Data-context property binding

The UI system SHALL provide a data context exposing named values with change notification, and property
bindings that map a source path on the data context to a target element property. Bindings SHALL re-evaluate
when the context changes or when the frame updates.

#### Scenario: Bound property updates
- **WHEN** a value on the data context changes
- **THEN** elements bound to that path SHALL reflect the new value without reloading the document

#### Scenario: Unbound value
- **WHEN** a binding references a path that does not exist on the data context
- **THEN** the binding SHALL report a diagnostic and the target property SHALL retain its last value

### Requirement: Binding provider seam

Path resolution SHALL go through an `IUIDataProvider` abstraction so the UI data layer does not hard-depend on
the reflection system.

#### Scenario: Custom data provider
- **WHEN** a custom data provider is installed
- **THEN** binding path resolution SHALL use that provider

### Requirement: Asset resolution seam

Document asset references (textures, fonts, sub-documents) SHALL be resolved through an `IUIAssetResolver`
abstraction so the UI data layer remains render-agnostic.

#### Scenario: Data layer has no render dependency
- **WHEN** the UI data layer is built without render or Aurora include paths
- **THEN** compilation SHALL succeed

### Requirement: C++ declarative builder parity

The UI system SHALL provide a C++ declarative builder that constructs the same element tree and attaches the
same bindings as the document loader, so a screen built in code behaves identically to one loaded from a
document.

#### Scenario: Builder matches loader
- **WHEN** the same screen is built with the C++ builder and loaded from an equivalent document
- **THEN** both SHALL produce the same element hierarchy and bound properties

#### Scenario: Tests avoid asset IO
- **WHEN** a UI test builds a screen with the C++ builder
- **THEN** it SHALL not require loading a UI document from disk

### Requirement: Document scope

This milestone SHALL provide the document model, loader, element-type registry, style/theme resources, and
property binding. A visual authoring tool, document-to-code generation, event graphs, and hot reload SHALL be
out of scope.

#### Scenario: No authoring or codegen dependency
- **WHEN** the UI build dependencies are inspected
- **THEN** no UI authoring tool or document code generator SHALL be required to build or run the system

### Requirement: Built-in binding converters

The binding system SHALL provide built-in converters `remap`, `format`, and `boolToVisible` that transform a
source value into a target value, with converter arguments supplied by the document.

#### Scenario: remap scales a value
- **WHEN** a binding uses `remap` with an input range and an output range
- **THEN** the target value SHALL be the source value linearly mapped from the input range into the output range

#### Scenario: boolToVisible passes a bool
- **WHEN** a binding uses `boolToVisible`
- **THEN** the target SHALL receive the source boolean value unchanged

### Requirement: Binding evaluation timing

Bindings SHALL re-evaluate when the data context changes, and SHALL NOT re-apply when the context is unchanged
since the last evaluation.

#### Scenario: Apply on change
- **WHEN** a value on the data context changes and bindings are applied
- **THEN** bound targets SHALL reflect the new value

#### Scenario: Skip when unchanged
- **WHEN** bindings are applied again without any data-context change
- **THEN** the system SHALL report that no update was needed

### Requirement: Load diagnostics

The loader SHALL return diagnostics describing content problems (unknown element type, unresolved asset,
unknown property path, duplicate name) with severity, node path, code, and message, and SHALL NOT throw.

#### Scenario: Diagnostics collected
- **WHEN** a document references an unknown element type and an unresolved texture
- **THEN** the loader SHALL return an error diagnostic for the type and a warning diagnostic for the asset

### Requirement: Generic nested scalar properties

The document loader SHALL flatten nested object members in props into dotted property paths and pass scalar leaves through the registered element's SetProperty. Boolean, int64-compatible integer, other numeric float, and string values SHALL preserve their corresponding UIPropertyValue types. The layout, visual, and button groups SHALL retain their specialized parsing and SHALL NOT be dispatched a second time through the generic path. Arrays, null, unsupported values, and unknown property paths SHALL remain silently ignored without failing document loading. Generic dispatch SHALL NOT require widget-specific casts.

#### Scenario: Registered setter receives nested scalar properties
- **WHEN** a registered element is loaded with nested boolean, signed-64-bit-compatible integer, float, and string props
- **THEN** its SetProperty SHALL receive the dotted leaf paths and corresponding typed scalar values

#### Scenario: Existing specialized groups are decoded once
- **WHEN** a document supplies layout arrays, a visual color or texture reference, or button interactability
- **THEN** the existing specialized parsing SHALL apply those values once without duplicate generic setter calls

#### Scenario: Unknown and unsupported properties are harmless
- **WHEN** a document contains unknown property paths, array leaves, or null leaves
- **THEN** loading SHALL continue without a property diagnostic or a failed document

### Requirement: Declarative text wrap property

The generic text property path SHALL accept boolean text.wrap and apply it through the same Text setter used by code-built widgets. JSON text properties and C++ construction SHALL produce the same wrap setting and constrained measure/paint behavior. An omitted wrap property SHALL remain false, preserving existing documents and no-wrap callers.

#### Scenario: JSON enables wrap
- **WHEN** a Text document supplies boolean wrap in its text properties
- **THEN** the loader SHALL apply the text.wrap property and finite-width measurement SHALL use soft wrapping

#### Scenario: C++ and document behavior match
- **WHEN** equivalent Text widgets are configured in C++ and JSON with the same content/font/wrap/width
- **THEN** their constrained desired size and painted line layout SHALL agree

#### Scenario: Existing document retains default
- **WHEN** a Text document omits text.wrap
- **THEN** its wrap flag SHALL remain false

### Requirement: Document editing operations

The editor SHALL support finding elements by name and adding, removing, renaming, and reordering elements,
using the element-type registry to create new elements.

#### Scenario: Add child
- **WHEN** a child of a registered type is added under a parent
- **THEN** it SHALL appear in the parent's children

#### Scenario: Remove by name
- **WHEN** an element is removed by name
- **THEN** it SHALL no longer be found in the tree

#### Scenario: Reorder siblings
- **WHEN** an element is moved up or down among its siblings
- **THEN** its position in the child order SHALL change by one

### Requirement: Serialization round-trip

The editor SHALL serialize the tree to the UI document format and load it back without losing the element
hierarchy, type, or name.

#### Scenario: Round-trip preserves structure
- **WHEN** a tree is serialized and then deserialized
- **THEN** the reloaded tree SHALL have the same element types and names

### Requirement: Undo and redo

The editor SHALL snapshot the document before each edit and SHALL restore prior and subsequent states through
undo and redo.

#### Scenario: Undo restores previous state
- **WHEN** an edit is undone
- **THEN** the document SHALL return to the state before the edit

#### Scenario: Redo reapplies
- **WHEN** a redo follows an undo
- **THEN** the edit SHALL be reapplied

### Requirement: Headless document editor is separate from runtime

The headless UI document editor SHALL live in `engine/ui/editor` as target `UI.Editor`, retaining its `ui/editor/...` public includes and existing editing, serialization, and undo/redo behavior. `UI.Editor` SHALL depend on the runtime toolkit; the runtime `UI` target SHALL NOT include or link document-editor implementation. Document-editor tests SHALL link `UI.Editor` in a separate `UIEditorTest` target.

#### Scenario: Runtime builds without document editing
- **WHEN** a runtime application links only `UI`
- **THEN** its dependency graph SHALL NOT include `UI.Editor` or its implementation

#### Scenario: Document editor links explicitly
- **WHEN** a caller links `UI.Editor` and includes `ui/editor/UIDocumentEditor.h`
- **THEN** headless document editing, serialization round-trip, and undo/redo SHALL remain available

#### Scenario: Tests preserve the runtime boundary
- **WHEN** UI test targets are configured
- **THEN** document-editor tests SHALL belong to `UIEditorTest`, and runtime core tests SHALL NOT require `UI.Editor`

### Requirement: Pure geometry and affine utilities

UI.Interface SHALL expose a pure UIGeometry value containing absolute layout-surface bounds and a cumulative layout-to-rendered-surface affine transform. That target in engine/ui SHALL provide affine inversion and transformed-rectangle AABB operations without referencing UIElement, UIContext, renderer implementations, or plugin headers. Inversion SHALL report non-invertible transforms rather than produce undefined pointer coordinates.

#### Scenario: Rectangle maps through all corners
- **WHEN** an affine transform rotates or scales a rectangle
- **THEN** TransformRect SHALL return the axis-aligned bounds of all four transformed corners

#### Scenario: Inverse round-trip
- **WHEN** a point is mapped through an invertible affine transform and its inverse
- **THEN** the result SHALL recover the original point within floating-point tolerance

#### Scenario: Singular inverse is rejected
- **WHEN** an affine transform has no inverse
- **THEN** inverse computation SHALL report failure rather than return a usable pointer mapping

#### Scenario: Interface builds independently
- **WHEN** a consumer includes the geometry utilities using UI.Interface without UI runtime or renderer targets
- **THEN** it SHALL compile and use those pure data operations without plugin implementation dependencies

### Requirement: On-demand accumulated element geometry

UIElement::GetGeometry(extraTransform = {}) SHALL return its existing absolute layout bounds and the configured context surface base composed with all ancestor and element render transforms in paint order. The default query SHALL include that base without requiring the caller to pass it. An optional extra compositor transform SHALL premultiply the complete accumulated result rather than replace the base. The query SHALL work before paint after bounds have been assigned. It SHALL NOT change GetBounds/Arrange into parent-local coordinates, subtract the element origin during inverse mapping, or require a geometry cache or invalidation redesign.

#### Scenario: Nested transforms compose in paint order
- **WHEN** an element under transformed ancestors is queried
- **THEN** its geometry transform SHALL equal surfaceBase * root * successive ancestors * element

#### Scenario: Extra compositor preserves the configured base
- **WHEN** an element is queried with an extra compositor transform
- **THEN** that transform SHALL premultiply the result including the existing context surface base

#### Scenario: Geometry is available before painting
- **WHEN** a subtree has been arranged and GetGeometry is queried before first paint
- **THEN** it SHALL return the assigned bounds and current accumulated transform

#### Scenario: Absolute layout origin is preserved
- **WHEN** an element whose layout bounds start at x=100 is translated by 20 and a surface point at x=120 is mapped back
- **THEN** the layout x coordinate SHALL be 100 rather than zero

### Requirement: Persistent context surface transform

UIContext SHALL expose SetSurfaceTransform/GetSurfaceTransform with identity as the default and one authoritative surface-base state shared by context configuration, root painting, and default element geometry queries. Ordinary Paint and pointer routing SHALL use that same base. The compatibility Paint(context, rootTransform) entry SHALL store its transform before using the ordinary paint path. The context's root surface clip and existing content-size/DPI rules SHALL remain unchanged.

#### Scenario: Input before first paint
- **WHEN** the host arranges a context and sets its surface transform before any paint
- **THEN** its router SHALL use that transform for the first pointer hit-test and delivery

#### Scenario: Compatibility paint configures future input
- **WHEN** a caller paints with the explicit rootTransform overload
- **THEN** subsequent ordinary paint and input SHALL use the stored rootTransform

#### Scenario: Root surface clip is stable
- **WHEN** a surface transform translates or scales UI content
- **THEN** the root clip SHALL remain the configured surface rectangle rather than being transformed again

### Requirement: Transformed clipping and quad culling

PushClip SHALL transform its layout rectangle through the current cumulative matrix, convert it to a rendered-surface AABB, and intersect that with the active surface clip. Quad emission SHALL transform its corners before deciding whether its surface AABB is outside that clip. Rotated clipping SHALL be explicitly conservative and axis-aligned for scissor rendering rather than exact polygon clipping.

#### Scenario: Transformed content enters the clip
- **WHEN** a quad lies outside the clip in layout coordinates but its render transform moves it into view
- **THEN** it SHALL NOT be culled using its original layout rectangle

#### Scenario: Nested transformed clips intersect
- **WHEN** nested clipping rectangles have different accumulated transforms
- **THEN** the effective draw clip SHALL be the intersection of their transformed surface AABBs and the root surface clip

#### Scenario: Rotated clip is conservative
- **WHEN** a clipping rectangle is rotated
- **THEN** clipping SHALL use its surface AABB, with no claim of an exact rotated-polygon mask

### Requirement: Multi-pointer capture

Pointer events SHALL carry a pointer id, and the router SHALL track capture per pointer so that each pointer's move/release is delivered to the element it pressed, independently of other pointers. Captured delivery SHALL map the rendered-surface pointer through the target's current inverse accumulated transform. When captured geometry is non-invertible, the router SHALL clear that pointer's capture and send coordinate-free pointer cancellation rather than deliver undefined coordinates or leave the gesture captured.

#### Scenario: Independent captures
- **WHEN** two pointers press two different elements and then move
- **THEN** each element SHALL receive only its own pointer's events

#### Scenario: Release clears one capture
- **WHEN** a captured pointer is released
- **THEN** only that pointer's capture SHALL be cleared

#### Scenario: Captured geometry changes
- **WHEN** a captured element's invertible transform changes before a subsequent move or release
- **THEN** that event SHALL be mapped using its current geometry rather than the transform at the original press

#### Scenario: Capture becomes non-invertible
- **WHEN** a captured target's accumulated transform becomes singular
- **THEN** the router SHALL clear that pointer's capture, notify cancellation without x/y, and SHALL NOT invoke its ordinary pointer handler with undefined coordinates

### Requirement: Focus on press

A pointer press SHALL focus the nearest focusable ancestor of the hit element, so a text field begins
receiving input when clicked.

#### Scenario: Click focuses text field
- **WHEN** a press hits a focusable element
- **THEN** that element SHALL become focused and receive subsequent key and text events

### Requirement: Text input routing

Committed text input SHALL be delivered to the focused element and bubble to ancestors while unhandled.

#### Scenario: Text goes to focus
- **WHEN** text input is dispatched while an element is focused
- **THEN** the focused element SHALL receive an `OnTextInput` call

### Requirement: Edit box

An `EditBox` SHALL be a focusable text field that inserts committed text at its caret, deletes the character
before the caret on backspace, and moves the caret on left/right key input.

#### Scenario: Typing inserts text
- **WHEN** text is committed while an edit box is focused
- **THEN** the text SHALL be inserted at the caret and the caret SHALL advance

#### Scenario: Backspace deletes
- **WHEN** backspace is received
- **THEN** the character before the caret SHALL be removed

#### Scenario: Caret moves
- **WHEN** a left or right key is received
- **THEN** the caret SHALL move within the text bounds

### Requirement: Per-receiver pointer coordinates

UIEventRouter SHALL map pointer x/y separately for every target, bubbling ancestor, and hover enter/leave receiver using that receiver's inverse accumulated geometry. Handler x/y SHALL remain untransformed absolute layout-surface coordinates without subtracting the receiver's bounds origin. The original rendered-surface point SHALL be preserved for deriving each receiver's event copy, and pointer id, action, button, and wheel data SHALL be preserved.

#### Scenario: Transformed button receives layout coordinates
- **WHEN** a pointer presses and releases inside the painted bounds of a transformed Button
- **THEN** its handler SHALL receive points in its original absolute layout bounds and SHALL apply its existing inside/outside click behavior

#### Scenario: Bubbling remaps for each ancestor
- **WHEN** an event bubbles from a transformed child to an ancestor with different accumulated geometry
- **THEN** the ancestor SHALL receive coordinates mapped from the original surface point through its own inverse rather than a copy of the child's mapped coordinates

#### Scenario: Hover transitions map independently
- **WHEN** the hovered element changes between differently transformed elements
- **THEN** leave and enter callbacks SHALL each receive coordinates appropriate to their respective receiver

### Requirement: Direct child routing follows relative geometry

Direct ChildRouting SHALL accept pointer x/y in the calling parent's layout coordinates and SHALL map them with each child's local inverse before its hit-test and delivery. Uncaptured target selection SHALL use reverse stable paint/z order and the conservative rendered-surface AABBs of ancestors that declare ClipsChildren, obtained through geometry including the configured surface base. The helper SHALL NOT infer a viewport from a detached root's GetBounds; its caller SHALL enforce viewport boundaries through its host or UIEventRouter, which owns the fixed context surface clip. Captured child input SHALL use the same relative coordinate mapping without requiring a new clipped hit-test. Direct routing SHALL NOT apply the full accumulated inverse again to an already parent-mapped point or dispatch through a non-invertible surface base or accumulated child path. An unmappable direct capture SHALL be cleared before coordinate-free cancellation is sent to the captured child.

#### Scenario: Direct route maps one transform step
- **WHEN** a parent's handler forwards an already mapped pointer event to a transformed child
- **THEN** the child SHALL receive the result of its local inverse only, with no duplicate ancestor/surface conversion

#### Scenario: Direct overlap follows paint order
- **WHEN** overlapping children differ in z or equal-z insertion order
- **THEN** direct routing SHALL consider them in reverse of their stable paint order

#### Scenario: Direct captured child remains mapped
- **WHEN** a direct-route child is captured and receives later move/release outside its bounds
- **THEN** the captured input SHALL use its current local inverse without requiring a new clipped hit-test, and a non-invertible accumulated path SHALL prevent undefined dispatch

#### Scenario: Direct route respects surface-base clipping
- **WHEN** a direct child route receives a parent-layout point under a configured surface transform
- **THEN** uncaptured target selection SHALL check the corresponding rendered-surface point against ClipsChildren ancestor AABBs using the parent's default geometry

#### Scenario: Detached bounds do not invent a viewport
- **WHEN** a detached root does not clip children and a child extends beyond that root's bounds
- **THEN** direct routing SHALL NOT reject the child solely because of those root bounds, and the caller SHALL remain responsible for any actual viewport restriction

### Requirement: Unusable capture sends coordinate-free cancellation

UIElement SHALL provide OnPointerCancel(pointerId) with a default no-op implementation. If a router's captured target cannot map a pointer due to non-invertible geometry, the router SHALL clear that pointer's capture before notifying the target and its ancestors through OnPointerCancel. Direct-child capture owners SHALL clear and notify an unmappable captured child, and slot containers SHALL be able to forward cancellation through their direct capture. Cancellation SHALL require no mapped x/y, SHALL NOT produce a Button click, and SHALL clear active pressed/drag/preview state in the affected Button or reflected form. This requirement SHALL be limited to invalid geometry cleanup and SHALL NOT change focus policy or other pointers' captures.

#### Scenario: Singular release does not leave a Button pressed
- **WHEN** a Button is captured on press and its accumulated geometry becomes singular before release
- **THEN** cancellation SHALL clear its pressed state and capture without emitting a click, allowing a later invertible gesture to work normally

#### Scenario: Cancellation traverses target and ancestors
- **WHEN** a router cannot map a captured pointer because an element, ancestor, or configured surface transform is singular
- **THEN** it SHALL clear the capture and notify target-to-ancestor cancellation without attempting to construct local coordinates

#### Scenario: Direct capture cancels safely
- **WHEN** a direct-child capture becomes unmappable
- **THEN** its owner SHALL clear the captured reference before notifying that child, including forwarding through a slot container's own direct capture where applicable

#### Scenario: Reflected preview and drag are canceled
- **WHEN** a reflected form has an active drag or live preview and its captured geometry becomes unusable
- **THEN** pointer cancellation SHALL clear that interaction and restore an uncommitted preview without retaining obsolete capture or field references

#### Scenario: Other pointers remain captured
- **WHEN** one pointer's unusable capture is canceled while a different pointer retains valid captured geometry
- **THEN** the second pointer's router capture SHALL remain active

### Requirement: Input state belongs to a live context attachment

Focus, hover and per-pointer capture SHALL reference only elements attached to the router's live
UIContext. Removing or clearing a subtree SHALL clear all input state targeting that subtree while
preserving targets in unrelated subtrees. A later attachment, in the same context or a different
context, SHALL NOT inherit the removed attachment's focus, hover, capture or pressed interaction.
MoveChild sibling reordering SHALL preserve the current attachment and its input state.

#### Scenario: Detach one of two independently captured branches

- **WHEN** two pointers capture elements in different branches and one branch is removed
- **THEN** the removed branch's focus, hover and captures are cleared before it can be destroyed
- **AND** unrelated focus, hover and other-pointer capture remain intact

#### Scenario: Reattach a retained pressed button

- **WHEN** a pressed button is removed, retained by its caller, attached again and receives UP
- **THEN** it does not click from the removed attachment's press
- **AND** a fresh DOWN and UP can establish a new click

#### Scenario: Transfer or reorder an element

- **WHEN** RemoveChild and AddChild transfer an element within or between contexts
- **THEN** old router state is cleared and destination focus/capture require fresh input
- **WHEN** MoveChild only changes sibling order
- **THEN** existing focus, hover and captures remain associated with that live attachment

#### Scenario: Explicit focus uses a foreign or detached target

- **WHEN** SetFocus is given an element outside the router's context or a detached element
- **THEN** that element does not become a focused dispatch target

### Requirement: Detach cancellation is precise and reentrant

Before invoking detach cancellation callbacks, the implementation SHALL clear all affected router
and native direct-child input references. It SHALL call OnPointerCancel on affected actual targets,
deduplicated by target and pointer id within the pending interaction batch; focus and hover SHALL contribute pointer id 0 and captures
SHALL contribute their original pointer ids. Removal SHALL NOT fabricate OnPointerLeave coordinates
or cancel unrelated ancestors/subtrees. Reset SHALL retain silent state-clearing behavior.
Pending cancellation identity SHALL survive routing-state removal until delivery, SHALL NOT keep
the target alive, and SHALL be claimed before user cancellation code runs. Nested detach operations
SHALL settle undelivered old cancellation before returning ownership or destroying their extracted
subtree. An outer cancellation batch SHALL NOT cancel fresh interaction begun after transfer.

#### Scenario: One element is focused, hovered and captured by pointer zero

- **WHEN** that element's subtree is detached
- **THEN** all three references are already cleared when cancellation runs
- **AND** that target receives a single cancellation for pointer zero and no synthetic leave event

#### Scenario: A cancellation callback mutates the tree

- **WHEN** cancellation removes another captured subtree or ClearChildren cancellation adds a child
- **THEN** pending cleanup does not access invalidated nodes or iterators
- **AND** the outer ClearChildren preserves children added after its original collection was extracted

#### Scenario: A cancellation callback transfers another pending pressed target

- **WHEN** an earlier cancellation callback transfers another pending target to a different context
- **THEN** nested removal cancels that target's old pressed state before returning it to the caller
- **AND** fresh input started after transfer survives the remaining old cancellation batch

### Requirement: Dispatch validates callback lifetime and attachment

Pointer, key, text, hover and direct-child routing SHALL revalidate object lifetime and tree
attachment after callbacks before accessing the old target, walking an ancestor or delivering to a
previously computed hit. Detach followed by immediate reattachment SHALL invalidate the old event
path even if the object survives at the same address. Direct-child routing SHALL release capture
before UP callbacks and treat WantsPointerCapture as a callback boundary. Native containers retaining
a single direct capture SHALL preserve it when an event or cancellation belongs to another pointer;
those unrelated events SHALL return unhandled without replacing or releasing the captured target.

#### Scenario: A handler removes its own ancestor

- **WHEN** a pointer, key or text handler removes and destroys its own ancestor while unhandled
- **THEN** bubbling stops without reading the destroyed element or ancestor

#### Scenario: Hover callbacks destroy current or next targets

- **WHEN** enter destroys the current hit or leave destroys the next computed hit
- **THEN** no subsequent enter or pointer delivery uses the destroyed target

#### Scenario: A handler reparents its own element

- **WHEN** a handler removes and reattaches its element during an event
- **THEN** the original event does not bubble through either stale old ancestors or new ancestors

#### Scenario: An unrelated pointer targets a direct container with capture

- **WHEN** pointer 22 has direct capture and the container receives Cancel, UP or DOWN for pointer 11
- **THEN** pointer 11 does not clear, replace or deliver through pointer 22's capture
- **AND** pointer 22 can still release its captured target normally

#### Scenario: A handler destroys its router or context

- **WHEN** pointer, key, text or hover user code destroys the active router or its UIContext
- **THEN** in-progress dispatch stops without reading the destroyed owner or following its old route

### Requirement: Router and context teardown leave no borrowed targets

Every router attached to a context SHALL observe its subtree invalidation. Router destruction SHALL
remove its context registration. Context destruction SHALL invalidate registered routers before tree
release, and a surviving router SHALL be safe and inert until its own destruction.

#### Scenario: Two routers observe one context

- **WHEN** a subtree referenced by multiple routers is detached
- **THEN** each router loses only its affected targets before any cancellation callback runs

#### Scenario: Context is destroyed first

- **WHEN** a context is destroyed while a router survives
- **THEN** router targets become null and hit testing, dispatch and Reset are safe without context access

#### Scenario: Router is destroyed first

- **WHEN** a router is destroyed and its former context later removes children
- **THEN** no notification calls into the destroyed router

### Requirement: Desired-size and arrangement invalidation propagate upward

MarkLayoutDirty SHALL invalidate the element's and ancestors' measurement entries/generations and remembered Layout parent-input keys, and mark them for arrangement and paint without recursively marking descendants. MarkArrangeDirty SHALL mark the element and ancestors for arrangement/paint without invalidating measurement. Repeated invalidation SHALL remain effective even while the branch is already dirty. Child membership/order and size-affecting setters SHALL use desired-size invalidation; scroll offsets SHALL use arrangement-only invalidation.

#### Scenario: A leaf changes while its AUTO parent is clean
- **WHEN** a controlled leaf changes intrinsic size after a completed incremental layout
- **THEN** its ancestors SHALL be eligible for remeasurement and arrangement and an unrelated unchanged sibling branch SHALL remain reusable

#### Scenario: Dirty state does not suppress a second cache invalidation
- **WHEN** a measurement is cached while a branch is dirty and that branch's size input changes again
- **THEN** a subsequent same-constraint query SHALL recompute rather than reuse the earlier result

#### Scenario: Scrolling preserves desired-size results
- **WHEN** only a controlled ScrollView or ListView offset changes
- **THEN** the next incremental layout SHALL update content placement without discarding otherwise valid desired-size entries

#### Scenario: Explicit old-bounds arrangement cannot revive a stale layout key
- **WHEN** desired size changes, an external host arranges the element at its previous bounds, and LayoutIfNeeded is then requested with the previous parent rectangle
- **THEN** the new desired size SHALL still be resolved instead of skipping with the old Layout parent-input key

### Requirement: Incremental layout has explicit compatible entries

UIElement SHALL provide LayoutIfNeeded(parentRect) and ArrangeIfNeeded(finalRect), and UIContext layout SHALL use the incremental arrangement entry for its stable internal root. A branch MAY be skipped only when its relevant previous input is unchanged, arrangement is clean, and its subtree contains no volatile element. Layout SHALL compare parent-rectangle inputs; Arrange SHALL compare final-rectangle inputs. Existing external Layout/Arrange SHALL remain forced entries; nested container calls within an incremental pass SHALL participate in incremental checks.

#### Scenario: Unchanged controlled tree skips layout work
- **WHEN** a controlled tree completes an incremental pass and the same allocation is requested without mutation
- **THEN** its measurement/arrangement hooks SHALL NOT run again solely because another frame begins

#### Scenario: A changed allocation reaches descendants
- **WHEN** a parent rectangle or parent-owned final rectangle changes
- **THEN** the affected branch SHALL update descendant allocations before return without requiring blanket descendant dirty propagation

#### Scenario: Explicit existing host call remains forced
- **WHEN** an external host calls existing Layout or Arrange twice with the same rectangle
- **THEN** its requested arrangement hooks SHALL execute on both calls

#### Scenario: Stable sibling of volatile content can skip
- **WHEN** a parent executes because one child branch is volatile while another child branch is controlled and unchanged
- **THEN** the unchanged child's nested incremental layout MAY skip independently

### Requirement: Volatile layout protects unnotified dependencies

UIElement SHALL be layout-volatile by default. Controlled native widgets SHALL explicitly opt out through SetLayoutVolatile(false), and opting out SHALL require notification of size/allocation-affecting state. Ancestors SHALL maintain the count of volatile nodes in their live subtree across policy changes, add/remove/clear, and reattachment. A nonzero count SHALL disable branch skipping and measurement reuse for that branch. Text with a nonempty localization key SHALL remain volatile; clearing its key SHALL restore its controlled policy. Legacy/editor polling and externally mutable theme consumers SHALL remain compatible without a global revision redesign.

#### Scenario: Legacy intrinsic size changes without a setter
- **WHEN** a default-volatile custom widget changes externally observed Measure output between incremental frames
- **THEN** the next pass SHALL observe the new output rather than reuse a cached ancestor measurement

#### Scenario: Removing a volatile descendant restores reuse
- **WHEN** the last volatile descendant is detached from an otherwise controlled branch
- **THEN** the old ancestor counts SHALL update and the branch SHALL become eligible for reuse after its invalidated work completes

#### Scenario: Reattachment transfers volatile ancestry
- **WHEN** a detached volatile subtree is attached to another controlled parent
- **THEN** the new ancestor chain SHALL account for its volatile nodes without retaining membership in the old chain

#### Scenario: Localized Text follows external locale changes
- **WHEN** a Text with a localization key is laid out, the active locale/table changes, and incremental layout runs again
- **THEN** its resolved desired size and allocation SHALL follow the new text without a separate Text setter call

### Requirement: Stable measurement queries use a bounded normalized cache

MeasureDesired MAY reuse measurements only for branches containing no volatile node. Its cache SHALL retain at most four results keyed by normalized minimum/maximum width/height and the current measurement generation. Hits and misses SHALL preserve the common constraint normalization/clamping and legacy adaptation contract. The common query framework and built-in measurements SHALL NOT change bounds, child membership/order, scroll offsets/arranged extent, or dirty flags. A dependency invalidation raised defensively by a custom measurement callback SHALL prevent storing a result for an obsolete generation.

#### Scenario: Equivalent normalized constraints reuse a result
- **WHEN** a controlled branch is queried repeatedly with equivalent normalized constraints and unchanged generation
- **THEN** the common entry MAY reuse its constrained desired size without invoking OnMeasure again

#### Scenario: Distinct budgets retain distinct desired sizes
- **WHEN** width-dependent content is queried under different normalized widths
- **THEN** a cached result for one budget SHALL NOT be returned for a different budget

#### Scenario: Query does not consume pending layout
- **WHEN** a dirty controlled element is measured repeatedly and a cache hit occurs
- **THEN** its bounds, tree, scroll state, and pending layout/paint flags SHALL remain unchanged

#### Scenario: Constraint history remains bounded
- **WHEN** more than four distinct normalized budgets are queried
- **THEN** retained measurement entries SHALL remain bounded to four and any recomputed result SHALL retain the common query semantics

### Requirement: Geometry assignment and callback invalidation remain distinct

Arrange SHALL assign its final bounds internally without invalidating ancestor measurement. Public SetBounds SHALL request arrangement/paint as needed. Changed geometry SHALL flow to descendants through actual allocation calls. Arrange SHALL retain any invalidation raised during its child arrangement callback rather than clearing it unconditionally at return. Layout SHALL also preserve invalidation raised during its measurement stage before Arrange begins. Render transforms SHALL invalidate paint without requesting layout.

#### Scenario: Assigning a child slot does not invalidate parent measurement
- **WHEN** a parent arranges a child into its final slot during a stable incremental pass
- **THEN** that geometry assignment SHALL NOT itself discard the parent's desired-size entries

#### Scenario: Callback requests another layout pass
- **WHEN** ArrangeChildren changes the live tree or otherwise raises layout invalidation
- **THEN** the invalidation SHALL remain observable after the current arrangement returns until a later pass consumes it

#### Scenario: Custom measurement invalidates before arrangement starts
- **WHEN** a custom OnMeasure callback raises layout invalidation during Layout before its subsequent Arrange call begins
- **THEN** Layout SHALL preserve that invalidation after returning even if Arrange itself raises no new invalidation

#### Scenario: Virtual rows settle after membership changes
- **WHEN** a ListView arrangement creates or removes live rows
- **THEN** a subsequent pass SHALL be allowed to settle the new branch without losing invalidation or recreating an unchanged visible range

#### Scenario: Transform changes rendered geometry only
- **WHEN** only an element's render transform changes after a completed layout
- **THEN** its absolute layout bounds SHALL remain valid and the next paint/input geometry SHALL use the changed transform without layout invalidation

### Requirement: Native setters notify newly incremental behavior

Controlled native widgets SHALL notify changes that affect their measurement or allocation before subsequent incremental layout. Setting a ListView item factory SHALL discard existing children/live-row metadata and request desired-size/layout invalidation so the next arrangement rebuilds rows through the new factory. Changes that only affect draw appearance SHALL not unnecessarily discard stable measurements. Direct custom/editor UIElement subclasses SHALL retain the conservative volatile default unless they explicitly opt in; a subclass of a controlled native widget SHALL restore volatility if it introduces dependencies without notification.

#### Scenario: A late list factory materializes rows
- **WHEN** an arranged ListView with a nonempty item range receives its item factory after the first layout
- **THEN** the next incremental pass SHALL materialize and arrange the visible rows

#### Scenario: A replacement factory rebuilds live rows
- **WHEN** a ListView with materialized rows receives a different item factory
- **THEN** existing row children SHALL be discarded and the next incremental arrangement SHALL construct the visible range using the replacement factory

#### Scenario: Paint-only property does not request measurement
- **WHEN** a controlled widget changes only its color, opacity, hover, or pressed appearance
- **THEN** its otherwise valid constrained desired-size entries SHALL remain reusable

### Requirement: Pure normalized size constraints

UI.Interface SHALL expose pure UISize/UIConstraints min/max values and normalization, deflation, finite-result Constrain, and ResolveMeasureConstraints utilities in engine/ui without plugin/runtime/render dependencies. Positive infinity SHALL be allowed as an unbounded maximum. Negative, NaN, or infinite minima SHALL normalize to zero; negative/NaN maxima SHALL normalize to zero. A minimum greater than its maximum SHALL be lowered rather than increasing the hard maximum. Negative/NaN desired components SHALL become zero before clamping; positive-infinite desired components SHALL become a finite maximum if present or the finite minimum if unbounded. Constrain SHALL always return finite components.

#### Scenario: Contradictory bounds preserve the hard maximum
- **WHEN** an axis has minimum 100 and maximum 20
- **THEN** normalization SHALL lower its minimum to 20 and SHALL NOT increase its maximum

#### Scenario: Invalid constraints are normalized
- **WHEN** constraint minima or maxima contain negative/NaN values, or a minimum is infinite
- **THEN** invalid components SHALL normalize to zero while a positive-infinite maximum remains unbounded

#### Scenario: Infinite desired output is finite
- **WHEN** desired output is positive infinity
- **THEN** Constrain SHALL use the finite maximum when available or the finite minimum when unbounded

#### Scenario: Constraint utility is interface-only
- **WHEN** a consumer includes the size/constraint utilities through UI.Interface
- **THEN** it SHALL use those pure operations without linking UI widget or render implementation

### Requirement: Padding budgets are bounded

Constraint deflation and layout ContentRect SHALL treat negative or non-finite padding as zero. Deflation SHALL reduce normalized available extents without negative or undefined results. Excessive padding SHALL produce an empty content rectangle within the original bounds rather than an inverted or out-of-bounds rectangle.

#### Scenario: Excessive padding remains inside the element
- **WHEN** padding exceeds an element's arranged extent
- **THEN** its content rectangle SHALL be empty and remain within that element's bounds

#### Scenario: Deflating an unbounded maximum stays defined
- **WHEN** a normalized unbounded budget is deflated by sanitized finite padding
- **THEN** its maximum SHALL remain unbounded without producing NaN

### Requirement: Common desired-size query enforces constraints

UIElement::MeasureDesired(constraints) SHALL be a non-virtual entry that normalizes constraints, calls protected OnMeasure, and hard-clamps/sanitizes its returned outer desired size. OnMeasure SHALL include the element's own padding in its outer desired result; the common wrapper SHALL NOT add padding again. The default UIElement::OnMeasure SHALL adapt legacy Measure(float&, float&) in one direction for custom widgets inheriting that hook. Migrated Text/HBox/VBox/StackPanel/DockPanel/GridPanel/Overlay/ScrollView/ListView classes SHALL make their legacy Measure wrapper final and delegate it to unbounded MeasureDesired. Subclasses of those native classes SHALL customize OnMeasure rather than legacy Measure, so supported old/new queries use one derived hook without recursion. This source migration SHALL be explicit; legacy-only native subclass overrides SHALL be rejected at compile time.

#### Scenario: Callback output cannot bypass a tight budget
- **WHEN** an OnMeasure callback returns size 500 under a tight axis budget of 100
- **THEN** MeasureDesired SHALL return 100 on that axis

#### Scenario: Default bridge preserves a legacy custom widget
- **WHEN** a custom UIElement subclass inherits the default OnMeasure and overrides only legacy Measure
- **THEN** the default bridge SHALL use that result as outer desired size and the common entry SHALL enforce constraints

#### Scenario: Migrated native subclass uses the same hook through both entries
- **WHEN** a Text or migrated container subclass overrides OnMeasure and is queried through legacy Measure or MeasureDesired
- **THEN** both entries SHALL invoke the same derived hook, the constrained query SHALL enforce its hard budget, and neither entry SHALL recurse

#### Scenario: Ambiguous legacy native subclass requires migration
- **WHEN** a subclass of a migrated native widget attempts to override only legacy Measure
- **THEN** the final wrapper SHALL cause a compile-time override error rather than silently bypassing the override during constrained measurement

#### Scenario: Native subclass cache follows its existing notification policy
- **WHEN** a retained native subclass notifies a measurement-input change with MarkLayoutDirty
- **THEN** old and constrained queries SHALL observe the new result instead of retaining an earlier cached desired size

### Requirement: Rule-aware measurement distinguishes requests from slots

MeasureForLayout and Layout SHALL use pure rule resolution to make finite known Fixed/Fill/Percent/anchor axes tight before measuring width-dependent content. Ordinary explicit Fixed size SHALL be allowed to exceed parent available size. A parent-owned tight slot SHALL instead remain authoritative through MeasureDesired and final Arrange, without reapplying the child's own layout rule. Fill/Percent on an unbounded reference axis SHALL fall back to finite intrinsic measurement.

#### Scenario: Fixed width reaches the callback before height measurement
- **WHEN** a width-dependent child declares a fixed width of 120
- **THEN** its rule-aware measurement callback SHALL receive a tight width of 120 before computing desired height

#### Scenario: Ordinary Fixed can exceed available size
- **WHEN** ordinary Layout resolves a fixed width of 500 in a parent with available width 200
- **THEN** its fixed request SHALL remain 500 unless a parent-owned hard slot overrides that request

#### Scenario: Final slot overrides the child's own rule
- **WHEN** a container measures a child with a tight width of 100 and arranges that final slot while the child declares Fixed 500
- **THEN** the child's measured allocation and final arranged width SHALL be 100 without another own-rule resolution

#### Scenario: Unbounded Fill and Percent remain finite
- **WHEN** a Fill or Percent rule has no finite reference extent
- **THEN** it SHALL use intrinsic measurement and SHALL NOT return an infinite desired size

### Requirement: Measurement does not mutate layout state

Built-in measurement queries SHALL NOT assign bounds, arrange children, modify scroll offsets or stored arranged extents, create/remove/reorder elements, or mark/clear dirty flags. This requirement SHALL NOT introduce a geometry/measurement cache or invalidation redesign.

#### Scenario: Repeated query preserves state
- **WHEN** an arranged subtree is measured repeatedly under different constraints
- **THEN** its bounds, child membership/order, scroll state, and dirty flags SHALL remain unchanged

### Requirement: Containers propagate available budgets

HBox/VBox/StackPanel/DockPanel/GridPanel/Overlay SHALL propagate constraints through their content areas and include padding once in outer desired size. Linear allocation SHALL reserve non-Fill child extents and spacing before assigning nonnegative remaining Fill space. Slot-owning containers SHALL measure against their slot budgets and call child Arrange on final rectangles without another Layout resolution.

#### Scenario: Fill uses remaining space
- **WHEN** a linear container has preceding Fixed/Auto children, spacing, and a Fill child under finite width
- **THEN** the Fill allocation SHALL use only the remaining nonnegative space and SHALL NOT overlap preceding allocations

#### Scenario: Grid and nested content see cell budgets
- **WHEN** a grid measures nested width-dependent content under finite bounds
- **THEN** children SHALL receive their cell budgets rather than only an unconstrained intrinsic query

### Requirement: Editor custom content uses actual column width

AssetViewer SHALL measure custom content with the actual allocated column width after relevant padding and scrollbar reservation. Its content height and final Arrange rectangle SHALL correspond to that width rather than an unconstrained intrinsic query.

#### Scenario: Scrollbar reservation changes wrapped height
- **WHEN** the viewer's scrollbar reservation narrows width-dependent custom content
- **THEN** the height used for arrangement/scrolling SHALL come from measuring that resulting content width

### Requirement: Explicit logical allocation preserves measurement budgets

UIElement SHALL retain a finite nonnegative logical size assigned only by SetBounds, Arrange, or rule-aware Layout, independent of float positioned endpoint subtraction. Layout/Arrange and their incremental entries SHALL accept an explicit logical parent/allocation size. Rect-only Layout SHALL use actual endpoint differences as its parent reference budget and still resolve the element's own layout rules. Rect-only Arrange and SetBounds SHALL derive their final allocation from endpoint differences rather than an earlier desired size or Fixed request. Native containers SHALL derive content budgets through pure logical padding deflation and preserve child logical allocations before adding their absolute position. Measurement queries SHALL NOT mutate this allocation output.

#### Scenario: Fractional final slot preserves its assigned budget
- **WHEN** a slot owner passes a logical size of width 12.5 to Arrange alongside positioned float bounds
- **THEN** the child's arranged logical size SHALL remain 12.5 even if subtracting those bounds yields a different width
- **AND** its content budget SHALL use pure deflation of 12.5

#### Scenario: Rect-only allocation remains an actual slot
- **WHEN** a caller supplies only a final rectangle to Arrange or SetBounds
- **THEN** logical size SHALL be derived from the rectangle's actual endpoint differences rather than an earlier desired size or a Fixed request

#### Scenario: Identical positioned endpoints retain distinct logical budgets
- **WHEN** two explicit allocations have identical positioned float rectangles but different logical sizes
- **THEN** incremental arrangement SHALL reevaluate the new allocation rather than skipping solely because its rectangle is unchanged

#### Scenario: Incremental layout includes parent logical size
- **WHEN** an unchanged parent rectangle is supplied to incremental Layout with a changed logical parent size
- **THEN** the child SHALL resolve and measure against that changed budget before arranging

#### Scenario: Query cannot replace the current allocation
- **WHEN** an arranged Text or container is measured with an unrelated width budget
- **THEN** the stored arranged logical size, positioned bounds, and subsequent paint budget SHALL remain unchanged

#### Scenario: Native no-argument force reflow preserves its allocation
- **WHEN** DockPanel, GridPanel, or StackPanel's no-argument Layout wrapper forces a new pass
- **THEN** it SHALL preserve the existing positioned bounds and logical allocation size while reevaluating descendants

#### Scenario: Nested containers preserve logical widths
- **WHEN** HBox, VBox, StackPanel, DockPanel, GridPanel, Overlay, ScrollView, ListView, or the default UIElement arranges children at fractional origins
- **THEN** their child width/height budgets SHALL come from the pre-positioning logical allocation rather than subtraction of absolute edges

### Requirement: Aurora-backed UI render module

The UI render implementation SHALL live in `engine/ui/render`, SHALL retain the `UIRender` target and `ui/render/...` public include paths, and SHALL use the `sky::aurora` namespace for Aurora types. It SHALL consume `UIDrawData` and `IUITextureRegistry` through `UI.Interface` and SHALL link `UI.Interface`, `Aurora.RHI.Static`, and `Aurora.ShaderCompiler.Static`, without requiring the `UI` widget runtime or `Aurora.Pipeline.Static`. Neither `UI` nor `UI.Interface` SHALL link or include the Aurora UI renderer implementation. This restructuring SHALL preserve the existing Aurora graphics-encoder integration; the direct graphics-encoder integration SHALL remain available independently of frame graph features.

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

### Requirement: UI 通过 Aurora encoder 与宿主图集成

UIRenderer SHALL translate UIDrawData through Aurora GraphicsEncoder and ResourceGroup interfaces. Production host composition MAY provide a registered FrameCustomPassEncoder with declared resources; the UI module SHALL NOT depend on the removed PipelinePass, RenderGraph or DrawItem API. Native Vulkan/DX12/Metal commands SHALL remain behind Aurora backends.

#### Scenario: Direct renderer remains usable
- **WHEN** a host supplies a valid graphics encoder and UIDrawData
- **THEN** UIRenderer records the indexed batches without constructing a legacy graph

#### Scenario: Host graph integration
- **WHEN** a host composes UI with scene output in a FrozenFrameGraph
- **THEN** the host owns graph declarations and frame retirement; UIRenderer does not construct a second graph

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

The current UI renderer SHALL project pixel coordinates on the CPU and bind per-batch textures/samplers in set 3. It SHALL NOT require an unused Global/Pass/Material cbuffer or the removed BatchPackWriter. Future uniform inputs SHALL use the shared declaration and target UniformUploadPlan contract, with Global 0 / Pass 1 / Material 2 / Batch 3 ownership.

#### Scenario: CPU projection
- **WHEN** draw vertices are uploaded for a surface size and backend clip-space convention
- **THEN** positions are transformed consistently without an implicit Global uniform block

#### Scenario: Texture batch
- **WHEN** the texture changes between UI draw commands
- **THEN** the renderer binds the matching Batch set 3 image/sampler after the required pipeline

### Requirement: Textures and samplers via resource groups

The render module SHALL bind UI textures and samplers through Aurora resource groups and descriptor writes,
not through uniform blocks.

#### Scenario: Atlas texture bound
- **WHEN** a draw command references an atlas texture handle
- **THEN** the corresponding Aurora image and sampler SHALL be bound for that draw

### Requirement: Backend-agnostic with Vulkan first

The UI render pass SHALL be written against Aurora interfaces only and SHALL compile its shaders to the target of
the active backend (SPIR-V for Vulkan, DXIL for DX12, MSL for Metal), so it runs on any backend whose Aurora
capabilities have landed.

#### Scenario: Runs on Vulkan
- **WHEN** the engine runs with the Vulkan backend and a UI tree is painted
- **THEN** the UI pass SHALL execute with SPIR-V bytecode and produce visible output

#### Scenario: Runs on DX12
- **WHEN** the engine runs with the DX12 backend and a UI tree is painted
- **THEN** the UI shader SHALL be compiled to DXIL and the UI pipeline state SHALL be created successfully

#### Scenario: Upright on every backend
- **WHEN** the UI is painted on a backend whose clip-space Y axis differs from another's
- **THEN** the UI SHALL be upright (pixel y=0 at the framebuffer top) on both, with the projection's Y sign taken
  from the device's clip-space Y axis

#### Scenario: Unsupported backend degrades gracefully
- **WHEN** the active backend lacks a required Aurora capability
- **THEN** the UI pass SHALL report the limitation without crashing

### Requirement: Frame data lifetime

The render module SHALL double- or triple-buffer per-frame UI GPU data so the render thread can consume one
frame while the main thread produces the next, consistent with `DeviceFrameContext` in-flight frame semantics.

#### Scenario: No CPU/GPU hazard across frames
- **WHEN** the main thread writes the next frame's UI data while the previous frame's GPU work is in flight
- **THEN** the in-flight frame's buffers SHALL NOT be overwritten

### Requirement: Rounded shapes evaluate layout-space distance

The Aurora UI renderer's rounded-shape path SHALL evaluate its signed-distance function using interpolated original layout coordinates carried in the shape quad's UV, together with the authored layout-space center, half-size, and radius, rather than comparing framebuffer position with layout-space shape data. Transformed shape vertices SHALL retain consistent shape-local distance calculations. Ordinary textured quads SHALL retain their existing texture UV semantics. The actual shader source used by runtime compilation SHALL also be consumable by offline compilation validation without a separately copied test shader.

#### Scenario: Transformed rounded shape uses coherent coordinates
- **WHEN** a rounded shape is translated, rotated, or scaled through UI geometry
- **THEN** its shader SHALL evaluate the shape distance using the corresponding interpolated layout position and authored layout-space radius/extent

#### Scenario: Texture UVs remain texture coordinates
- **WHEN** an ordinary textured quad is emitted
- **THEN** its UV values SHALL continue to sample the requested texture region rather than be interpreted as a shape-layout position

#### Scenario: Offline validation compiles runtime source
- **WHEN** the available shader compiler validates the UI shader offline
- **THEN** it SHALL compile the actual source artifact consumed by the runtime renderer, including the rounded-shape entry point

### Requirement: Scrollable container

ScrollView SHALL clip its content and expose ScrollAxes Both, Horizontal, Vertical, and None, with Both as the default. Content measurement SHALL permit full unbounded AUTO extent on scrolling axes and SHALL hard-bound non-scrolling axes to the viewport. An AUTO viewport query SHALL first measure under the available budget and clamp a candidate outer viewport. Vertical, Both, and None SHALL retain its candidate width and remeasure dependent height at the resulting actual inner width; Horizontal SHALL retain its candidate height and remeasure width at the resulting actual inner height, avoiding repeated Percent-driven shrinking of the frozen axis. Content Fixed/Percent/Fill rules SHALL resolve once against the viewport reference for each query; the final content rectangle SHALL then be assigned through Arrange without a second Layout resolution. Stored content extent SHALL equal the final allocated content size, and enabled scroll offsets SHALL be clamped to max(0, contentExtent - viewportExtent). Disabled axes SHALL NOT retain scroll offset. Wheel input SHALL change vertical offset when vertical scrolling is enabled. Measurement SHALL NOT change offsets, arranged extent, bounds, or tree state. Removing/clearing content through inherited child operations SHALL clear the cached content pointer.

#### Scenario: Scroll clamps to content
- **WHEN** the scroll offset is set beyond the content bounds and the view is arranged
- **THEN** it SHALL be clamped using the same final content extent as the arranged content rectangle

#### Scenario: Wheel scrolls
- **WHEN** a wheel event is delivered to a view with vertical scrolling enabled
- **THEN** the vertical offset SHALL change by the wheel delta and SHALL clamp during arrangement

#### Scenario: Content is clipped
- **WHEN** content is larger than the view
- **THEN** drawn content SHALL be clipped to the view bounds

#### Scenario: Vertical scrolling constrains wrapping width
- **WHEN** a Vertical view measures AUTO wrapping content
- **THEN** the width SHALL be bounded by the viewport and the height SHALL be measured as a full unbounded scroll extent

#### Scenario: Non-scroll axis is a hard viewport maximum
- **WHEN** a child declares Fixed width 500 in a Vertical view of width 200
- **THEN** its final measured/arranged width SHALL be no greater than 200 while height can extend on the scrolling axis

#### Scenario: AUTO viewport retains its allocated wrapping width
- **WHEN** a Vertical, Both, or None view chooses an AUTO viewport width from content with a Percent width rule
- **THEN** it SHALL freeze the first clamped candidate width and measure dependent height at that actual inner width rather than repeatedly shrinking the viewport with the Percent rule

#### Scenario: Horizontal AUTO viewport retains its allocated height
- **WHEN** a Horizontal view chooses an AUTO viewport height
- **THEN** it SHALL freeze the first clamped candidate height and remeasure width at that actual inner height

#### Scenario: Content rules use the viewport once
- **WHEN** content declares Percent or Fill size
- **THEN** that rule SHALL use the viewport as its reference once and the resulting final rectangle SHALL NOT resolve it a second time

#### Scenario: ScrollView owns final content placement
- **WHEN** content declares anchors or offsets and its extent has been resolved against the viewport
- **THEN** its final origin SHALL be view.left minus scrollX and view.top minus scrollY, without reapplying content positioning offsets

#### Scenario: Both and None have explicit policies
- **WHEN** Both or None is selected
- **THEN** Both SHALL allow full AUTO extent on both scrolling axes, while None SHALL hard-bound both axes to the viewport and disable both offsets

#### Scenario: Removed content is safe
- **WHEN** the content child is removed or children are cleared through inherited operations
- **THEN** the next measure/arrange SHALL NOT access a stale cached content pointer

### Requirement: Virtualized list

ListView SHALL show fixed-height items and materialize only the visible range for a given arranged viewport/scroll offset. Measurement SHALL use the full item-count extent independently of currently live rows, subject to caller constraints, and SHALL NOT invoke the factory, change live items, or alter scroll state. Arrangement SHALL create/remove visible items and assign their final item rectangles through Arrange without reapplying child layout rules.

#### Scenario: Only visible items exist
- **WHEN** a list with many items is laid out in a short view
- **THEN** the number of live item elements SHALL correspond to the visible range rather than the item count

#### Scenario: Scrolling changes the visible range
- **WHEN** the list is scrolled down by several item heights and arranged
- **THEN** materialized indices SHALL shift to cover the newly visible range

#### Scenario: Measurement creates no rows
- **WHEN** a list with an item factory is measured before first arrangement
- **THEN** measurement SHALL report its fixed-item extent under the budget without invoking the factory or creating children

#### Scenario: Visible rows respect final slots
- **WHEN** a visible item declares conflicting own layout rules
- **THEN** its arranged bounds SHALL still equal the item rectangle allocated by the list

### Requirement: DPI scaling

`UIContext` SHALL apply a DPI scale to the logical content size, so layout and painting operate in scaled
device coordinates.

#### Scenario: Scale applied to content
- **WHEN** the content size is 100 by 100 and the DPI scale is set to 2
- **THEN** the root bounds SHALL be 200 by 200

#### Scenario: Reset scale
- **WHEN** the DPI scale is set back to 1
- **THEN** the root bounds SHALL return to the logical size

### Requirement: Localization

The UI system SHALL provide per-locale string tables, a current locale, and key translation that falls back to
the key when a translation is missing.

#### Scenario: Translate in current locale
- **WHEN** a key has a value in the current locale
- **THEN** translation SHALL return that value

#### Scenario: Missing key falls back
- **WHEN** a key has no value in the current locale
- **THEN** translation SHALL return the key itself

### Requirement: Localized text

A `Text` widget SHALL be able to reference a localization key and resolve it when measuring and painting, so a
locale change is reflected without rebuilding the tree.

#### Scenario: Key resolves to text
- **WHEN** a text widget references a key present in the current locale
- **THEN** its measured/painted text SHALL be the translated value

#### Scenario: Locale change updates text
- **WHEN** the locale changes and the widget is measured again
- **THEN** the resolved text SHALL reflect the new locale

### Requirement: Font registration behind a provider seam

The UI text system SHALL register fonts through an `IUIFontProvider` abstraction exposed by `UI.Interface`, so the core does not depend on a specific font rasterizer. The core SHALL ship a built-in fallback provider, and a FreeType-backed provider SHALL be usable through the optional `UI.FreeType` adaptor when `SKY_BUILD_FREETYPE` is enabled, without making FreeType or the legacy render module a hard dependency of core.

#### Scenario: Register a font
- **WHEN** a font is registered with a family name and size
- **THEN** subsequent text requests for that family and size SHALL resolve to the registered font

#### Scenario: FreeType provider when available
- **WHEN** a FreeType-backed provider from `UI.FreeType` is installed in a build with `SKY_BUILD_FREETYPE` enabled
- **THEN** glyph rasterization SHALL go through that provider

#### Scenario: Core works without FreeType
- **WHEN** the UI core is built with `SKY_BUILD_FREETYPE` disabled
- **THEN** text SHALL still function using the built-in fallback provider

### Requirement: Glyph atlas pages

The text system SHALL rasterize glyphs into atlas pages and register each atlas page with the UI texture
registry, returning a stable texture handle for use in draw commands.

#### Scenario: New glyph added to atlas
- **WHEN** a glyph not yet present is requested
- **THEN** it SHALL be rasterized and packed into an atlas page, and the atlas page SHALL have a registered texture handle

#### Scenario: Atlas page reused
- **WHEN** the same glyph is requested again
- **THEN** the cached atlas entry SHALL be reused without re-rasterizing

#### Scenario: Atlas growth
- **WHEN** the active atlas page has no room for a new glyph
- **THEN** a new atlas page SHALL be created and registered, and existing handles SHALL remain valid

### Requirement: Text measurement

The text system SHALL measure a string and report its bounds for a given font/size using a line-layout algorithm shared with emission. Existing no-wrap calls SHALL preserve explicit-newline and glyph-advance behavior. An optional soft-wrap query SHALL use finite available logical content width to insert byte-wise breaks, preserve explicit newlines, and make progress even when one glyph is wider than the line. Text paint SHALL use the arranged logical allocation deflated by the same pure padding operation as measurement, without reconstructing its budget from positioned float edges. Overflow comparison MAY allow small width-relative arithmetic rounding but SHALL NOT expand tolerance based on absolute origin. A zero budget SHALL remain zero and a narrower logical width beyond arithmetic tolerance SHALL still wrap, even if its positioned endpoints round to the same coordinates as a fitting allocation. Unbounded width SHALL not cause soft wrapping. Full Unicode shaping and language-aware word breaking SHALL remain out of scope.

#### Scenario: Measure single line
- **WHEN** a single-line string is measured without soft wrapping
- **THEN** its width SHALL equal the sum of glyph advances and its height SHALL equal the font line height

#### Scenario: Measure multi-line
- **WHEN** a string containing a newline is measured
- **THEN** its height SHALL account for the explicit lines

#### Scenario: Finite width adds soft lines
- **WHEN** soft wrapping is enabled and the next glyph overflows a nonempty finite-width line beyond width-relative arithmetic rounding
- **THEN** the shared layout SHALL begin a new line before that byte and include the new line in its height

#### Scenario: Fractional origin preserves an exact fitting line
- **WHEN** two glyph advances of 6.1 fit a logical width of 12.2 and fractional placement at origin 500.1 reduces the width obtained by subtracting float edges
- **THEN** Text painting SHALL use its logical content allocation and keep both glyphs on the measured line

#### Scenario: Fractional padding preserves an exact fitting line
- **WHEN** Fixed outer width 12.5, origin 500.1, padding left 0.1 and right 0.2 produce a fitting logical content width but a smaller positioned ContentRect width
- **THEN** Text SHALL preserve the measured line through its explicitly retained logical allocation

#### Scenario: Meaningfully narrower width still wraps
- **WHEN** the same glyphs are measured and painted with a content width of 12.19998 or 12.1 at positive or negative fractional origins
- **THEN** the second glyph SHALL start a new line in both desired height and emitted geometry

#### Scenario: Zero width still consumes input
- **WHEN** positive-advance glyphs are wrapped with zero available content width
- **THEN** each byte SHALL be consumed and each oversized glyph SHALL occupy its own line regardless of origin

### Requirement: Text layout to draw data

The text system SHALL produce render-agnostic glyph draw data containing position, uv, color, atlas texture handle, and clip rect, consistent with UI core. Text measurement and emission SHALL use the same line-layout algorithm and content width. Each line SHALL be aligned independently within the content rectangle; ordinary UiDraw/no-wrap entry points SHALL preserve their previous behavior.

#### Scenario: Text emits quads
- **WHEN** text is laid out and painted
- **THEN** each visible glyph SHALL contribute geometry referencing its atlas texture handle

#### Scenario: Text respects clip
- **WHEN** text is laid out inside a clipped container
- **THEN** its draw commands SHALL use the container's clip rect

#### Scenario: Measured and painted wrapped lines agree
- **WHEN** a Text is measured and painted with the same finite content width and wrap setting
- **THEN** emitted line breaks and layout height SHALL correspond to the measured line layout

#### Scenario: Fractional placement preserves an exactly fitting line
- **WHEN** glyph advances exactly fit the logical wrap width and subtraction of fractional arranged edges introduces only floating-point rounding error
- **THEN** painting SHALL preserve the measured line break, while a meaningful reduction in available width SHALL still produce a soft break

#### Scenario: Each line aligns independently
- **WHEN** multiline Text uses Center or Right alignment
- **THEN** each line SHALL be positioned from its own measured width within the padded content rectangle

### Requirement: Bitmap atlas scope

This milestone SHALL support bitmap glyph atlases and Latin text. Non-Latin shaping, bi-directional text,
and signed-distance-field atlases SHALL be out of scope.

#### Scenario: Unsupported shaping is not required
- **WHEN** only Latin text is laid out in this milestone
- **THEN** no shaping or bi-directional library SHALL be a build dependency of the text system

### Requirement: Built-in fallback provider

The core SHALL provide a built-in `IUIFontProvider` that needs no external font file or plugin, so text
measurement and layout function in any build. It SHALL produce deterministic metrics so measurement is exact.

#### Scenario: Text works without a font plugin
- **WHEN** text is measured or laid out using the built-in provider
- **THEN** it SHALL produce non-zero, deterministic bounds without loading a font file

### Requirement: Text widget

The UI system SHALL provide a Text widget holding content, font size, color, alignment, and a default-false wrap flag. Its OnMeasure SHALL use the supplied budget after deflating own padding, then return outer desired size including padding for common hard clamping. Paint SHALL use the actual padded content width with the same line-layout rules. Migrated legacy Measure SHALL delegate through its own OnMeasure implementation without recursion, and a missing text system SHALL produce no glyph draw data.

#### Scenario: Auto size from content
- **WHEN** an unconstrained AUTO Text is measured with zero padding
- **THEN** its desired size SHALL equal the measured text bounds

#### Scenario: Paint emits glyphs
- **WHEN** a configured Text is painted
- **THEN** it SHALL emit draw data referencing its atlas texture handle

#### Scenario: No text system configured
- **WHEN** an unconstrained zero-padding Text is measured or painted without a text system
- **THEN** it SHALL report zero content size and emit no glyph draw data

#### Scenario: Padding is included once
- **WHEN** a wrapping Text is measured under finite outer width with padding
- **THEN** its line layout SHALL use the deflated content width and its outer desired size SHALL include padding once before hard clamping

#### Scenario: Existing Text does not wrap implicitly
- **WHEN** no wrap flag is supplied
- **THEN** Text SHALL retain no-soft-wrap behavior

### Requirement: Optional FreeType provider

The UI foundation SHALL provide a FreeType-backed `IUIFontProvider` in `engine/ui/freetype` as target `UI.FreeType`, built only when `SKY_BUILD_FREETYPE` is enabled. Provider callers SHALL explicitly link that target; the `UI` runtime core SHALL NOT link FreeType or the adaptor. The provider SHALL load font bytes through the core file system and SHALL NOT depend on the legacy render text stack. The existing `ui/freetype/...` include paths SHALL remain available from `UI.FreeType`.

#### Scenario: FreeType glyphs when enabled
- **WHEN** a caller links `UI.FreeType` in a build with `SKY_BUILD_FREETYPE` on and a valid font file is loaded
- **THEN** requesting a glyph SHALL return a non-empty coverage bitmap and positive advance

#### Scenario: Provider absent when disabled
- **WHEN** `SKY_BUILD_FREETYPE` is off
- **THEN** the provider and its tests SHALL NOT be built, and the built-in provider SHALL still be available

#### Scenario: Core stays independent when the adaptor is enabled
- **WHEN** `SKY_BUILD_FREETYPE` is on and a caller links only `UI`
- **THEN** the caller SHALL be able to use the fallback text system without acquiring a `UI.FreeType` or FreeType dependency through `UI`

### Requirement: Borrowed atlas registry lifetime

UIFontAtlas and UITextSystem SHALL support paired attachment and detachment of borrowed texture registries. The owner SHALL detach a registry before destroying it. Detachment SHALL stop all future glyph update and page registration calls to that registry without invoking it or releasing its textures. Duplicate attachment and unknown/null detachment SHALL be harmless.

#### Scenario: Detached renderer is destroyed
- **WHEN** a secondary atlas registry is detached and destroyed and new glyphs or pages are subsequently requested
- **THEN** only still-attached registries SHALL receive page registrations and updates
- **AND** cached glyph/page handles SHALL remain valid

#### Scenario: Registry is reattached
- **WHEN** a previously detached registry is attached again
- **THEN** all current pages SHALL be replayed under their existing ids exactly once
- **AND** a retained backend page SHALL receive current atlas pixels before cached glyphs are used
- **AND** duplicate attachment SHALL NOT replay pages or duplicate future update calls

#### Scenario: Default registry is removed
- **WHEN** UITextSystem's default registry is detached
- **THEN** GetRegistry SHALL return null until another registry is explicitly attached
- **AND** removing a secondary registry SHALL preserve an existing default

#### Scenario: Editor surface cleanup
- **WHEN** the editor explicitly destroys a floating surface, processes its OS-close, or shuts down all surfaces
- **THEN** each renderer SHALL be detached from the text system before that renderer is destroyed

### Requirement: Theme-driven widget painting

Widgets SHALL resolve their appearance from the active `UITheme` through their style classes during paint,
rather than from hard-coded constants. The paint context SHALL expose the active theme to widgets, and a theme
replacement SHALL affect subsequently painted widgets.

#### Scenario: Theme color used for paint
- **WHEN** a widget with a style class is painted and the theme maps that class to a color
- **THEN** the emitted draw data SHALL use the theme's color

#### Scenario: Theme replacement affects paint
- **WHEN** the active theme's style for a widget class is changed and the widget is painted again
- **THEN** the emitted draw data SHALL use the new color

### Requirement: Panel container

A `Panel` SHALL be a container element that clips its children to its bounds and paints a theme-driven
background over its bounds.

#### Scenario: Panel clips children
- **WHEN** a child of a panel paints a rect extending beyond the panel bounds
- **THEN** the child's draw command clip SHALL be limited to the panel bounds

#### Scenario: Panel paints background
- **WHEN** a panel with a style class is painted
- **THEN** it SHALL emit geometry covering its bounds using the resolved background color

### Requirement: Image widget

An `Image` SHALL paint a textured quad over its bounds using an opaque texture handle and a UV rect. An invalid
texture handle SHALL emit no geometry.

#### Scenario: Image binds texture handle
- **WHEN** an image is configured with a texture handle and painted
- **THEN** its draw command SHALL reference that handle

#### Scenario: Image without texture
- **WHEN** an image has an invalid texture handle
- **THEN** painting it SHALL emit no draw command

### Requirement: Button widget

A `Button` SHALL track interaction state (normal, hover, pressed, disabled), paint theme-driven colors per
state, ignore input while disabled, and invoke its click callback when a press and release both occur inside
its bounds.

#### Scenario: Click fires inside bounds
- **WHEN** a button receives a pointer down and a pointer up both inside its bounds
- **THEN** its click callback SHALL be invoked

#### Scenario: Release outside does not click
- **WHEN** a button receives a pointer down inside its bounds and a pointer up outside its bounds
- **THEN** its click callback SHALL NOT be invoked

#### Scenario: Disabled ignores input
- **WHEN** a button is disabled and receives pointer input
- **THEN** it SHALL change no state and SHALL NOT invoke its click callback

#### Scenario: Pressed state affects paint
- **WHEN** a button is pressed and painted
- **THEN** the emitted draw data SHALL use the theme's pressed color

### Requirement: Core paint clipping for containers

The core paint walk SHALL push and pop the clip rect for any element that clips its children, so container
widgets do not implement clip bookkeeping themselves.

#### Scenario: Clip restored after children
- **WHEN** a clipping container finishes painting its children
- **THEN** the paint clip SHALL be restored to the value before the container
