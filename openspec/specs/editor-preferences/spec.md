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

### Requirement: Typed preference model
The preferences model SHALL be UI-toolkit-free and SHALL support typed values (boolean, integer, float,
string, color) with a default for each setting and a stable key, and SHALL be testable headlessly.

#### Scenario: Defaults
- **WHEN** a setting has no stored value
- **THEN** the model SHALL report its declared default

#### Scenario: Type-safe access
- **WHEN** a page reads or writes a setting
- **THEN** the value SHALL be accessed by its declared type and key

### Requirement: Apply, OK, Cancel and Reset
The dialog SHALL edit a working copy of the settings: OK and Apply commit it, Cancel discards it, and Reset
restores defaults; committed changes SHALL take effect without restarting where the owning subsystem supports it.

#### Scenario: Cancel discards
- **WHEN** the user changes settings and presses Cancel
- **THEN** the stored settings and the running editor SHALL be unchanged

#### Scenario: Apply commits
- **WHEN** the user changes settings and presses Apply
- **THEN** the working copy SHALL be committed and persisted

#### Scenario: Reset
- **WHEN** the user chooses Reset on a page
- **THEN** that page's settings SHALL return to their defaults in the working copy

### Requirement: Per-user persistence
Committed preferences SHALL persist per user and SHALL be restored on the next startup; unknown keys in a
stored file SHALL be skipped without failing to load.

#### Scenario: Save and restore
- **WHEN** the user commits settings and restarts the editor
- **THEN** the dialog SHALL show the previously committed values

#### Scenario: Forward-compatible load
- **WHEN** the stored file contains keys not known to the current build
- **THEN** loading SHALL succeed and those keys SHALL be ignored

