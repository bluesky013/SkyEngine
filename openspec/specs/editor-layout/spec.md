# editor-layout Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
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
The layout model SHALL support split, move-to-area, tabify, dock-at-position, float, dock-floating, close,
set-ratio, and reset-to-default operations.

#### Scenario: Split and resize
- **WHEN** a tab node is split and the splitter ratio is changed
- **THEN** the model SHALL contain the new split with the requested ratio

#### Scenario: Move and tabify
- **WHEN** a panel is moved to another area or tabified with another panel
- **THEN** the panel node SHALL be re-parented into the target tab node

#### Scenario: Dock a panel at a drop position
- **WHEN** a panel is docked onto a target panel with a drop position
- **THEN** the model SHALL either tabify it (center) or insert it into a new tab split on the requested side of
  the target and normalize the parent split

#### Scenario: Float a docked panel
- **WHEN** a docked panel is floated
- **THEN** it SHALL be removed from the dock tree and added to the floating set

#### Scenario: Dock a floating panel
- **WHEN** a floating panel is docked
- **THEN** it SHALL be removed from the floating set and re-inserted into the dock tree

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

#### Scenario: Floating geometry persists
- **WHEN** a layout with floating panels and their window geometry is saved and restored
- **THEN** the restored layout SHALL reproduce the floating panels and their geometry

#### Scenario: Auto-save and restore per user
- **WHEN** the layout is changed
- **THEN** the editor SHALL auto-save it under the user-config path and SHALL load it on the next startup

#### Scenario: Load an older layout version
- **WHEN** a stored layout from an older version without floating data is loaded
- **THEN** the editor SHALL load it and start with a valid layout

### Requirement: Layout interaction in the shell
The editor shell SHALL render the layout with draggable splitters and a tab bar, and SHALL place panels inside the
single main window by default.

#### Scenario: Splitter and tab interaction
- **WHEN** the user drags a splitter or activates/reorders a tab
- **THEN** the shell SHALL update the layout model and re-layout the affected areas

#### Scenario: Drag-to-dock
- **WHEN** the user drags a tab over another panel area
- **THEN** the shell SHALL show a drop-zone highlight and, on drop, dock the panel at the indicated position
  (center = tabify, edge = split)

#### Scenario: Close a tab
- **WHEN** the user activates the close affordance on a tab
- **THEN** the shell SHALL close that panel through the layout model and collapse the emptied area

#### Scenario: No child OS window by default
- **WHEN** panels are docked in the default (non-floating) configuration
- **THEN** they SHALL be placed within the main window without creating child OS windows

### Requirement: Panel visibility through the layout model

The editor SHALL express panel visibility through the layout model: hiding a panel uses the model's close
operation, and showing a hidden panel re-adds it to the layout; the shell SHALL NOT track hidden panels itself.

#### Scenario: Hide a panel
- **WHEN** a panel is hidden (e.g. from the View menu)
- **THEN** it SHALL be removed from the layout model and the shell SHALL rebuild without it

#### Scenario: Show a hidden panel
- **WHEN** a previously hidden panel is shown
- **THEN** it SHALL be re-added to the layout model (split/tab) and the shell SHALL rebuild with it

### Requirement: Floating panel state in the layout model
The layout model SHALL represent floating panels as a set of entries (panel id plus window geometry) separate from
the dock tree, and a panel SHALL be either docked in the tree or floating, never both. The model SHALL contain no
UI-toolkit or render types.

#### Scenario: A panel is docked or floating
- **WHEN** a panel is floated or docked
- **THEN** the model SHALL report it in exactly one of the dock tree or the floating set

#### Scenario: Update floating geometry
- **WHEN** a floating panel is moved or resized
- **THEN** the model SHALL record its new geometry

#### Scenario: Geometry write-back from the window
- **WHEN** the renderer reports a floating window's move or resize
- **THEN** the model SHALL update that panel's geometry and remain the single serialized source

### Requirement: Dock target and drop zones
The layout model SHALL express docking as a target panel plus a drop position of center or one of the four edges,
and SHALL reject a drop that would dock a panel onto itself.

#### Scenario: Edge drop splits on the requested side
- **WHEN** a panel is dropped on the left, right, top, or bottom edge of a target
- **THEN** the model SHALL place the panel in a new tab split on that side of the target with the requested
  orientation

#### Scenario: Invalid drop
- **WHEN** a panel is dropped onto itself
- **THEN** the model SHALL make no change

### Requirement: Reset layout including floating panels
Resetting the layout SHALL restore the default dock arrangement and SHALL clear the floating set.

#### Scenario: Reset from a modified layout
- **WHEN** a layout has been rearranged and contains floating panels and it is reset
- **THEN** the model SHALL return to the default arrangement with no floating panels

