## ADDED Requirements

### Requirement: Toolkit-independent command service
Undo/redo SHALL be provided by a service in `EditorCore` whose contract references no UI-toolkit type. It SHALL
support executing undoable commands and undoing/redoing them.

#### Scenario: Execute then undo
- **WHEN** an undoable command is executed and then undone
- **THEN** the affected state SHALL return to its value before the command

#### Scenario: Redo reapplies
- **WHEN** a redo follows an undo
- **THEN** the command SHALL be reapplied

### Requirement: Transaction grouping
The service SHALL group multiple commands into a single user-visible undo step via transactions.

#### Scenario: Transaction is one step
- **WHEN** several commands execute inside one transaction and the transaction ends
- **THEN** a single undo SHALL revert all of them

#### Scenario: Can-undo reflects state
- **WHEN** no command has been executed
- **THEN** `CanUndo` SHALL report false, and after a command it SHALL report true

### Requirement: Reflection-driven property edits
Property edits SHALL be recorded through a generic command generated from the reflection model, capturing the
object handle, member path, and the old and new values, without per-type command code.

#### Scenario: Property undo restores old value
- **WHEN** a reflected property is changed through the property model and then undone
- **THEN** the property SHALL return to its previous value using the captured old value

### Requirement: Change notification
The service SHALL notify observers when the undo/redo stacks change so views can update enabled state.

#### Scenario: Observer notified after edit
- **WHEN** a command is executed, undone, or redone
- **THEN** registered observers SHALL be notified
