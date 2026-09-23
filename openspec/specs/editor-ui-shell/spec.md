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

### Requirement: Engine-drawn tool/menu bar

The shell SHALL provide an engine-drawn tool/menu bar with labeled action items; selecting an item SHALL run the
associated editor action. Items SHALL be grouped (for example a View group whose items toggle panel visibility).

#### Scenario: Run a toolbar item
- **WHEN** the user selects an item in the tool/menu bar
- **THEN** the associated editor action SHALL run

#### Scenario: View group toggles a panel
- **WHEN** the user selects a panel item in the View group
- **THEN** that panel SHALL be hidden if shown, or shown if hidden

