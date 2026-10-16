## MODIFIED Requirements

### Requirement: Control factory seam
EditorWidgets SHALL define the reflected control/view seam without EditorCore depending on the UI toolkit. Each ReflectedFormView SHALL own an extensible kind-to-widget registry and its stateful field widgets; registration SHALL be exposed through that form rather than a process-global ReflectedWidgetRegistry accessor.

#### Scenario: Control chosen by kind
- **WHEN** the view builds a field whose kind is Bool, Integer, Float, String, or Enum
- **THEN** the form's registry SHALL provide the matching default widget

#### Scenario: Custom control factory
- **WHEN** a module registers a custom widget for a kind on one form
- **THEN** that form SHALL use it for fields of that kind without replacing another form's widget

### Requirement: Automatic form layout
EditorWidgets SHALL render a form by laying out its sections, field rows, and nesting automatically: section headers, a label column and a control column per row, indentation per depth, expand/collapse for struct fields, and scrolling for overflow. A consumer panel SHALL NOT require per-type layout code or a dependency on EditorShell to use this form view.

#### Scenario: Rows are laid out automatically
- **WHEN** a form with sections, nested struct fields, and sequence fields is rendered
- **THEN** the view SHALL place section headers, labels, controls, and nested rows without consumer layout code

#### Scenario: Expand and collapse a struct
- **WHEN** the user toggles a struct field
- **THEN** its child rows SHALL be shown or hidden

## ADDED Requirements

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
