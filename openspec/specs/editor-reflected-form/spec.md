# editor-reflected-form Specification

## Purpose
TBD - created by archiving change editor-property-ui. Update Purpose after archive.
## Requirements
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
EditorWidgets SHALL define the reflected control/view seam without EditorCore depending on the UI toolkit. Each ReflectedFormView SHALL own an extensible kind-to-widget registry and its stateful field widgets; registration SHALL be exposed through that form rather than a process-global ReflectedWidgetRegistry accessor.

#### Scenario: Control chosen by kind
- **WHEN** the view builds a field whose kind is Bool, Integer, Float, String, or Enum
- **THEN** the form's registry SHALL provide the matching default widget

#### Scenario: Custom control factory
- **WHEN** a module registers a custom widget for a kind on one form
- **THEN** that form SHALL use it for fields of that kind without replacing another form's widget

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
EditorWidgets SHALL render a form by laying out its sections, field rows, and nesting automatically: section headers, a label column and a control column per row, indentation per depth, expand/collapse for struct fields, and scrolling for overflow. A consumer panel SHALL NOT require per-type layout code or a dependency on EditorShell to use this form view.

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

### Requirement: Enum member widget

Enum members SHALL be rendered and edited by a registered widget like any other kind; the form view
SHALL NOT contain enum-specific drawing or popup logic.

#### Scenario: Enum edited through its widget

- **WHEN** a form shows an enum member
- **THEN** clicking the control SHALL open the enum options through the enum widget and selecting one
  SHALL set the value as one undoable edit

#### Scenario: View has no enum special case

- **WHEN** the form view renders or hit-tests a field
- **THEN** it SHALL delegate enum members to the widget registry rather than handling them inline

### Requirement: Explicit reflected form context
ReflectedFormView SHALL accept a ReflectedFormContext providing CommandService, PropertyEditorRegistry, PropertyChangeNotifier, and UiTheme references. Its existing text/title constructor SHALL remain available and SHALL use the process-shared default services/theme. Field resolution, edit/undo handling, external-change refresh, and presentation SHALL use the selected context. The form SHALL borrow these dependencies, whose owner SHALL keep them alive for the form's lifetime.

#### Scenario: Form uses injected commands and metadata
- **WHEN** a form is constructed with an explicit command service and property metadata registry
- **THEN** its field resolution and accepted edits/undo SHALL use those objects rather than a later default-service lookup

#### Scenario: Form uses injected notifier and theme
- **WHEN** the explicit context's notifier changes or its theme values are used to render
- **THEN** the form SHALL observe that notifier and use that theme independently of the default context

#### Scenario: Existing construction uses shared defaults
- **WHEN** a caller uses the existing text/title constructor from a feature DLL
- **THEN** the form SHALL use the same default services/theme as a form created by the editor host

### Requirement: Form interaction state is isolated
Each reflected form SHALL own its widget registry and mutable widget interaction state, including popup targets, drag fields, capture, and local editor modes. Default widget initialization SHALL occur for every form. Registering defaults SHALL preserve existing custom property metadata, and no process-global mutable widget instance SHALL be required.

#### Scenario: Popups are local to their form
- **WHEN** one form opens a field popup while a second form exists
- **THEN** only the first form SHALL report and route that popup's pointer capture

#### Scenario: Drag does not affect another form
- **WHEN** one form drags a field and a second form is painted or receives unrelated input
- **THEN** the second form SHALL NOT reuse the first form's drag target, popup, or field pointers

#### Scenario: Local widget registration remains local
- **WHEN** one form replaces a kind's widget through its registry
- **THEN** another form SHALL retain its own widget implementation and state

### Requirement: Rebinding clears obsolete interactions
Before Bind or explicit Refresh replaces reflected field storage, the form SHALL clear edit callbacks, active drag/capture/hover field references, and popup/widget interaction state that refer to old fields. It SHALL retain its context and form-local extension registrations. An old interaction SHALL NOT commit against the previous bound object after rebinding. External notifications received during an interaction SHALL remain deferred until the interaction ends.

#### Scenario: Bind cancels an old popup or drag
- **WHEN** a form with an open popup or active drag binds another reflected object
- **THEN** it SHALL clear the old capture/interaction before building the new fields and SHALL NOT access the old field pointers

#### Scenario: Bind cancels a pending inline edit
- **WHEN** a form with an unfinished inline edit binds another object
- **THEN** later key/text input SHALL NOT invoke the old field's commit callback

#### Scenario: Refresh retains local extensions safely
- **WHEN** the form is explicitly refreshed after a custom widget has been registered
- **THEN** field interaction pointers SHALL be cleared before rebuild and the form's custom widget registration SHALL remain available

#### Scenario: Deferred external refresh is delivered
- **WHEN** an external change is notified during editing or dragging and the interaction subsequently ends
- **THEN** the form SHALL apply the pending refresh without retaining invalid pointers into the replaced field storage

### Requirement: History traversal cancels live previews first
The form SHALL cancel uncommitted live previews before invoking Undo or Redo. A subsequent refresh/reset SHALL NOT overwrite the value restored by the command. Widget callback dispatch SHALL retain the callback and widget lifetime needed for a successful edit that synchronously refreshes its form.

#### Scenario: Undo during a live color preview
- **WHEN** a color field has undo history and the user invokes Undo while its picker is applying an uncommitted preview
- **THEN** the form SHALL cancel the preview first and leave the field at the command's undone value with the matching history state

#### Scenario: Redo during a live color preview
- **WHEN** the user invokes Redo while a color picker is applying an uncommitted preview
- **THEN** interaction reset SHALL occur before Redo and SHALL NOT restore the picker's opening value over the redone value

#### Scenario: Successful commit refreshes synchronously
- **WHEN** a widget's commit callback edits the form and synchronously rebuilds or resets it
- **THEN** dispatch SHALL complete without using a destroyed callback or widget

### Requirement: Active widgets survive local registry replacement
The form SHALL retain shared ownership of the widget serving an active interaction, and event/popup dispatch SHALL keep its target alive across host callbacks. Replacing a kind's registry entry SHALL NOT invalidate a widget still being dispatched. Interaction reset SHALL include a retained active widget even when that widget is no longer the current registry entry.

An inline edit callback SHALL retain its creating widget after pointer release and registry replacement. Commit, cancel, and rebinding SHALL release this ownership when the callback is no longer needed, while synchronous refresh during commit SHALL NOT destroy a callback's widget before that callback returns.

#### Scenario: Replace a widget while inline editing
- **WHEN** a widget starts an inline edit and its kind is replaced after pointer release
- **THEN** the original widget SHALL remain alive until its edit callback completes or is canceled
- **AND** the original widget SHALL be released after the edit ends when no other owner remains

#### Scenario: Replace a captured widget
- **WHEN** a form replaces a kind's widget while the old widget serves capture or an open popup
- **THEN** later dispatch to the active interaction SHALL NOT dereference a destroyed widget

#### Scenario: Reset a replaced active instance
- **WHEN** a retained active widget has been replaced in the registry and the form binds or refreshes
- **THEN** the old active widget's interaction SHALL be reset before its retained ownership is released

### Requirement: Form preferred extent is valid before painting
ReflectedFormView SHALL calculate natural content/preferred height from its current sections, expanded fields, widget row counts, header visibility, and theme metrics without requiring a previous Paint. Bind, Refresh, expansion/collapse, sequence membership, header visibility, and widget registration/replacement SHALL invalidate layout so a host can allocate the current extent before painting. A per-form registry change SHALL NOT require rebuilding fields or ending an active interaction solely to invalidate layout. Arbitrary state changes inside an already-registered custom widget SHALL require explicit Refresh or conservative volatility. Measurement/height queries SHALL NOT use the previously painted content extent as their sole source of truth.

#### Scenario: First allocation follows binding
- **WHEN** a host binds a nonempty reflected form and asks for preferred height before any Paint
- **THEN** the returned height SHALL fit the current visible rows and header

#### Scenario: Structural change affects the next allocation
- **WHEN** an expanded field, sequence membership, header visibility, or registered widget implementation changes
- **THEN** the form SHALL notify layout and the next pre-paint height query SHALL reflect the changed extent

#### Scenario: Widget replacement preserves an active interaction
- **WHEN** a form replaces a widget registration while a previous widget owns an active interaction
- **THEN** layout SHALL be invalidated to reflect the new row count without destroying the active interaction's retained owner

### Requirement: Form external dependencies synchronize safely before layout
A form SHALL observe its injected PropertyChangeNotifier and CommandService changes, invalidate layout, and retain pending refresh until a safe public pre-layout synchronization step can consume it. Notification callbacks SHALL NOT synchronously rebuild active field/widget interaction state. Pending refresh SHALL survive an active interaction and SHALL be consumed when safe even if no later notification occurs. A destroyed view SHALL remove its subscriptions. Explicit Refresh remains required for arbitrary unnotified external model mutation.

#### Scenario: Property notification changes row structure
- **WHEN** bound data changes externally, its notifier publishes the change, and the host synchronizes before allocation
- **THEN** the form's rows and preferred extent SHALL reflect the current data in that allocation pass without a preliminary Paint

#### Scenario: Undo occurs in another host
- **WHEN** another host sharing the injected command service performs undo or redo and the form synchronizes before layout
- **THEN** the form SHALL refresh to the current bound data before its host allocates rows

#### Scenario: Pending refresh outlives an interaction
- **WHEN** an external change is notified while a text edit, drag, or popup interaction is active
- **THEN** the form SHALL retain safe active interaction state and later consume the pending refresh once safe without requiring another notification

#### Scenario: Destroyed view receives no service callback
- **WHEN** a form is destroyed and its former notifier or command service later changes
- **THEN** no remaining subscription SHALL call the destroyed view

### Requirement: Binding replacement distinguishes cancellation from abandonment
ReflectedFormView SHALL distinguish live-model cancellation from invalid-binding abandonment. CancelInteractions and Refresh SHALL require the current model to remain valid and SHALL restore uncommitted live previews before clearing interaction state. Bind, Unbind, and destruction SHALL abandon obsolete callbacks, popups, capture, drag, hover, and inline-edit targets without accessing the former model. Unbind SHALL leave no valid model for a later pending notification to rebuild. Form-local widget registrations and context dependencies SHALL remain unchanged by rebinding.

#### Scenario: Bind after the old model has been released
- **WHEN** a form has an active color preview and binds another object after its old model storage was released
- **THEN** it SHALL clear the obsolete interaction without reading or writing the released model and subsequent input SHALL NOT commit against that binding

#### Scenario: Host cancels before valid model replacement
- **WHEN** a host calls CancelInteractions while the old model remains valid, then replaces its storage and binds the replacement
- **THEN** the uncommitted preview SHALL be restored before release and the new binding SHALL have no old interaction state

#### Scenario: Escape restores a live preview
- **WHEN** Escape cancels an active preview whose model remains valid
- **THEN** the model SHALL return to its opening value without adding a command

#### Scenario: Unbind survives later notifications
- **WHEN** a form unbinds after model release and receives property or command notifications
- **THEN** later synchronization SHALL NOT rebuild the former binding or access its data

#### Scenario: World config storage is replaced at the same document address
- **WHEN** WorldConfigPanel refreshes after its document's config storage was replaced during an active preview
- **THEN** it SHALL abandon the old binding before resolving the new config and display the new config without an old rollback

### Requirement: Obsolete binding callbacks become inert
A form SHALL expose a weak binding-generation lifetime distinct from its service subscription lifetime. Abandonment SHALL expire that generation before clearing widgets. Cancellation SHALL permit live rollback before expiring it. Built-in color and inline-edit callbacks SHALL reject an expired generation before accessing a field or host. Custom widgets SHALL drop obsolete target pointers for either reset mode and SHALL NOT access model data during Abandon.

#### Scenario: Callback belongs to an old field generation
- **WHEN** a retained callback is invoked after its binding was reset, rebound, unbound, or destroyed
- **THEN** built-in binding callbacks SHALL perform no model or host access

#### Scenario: Form destruction outlives the model
- **WHEN** a form is destroyed after its borrowed model was already released
- **THEN** teardown SHALL abandon interactions without rollback and remove notifier/command subscriptions without model access

### Requirement: Forms support domain-owned edit commands
ReflectedForm SHALL accept an optional UI-free edit-command factory receiving a transient top-level property descriptor and its staged new value. Scalar, nested, reset, sequence-add, and sequence-remove edits SHALL use that seam and emit the committed-change callback once. Without a factory the form SHALL retain generic descriptor-command behavior. A domain factory SHALL NOT retain the descriptor, field, form, or borrowed model as a historical target.

#### Scenario: Disposable model uses a domain command
- **WHEN** an asset form commits an edit using a registered domain factory and later replaces its materialized model
- **THEN** the recorded command SHALL remain independent of the released model's storage

#### Scenario: Sequence and reset respect the same factory
- **WHEN** a domain-bound form adds/removes a sequence element or resets a property
- **THEN** it SHALL create the edit through the same factory and notify its host once

### Requirement: Synchronous domain observers cannot invoke obsolete form callbacks
ReflectedForm SHALL snapshot its factory and changed callback before command dispatch and SHALL use a weak binding generation to suppress the changed callback after a synchronous observer destroys or rebinds the form. Dispatch SHALL NOT retain that generation or access form members after execution. An active-edit counter SHALL remain safe to release independently of the form's lifetime.

#### Scenario: Persistence observer destroys the editing form
- **WHEN** catalog persistence synchronously invokes an observer that destroys the ReflectedForm executing the domain edit
- **THEN** Edit SHALL return without accessing that form and SHALL NOT invoke its obsolete changed callback

#### Scenario: Persistence observer rebinds the same form
- **WHEN** a synchronous observer rebuilds or clears the editing form's binding
- **THEN** the former binding's changed callback SHALL NOT execute after command dispatch

### Requirement: End interactions before shared history traversal

Reflected forms SHALL subscribe to the command service supplied by their context and cancel live interactions before that service traverses history. Shell accelerators, shared toolbar/menu actions, and direct service calls SHALL use this ordering. Forms SHALL remove preparation subscriptions on destruction and guard copied callbacks against expired lifetime. Binding invalidation SHALL abandon invalid targets before they can receive a cancellation callback.

#### Scenario: Shared entry points preserve history values
- **WHEN** an asset or standalone form has committed A to B and is previewing C and Undo is invoked by a Shell accelerator, shared menu/toolbar action, or direct command-service call
- **THEN** the preview SHALL end before Undo restores A
- **AND** later Escape, pointer release, or layout synchronization SHALL preserve A and the advanced history cursor

#### Scenario: Form removed during preparation
- **WHEN** an earlier preparation observer destroys a form
- **THEN** later callbacks SHALL NOT access that form or its old binding

#### Scenario: Local history while inline editing
- **WHEN** the form receives a key-down history shortcut during inline editing
- **THEN** it SHALL route through the same service preparation boundary
- **AND** the corresponding key-up SHALL NOT traverse history again

### Requirement: Reflected field backing storage lifetime
ReflectedForm SHALL construct descriptor-backed fields at stable final addresses for the lifetime of one built tree. Member presentation order SHALL be resolved before constructing fields; known sibling/element capacity SHALL be reserved before recursively binding owned values and descendants. A child descriptor pointing into a struct field's owned Any and each child parent pointer SHALL remain valid after the complete form is built, independent of inline versus heap Any storage. Default capture SHALL obey the same lifetime rule. Rebuild SHALL own structural replacement; view consumers MAY change presentation/expansion state but SHALL NOT relocate or structurally mutate built fields.

#### Scenario: Inline nested structs remain readable
- **WHEN** a reflected struct smaller than Any's inline block contains another reflected inline struct
- **THEN** all descendant descriptors SHALL read from the final owning field values after construction returns

#### Scenario: Sibling order does not relocate built values
- **WHEN** multiple top-level and nested siblings have presentation order different from reflected member iteration order
- **THEN** the completed tree SHALL expose that presentation order without moving already bound field backing or invalidating child descriptors

#### Scenario: Parent links propagate a deep edit
- **WHEN** a leaf inside multiple inline struct levels is edited or reset
- **THEN** its final parent chain SHALL propagate the new value to the correct root member as one undoable command and SHALL preserve unrelated siblings

#### Scenario: Nested sequence fields retain live containing storage
- **WHEN** a sequence inside multiple reflected struct levels exposes many indexed children
- **THEN** all children SHALL resolve the expected copied element values through live containing storage and preserve their existing read-only element contract

#### Scenario: Scratch baseline fields do not escape their storage
- **WHEN** a form captures type defaults or an explicit baseline using a temporary scratch form
- **THEN** copied default values SHALL remain usable by the live tree without retaining scratch descriptors or addresses

### Requirement: Reflected order metadata participates before field construction
Effective form member order SHALL use explicit MemberAppearance.order when hasOrder is set, otherwise reflected CommonPropertyKey::ORDER when present, otherwise zero. Stable sorting SHALL preserve existing reflection iteration for ties; the current std::map fallback is key order rather than source registration order. The rule SHALL apply before constructing root and nested struct fields so inline owned-value/child-descriptor storage is not relocated by sorting built fields. This correction SHALL NOT require new Framework registration-order ABI or per-settings-page layout code.

#### Scenario: Explicit appearance overrides reflection
- **WHEN** a member declares reflected ORDER and an explicit appearance order
- **THEN** the form SHALL use the explicit appearance value, including an explicit zero

#### Scenario: Reflected order and stable ties
- **WHEN** reflected members have ORDER values or no explicit order and some effective values tie
- **THEN** fields SHALL follow effective order with stable existing reflection iteration for the tied members

#### Scenario: Nested fields retain valid bindings
- **WHEN** nested reflected members are ordered and later edited/reset/undone
- **THEN** their descriptors SHALL remain bound to valid owned storage rather than addresses invalidated by sorting already-built fields
