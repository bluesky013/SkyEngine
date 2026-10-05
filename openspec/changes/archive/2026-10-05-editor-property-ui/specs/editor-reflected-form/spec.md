## ADDED Requirements

### Requirement: Reflected data unit

The form framework SHALL operate on a `PropertyObject` (`void *object` plus `const TypeNode *type`) that points
at reflected **data**, and SHALL NOT require the object to be a component, actor, world, or any other container
type. `EditorCore` public headers for the framework SHALL NOT include a world, component, render, or UI-toolkit
header.

#### Scenario: Build a form from reflected data

- **WHEN** a form is built from a data object and its reflected `TypeNode`
- **THEN** it SHALL expose that data's members without any component or world wrapper

#### Scenario: Core builds without a toolkit or world

- **WHEN** the editor core is built with only `Core` and `Framework` available
- **THEN** the form model, registry, and provider interfaces SHALL compile and link

### Requirement: Recursive sectioned field model

The framework SHALL build a layout-neutral form from one or more `PropertyObject`s, grouped into sections, where
each section contains fields that recursively cover scalar members, nested struct members, and sequence members.
A struct field SHALL expose its reflected members as child fields; a sequence field SHALL expose one child per
element. Building SHALL be repeatable so a form can be refreshed.

#### Scenario: Sections group fields

- **WHEN** a form is built from multiple objects or categorised members
- **THEN** the fields SHALL be grouped into sections and the form SHALL expose the sections

#### Scenario: Struct members become child fields

- **WHEN** a member's value is a reflected struct
- **THEN** the field SHALL expose one child field per reflected member of that struct

#### Scenario: Sequence elements become child fields

- **WHEN** a member is a sequence with N elements
- **THEN** the field SHALL expose N child fields

#### Scenario: Rebuild reflects current data

- **WHEN** a member value changes and the form is rebuilt
- **THEN** the rebuilt field SHALL report the new value

### Requirement: Toolkit-independent editor-kind registry

The framework SHALL include a registry mapping a reflected member descriptor to an editor kind and metadata
(`Bool`, `Integer`, `Float`, `String`, `Enum`, `Color`, `Vector`, `Struct`, `Sequence`, or `Unknown`) determined
from reflection metadata, without `EditorCore` referencing the concrete C++ value types. The registry SHALL allow
modules to register custom kind resolvers and handlers.

#### Scenario: Kind resolved from reflection

- **WHEN** a descriptor for a scalar, enum, struct, or sequence member is resolved
- **THEN** the registry SHALL return the matching editor kind

#### Scenario: Enum options exposed

- **WHEN** a descriptor's type is an enum with registered values
- **THEN** the registry SHALL expose the enum options for the control

#### Scenario: Enum without registered values degrades

- **WHEN** a descriptor's type is an enum with no registered values
- **THEN** the registry SHALL still report `Enum` and the control SHALL degrade to a numeric editor

#### Scenario: Unknown kind is read-only

- **WHEN** a descriptor's type has no resolvable editor kind
- **THEN** the registry SHALL return `Unknown` and the view SHALL render it read-only

#### Scenario: Extension registers a custom kind

- **WHEN** a module registers a resolver for a type it owns
- **THEN** subsequent resolutions of that type SHALL return the registered kind

### Requirement: Property UI attributes

The framework SHALL read per-member UI attributes from the existing reflection attribute map
(`TypeMemberNode::properties`) into a toolkit-independent attribute set and SHALL honor them when building a
form: visibility, label / tooltip / order / category, `readonly`, `multiline`, numeric range / step,
enum-as-flags, asset type, and an explicit editor-kind / editor-hint override. Attributes SHALL take precedence
over type inference.

#### Scenario: Hidden member is omitted

- **WHEN** a member is marked not visible
- **THEN** the form SHALL omit its field

#### Scenario: Readonly member is not editable

- **WHEN** a member is marked readonly
- **THEN** its control SHALL be rendered read-only and edits through it SHALL be rejected

#### Scenario: Label and category overrides

- **WHEN** a member declares a label or category attribute
- **THEN** the form SHALL use them instead of the reflected member name / type name

#### Scenario: Missing attributes use defaults

- **WHEN** a member declares no UI attributes
- **THEN** the form SHALL fall back to type-inferred behavior with no error

### Requirement: Attribute-driven control selection

Control selection SHALL consider attributes before type inference, so a ranged numeric becomes a slider, a color
type or hint becomes a color editor, a multiline string becomes a multi-line text control, and an asset-typed
member becomes an asset control. When no attribute applies, selection SHALL fall back to the type-inferred
editor kind.

#### Scenario: Range selects a slider

- **WHEN** a numeric member declares a range
- **THEN** the control factory SHALL provide a slider bound to that range rather than a plain text box

#### Scenario: Color type selects a color editor

- **WHEN** a member's type is a registered color type (or declares a color editor kind)
- **THEN** the control factory SHALL provide a single color control instead of nested scalar fields

#### Scenario: Fallback to type inference

- **WHEN** no attribute overrides a member's control
- **THEN** selection SHALL use the type-inferred editor kind

### Requirement: Control factory seam

The shell SHALL define a control-factory seam that creates a UI control from a field's kind and metadata without
`EditorCore` depending on the UI toolkit.

#### Scenario: Control chosen by kind

- **WHEN** the view builds a field whose kind is `Bool`, `Integer`, `Float`, `String`, or `Enum`
- **THEN** the control factory SHALL provide the matching default control

#### Scenario: Custom control factory

- **WHEN** a module registers a control factory for a kind it owns
- **THEN** the view SHALL use that factory for fields of that kind

### Requirement: Object provider seams

The framework SHALL define provider seams so a panel supplies reflected objects without `EditorCore` depending
on any world type: a selection source and a named-config source, both host-injected. The shell SHALL operate
correctly when a provider is unset.

#### Scenario: Selection provider resolves objects

- **WHEN** a selection provider resolves the current selection
- **THEN** it SHALL return zero or more reflected objects for the view

#### Scenario: Missing provider degrades gracefully

- **WHEN** no provider is set or it returns no objects
- **THEN** the view SHALL show an empty state and SHALL NOT crash

### Requirement: Automatic form layout

The shell SHALL render a form by laying out its sections, field rows, and nesting automatically — section
headers, a label column and a control column per row, indentation per depth, expand/collapse for struct fields,
and scrolling for overflow — without per-type layout code in the consumer panel.

#### Scenario: Rows are laid out automatically

- **WHEN** a form with sections, nested struct fields, and sequence fields is rendered
- **THEN** the view SHALL place section headers, labels, controls, and nested rows without consumer layout code

#### Scenario: Expand and collapse a struct

- **WHEN** the user toggles a struct field
- **THEN** its child rows SHALL be shown or hidden

### Requirement: Undoable editing

The framework SHALL route value edits through the `CommandService` so every accepted edit is undoable, and views
SHALL refresh on selection changes and on command changes.

#### Scenario: Edit is undoable

- **WHEN** a scalar value is edited through the form
- **THEN** the underlying data SHALL change and a subsequent undo SHALL restore the previous value

#### Scenario: Refresh on command change

- **WHEN** an undo or redo is applied
- **THEN** the form view SHALL refresh to show the restored value

### Requirement: Headless usability

The form model, editor-kind registry, and provider interfaces SHALL be usable in a headless test without a
window, GPU, or UI toolkit.

#### Scenario: Build and edit headlessly

- **WHEN** a test builds a form, resolves kinds, and performs an edit without initializing a platform or renderer
- **THEN** the form SHALL read and write values and the undo service SHALL record the edit

### Requirement: Asset reference members

The framework SHALL treat a member carrying an asset-type attribute as an asset reference, SHALL expose
the required asset type, and SHALL resolve asset names/types through a host-provided catalog seam without
`EditorCore` depending on a concrete asset database.

#### Scenario: Asset type drives the control

- **WHEN** a member declares an asset type
- **THEN** the registry SHALL report the `Asset` editor kind and expose the required type

#### Scenario: Selecting an asset validates its type

- **WHEN** the user picks an asset of the required type
- **THEN** the member's value SHALL be set to that asset's id as one undoable edit, and a value whose type
  does not match SHALL be shown as invalid

### Requirement: Value validation and constraints

The framework SHALL filter input by the field's kind and SHALL parse/constrain committed values against the
reflected type and the member's range/step attributes.

#### Scenario: Typed input filtering

- **WHEN** the user edits a numeric field
- **THEN** characters invalid for the type SHALL be rejected before they reach the value

#### Scenario: Range and step applied

- **WHEN** a value is committed for a member with range/step attributes
- **THEN** the value SHALL be clamped to the range and quantized to the step; invalid text SHALL be
  rejected and the field shown as invalid

### Requirement: Reset to default

The framework SHALL capture each field's default value when a form is first bound and SHALL offer a
reset that writes the default back as one undoable edit; the UI SHALL indicate fields that differ from
their default.

#### Scenario: Modified indicator

- **WHEN** a field's value differs from its captured default
- **THEN** the view SHALL show a reset affordance for that field

#### Scenario: Reset is undoable

- **WHEN** the user resets a field to its default
- **THEN** the value SHALL return to the default and a subsequent undo SHALL restore the previous value

### Requirement: External change refresh

The framework SHALL expose a change notification seam so a data owner can signal that reflected data
changed outside the editor, and views SHALL refresh when notified without disrupting an in-progress edit.

#### Scenario: Notified change refreshes the view

- **WHEN** a data owner notifies a change and the view is not editing
- **THEN** the view SHALL rebuild from the current data

#### Scenario: Editing defers refresh

- **WHEN** a change is notified while an edit or drag is in progress
- **THEN** the refresh SHALL be deferred until the interaction completes

### Requirement: Themeable presentation

The framework SHALL render through a reusable, theme-driven view whose colors, metrics and fonts come
from a swappable theme, so panels share one look and can be restyled without editing layout code.

#### Scenario: One skin, many panels

- **WHEN** a panel renders a form
- **THEN** its controls SHALL be drawn by the shared skin, not by panel-specific layout code

#### Scenario: Custom theme

- **WHEN** a different theme is supplied
- **THEN** the view SHALL render with that theme's colors, metrics and fonts
