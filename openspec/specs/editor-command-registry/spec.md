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

### Requirement: Shared action chrome is independent of the shell
ActionBarBuilder, ToolBar, and MenuBar SHALL be provided by EditorWidgets and SHALL be usable without linking EditorShell. They SHALL continue to consume EditorActionRegistry, scope, icon, and accelerator data and SHALL invoke actions through a host-supplied callback rather than a concrete Shell instance. Host-only menu entries and policy SHALL remain with their host.

#### Scenario: Feature consumer builds scoped chrome
- **WHEN** a consumer linked only to the shared editor framework builds toolbar/menu items for its active scope
- **THEN** the builder SHALL use that scope's registered actions without requiring Shell

#### Scenario: Action invocation reaches the supplied host
- **WHEN** a shared toolbar/menu item is activated
- **THEN** it SHALL pass the registered action id to the supplied invoker so the host can apply its normal policy

#### Scenario: Accelerator resolution stays shared
- **WHEN** a registered accelerator is resolved outside the Shell
- **THEN** the shared builder SHALL return the same scoped action id used to build its toolbar/menu entries

### Requirement: Host-bound actions reject expired owners
EditorShell's asset-viewer action callbacks SHALL remain safe when process registry entries or copied EditorAction records outlive their creating shell. The shell SHALL invalidate its owner-lifetime signal before child/member teardown. Expired-owner enabled predicates SHALL return false and invoke handlers SHALL perform no shell/viewer access. Callback invocation SHALL NOT retain the owner signal in a way that makes an already-destroyed shell appear live during reentrant dispatch.

#### Scenario: Independent viewer consumes actions after shell destruction
- **WHEN** a shell is destroyed and another viewer builds toolbar/menu state from the retained registry entries
- **THEN** enabled predicates SHALL not dereference the expired shell and its actions SHALL be inert

#### Scenario: Copied action outlives its owner
- **WHEN** a consumer retains an EditorAction copy after the creating shell is destroyed
- **THEN** evaluating its enabled predicate or invoking it directly SHALL perform no access to the expired owner

#### Scenario: Older owner dies after action replacement
- **WHEN** another live shell replaces the same scoped action IDs before the former owner is destroyed
- **THEN** former-owner teardown SHALL preserve the newer registrations and old callback copies SHALL become inert independently

#### Scenario: Live shell rebuilds its viewer tree
- **WHEN** a live shell rebuilds its owned viewer/chrome while its actions remain registered
- **THEN** action predicates SHALL observe an absent target during destruction/reconstruction and SHALL NOT dereference the previous viewer

### Requirement: Unified dynamic action state

The editor action contract SHALL expose enabled, visible, checkable and checked state independently of UI and render implementations. Toolbar, menu and accelerator dispatch SHALL consume that same state. A hidden action SHALL occupy no chrome geometry and SHALL NOT invoke. A disabled action SHALL use disabled visual styling and SHALL NOT invoke; its registered accelerator SHALL retain consumed-input semantics.

#### Scenario: Live state changes while a menu is open

- **WHEN** an action's enabled, checked or visible predicate changes and the host refreshes its chrome
- **THEN** toolbar and menu state update without reconstructing the widget tree
- **AND** a still-visible open menu remains open and hidden rows are excluded from layout and hit testing

#### Scenario: Checked and disabled drawing

- **WHEN** a checkable action is checked or an action is disabled
- **THEN** menu and toolbar CPU draw data represent the corresponding theme state
- **AND** neither pointer nor accelerator dispatch runs a disabled handler

### Requirement: Registration-safe state and dispatch

Action state resolution and invocation SHALL use copied registration snapshots with registration generation and active scope. After each predicate, the registry SHALL verify the exact registration and scope remain current before evaluating another callback or invoking a handler. A retained chrome entry SHALL NOT invoke a removed, replaced or differently scoped registration.

#### Scenario: Predicate replaces its own action

- **WHEN** an enabled, visible or checked predicate removes or replaces its registration
- **THEN** the current dispatch is rejected safely
- **AND** neither the stale handler nor the replacement handler executes through that snapshot

#### Scenario: Chrome callback outlives its registration

- **WHEN** a retained toolbar or menu callback is triggered after its action is removed, replaced or its active scope changes
- **THEN** the callback has no action effect

#### Scenario: Reentrant chrome refresh

- **WHEN** a state predicate replaces the chrome entries, recursively requests refresh or destroys its chrome owner
- **THEN** the current refresh stops or skips the recursive request without reading invalid item storage
- **AND** a surviving chrome remains refreshable on the next host boundary
