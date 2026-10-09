## ADDED Requirements

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
