## ADDED Requirements

### Requirement: Toolkit-independent layout model
The editor layout SHALL be represented as a toolkit-independent data model: a tree of split, tab, and panel
nodes, where a panel node references a panel by id. The model SHALL contain no UI-toolkit or render types and
SHALL be usable without a window.

#### Scenario: Build and query a layout
- **WHEN** a layout is assembled from a split with two tab groups and queried
- **THEN** the model SHALL report the tree structure and the panel ids without requiring a window or GPU

### Requirement: Panel registry
The editor SHALL provide a panel registry that maps a panel id to its title, minimum size, and factory. A layout
SHALL reference panels only by id.

#### Scenario: Layout references registered panels
- **WHEN** a layout references a panel id
- **THEN** the registry SHALL resolve it to a panel definition, and unknown ids SHALL be reported rather than
  crashing

### Requirement: Layout operations
The layout model SHALL support split, move-to-area, tabify, close, set-ratio, and reset-to-default operations.

#### Scenario: Split and resize
- **WHEN** a tab node is split and the splitter ratio is changed
- **THEN** the model SHALL contain the new split with the requested ratio

#### Scenario: Move and tabify
- **WHEN** a panel is moved to another area or tabified with another panel
- **THEN** the panel node SHALL be re-parented into the target tab node

#### Scenario: Close and reset
- **WHEN** a panel is closed or the layout is reset to default
- **THEN** the layout SHALL reflect the closure or return to the default arrangement

### Requirement: Layout persistence
The editor SHALL serialize the layout to JSON with a version field and restore it on startup, and SHALL tolerate
layouts that reference unknown or missing panel ids.

#### Scenario: Save/restore round-trip
- **WHEN** a layout is saved and then restored
- **THEN** the restored layout SHALL be equivalent to the saved layout

#### Scenario: Stale layout
- **WHEN** a stored layout references a panel id that no longer exists
- **THEN** the editor SHALL skip that panel and still start with a valid layout

### Requirement: Layout interaction in the shell
The editor shell SHALL render the layout with draggable splitters and a tab bar, and SHALL place panels inside the
single main window by default.

#### Scenario: Splitter and tab interaction
- **WHEN** the user drags a splitter or activates/reorders a tab
- **THEN** the shell SHALL update the layout model and re-layout the affected areas

#### Scenario: No child OS window by default
- **WHEN** panels are docked in the default (non-floating) configuration
- **THEN** they SHALL be placed within the main window without creating child OS windows
