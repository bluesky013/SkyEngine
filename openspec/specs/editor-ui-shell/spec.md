# editor-ui-shell Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Panels built on the in-house UI framework
Editor panels SHALL be authored as `engine/ui` (`sky::ui`) element trees and SHALL NOT use OS-native widgets or
an external UI toolkit.

#### Scenario: Panel is a sky::ui tree
- **WHEN** the editor creates a panel
- **THEN** the panel SHALL be composed of `sky::ui` elements and emit draw data through the UI render pass

### Requirement: Docking provided by editor-layout
The shell SHALL arrange panels according to the `editor-layout` model. Panel docking, tabs, splits, and layout
persistence are specified by the `editor-layout` capability.

#### Scenario: Shell renders the layout model
- **WHEN** the layout model defines split and tab areas
- **THEN** the shell SHALL place the panels accordingly and route input to the active panel

### Requirement: Engine-drawn menus and theming
The editor menu bar and toolbars SHALL be drawn by the engine with `sky::ui` (not the OS), so they are consistent
across Windows and macOS, and SHALL resolve appearance from the `sky::ui` style/theme rather than hard-coded
values.

#### Scenario: Consistent menu bar
- **WHEN** the editor starts on Windows or macOS
- **THEN** the menu bar SHALL be rendered by the engine as `sky::ui` elements, without relying on an OS-native
  menu bar

#### Scenario: Theme change affects panels
- **WHEN** the active theme is changed
- **THEN** subsequent paints of shell and panel elements SHALL use the new theme values

### Requirement: Focus and input routing
The shell SHALL route pointer and keyboard input to the focused panel, and SHALL report whether a UI surface
wants input so game/viewport input is gated while UI is active.

#### Scenario: UI gating
- **WHEN** a focused or modal UI surface is active
- **THEN** the shell SHALL report that it wants input and the viewport SHALL not receive that input

### Requirement: Shell composed from the core layout and panel registry

The editor shell SHALL build its `sky::ui` element tree from the `editor-layout` model and the `PanelRegistry`
owned by `EditorCore`, and SHALL NOT hardcode panel content or placement. Panel ids that have no registered view
SHALL be skipped without failing.

#### Scenario: Panels follow the default layout
- **WHEN** the editor starts with the default layout and the registered core panel ids
- **THEN** the shell SHALL create a `sky::ui` element subtree for each panel id present in the layout

#### Scenario: Unknown panel id is skipped
- **WHEN** the layout references a panel id that has no registered view
- **THEN** the shell SHALL skip it and continue building the rest of the shell

### Requirement: Input routed through the UI event router

Platform window pointer and keyboard events SHALL be forwarded to the shell, which SHALL dispatch them through the
`sky::ui` `UIEventRouter`, and SHALL expose `UIContext::WantsInput()` so the host can gate non-UI input.

#### Scenario: Pointer routes to the topmost element
- **WHEN** a pointer event occurs over overlapping UI elements
- **THEN** the router SHALL dispatch it to the topmost visible element under the point

#### Scenario: Wants input reported
- **WHEN** a focused or modal UI surface is active
- **THEN** `UIContext::WantsInput()` SHALL be true

### Requirement: Viewport input gating

While the shell reports that it wants input, the editor viewport SHALL NOT receive the pointer/keyboard input.

#### Scenario: UI captures input
- **WHEN** the shell reports `WantsInput()` and a pointer event arrives over the UI
- **THEN** the viewport SHALL NOT receive that event

### Requirement: Theme-driven panel styling

The editor shell SHALL style its surfaces through the UI theme (`UITheme`/`UIStyle`), so panel, title, menu and
button colors come from a single theme rather than hardcoded per-panel values.

#### Scenario: Panels resolve colors from the theme
- **WHEN** a panel is painted
- **THEN** its background and title colors SHALL come from the theme's resolved style for the panel's classes

#### Scenario: Changing the theme changes the shell
- **WHEN** a theme style value is changed
- **THEN** the affected shell surfaces SHALL render with the new value, with no per-panel constant to change

### Requirement: Tab headers switch the active panel

A tab that contains more than one panel SHALL render a header row of its panel titles, and selecting a header SHALL
make that panel the tab's active panel and show its view.

#### Scenario: Switch tabs
- **WHEN** the user selects a panel title in a multi-panel tab's header
- **THEN** that panel SHALL become active and its view SHALL be shown

#### Scenario: Only the active panel body is shown
- **WHEN** a tab has multiple panels
- **THEN** only the active panel's body SHALL be laid out and painted

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

### Requirement: Preferences entry in the File menu
The File menu SHALL include a `Preferences…` command that opens the Preferences dialog, and the docked
`Config` panel SHALL no longer be part of the default layout or the View menu.

#### Scenario: Preferences command
- **WHEN** the user opens the File menu and selects `Preferences…`
- **THEN** the Preferences dialog SHALL open (see `editor-preferences`)

#### Scenario: Config panel retired
- **WHEN** the editor starts with the default layout
- **THEN** there SHALL be no `Config` panel/tab, and no `Config` entry in the View menu

### Requirement: Asset browser is a renderer

The asset browser panel SHALL render the catalog-built tree and item set and perform layout and hit
testing only; it SHALL NOT assemble the folder tree or query per-folder contents.

#### Scenario: Render core-built tree
- **WHEN** the panel displays the folder tree
- **THEN** it SHALL render the tree provided by the catalog without recursing per folder

#### Scenario: View holds no structure
- **WHEN** the catalog changes
- **THEN** the panel SHALL re-render from the catalog rather than maintaining its own folder model

### Requirement: Detail view shows targets and settings

The asset browser detail view SHALL show the active platform, the resolved cook targets, and, for
each target, the effective per-kind settings reported by the asset's builder (or the targets alone
when the kind reports no settings).

#### Scenario: Texture settings shown
- **WHEN** a texture is selected
- **THEN** the detail view SHALL show its targets and each target's effective settings (e.g. encode,
  srgb, max size, mip generation)

#### Scenario: No builder settings
- **WHEN** the asset's kind reports no settings
- **THEN** the detail view SHALL show the targets without settings and SHALL NOT error

### Requirement: Usable default bottom dock

The default layout SHALL give the bottom dock (asset browser + output log) a usable height, set
explicitly on the correct split rather than left to an accidental ratio.

#### Scenario: Bottom dock visible by default
- **WHEN** the editor starts with the default layout
- **THEN** the bottom dock SHALL show the asset browser with enough height to display items

### Requirement: Built-in asset browser panel

The shell SHALL register a built-in view for the `assets` panel id and SHALL include it in the default
layout, composed from the core layout and panel registry like the other built-in panels. The panel view
SHALL render the asset browser with the in-house `sky::ui` toolkit.

#### Scenario: Registered built-in view
- **WHEN** the shell registers its built-in panel views
- **THEN** an `assets` panel id SHALL have a view that renders the asset browser

#### Scenario: Present in the default layout
- **WHEN** the editor starts with the default layout
- **THEN** the `assets` panel SHALL be part of it and SHALL dock, float, and tab like the other panels

### Requirement: Editable per-asset cook settings in the asset browser

The asset browser details pane SHALL edit the selected asset's per-target cook settings through the generic
reflected form view (`ReflectedFormView`) bound to the catalog's effective reflected settings object with the
global-preset baseline, so that enums/bools/ints render as their standard typed controls and "reset"
restores the preset. Edits SHALL be committed through the asset catalog and persisted to the manifest; for
assets on a read-only mount the settings SHALL be shown non-editable.

#### Scenario: Bind the reflected form

- **WHEN** an asset with a builder-declared settings type is selected
- **THEN** the details pane binds a `ReflectedFormView` to the effective settings with the preset baseline

#### Scenario: Edit persists

- **WHEN** the user changes a value through the form
- **THEN** the catalog persists the resulting sparse override and the pane reflects the new effective value

#### Scenario: Reset restores the preset

- **WHEN** the user resets an overridden value through the form
- **THEN** the override key is removed and the shown value returns to the preset baseline

#### Scenario: Read-only mount

- **WHEN** the selected asset belongs to a read-only mount
- **THEN** the settings are displayed without editable controls

### Requirement: Shell hosts the asset viewer

`EditorShell` SHALL host the asset viewer widget as an overlay dialog (like the file browser), expose
`OpenAssetViewer(uuid)`, include it in modal input routing and layout, and SHALL route asset double-click /
`asset.open` to it when no type-specific `IEditorAssetEditor` is registered. The type-specific editor SHALL
take precedence when registered.

#### Scenario: Double-click opens the viewer

- **WHEN** an asset is double-clicked and no type-specific editor is registered
- **THEN** the shell opens the asset viewer for that uuid and it becomes the active modal

#### Scenario: Type-specific editor wins

- **WHEN** an `IEditorAssetEditor` is registered for the asset type
- **THEN** opening routes to that editor instead of the generic viewer

#### Scenario: Viewer participates in input routing

- **WHEN** the viewer is open
- **THEN** pointer/key/text input is routed to it (Escape closes) and normal panels do not receive it

