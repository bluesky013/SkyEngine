## ADDED Requirements

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
