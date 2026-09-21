## ADDED Requirements

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
