## REMOVED Requirements

### Requirement: Engine-drawn tool/menu bar
**Reason**: The flat "Show/Hide <panel>" item row is replaced by a real engine-drawn menu bar and status bar so
the chrome can host File/Edit/View/Window/Tools/Help commands.
**Migration**: Panel visibility and reset-layout actions move to the `View` menu; the `ToolBar` element and its
flat item list are removed from `EditorShell`.

## ADDED Requirements

### Requirement: Engine-drawn menu bar
The shell SHALL present an engine-drawn menu bar with labeled top-level menus (at least File, Edit, View, Window,
Tools, Help) drawn with `sky::ui` (not the OS), and selecting a menu item SHALL run its associated editor action.
The View menu SHALL expose panel visibility toggles, and the Window menu SHALL expose a reset-layout action.

#### Scenario: Open a menu and run an item
- **WHEN** the user opens a top-level menu and selects an item
- **THEN** the associated editor action SHALL run and the menu SHALL close

#### Scenario: View menu toggles a panel
- **WHEN** the user selects a panel entry in the View menu
- **THEN** that panel SHALL be hidden if shown, or shown if hidden, through the layout model

#### Scenario: Consistent menu bar across platforms
- **WHEN** the editor starts on Windows or macOS
- **THEN** the menu bar SHALL be rendered by the engine as `sky::ui` elements, without relying on an OS-native
  menu bar

### Requirement: Status bar
The shell SHALL present an engine-drawn status bar showing at least the project name, engine version, active RHI
backend, frame rate, edit mode, and selection count, resolved from existing editor services and not from
hard-coded constants.

#### Scenario: Status bar reflects state
- **WHEN** the selection or the frame rate changes
- **THEN** the status bar SHALL display the updated value on the next paint

### Requirement: Drag-to-dock interaction
The shell SHALL provide a drag interaction for panel tabs: while a tab is dragged the shell SHALL highlight the
candidate drop zone, and on release it SHALL dock the panel at the indicated position or float it when dropped
outside the main window. Gesture hit areas SHALL be discrete elements (splitter handles and tab headers) so panel
bodies remain directly hittable; the drop-zone highlight SHALL exist only while a drag is active. All gesture
hit-testing SHALL use the same coordinate space as the layout (device pixels), independent of the paint scale.

#### Scenario: Drop zone highlight
- **WHEN** the user drags a tab over a panel area
- **THEN** the shell SHALL show a drop-zone highlight indicating center or a side

#### Scenario: Float on drop outside
- **WHEN** the user drops a dragged tab outside the main window
- **THEN** the shell SHALL float the panel into its own native window

#### Scenario: Panel input is not swallowed
- **WHEN** the pointer is over a panel body and not over a splitter handle or tab header
- **THEN** the panel SHALL receive the event directly (no full-area overlay intercepts it)

### Requirement: Per-window input routing
The shell SHALL route keyboard and text input to the `UIContext` of the window identified by the event's
`winID`, and SHALL route non-drag pointer input to that window's own event router so each window keeps its own
focus and hover state. For a drag that may cross windows (tab reorder, drag-to-dock, tear-out), the shell SHALL
use OS-level pointer capture and screen coordinates so the drag continues when the pointer leaves the source
window.

#### Scenario: Keyboard goes to the focused window
- **WHEN** a key event arrives for a floating window's `winID`
- **THEN** the shell SHALL dispatch it to that window's `UIContext` router and not to the main window

#### Scenario: Drag leaves the window
- **WHEN** the user drags a tab and the pointer moves outside the source window while the button is held
- **THEN** the shell SHALL keep receiving the drag (via pointer capture / screen coordinates) and SHALL be able
  to drop outside the window
