## ADDED Requirements

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
