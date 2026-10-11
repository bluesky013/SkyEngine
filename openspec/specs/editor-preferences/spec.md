# editor-preferences Specification

## Purpose
TBD - created by archiving change editor-preferences-dialog. Update Purpose after archive.
## Requirements
### Requirement: Preferences dialog
The editor SHALL provide a modal Preferences dialog opened from the File menu, with a category list on the
left and the selected page on the right, and SHALL close on OK or Cancel.

#### Scenario: Open from the menu
- **WHEN** the user selects `File > Preferences…`
- **THEN** the Preferences dialog SHALL open, focused, over the editor content

#### Scenario: Modal input
- **WHEN** the dialog is open
- **THEN** pointer and keyboard input SHALL be captured by the dialog and the dock content SHALL NOT receive it

#### Scenario: Close
- **WHEN** the user presses OK, Cancel, or Esc
- **THEN** the dialog SHALL close and return input to the editor

### Requirement: Category pages contributed by modules
Preferences SHALL be organized into named categories/pages that modules contribute; the shell SHALL NOT
hard-code subsystem settings.

#### Scenario: Registered pages appear
- **WHEN** a module registers a preference page
- **THEN** that page SHALL appear as a category in the dialog and its settings SHALL render in the page host

#### Scenario: Built-in categories
- **WHEN** the editor starts
- **THEN** at least General, Editor, and Rendering pages SHALL be present, contributed through the
  registration model rather than the shell

### Requirement: Apply, OK, Cancel and Reset
The dialog SHALL edit the live settings through the reflected form (undoable); OK and Apply SHALL
persist them, Cancel SHALL restore the snapshot taken when the dialog opened, and Reset SHALL restore
the page's type defaults; committed changes SHALL take effect without restarting where the owning
subsystem supports it.

#### Scenario: Cancel discards
- **WHEN** the user changes settings and presses Cancel
- **THEN** the stored settings and the running editor SHALL be restored to the state on open

#### Scenario: Apply commits
- **WHEN** the user changes settings and presses Apply
- **THEN** the settings SHALL be persisted and become the new cancel baseline

#### Scenario: Reset
- **WHEN** the user chooses Reset on a page
- **THEN** that page's fields SHALL return to their type defaults

#### Scenario: Undoable edit
- **WHEN** the user edits a field
- **THEN** the change SHALL be routed through the command service and be undoable

### Requirement: Per-user persistence
Committed preferences SHALL persist per user and SHALL be restored on the next startup; unknown keys in a
stored file SHALL be skipped without failing to load.

#### Scenario: Save and restore
- **WHEN** the user commits settings and restarts the editor
- **THEN** the dialog SHALL show the previously committed values

#### Scenario: Forward-compatible load
- **WHEN** the stored file contains keys not known to the current build
- **THEN** loading SHALL succeed and those keys SHALL be ignored

### Requirement: Reflection-driven settings model
Settings SHALL be expressed as reflected data types (plain structs) registered with the reflection
context and decorated with UI metadata (label, order, range, enum names); the model SHALL be
UI-toolkit-free, SHALL provide each setting with a default, and SHALL be testable headlessly. The set
of preference pages SHALL be dynamic (modules register page types), while each page's fields are a
fixed reflected type; the shell SHALL NOT hand-draw per-type controls.

#### Scenario: Defaults
- **WHEN** a setting has no stored value
- **THEN** the model SHALL report its declared (default-constructed) value

#### Scenario: Metadata drives the control
- **WHEN** a page renders a reflected field
- **THEN** the control SHALL be chosen from the field's reflected kind + metadata (bool / integer /
  float slider / enum dropdown), not from a per-type switch

#### Scenario: Registered pages
- **WHEN** a module registers a preference page type
- **THEN** that page SHALL appear as a category and its fields SHALL render through the generic
  reflected form

