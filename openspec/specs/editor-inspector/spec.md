# editor-inspector Specification

## Purpose
TBD - created by archiving change editor-property-ui. Update Purpose after archive.
## Requirements
### Requirement: Selection-driven inspector

The inspector panel SHALL resolve the current editor selection to reflected data through the selection property
source and SHALL render that data with the `editor-reflected-form` framework. It SHALL show an empty state when
no source is set or the selection resolves to no data.

#### Scenario: Selected data is inspected

- **WHEN** the selection changes to an item whose source resolves to a reflected data object
- **THEN** the inspector SHALL render that object's members as a form

#### Scenario: Selection resolves to multiple objects

- **WHEN** the selection resolves to more than one reflected data object
- **THEN** the inspector SHALL render one section per object

#### Scenario: Empty state

- **WHEN** no selection is resolved or no source is set
- **THEN** the inspector SHALL show an empty state and SHALL NOT crash

### Requirement: Inspector refresh

The inspector SHALL rebuild when the selection changes and when the undo/redo stack changes.

#### Scenario: Rebuild on selection change

- **WHEN** the editor selection changes to another resolved object
- **THEN** the inspector SHALL rebuild to show that object's members

#### Scenario: Refresh on undo

- **WHEN** an edit made through the inspector is undone
- **THEN** the inspector SHALL show the restored value

### Requirement: Inspector edits are undoable

Edits made through the inspector SHALL be routed through the `CommandService` and SHALL be undoable. Basic
controls SHALL include a bool toggle, integer/float/string text commit, and enum cycling.

#### Scenario: Edit is undoable

- **WHEN** the user edits a scalar value in the inspector
- **THEN** the underlying data SHALL change and a subsequent undo SHALL restore the previous value

#### Scenario: Bool toggle

- **WHEN** the user activates a bool member row
- **THEN** the member SHALL be set to the opposite value through an undoable command

