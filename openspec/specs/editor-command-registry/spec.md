# editor-command-registry Specification

## Purpose
TBD - created by archiving change editor-command-registry. Update Purpose after archive.
## Requirements
### Requirement: Editor action registry
The editor SHALL provide a toolkit-agnostic `EditorActionRegistry` where modules and plugins register
commands as `EditorAction` records (id, label, icon, toolbar group, menu / submenu, order, enabled predicate,
invoke). Registering an id again SHALL override the previous record. No UI or render type SHALL appear in the
action contract.

#### Scenario: Register and resolve
- **WHEN** a module registers an action by id and the editor invokes that id
- **THEN** the registry SHALL resolve it and run its invoke callback

#### Scenario: Override
- **WHEN** the same id is registered again
- **THEN** the later registration SHALL replace the previous one

#### Scenario: Disabled action
- **WHEN** an action declares an `enabled` predicate that is false
- **THEN** invoking it SHALL be a no-op (consumed) and any toolbar/menu view SHALL reflect it as disabled

### Requirement: Toolbar built from actions
The toolbar SHALL be built from the registry's actions that declare a toolbar group: ordered by (group, order),
grouped with visible separators, drawn as icon buttons, and disabled/dimmed per the action's `enabled`.

#### Scenario: Groups
- **WHEN** actions declare different toolbar groups
- **THEN** the toolbar SHALL show them in group order separated by divider lines

#### Scenario: Icon and tooltip
- **WHEN** an action declares an icon
- **THEN** the toolbar SHALL draw the icon (icon-only) and reveal the label as a hover tooltip

### Requirement: Menus built from actions
The editor menus SHALL be built from the registry's actions that declare a menu: top-level menus ordered by
`menuOrder`, items ordered by `order`, and items with a `submenu` nested one level deep. Host-only entries the
shell owns (e.g. Preferences) and panels (dynamic) MAY be injected by the shell.

#### Scenario: Nesting
- **WHEN** an action declares a `submenu`
- **THEN** it SHALL appear under that second-level submenu within its top-level menu

### Requirement: Shortcuts drive actions by id
Keyboard shortcuts SHALL resolve to registry actions by id (not to per-command handlers); a shortcut for a
missing/disabled action SHALL be a no-op.

#### Scenario: Save shortcut
- **WHEN** the user presses Ctrl+S and a `file.save` action is registered
- **THEN** that action's invoke SHALL run

### Requirement: Plugin extensibility
A plugin SHALL contribute toolbar/menu actions through its editor extension registration without depending on
the shell or any concrete editor type.

#### Scenario: Plugin action
- **WHEN** a plugin registers an action in its `Register()`
- **THEN** it SHALL appear in the toolbar and/or menus alongside the built-in actions

