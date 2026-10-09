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

### Requirement: Cook targets from platform preset bundles

The editor SHALL resolve an asset's cook targets to the active platform's preset product bundles when
the asset declares no cook-target override and the project declares no named targets, and the Cook
action SHALL cook each of them.

#### Scenario: Target-less project
- **WHEN** an asset with no cook override is cooked and the project has no named targets
- **THEN** the cook SHALL run for each product bundle in the active platform preset (e.g. common,
  tex_pc) instead of doing nothing

#### Scenario: Asset override wins
- **WHEN** an asset declares cook targets
- **THEN** the cook SHALL run for the declared targets

### Requirement: Asset browser actions

The editor SHALL register the asset browser actions in the `EditorActionRegistry` as `EditorAction`
records with ids, labels, and enabled predicates: New Asset, Import, Rename, Move, Duplicate, Delete,
Cook/Build, Reimport, Copy Reference, Show in Explorer, and Find References, and Refresh. Mutating
actions SHALL declare an enabled predicate that is false when the selection is empty or read-only.

#### Scenario: Actions resolve by id
- **WHEN** the editor invokes an asset browser action id
- **THEN** the registry SHALL resolve it and run its invoke callback

#### Scenario: Enabled predicate drives the UI
- **WHEN** the selection is empty or only read-only assets
- **THEN** the mutating actions SHALL report disabled and invoking them SHALL be a no-op

#### Scenario: Shared implementation
- **WHEN** an asset action is triggered from the toolbar, a menu, a shortcut, or the panel context menu
- **THEN** the same registered action SHALL run

